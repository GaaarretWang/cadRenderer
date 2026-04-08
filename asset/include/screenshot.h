#pragma  once
#include <encoder.h>

enum ScreenshotHandlerType {
    ENCODER,
    DECODER,
    NONE
};


class ScreenshotHandler : public vsg::Inherit<vsg::Visitor, ScreenshotHandler>
{
public:
    int mFrameCount = 0;
    NvEncoderWrapper* m_encoder = nullptr;
    VkExtent2D m_extent;
    VkExtent2D m_encode_extent;
    // Constructor.
    ScreenshotHandler()
    {
    }

    ScreenshotHandler(vsg::ref_ptr<vsg::Window> window, VkExtent2D extent, VkExtent2D encode_extent, ScreenshotHandlerType type): m_extent(extent), m_encode_extent(encode_extent)
    {
        if(type != NONE){
            m_encoder = new NvEncoderWrapper();

            vsg::Instance* instance;
            try
            {
                instance = window->getOrCreateDevice()->getInstance();
            }
            catch(const vsg::Exception& e)
            {
                std::cerr << e.message << '\n';
                std::cout << "1111111111111111111" << std::endl;
            }
            m_encoder->initCuda(instance, window);

            if(type == ENCODER)
                m_encoder->initEncoder(m_extent, m_encode_extent);
            else if(type == DECODER){
                m_encoder->initDecoder(m_extent);
            }
        }
    }

    void decodeImage(vsg::ref_ptr<vsg::Window> window, std::vector<std::vector<uint8_t>>& vPacket)
    {
        auto width = m_extent.width;
        auto height = m_extent.height;

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();
        auto swapchain = window->getSwapchain();

        // First: decode with CUDA so decode_image has valid data before Vulkan reads from it
        m_encoder->decode(vPacket);
        cudaDeviceSynchronize();

        auto sourceImage = m_encoder->decode_image;

        VkFormat sourceImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkFormat targetImageFormat = sourceImageFormat;

        //
        // 1) Check to see if Blit is supported.
        //
        VkFormatProperties srcFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), sourceImageFormat, &srcFormatProperties);

        VkFormatProperties destFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_UNORM, &destFormatProperties);

        bool supportsBlit = ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
                            ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);

        if (supportsBlit)
        {
            // we can automatically convert the image format when blit, so take advantage of it to ensure RGBA
            targetImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        }

        // vsg::info("supportsBlit = ", supportsBlit);

        //
        // 3) create command buffer and submit to graphics queue
        //
        auto commands = vsg::Commands::create();

        // 3.a) transition destinationImage to transfer destination initialLayout
        auto transitionDestinationImageToDestinationLayoutBarrier = vsg::ImageMemoryBarrier::create(
            0,                                                             // srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,                                  // dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,                                     // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                          // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            m_encoder->output_image,                                              // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        // 3.b) transition decode_image from undefined to transfer source layout
        auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_MEMORY_READ_BIT,                                     // srcAccessMask
            VK_ACCESS_TRANSFER_READ_BIT,                                   // dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,                                     // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                          // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            sourceImage,                                                   // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        auto cmd_transitionForTransferBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,                       // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT,                       // dstStageMask
            0,                                                    // dependencyFlags
            transitionDestinationImageToDestinationLayoutBarrier, // barrier
            transitionSourceImageToTransferSourceLayoutBarrier    // barrier
        );

        commands->addChild(cmd_transitionForTransferBarrier);

        if (supportsBlit)
        {
            // 3.c.1) if blit using vkCmdBlitImage
            VkImageBlit region{};
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.layerCount = 1;
            region.srcOffsets[0] = VkOffset3D{0, 0, 0};
            region.srcOffsets[1] = VkOffset3D{static_cast<int32_t>(width), static_cast<int32_t>(height), 1};
            region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.layerCount = 1;
            region.dstOffsets[0] = VkOffset3D{0, 0, 0};
            region.dstOffsets[1] = VkOffset3D{static_cast<int32_t>(width), static_cast<int32_t>(height), 1};

            auto blitImage = vsg::BlitImage::create();
            blitImage->srcImage = sourceImage;
            blitImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blitImage->dstImage = m_encoder->output_image;
            blitImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blitImage->regions.push_back(region);
            blitImage->filter = VK_FILTER_NEAREST;

            commands->addChild(blitImage);
        }
        else
        {
            // 3.c.2) else use vkCmdCopyImage

            VkImageCopy region{};
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.layerCount = 1;
            region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.layerCount = 1;
            region.extent.width = width;
            region.extent.height = height;
            region.extent.depth = 1;

            auto copyImage = vsg::CopyImage::create();
            copyImage->srcImage = sourceImage;
            copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            copyImage->dstImage = m_encoder->output_image;
            copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copyImage->regions.push_back(region);

            commands->addChild(copyImage);
        }

        // 3.d) transition destination image from transfer destination layout to general layout to enable mapping to image DeviceMemory
        auto transitionDestinationImageToMemoryReadBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_TRANSFER_WRITE_BIT,                                  // srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,                                     // dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                          // oldLayout
            VK_IMAGE_LAYOUT_GENERAL,                                       // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            m_encoder->output_image,                                              // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        // 3.e) transition decode_image back to general layout
        auto transitionSourceImageBackToPresentBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_TRANSFER_READ_BIT,                                   // srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,                                     // dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                          // oldLayout
            VK_IMAGE_LAYOUT_GENERAL,                                       // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            sourceImage,                                                   // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        auto cmd_transitionFromTransferBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,                // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT,                // dstStageMask
            0,                                             // dependencyFlags
            transitionDestinationImageToMemoryReadBarrier, // barrier
            transitionSourceImageBackToPresentBarrier      // barrier
        );

        commands->addChild(cmd_transitionFromTransferBarrier);

        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) {
            commands->record(commandBuffer);
        });
    }

    vsg::ref_ptr<vsg::Image> screenshot_image(vsg::ref_ptr<vsg::Window> window)
    {
        auto width = m_extent.width;
        auto height = m_extent.height; // Read the window width and height.

        auto swapchain = window->getSwapchain(); // Fetch the swapchain associated with the window.
        VkFormat sourceImageFormat = swapchain->getImageFormat();

        // get the colour buffer image of the previous rendered frame as the current frame hasn't been rendered yet.  The 1 in window->imageIndex(1) means image from 1 frame ago.
        // The image index must stay at 0 here.
        auto sourceImage = window->imageView(window->imageIndex(0))->image; // Use the previous rendered frame as the source color image.
        return sourceImage;
    }

    vsg::ref_ptr<vsg::Image> screenshot_depth(vsg::ref_ptr<vsg::Window> window)
    {
        auto width = m_extent.width;
        auto height = m_extent.height; // Read the window size.

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice(); // Fetch the device and physical device.

        // Fetch the source image and format; sourceImage is the window depth image.
        vsg::ref_ptr<vsg::Image> sourceImage(window->getDepthImage());
        // sourceImage is the source image, sourceImageFormat is its format, and targetImageFormat is the destination format.
        VkFormat sourceImageFormat = window->depthFormat();
        return sourceImage;
    }

    void screenshot_cpuimage(vsg::ref_ptr<vsg::Window> window, uint8_t* &color)
    {
        auto width = m_extent.width;
        auto height = m_extent.height; // Read the window width and height.

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();
        auto swapchain = window->getSwapchain(); // Fetch the swapchain associated with the window.

        // get the colour buffer image of the previous rendered frame as the current frame hasn't been rendered yet.  The 1 in window->imageIndex(1) means image from 1 frame ago.
        // The image index must stay at 0 here.
        auto sourceImage = window->imageView(window->imageIndex(0))->image; // Use the previous rendered frame as the source color image.

        VkFormat sourceImageFormat = swapchain->getImageFormat();
        VkFormat targetImageFormat = sourceImageFormat; // Initialize the target format from the source image format.

        //
        // 1) Check to see if Blit is supported.
        // Query format properties for the source and destination formats.
        VkFormatProperties srcFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), sourceImageFormat, &srcFormatProperties);

        VkFormatProperties destFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_UNORM, &destFormatProperties);

        // Check whether blit operations are supported for the source and destination formats.
        bool supportsBlit = ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
                            ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);

        // Apply the blit support decision.
        if (supportsBlit)
        { // When blit is supported, switch the target format to RGBA.
            // we can automatically convert the image format when blit, so take advantage of it to ensure RGBA
            targetImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        }

        // vsg::info("supportsBlit = ", supportsBlit);
        //
        // 2) create image to write to
        // Create the vsg::Image used to store the output.
        auto destinationImage = vsg::Image::create();
        destinationImage->imageType = VK_IMAGE_TYPE_2D;
        destinationImage->format = targetImageFormat;
        destinationImage->extent.width = width;
        destinationImage->extent.height = height;
        destinationImage->extent.depth = 1;
        destinationImage->arrayLayers = 1;
        destinationImage->mipLevels = 1;
        destinationImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        destinationImage->samples = VK_SAMPLE_COUNT_1_BIT;
        destinationImage->tiling = VK_IMAGE_TILING_LINEAR;
        destinationImage->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        destinationImage->compile(device);

        // Create the device memory that backs the image data.
        auto deviceMemory = vsg::DeviceMemory::create(device, destinationImage->getMemoryRequirements(device->deviceID), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Bind the image to its device memory so the data can be read and written.
        // The zero here is the byte offset within the device-memory allocation.
        // It specifies where the image data starts in that allocation.
        destinationImage->bind(deviceMemory, 0);

        //
        // 3) create command buffer and submit to graphics queue
        // Create the command buffer and submit it to the graphics queue.
        auto commands = vsg::Commands::create(); // Create the command buffer.

        //if (event) // If an event exists, wait on it and then reset it.
        //{          // Add the wait/reset commands into the command buffer.
        //    vsg::info("Using vsg::Event/vkEvent");
        //    commands->addChild(vsg::WaitEvents::create(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, event));
        //    commands->addChild(vsg::ResetEvent::create(event, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT));
        //}

        // 3.a) transition destinationImage to transfer destination initialLayout
        // Transition the image layouts.
        //
        // These barriers transition destinationImage from UNDEFINED to transfer-destination layout,
        // and transition sourceImage from PRESENT_SRC to transfer-source layout before the copy.
        //
        //

        auto transitionDestinationImageToDestinationLayoutBarrier = vsg::ImageMemoryBarrier::create(
            0,                                                             // srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,                                  // dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,                                     // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                          // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            destinationImage,                                              // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        // 3.b) transition swapChainImage from present to transfer source initialLayout
        auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_MEMORY_READ_BIT,                                     // srcAccessMask
            VK_ACCESS_TRANSFER_READ_BIT,                                   // dstAccessMask
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                               // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                          // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            sourceImage,                                                   // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        auto cmd_transitionForTransferBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,                       // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT,                       // dstStageMask
            0,                                                    // dependencyFlags
            transitionDestinationImageToDestinationLayoutBarrier, // barrier
            transitionSourceImageToTransferSourceLayoutBarrier    // barrier
        );

        commands->addChild(cmd_transitionForTransferBarrier);

        // Choose either a blit or a direct image copy based on platform support.
        if (supportsBlit)
        {
            // 3.c.1) If blit is supported, use vkCmdBlitImage.
            VkImageBlit region{};
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.layerCount = 1;
            region.srcOffsets[0] = VkOffset3D{0, 0, 0};
            region.srcOffsets[1] = VkOffset3D{static_cast<int32_t>(width), static_cast<int32_t>(height), 1};
            region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.layerCount = 1;
            region.dstOffsets[0] = VkOffset3D{0, 0, 0};
            region.dstOffsets[1] = VkOffset3D{static_cast<int32_t>(width), static_cast<int32_t>(height), 1};

            auto blitImage = vsg::BlitImage::create();
            blitImage->srcImage = sourceImage;
            blitImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blitImage->dstImage = destinationImage;
            blitImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blitImage->regions.push_back(region);
            blitImage->filter = VK_FILTER_NEAREST;

            commands->addChild(blitImage);
        }
        else
        {
            // 3.c.2) Otherwise fall back to vkCmdCopyImage.

            VkImageCopy region{};
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.layerCount = 1;
            region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.layerCount = 1;
            region.extent.width = width;
            region.extent.height = height;
            region.extent.depth = 1;

            auto copyImage = vsg::CopyImage::create();
            copyImage->srcImage = sourceImage;
            copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            copyImage->dstImage = destinationImage;
            copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copyImage->regions.push_back(region);

            commands->addChild(copyImage);
        }

        // 3.d) transition destination image from transfer destination layout to general layout to enable mapping to image DeviceMemory
        // Transition the image layouts again after the copy.
        // destinationImage moves to GENERAL so its memory can be mapped,
        // and sourceImage moves back to PRESENT_SRC_KHR.
        //
        auto transitionDestinationImageToMemoryReadBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_TRANSFER_WRITE_BIT,                                  // srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,                                     // dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                          // oldLayout
            VK_IMAGE_LAYOUT_GENERAL,                                       // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            destinationImage,                                              // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        // 3.e) transition swap chain image back to present
        auto transitionSourceImageBackToPresentBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_TRANSFER_READ_BIT,                                   // srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,                                     // dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                          // oldLayout
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                               // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            sourceImage,                                                   // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        auto cmd_transitionFromTransferBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,                // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT,                // dstStageMask
            0,                                             // dependencyFlags
            transitionDestinationImageToMemoryReadBarrier, // barrier
            transitionSourceImageBackToPresentBarrier      // barrier
        );

        commands->addChild(cmd_transitionFromTransferBarrier);

        // Submit the command buffer.
        // A fence is used for synchronization, then the graphics queue and command pool are fetched.
        //
        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        // Submit the recorded commands to the graphics queue.
        // The lambda receives the commandBuffer used to record the commands.
        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) {
            commands->record(commandBuffer);
        });

        //
        // 4) Map the image and copy its contents out.
        // Query the image subresource layout.
        VkImageSubresource subResource{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
        VkSubresourceLayout subResourceLayout;
        // vkGetImageSubresourceLayout returns the row pitch and other layout details for the target image.
        //
        vkGetImageSubresourceLayout(*device, destinationImage->vk(device->deviceID), &subResource, &subResourceLayout);

        // Create the host-visible image data object from the layout information.
        size_t destRowWidth = width * sizeof(vsg::ubvec4); // Byte width of one target-image row.
        //vsg::ref_ptr<vsg::Data> imageData;
        vsg::ref_ptr<vsg::ubvec4Array2D> imageData;
        if (destRowWidth == subResourceLayout.rowPitch) // Compare the actual row pitch against the packed row width.
        {
            // When they match, the image data is tightly packed and can be mapped directly as ubvec4Array2D.
            imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{targetImageFormat}, width, height); // deviceMemory, offset, flags and dimensions
            //auto imageData = vsg::MappedData<vsg::floatArray2D>::create(destinationMemory, 0, 0, vsg::Data::Properties{ targetImageFormat }, width, height);

            //vsg::ubvec4Array2D& colorMapData = *imageData;
            // Save the mapped color data as a PNG image.
            //saveColorMapImage('./', colorMapData);
            //ubvec4Array2DToMat(colorMapData, partnum);
        }
        else
        { // When the row pitch differs, map a byte array first and then copy into a tightly packed 2D image.
            // Map the buffer memory and assign as a ubyteArray that will automatically unmap itself on destruction.
            // A ubyteArray is used as the graphics buffer memory is not contiguous like vsg::Array2D, so map to a flat buffer first then copy to Array2D.
            auto mappedData = vsg::MappedData<vsg::ubyteArray>::create(deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{targetImageFormat}, subResourceLayout.rowPitch * height);
            imageData = vsg::ubvec4Array2D::create(width, height, vsg::Data::Properties{targetImageFormat});
            for (uint32_t row = 0; row < height; ++row)
            {
                std::memcpy(imageData->dataPointer(row * width), mappedData->dataPointer(row * subResourceLayout.rowPitch), destRowWidth);
            }
        }
        
        uint8_t* beginPointer = static_cast<uint8_t*>(imageData->dataPointer(0));
        std::copy(beginPointer, beginPointer + width * height * 4, color);

        // std::vector<uint8_t> color_image(3 * width * height);
        // for (int i = 0; i < width * height; i++)
        // {
        //     auto color = static_cast<vsg::ubvec4*>(imageData->dataPointer(i));
        //     color_image[i * 3] = color->x;
        //     color_image[i * 3 + 1] = color->y;
        //     color_image[i * 3 + 2] = color->z;
        // }
        // color = color_image.data();
        
        // size = 3 * width * height;
        // std::ofstream outputFile("imageData.txt");
        // for (int i = 0; i < width * height; i++)
        // {
        //     auto color = static_cast<vsg::ubvec4*>(imageData->dataPointer(i));
        //     outputFile << static_cast<int>(color->x) << " "
        //                << static_cast<int>(color->y) << " "
        //                << static_cast<int>(color->z) << " "
        //                << static_cast<int>(color->w) << std::endl;
        // }
        // outputFile.close();
        // int a = 0;
        // std::cout << "done!" << std::endl;
        // std::cin >> a;
    }

    void encodeImage(vsg::ref_ptr<vsg::Window> window, std::vector<std::vector<uint8_t>>& vPacket){
        auto width = m_extent.width;
        auto height = m_extent.height;

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();
        auto swapchain = window->getSwapchain();

        // get the colour buffer image of the previous rendered frame as the current frame hasn't been rendered yet.  The 1 in window->imageIndex(1) means image from 1 frame ago.
        auto sourceImage = window->imageView(window->imageIndex(0))->image;

        VkFormat sourceImageFormat = swapchain->getImageFormat();
        VkFormat targetImageFormat = sourceImageFormat;

        //
        // 1) Check to see if Blit is supported.
        //
        VkFormatProperties srcFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), sourceImageFormat, &srcFormatProperties);

        VkFormatProperties destFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_UNORM, &destFormatProperties);

        bool supportsBlit = ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
                            ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);

        if (supportsBlit)
        {
            // we can automatically convert the image format when blit, so take advantage of it to ensure RGBA
            targetImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        }

        //
        // 2) create image to write to
        //

        //
        // 3) create command buffer and submit to graphics queue
        //
        auto commands = vsg::Commands::create();

        // 3.a) transition destinationImage to transfer destination initialLayout
        auto transitionDestinationImageToDestinationLayoutBarrier = vsg::ImageMemoryBarrier::create(
            0,                                                             // srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,                                  // dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,                                     // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                          // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            m_encoder->encode_destination_image,                           // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        // 3.b) transition swapChainImage from present to transfer source initialLayout
        auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_MEMORY_READ_BIT,                                     // srcAccessMask
            VK_ACCESS_TRANSFER_READ_BIT,                                   // dstAccessMask
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                               // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                          // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            sourceImage,                                                   // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        auto cmd_transitionForTransferBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,                       // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT,                       // dstStageMask
            0,                                                    // dependencyFlags
            transitionDestinationImageToDestinationLayoutBarrier, // barrier
            transitionSourceImageToTransferSourceLayoutBarrier    // barrier
        );

        commands->addChild(cmd_transitionForTransferBarrier);

        if (supportsBlit)
        {
            // 3.c.1) if blit using vkCmdBlitImage
            VkImageBlit region{};
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.layerCount = 1;
            region.srcOffsets[0] = VkOffset3D{0, 0, 0};
            region.srcOffsets[1] = VkOffset3D{static_cast<int32_t>(width), static_cast<int32_t>(height), 1};
            region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.layerCount = 1;
            region.dstOffsets[0] = VkOffset3D{0, 0, 0};
            region.dstOffsets[1] = VkOffset3D{static_cast<int32_t>(width), static_cast<int32_t>(height), 1};

            auto blitImage = vsg::BlitImage::create();
            blitImage->srcImage = sourceImage;
            blitImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blitImage->dstImage = m_encoder->encode_destination_image;
            blitImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blitImage->regions.push_back(region);
            blitImage->filter = VK_FILTER_NEAREST;

            commands->addChild(blitImage);
        }
        else
        {
            // 3.c.2) else use vkCmdCopyImage

            VkImageCopy region{};
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.layerCount = 1;
            region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.layerCount = 1;
            region.extent.width = width;
            region.extent.height = height;
            region.extent.depth = 1;

            auto copyImage = vsg::CopyImage::create();
            copyImage->srcImage = sourceImage;
            copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            copyImage->dstImage = m_encoder->encode_destination_image;
            copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copyImage->regions.push_back(region);

            commands->addChild(copyImage);
        }

        // 3.d) transition destination image from transfer destination layout to general layout to enable mapping to image DeviceMemory
        auto transitionDestinationImageToMemoryReadBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_TRANSFER_WRITE_BIT,                                  // srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,                                     // dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                          // oldLayout
            VK_IMAGE_LAYOUT_GENERAL,                                       // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            m_encoder->encode_destination_image,                                              // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        // 3.e) transition swap chain image back to present
        auto transitionSourceImageBackToPresentBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_TRANSFER_READ_BIT,                                   // srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,                                     // dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                          // oldLayout
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                               // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            sourceImage,                                                   // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
        );

        auto cmd_transitionFromTransferBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,                // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT,                // dstStageMask
            0,                                             // dependencyFlags
            transitionDestinationImageToMemoryReadBarrier, // barrier
            transitionSourceImageBackToPresentBarrier      // barrier
        );

        commands->addChild(cmd_transitionFromTransferBarrier);

        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) { // Wait up to 100000000000.
            commands->record(commandBuffer);
        });

        // auto bufferSize = encodeSurfaces[i].vulkanImage->getMemoryRequirements(device->deviceID).size;            
        // Cudaimage* cuImage = new Cudaimage(encodeSurfaces[i].vulkanImage,
        //         device, bufferSize, extent);
        return m_encoder->encode(vPacket);
    }

    void screenshot_cpudepth(vsg::ref_ptr<vsg::Window> window)
    {
        auto width = m_extent.width;
        auto height = m_extent.height; // Read the window size.

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice(); // Fetch the device and physical device.

        // Fetch the source image and format; sourceImage is the current window depth image.
        vsg::ref_ptr<vsg::Image> sourceImage(window->getDepthImage());
        // sourceImage is the source image, sourceImageFormat is its format, and targetImageFormat is the destination format.
        VkFormat sourceImageFormat = window->depthFormat();
        VkFormat targetImageFormat = sourceImageFormat;

        auto memoryRequirements = sourceImage->getMemoryRequirements(device->deviceID);

        // 1. Create the destination buffer used to receive the copied depth image.
        // vsg::createBufferAndMemory sets up the buffer size, usage flags, and memory properties.
        VkDeviceSize bufferSize = memoryRequirements.size;
        auto destinationBuffer = vsg::createBufferAndMemory(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        auto destinationMemory = destinationBuffer->getDeviceMemory(device->deviceID);

        VkImageAspectFlags imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT; // | VK_IMAGE_ASPECT_STENCIL_BIT; // need to match imageAspectFlags setting to WindowTraits::depthFormat.

        // 2.a) transition depth image for reading
        // Transition image layouts and copy the source image into the destination buffer.
        auto commands = vsg::Commands::create();

        auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // srcAccessMask
            VK_ACCESS_TRANSFER_READ_BIT,                                                                // dstAccessMask
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,                                           // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                                                       // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                                                    // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                                                    // dstQueueFamilyIndex
            sourceImage,                                                                                // image
            VkImageSubresourceRange{imageAspectFlags, 0, 1, 0, 1}                                       // subresourceRange
        );

        auto transitionDestinationBufferToTransferWriteBarrier = vsg::BufferMemoryBarrier::create(
            VK_ACCESS_MEMORY_READ_BIT,    // srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask
            VK_QUEUE_FAMILY_IGNORED,      // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,      // dstQueueFamilyIndex
            destinationBuffer,            // buffer
            0,                            // offset
            bufferSize                    // size
        );

        auto cmd_transitionSourceImageToTransferSourceLayoutBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT,                                                            // dstStageMask
            0,                                                                                         // dependencyFlags
            transitionSourceImageToTransferSourceLayoutBarrier,                                        // barrier
            transitionDestinationBufferToTransferWriteBarrier                                          // barrier
        );
        commands->addChild(cmd_transitionSourceImageToTransferSourceLayoutBarrier);

        // 2.b) copy image to buffer
        {
            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = width; // need to figure out actual row length from somewhere...
            region.bufferImageHeight = height;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = VkOffset3D{0, 0, 0};
            region.imageExtent = VkExtent3D{width, height, 1};

            auto copyImage = vsg::CopyImageToBuffer::create();
            copyImage->srcImage = sourceImage;
            copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            copyImage->dstBuffer = destinationBuffer;
            copyImage->regions.push_back(region);

            commands->addChild(copyImage);
        }

        //2.c) transition depth image back for rendering
        auto transitionSourceImageBackToPresentBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_TRANSFER_READ_BIT,                                                                // srcAccessMask
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                                                       // oldLayout
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,                                           // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                                                    // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                                                    // dstQueueFamilyIndex
            sourceImage,                                                                                // image
            VkImageSubresourceRange{imageAspectFlags, 0, 1, 0, 1}                                       // subresourceRange
        );

        auto transitionDestinationBufferToMemoryReadBarrier = vsg::BufferMemoryBarrier::create(
            VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,    // dstAccessMask
            VK_QUEUE_FAMILY_IGNORED,      // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,      // dstQueueFamilyIndex
            destinationBuffer,            // buffer
            0,                            // offset
            bufferSize                    // size
        );

        auto cmd_transitionSourceImageBackToPresentBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,                                                            // srcStageMask
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
            0,                                                                                         // dependencyFlags
            transitionSourceImageBackToPresentBarrier,                                                 // barrier
            transitionDestinationBufferToMemoryReadBarrier                                             // barrier
        );

        commands->addChild(cmd_transitionSourceImageBackToPresentBarrier);

        // Submit the command buffer for execution.
        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) {
            commands->record(commandBuffer);
        });

        // 3. Map the buffer and copy data out of it.
        //
        // Map the buffer memory and assign as a vec4Array2D that will automatically unmap itself on destruction.
        if (targetImageFormat == VK_FORMAT_D32_SFLOAT || targetImageFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
        { // Handle VK_FORMAT_D32_SFLOAT-style depth formats.
            // Create the mapped data object.
            // For floating-point depth formats, map the data as a floatArray2D.
            auto imageData = vsg::MappedData<vsg::floatArray2D>::create(destinationMemory, 0, 0, vsg::Data::Properties{targetImageFormat}, width, height); // deviceMemory, offset, flags and dimensions
            // std::ofstream outputFile("imageData.txt");
            // for (int i = 0; i < width * height; i++)
            // {
            //     auto color = static_cast<float*>(imageData->dataPointer(i));
            //     outputFile << static_cast<int>(*color * 255555) << " "
            //                << static_cast<int>(*color * 255555) << " "
            //                << static_cast<int>(*color * 255555) << " "
            //                << static_cast<int>(255) << std::endl;
            // }
            // outputFile.close();
            // int a = 0;
            // std::cout << "done!" << std::endl;
            // std::cin >> a;
        }
        else                                                                                                                                              // Handle non-floating-point depth formats.
        {                                                                                                                                                 // For those formats, map the data as a ushortArray2D.
            auto imageData = vsg::MappedData<vsg::ushortArray2D>::create(destinationMemory, 0, 0, vsg::Data::Properties{targetImageFormat}, width, height); // deviceMemory, offset, flags and dimensions
            std::ofstream outputFile("imageData.txt");
            for (int i = 0; i < width * height; i++)
            {
                auto color = static_cast<uint16_t*>(imageData->dataPointer(i));
                outputFile << static_cast<int>(*color) << " "
                           << static_cast<int>(*color) << " "
                           << static_cast<int>(*color) << " "
                           << static_cast<int>(255) << std::endl;
            }
            outputFile.close();
            int a = 0;
            std::cout << "done!" << std::endl;
            std::cin >> a;

        }
    }
};
