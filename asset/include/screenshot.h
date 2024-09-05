#include <encoder.h>

class ScreenshotHandler : public vsg::Inherit<vsg::Visitor, ScreenshotHandler>
{
public:
    int mFrameCount = 0;
    NvEncoderWrapper* m_encoder = nullptr;
    //构造函数
    ScreenshotHandler()
    {
    }

    ScreenshotHandler(vsg::ref_ptr<vsg::Window> window, VkExtent2D extent)
    {
        m_encoder = new NvEncoderWrapper();
        m_encoder->initCuda(window->getOrCreateDevice()->getInstance(), window);
        m_encoder->initEncoder(extent);
        m_encoder->initDecoder();
    }

    vsg::ref_ptr<vsg::Image> screenshot_image(vsg::ref_ptr<vsg::Window> window)
    {
        auto width = window->extent2D().width;
        auto height = window->extent2D().height; //获取窗口的宽度和高度

        auto swapchain = window->getSwapchain(); //获取与窗口相关的设备、物理设备和交换链
        VkFormat sourceImageFormat = swapchain->getImageFormat();

        // get the colour buffer image of the previous rendered frame as the current frame hasn't been rendered yet.  The 1 in window->imageIndex(1) means image from 1 frame ago.
        // 下标要为0
        auto sourceImage = window->imageView(window->imageIndex(0))->image; //获取之前渲染帧的颜色缓冲图像作为当前帧的来源图像 window->imageIndex(1)表示获取之前一帧的图像
        return sourceImage;
    }

    vsg::ref_ptr<vsg::Image> screenshot_depth(vsg::ref_ptr<vsg::Window> window)
    {
        auto width = window->extent2D().width;
        auto height = window->extent2D().height; //获取窗口大小

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice(); //获取设备和物理设备

        //获取源图像和图像格式 sourceImage 是当前窗口渲染绘制的画面的深度图像
        vsg::ref_ptr<vsg::Image> sourceImage(window->getDepthImage());
        //sourceImage表示源图像，sourceImageFormat表示源图像的格式 targetImageFormat表示目标图像的格式
        VkFormat sourceImageFormat = window->depthFormat();
        return sourceImage;
    }

    void screenshot_cpuimage(vsg::ref_ptr<vsg::Window> window, uint8_t* &color)
    {
        auto width = window->extent2D().width;
        auto height = window->extent2D().height; //获取窗口的宽度和高度

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();
        auto swapchain = window->getSwapchain(); //获取与窗口相关的设备、物理设备和交换链

        // get the colour buffer image of the previous rendered frame as the current frame hasn't been rendered yet.  The 1 in window->imageIndex(1) means image from 1 frame ago.
        // 下标要为0
        auto sourceImage = window->imageView(window->imageIndex(0))->image; //获取之前渲染帧的颜色缓冲图像作为当前帧的来源图像 window->imageIndex(1)表示获取之前一帧的图像

        VkFormat sourceImageFormat = swapchain->getImageFormat();
        VkFormat targetImageFormat = sourceImageFormat; //获取源图像和目标图像的格式，并将目标图像格式初始化为源图像格式

        //
        // 1) Check to see if Blit is supported.
        //获取源图像格式和目标图像格式的属性
        VkFormatProperties srcFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), sourceImageFormat, &srcFormatProperties);

        VkFormatProperties destFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_UNORM, &destFormatProperties);

        //检查是否支持图像拷贝操作（Blit）。它通过检查源图像格式和目标图像格式的属性来确定是否支持Blit操作
        bool supportsBlit = ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
                            ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);

        //确定是否支持Blit操作
        if (supportsBlit)
        { //如果支持Blit操作，则将目标图像格式设置为VK_FORMAT_R8G8B8A8_UNORM，以确保输出图像为RGBA格式
            // we can automatically convert the image format when blit, so take advantage of it to ensure RGBA
            targetImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        }

        // vsg::info("supportsBlit = ", supportsBlit);

        //
        // 2) create image to write to
        //创建一个用于存储输出图像的vsg::Image对象
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

        //创建用于存储图像数据的设备内存对象
        auto deviceMemory = vsg::DeviceMemory::create(device, destinationImage->getMemoryRequirements(device->deviceID), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        //将vsg::Image对象与设备内存对象进行绑定，以便进行图像数据的读写操作。
        //绑定后，图像数据可以在设备内存和主机内存之间进行传输。其中数字0代表绑定的偏移量（offset）
        //以便在设备内存中的特定位置开始存储图像数据。偏移量是以字节为单位的整数值
        destinationImage->bind(deviceMemory, 0);

        //
        // 3) create command buffer and submit to graphics queue
        //创建命令缓冲区（command buffer）并将其提交给图形队列（graphics queue）
        auto commands = vsg::Commands::create(); //创建命令缓冲区

        //if (event) //是否存在事件（event），如果存在则执行等待事件和重置事件的操作
        //{          //在命令缓冲区中添加等待事件和重置事件的指令
        //    vsg::info("Using vsg::Event/vkEvent");
        //    commands->addChild(vsg::WaitEvents::create(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, event));
        //    commands->addChild(vsg::ResetEvent::create(event, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT));
        //}

        // 3.a) transition destinationImage to transfer destination initialLayout
        //转换图像布局
        //
        //这部分代码执行图像布局转换的操作。它使用`vsg::ImageMemoryBarrier`和`vsg::PipelineBarrier`类创建转换图像布局的屏障对象，
        //并将其添加到命令缓冲区中。其中，`transitionDestinationImageToDestinationLayoutBarrier`用于将`destinationImage`的布局从未
        //定义的布局转换为传输目标的初始布局，`transitionSourceImageToTransferSourceLayoutBarrier`用于将`sourceImage`的布局从呈现源
        //的布局转换为传输源的初始布局

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

        //图像拷贝或位块传输 据具体的需求和平台功能进行选择，将相应的图像拷贝或位块传输指令添加到命令缓冲区中
        if (supportsBlit)
        {
            // 3.c.1) if blit using vkCmdBlitImage 使用`vkCmdBlitImage`进行图像拷贝
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
            // 3.c.2) else use vkCmdCopyImage 使用`vkCmdCopyImage`进行图像拷贝

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
        //再次转换图像布局
        //使用`vsg::ImageMemoryBarrier`和`vsg::PipelineBarrier`类创建屏障对象，并将其添加到命令缓冲区中。
        //其中，`transitionDestinationImageToMemoryReadBarrier`用于将`destinationImage`的布局从传输目标布局转换为通用布局，
        // 以便将其映射到图像设备内存；`transitionSourceImageBackToPresentBarrier`用于将`sourceImage`的布局从传输源布局转换为呈现源布局
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

        //提交命令缓冲区
        //创建了一个`vsg::Fence`对象用于同步命令缓冲区的提交。然后获取图形队列的队列族索引和命令池对象，
        //接着通过`device->getQueue(queueFamilyIndex)`获取图形队列对象
        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        //将命令缓冲区提交到图形队列中 在这个函数中，使用lambda表达式对命令缓冲区进行记录（record）操作。
        //lambda表达式的参数是命令缓冲区对象commandBuffer，可以在其中添加其他需要执行的命令
        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) {
            commands->record(commandBuffer);
        });

        //
        // 4) map image and copy 将图像数据映射到内存中，并将其保存为图像文件
        //获取图像子资源布局
        VkImageSubresource subResource{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
        VkSubresourceLayout subResourceLayout;
        //这部分代码使用Vulkan函数`vkGetImageSubresourceLayout`获取目标图像的子资源布局。通过传递目标图像、子资源信息和布局结构体的指针，
        //该函数会返回目标图像在内存中的布局信息，包括行间距、像素间距等
        vkGetImageSubresourceLayout(*device, destinationImage->vk(device->deviceID), &subResource, &subResourceLayout);

        //根据图像的布局信息创建图像数据对象
        size_t destRowWidth = width * sizeof(vsg::ubvec4); //计算出目标图像每行的字节数（`destRowWidth`）
        //vsg::ref_ptr<vsg::Data> imageData;
        vsg::ref_ptr<vsg::ubvec4Array2D> imageData;
        if (destRowWidth == subResourceLayout.rowPitch) //内存布局的行间距（`subResourceLayout.rowPitch`）与计算的字节数进行比较
        {                                               //√
            //如果二者相等，说明图像数据是连续的，可以直接使用`vsg::MappedData`类创建一个二维数组对象（`vsg::ubvec4Array2D`）来保存图像数据
            imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{targetImageFormat}, width, height); // deviceMemory, offset, flags and dimensions
            //auto imageData = vsg::MappedData<vsg::floatArray2D>::create(destinationMemory, 0, 0, vsg::Data::Properties{ targetImageFormat }, width, height);

            //vsg::ubvec4Array2D& colorMapData = *imageData;
            // 保存颜色映射数据为 PNG 图像******************************************************************
            //saveColorMapImage('./', colorMapData);
            //ubvec4Array2DToMat(colorMapData, partnum);
        }
        else
        { //如果二者不相等，说明图像数据在内存中是非连续的，需要使用`vsg::MappedData`类创建一个字节数组对象（`vsg::ubyteArray`），然后将数据从字节数组复制到二维数组中
            // Map the buffer memory and assign as a ubyteArray that will automatically unmap itself on destruction.
            // A ubyteArray is used as the graphics buffer memory is not contiguous like vsg::Array2D, so map to a flat buffer first then copy to Array2D.
            auto mappedData = vsg::MappedData<vsg::ubyteArray>::create(deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{targetImageFormat}, subResourceLayout.rowPitch * height);
            imageData = vsg::ubvec4Array2D::create(width, height, vsg::Data::Properties{targetImageFormat});
            for (uint32_t row = 0; row < height; ++row)
            {
                std::memcpy(imageData->dataPointer(row * width), mappedData->dataPointer(row * subResourceLayout.rowPitch), destRowWidth);
            }
        }

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

    void screenshot_encodeimage(vsg::ref_ptr<vsg::Window> window, std::vector<std::vector<uint8_t>> &color)
    {
        auto width = window->extent2D().width;
        auto height = window->extent2D().height; //获取窗口的宽度和高度

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();
        auto swapchain = window->getSwapchain(); //获取与窗口相关的设备、物理设备和交换链

        // get the colour buffer image of the previous rendered frame as the current frame hasn't been rendered yet.  The 1 in window->imageIndex(1) means image from 1 frame ago.
        // 下标要为0
        auto sourceImage = window->imageView(window->imageIndex(0))->image; //获取之前渲染帧的颜色缓冲图像作为当前帧的来源图像 window->imageIndex(1)表示获取之前一帧的图像

        VkFormat sourceImageFormat = swapchain->getImageFormat();
        VkFormat targetImageFormat = sourceImageFormat; //获取源图像和目标图像的格式，并将目标图像格式初始化为源图像格式

        //
        // 1) Check to see if Blit is supported.
        //获取源图像格式和目标图像格式的属性
        VkFormatProperties srcFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), sourceImageFormat, &srcFormatProperties);

        VkFormatProperties destFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_UNORM, &destFormatProperties);

        //检查是否支持图像拷贝操作（Blit）。它通过检查源图像格式和目标图像格式的属性来确定是否支持Blit操作
        bool supportsBlit = ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
                            ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);

        //确定是否支持Blit操作
        if (supportsBlit)
        { //如果支持Blit操作，则将目标图像格式设置为VK_FORMAT_R8G8B8A8_UNORM，以确保输出图像为RGBA格式
            // we can automatically convert the image format when blit, so take advantage of it to ensure RGBA
            targetImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        }

        // vsg::info("supportsBlit = ", supportsBlit);
        // if(m_encoder != nullptr)
        //     std::cout << m_encoder;

        
        // 2) create image to write to
        // 创建一个用于存储输出图像的vsg::Image对象
        CUarray inputArray = (CUarray)m_encoder->getEncoder()->GetNextInputFrame()->inputPtr;
		const DeviceAlloc* inputSurf = m_encoder->mapCUarrayToDeviceAlloc[inputArray];
        auto destinationImage = inputSurf->vulkanImage;

        
        // 3) create command buffer and submit to graphics queue
        // 创建命令缓冲区（command buffer）并将其提交给图形队列（graphics queue）
        auto commands = vsg::Commands::create(); //创建命令缓冲区

        // 3.a) transition destinationImage to transfer destination initialLayout
        //转换图像布局
        //
        //这部分代码执行图像布局转换的操作。它使用`vsg::ImageMemoryBarrier`和`vsg::PipelineBarrier`类创建转换图像布局的屏障对象，
        //并将其添加到命令缓冲区中。其中，`transitionDestinationImageToDestinationLayoutBarrier`用于将`destinationImage`的布局从未
        //定义的布局转换为传输目标的初始布局，`transitionSourceImageToTransferSourceLayoutBarrier`用于将`sourceImage`的布局从呈现源
        //的布局转换为传输源的初始布局

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

        //图像拷贝或位块传输 据具体的需求和平台功能进行选择，将相应的图像拷贝或位块传输指令添加到命令缓冲区中
        if (supportsBlit)
        {
            // 3.c.1) if blit using vkCmdBlitImage 使用`vkCmdBlitImage`进行图像拷贝
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
            // 3.c.2) else use vkCmdCopyImage 使用`vkCmdCopyImage`进行图像拷贝

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
        //再次转换图像布局
        //使用`vsg::ImageMemoryBarrier`和`vsg::PipelineBarrier`类创建屏障对象，并将其添加到命令缓冲区中。
        //其中，`transitionDestinationImageToMemoryReadBarrier`用于将`destinationImage`的布局从传输目标布局转换为通用布局，
        // 以便将其映射到图像设备内存；`transitionSourceImageBackToPresentBarrier`用于将`sourceImage`的布局从传输源布局转换为呈现源布局
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

        //提交命令缓冲区
        //创建了一个`vsg::Fence`对象用于同步命令缓冲区的提交。然后获取图形队列的队列族索引和命令池对象，
        //接着通过`device->getQueue(queueFamilyIndex)`获取图形队列对象
        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        //将命令缓冲区提交到图形队列中 在这个函数中，使用lambda表达式对命令缓冲区进行记录（record）操作。
        //lambda表达式的参数是命令缓冲区对象commandBuffer，可以在其中添加其他需要执行的命令
        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) {
            commands->record(commandBuffer);
        });
		inputSurf->cudaSemaphore->wait();
		std::vector<std::vector<uint8_t>> vPacket;
		m_encoder->getEncoder()->EncodeFrame(vPacket);
        if(vPacket.size() > 0){
            color = vPacket;
            // std::cout << vPacket.size() << std::endl;
            // std::cout << vPacket[0].size() << std::endl;
            // std::cout <<"send:" << std::endl;
            // for(int i = 0; i < vPacket[0].size(); i ++){
            //     std::cout << (int)vPacket[0][i] << " ";
            // }
            // std::cout <<std::endl;
            // int nFrameReturned = m_encoder->getDecoder()->Decode(vPacket[0].data(), vPacket[0].size(), 0, mFrameCount);
            // std::cout << "nFrameReturned" << nFrameReturned << std::endl;
        }
        // if (vPacket.size() > 0){
		// 	int nFrameReturned = m_encoder->getDecoder()->Decode(vPacket[0].data(), vPacket[0].size(), 0, mFrameCount ++);
        //     std::cout << "nFrameReturned" << nFrameReturned << std::endl;
        // }

    }

    void screenshot_cpudepth(vsg::ref_ptr<vsg::Window> window)
    {
        auto width = window->extent2D().width;
        auto height = window->extent2D().height; //获取窗口大小

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice(); //获取设备和物理设备

        //获取源图像和图像格式 sourceImage 是当前窗口渲染绘制的画面的深度图像
        vsg::ref_ptr<vsg::Image> sourceImage(window->getDepthImage());
        //sourceImage表示源图像，sourceImageFormat表示源图像的格式 targetImageFormat表示目标图像的格式
        VkFormat sourceImageFormat = window->depthFormat();
        VkFormat targetImageFormat = sourceImageFormat;

        auto memoryRequirements = sourceImage->getMemoryRequirements(device->deviceID);

        // 1. create buffer to copy to. 创建目标缓冲区 将深度图像数据复制到其中。
        // 通过vsg::createBufferAndMemory函数创建了一个缓冲区对象，并指定了缓冲区的大小、用途和内存属性
        VkDeviceSize bufferSize = memoryRequirements.size;
        auto destinationBuffer = vsg::createBufferAndMemory(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        auto destinationMemory = destinationBuffer->getDeviceMemory(device->deviceID);

        VkImageAspectFlags imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT; // | VK_IMAGE_ASPECT_STENCIL_BIT; // need to match imageAspectFlags setting to WindowTraits::depthFormat.

        // 2.a) transition depth image for reading
        // 图像布局转换和缓冲区拷贝 用于将源图像的数据复制到目标缓冲区中
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

        //提交命令 将命令提交到队列中执行
        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) {
            commands->record(commandBuffer);
        });

        // 3. map buffer and copy data. 用于将缓冲区中的数据映射到内存并进行复制操作的部分
        //
        // Map the buffer memory and assign as a vec4Array2D that will automatically unmap itself on destruction.
        if (targetImageFormat == VK_FORMAT_D32_SFLOAT || targetImageFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
        { //判断目标图像格式 VK_FORMAT_D32_SFLOAT
            //创建映射的数据对象
            //如果目标图像格式满足条件，使用vsg::MappedData类创建一个浮点数数组的映射数据对象imageData，并指定了映射的设备内存、偏移量、标志、目标图像格式、宽度和高度
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
        else                                                                                                                                              //处理非浮点数格式的目标图像
        {                                                                                                                                                 //如果目标图像格式不满足条件，创建一个无符号整数数组的映射数据对象imageData，并指定了映射的设备内存、偏移量、标志、目标图像格式、宽度和高度
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

    void screenshot_encodedepth(vsg::ref_ptr<vsg::Window> window, std::vector<std::vector<uint8_t>> &color)
    {
        auto width = window->extent2D().width;
        auto height = window->extent2D().height; //获取窗口大小

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice(); //获取设备和物理设备

        //获取源图像和图像格式 sourceImage 是当前窗口渲染绘制的画面的深度图像
        vsg::ref_ptr<vsg::Image> sourceImage(window->getDepthImage());
        //sourceImage表示源图像，sourceImageFormat表示源图像的格式 targetImageFormat表示目标图像的格式
        VkFormat sourceImageFormat = window->depthFormat();
        VkFormat targetImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

        //
        // 1) Check to see if Blit is supported.
        //获取源图像格式和目标图像格式的属性
        VkFormatProperties srcFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), sourceImageFormat, &srcFormatProperties);

        VkFormatProperties destFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_UNORM, &destFormatProperties);

        //检查是否支持图像拷贝操作（Blit）。它通过检查源图像格式和目标图像格式的属性来确定是否支持Blit操作
        bool supportsBlit = ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
                            ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);

        //确定是否支持Blit操作
        if (supportsBlit)
        { //如果支持Blit操作，则将目标图像格式设置为VK_FORMAT_R8G8B8A8_UNORM，以确保输出图像为RGBA格式
            // we can automatically convert the image format when blit, so take advantage of it to ensure RGBA
            targetImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        }

        CUarray inputArray = (CUarray)m_encoder->getEncoder()->GetNextInputFrame()->inputPtr;
		const DeviceAlloc* inputSurf = m_encoder->mapCUarrayToDeviceAlloc[inputArray];
        auto destinationImage = inputSurf->vulkanImage;

        
        // 3) create command buffer and submit to graphics queue
        // 创建命令缓冲区（command buffer）并将其提交给图形队列（graphics queue）
        auto commands = vsg::Commands::create(); //创建命令缓冲区

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

        //图像拷贝或位块传输 据具体的需求和平台功能进行选择，将相应的图像拷贝或位块传输指令添加到命令缓冲区中
        if (supportsBlit)
        {
            // 3.c.1) if blit using vkCmdBlitImage 使用`vkCmdBlitImage`进行图像拷贝
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
            // 3.c.2) else use vkCmdCopyImage 使用`vkCmdCopyImage`进行图像拷贝

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
        //再次转换图像布局
        //使用`vsg::ImageMemoryBarrier`和`vsg::PipelineBarrier`类创建屏障对象，并将其添加到命令缓冲区中。
        //其中，`transitionDestinationImageToMemoryReadBarrier`用于将`destinationImage`的布局从传输目标布局转换为通用布局，
        // 以便将其映射到图像设备内存；`transitionSourceImageBackToPresentBarrier`用于将`sourceImage`的布局从传输源布局转换为呈现源布局
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

        //提交命令缓冲区
        //创建了一个`vsg::Fence`对象用于同步命令缓冲区的提交。然后获取图形队列的队列族索引和命令池对象，
        //接着通过`device->getQueue(queueFamilyIndex)`获取图形队列对象
        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        //将命令缓冲区提交到图形队列中 在这个函数中，使用lambda表达式对命令缓冲区进行记录（record）操作。
        //lambda表达式的参数是命令缓冲区对象commandBuffer，可以在其中添加其他需要执行的命令
        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) {
            commands->record(commandBuffer);
        });
		inputSurf->cudaSemaphore->wait();

		std::vector<std::vector<uint8_t>> vPacket;
		m_encoder->getEncoder()->EncodeFrame(vPacket);
        if(vPacket.size() > 0){
            color = vPacket;
            std::cout << vPacket.size() << std::endl;
            std::cout << vPacket[0].size() << std::endl;
            // std::cout <<"send:" << std::endl;
            // for(int i = 0; i < vPacket[0].size(); i ++){
            //     std::cout << (int)vPacket[0][i] << " ";
            // }
            // std::cout <<std::endl;
            // int nFrameReturned = m_encoder->getDecoder()->Decode(vPacket[0].data(), vPacket[0].size(), 0, mFrameCount);
            // std::cout << "nFrameReturned" << nFrameReturned << std::endl;
        }
    }
};