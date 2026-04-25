#include "OffscreenRenderTarget.h"

namespace
{
VkAccessFlags layoutAccessMask(VkImageLayout layout)
{
    switch (layout)
    {
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    default:
        return 0;
    }
}

VkPipelineStageFlags layoutStageMask(VkImageLayout layout)
{
    switch (layout)
    {
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    default:
        return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
}
}

void ColorRenderTarget::init(vsg::ref_ptr<vsg::Device> device,
                             VkExtent2D extent,
                             VkFormat imageFormat,
                             VkImageUsageFlags extraUsage,
                             VkImageLayout finalLayout,
                             VkSampleCountFlagBits samples)
{
    _extent = extent;
    _samples = samples;

    bool multisampling = samples != VK_SAMPLE_COUNT_1_BIT;

    if (multisampling)
    {
        multisampleImage = vsg::Image::create();
        multisampleImage->imageType = VK_IMAGE_TYPE_2D;
        multisampleImage->format = imageFormat;
        multisampleImage->extent.width = extent.width;
        multisampleImage->extent.height = extent.height;
        multisampleImage->extent.depth = 1;
        multisampleImage->mipLevels = 1;
        multisampleImage->arrayLayers = 1;
        multisampleImage->samples = samples;
        multisampleImage->tiling = VK_IMAGE_TILING_OPTIMAL;
        multisampleImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        multisampleImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        multisampleImage->flags = 0;
        multisampleImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        multisampleImage->compile(device);
        multisampleImage->allocateAndBindMemory(device);

        multisampleImageView = vsg::ImageView::create(multisampleImage, VK_IMAGE_ASPECT_COLOR_BIT);
        multisampleImageView->compile(device);
    }

    colorImage = vsg::Image::create();
    colorImage->imageType = VK_IMAGE_TYPE_2D;
    colorImage->format = imageFormat;
    colorImage->extent.width = extent.width;
    colorImage->extent.height = extent.height;
    colorImage->extent.depth = 1;
    colorImage->mipLevels = 1;
    colorImage->arrayLayers = 1;
    colorImage->samples = VK_SAMPLE_COUNT_1_BIT;
    colorImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | extraUsage;
    colorImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImage->flags = 0;
    colorImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    colorImage->compile(device);
    colorImage->allocateAndBindMemory(device);

    colorImageView = vsg::ImageView::create(colorImage, VK_IMAGE_ASPECT_COLOR_BIT);
    colorImageView->compile(device);

    if (multisampling)
    {
        vsg::AttachmentDescription colorAttachment = {};
        colorAttachment.format = imageFormat;
        colorAttachment.samples = samples;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        vsg::AttachmentDescription resolveAttachment = {};
        resolveAttachment.format = imageFormat;
        resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAttachment.initialLayout = finalLayout;
        resolveAttachment.finalLayout = finalLayout;

        vsg::AttachmentReference colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference resolveAttachmentRef = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        vsg::SubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachments.emplace_back(colorAttachmentRef);
        subpass.resolveAttachments.emplace_back(resolveAttachmentRef);

        vsg::SubpassDependency colorDependency = {};
        colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        colorDependency.dstSubpass = 0;
        colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.srcAccessMask = 0;
        colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency.dependencyFlags = 0;

        vsg::SubpassDependency colorOutputDependency = {};
        colorOutputDependency.srcSubpass = 0;
        colorOutputDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        colorOutputDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorOutputDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        colorOutputDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorOutputDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        colorOutputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        renderPass = vsg::RenderPass::create(
            device,
            vsg::RenderPass::Attachments{colorAttachment, resolveAttachment},
            vsg::RenderPass::Subpasses{subpass},
            vsg::RenderPass::Dependencies{colorDependency, colorOutputDependency});
        framebuffer = vsg::Framebuffer::create(
            renderPass,
            vsg::ImageViews{multisampleImageView, colorImageView},
            extent.width,
            extent.height,
            1);
    }
    else
    {
        vsg::AttachmentDescription colorAttachment = {};
        colorAttachment.format = imageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = finalLayout;
        colorAttachment.finalLayout = finalLayout;

        vsg::AttachmentReference colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::SubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachments.emplace_back(colorAttachmentRef);

        vsg::SubpassDependency colorDependency = {};
        colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        colorDependency.dstSubpass = 0;
        colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.srcAccessMask = 0;
        colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency.dependencyFlags = 0;

        vsg::SubpassDependency colorOutputDependency = {};
        colorOutputDependency.srcSubpass = 0;
        colorOutputDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        colorOutputDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorOutputDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        colorOutputDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorOutputDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        colorOutputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        renderPass = vsg::RenderPass::create(
            device,
            vsg::RenderPass::Attachments{colorAttachment},
            vsg::RenderPass::Subpasses{subpass},
            vsg::RenderPass::Dependencies{colorDependency, colorOutputDependency});
        framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{colorImageView}, extent.width, extent.height, 1);
    }

    auto physicalDevice = device->getPhysicalDevice();
    int graphicsFamily = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    auto commandPool = vsg::CommandPool::create(device, graphicsFamily);
    vsg::submitCommandsToQueue(commandPool, device->getQueue(graphicsFamily), [&](vsg::CommandBuffer& commandBuffer) {
        auto colorImageBarrier = vsg::ImageMemoryBarrier::create(
            0,
            layoutAccessMask(finalLayout),
            VK_IMAGE_LAYOUT_UNDEFINED,
            finalLayout,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            colorImage,
            colorImageView->subresourceRange);

        auto colorPipelineBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            layoutStageMask(finalLayout),
            0,
            colorImageBarrier);
        colorPipelineBarrier->record(commandBuffer);

        if (multisampling)
        {
            auto multisampleImageBarrier = vsg::ImageMemoryBarrier::create(
                0,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                multisampleImage,
                multisampleImageView->subresourceRange);

            auto multisamplePipelineBarrier = vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                multisampleImageBarrier);
            multisamplePipelineBarrier->record(commandBuffer);
        }
    });
}

void OffscreenRenderTarget::init(vsg::ref_ptr<vsg::Device> device, VkExtent2D extent, VkSampleCountFlagBits samples, VkFormat depthFormat, VkImageUsageFlags depthImageUsage)
{
    _extent = extent;
    _samples = samples;

    bool multisampling = samples != VK_SAMPLE_COUNT_1_BIT;

    // Multisample color image (same as Window::buildSwapchain multisample path)
    if (multisampling)
    {
        multisampleImage = vsg::Image::create();
        multisampleImage->imageType = VK_IMAGE_TYPE_2D;
        multisampleImage->format = VK_FORMAT_R8G8B8A8_UNORM;
        multisampleImage->extent.width = extent.width;
        multisampleImage->extent.height = extent.height;
        multisampleImage->extent.depth = 1;
        multisampleImage->mipLevels = 1;
        multisampleImage->arrayLayers = 1;
        multisampleImage->samples = samples;
        multisampleImage->tiling = VK_IMAGE_TILING_OPTIMAL;
        multisampleImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        multisampleImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        multisampleImage->flags = 0;
        multisampleImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        multisampleImage->compile(device);
        multisampleImage->allocateAndBindMemory(device);

        multisampleImageView = vsg::ImageView::create(multisampleImage, VK_IMAGE_ASPECT_COLOR_BIT);
        multisampleImageView->compile(device);
    }

    // Resolved scene color for the standalone composite pass.
    colorImage = vsg::Image::create();
    colorImage->imageType = VK_IMAGE_TYPE_2D;
    colorImage->format = VK_FORMAT_R32G32B32A32_SFLOAT;
    colorImage->extent.width = extent.width;
    colorImage->extent.height = extent.height;
    colorImage->extent.depth = 1;
    colorImage->mipLevels = 1;
    colorImage->arrayLayers = 1;
    colorImage->samples = VK_SAMPLE_COUNT_1_BIT;
    colorImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImage->flags = 0;
    colorImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    colorImage->compile(device);
    colorImage->allocateAndBindMemory(device);

    colorImageView = vsg::ImageView::create(colorImage, VK_IMAGE_ASPECT_COLOR_BIT);
    colorImageView->compile(device);

    // GBuffer0 - same format and usage as Window::buildSwapchain
    gbufferImage0 = vsg::Image::create();
    gbufferImage0->imageType = VK_IMAGE_TYPE_2D;
    gbufferImage0->format = VK_FORMAT_R32G32B32A32_SFLOAT;
    gbufferImage0->extent.width = extent.width;
    gbufferImage0->extent.height = extent.height;
    gbufferImage0->extent.depth = 1;
    gbufferImage0->mipLevels = 1;
    gbufferImage0->arrayLayers = 1;
    gbufferImage0->samples = samples;
    gbufferImage0->tiling = VK_IMAGE_TILING_OPTIMAL;
    gbufferImage0->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    gbufferImage0->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gbufferImage0->flags = 0;
    gbufferImage0->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    gbufferImage0->compile(device);
    gbufferImage0->allocateAndBindMemory(device);

    gbufferImageView0 = vsg::ImageView::create(gbufferImage0, VK_IMAGE_ASPECT_COLOR_BIT);
    gbufferImageView0->compile(device);

    // GBuffer1
    gbufferImage1 = vsg::Image::create();
    gbufferImage1->imageType = VK_IMAGE_TYPE_2D;
    gbufferImage1->format = VK_FORMAT_R32G32B32A32_SFLOAT;
    gbufferImage1->extent.width = extent.width;
    gbufferImage1->extent.height = extent.height;
    gbufferImage1->extent.depth = 1;
    gbufferImage1->mipLevels = 1;
    gbufferImage1->arrayLayers = 1;
    gbufferImage1->samples = samples;
    gbufferImage1->tiling = VK_IMAGE_TILING_OPTIMAL;
    gbufferImage1->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    gbufferImage1->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gbufferImage1->flags = 0;
    gbufferImage1->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    gbufferImage1->compile(device);
    gbufferImage1->allocateAndBindMemory(device);

    gbufferImageView1 = vsg::ImageView::create(gbufferImage1, VK_IMAGE_ASPECT_COLOR_BIT);
    gbufferImageView1->compile(device);

    // GBuffer2
    gbufferImage2 = vsg::Image::create();
    gbufferImage2->imageType = VK_IMAGE_TYPE_2D;
    gbufferImage2->format = VK_FORMAT_R32G32B32A32_SFLOAT;
    gbufferImage2->extent.width = extent.width;
    gbufferImage2->extent.height = extent.height;
    gbufferImage2->extent.depth = 1;
    gbufferImage2->mipLevels = 1;
    gbufferImage2->arrayLayers = 1;
    gbufferImage2->samples = samples;
    gbufferImage2->tiling = VK_IMAGE_TILING_OPTIMAL;
    gbufferImage2->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    gbufferImage2->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gbufferImage2->flags = 0;
    gbufferImage2->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    gbufferImage2->compile(device);
    gbufferImage2->allocateAndBindMemory(device);

    gbufferImageView2 = vsg::ImageView::create(gbufferImage2, VK_IMAGE_ASPECT_COLOR_BIT);
    gbufferImageView2->compile(device);

    maskImage = vsg::Image::create();
    maskImage->imageType = VK_IMAGE_TYPE_2D;
    maskImage->format = VK_FORMAT_R32_SFLOAT;
    maskImage->extent.width = extent.width;
    maskImage->extent.height = extent.height;
    maskImage->extent.depth = 1;
    maskImage->mipLevels = 1;
    maskImage->arrayLayers = 1;
    maskImage->samples = samples;
    maskImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    maskImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    maskImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    maskImage->flags = 0;
    maskImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    maskImage->compile(device);
    maskImage->allocateAndBindMemory(device);

    maskImageView = vsg::ImageView::create(maskImage, VK_IMAGE_ASPECT_COLOR_BIT);
    maskImageView->compile(device);

    materialImage = vsg::Image::create();
    materialImage->imageType = VK_IMAGE_TYPE_2D;
    materialImage->format = VK_FORMAT_R32G32B32A32_SFLOAT;
    materialImage->extent.width = extent.width;
    materialImage->extent.height = extent.height;
    materialImage->extent.depth = 1;
    materialImage->mipLevels = 1;
    materialImage->arrayLayers = 1;
    materialImage->samples = samples;
    materialImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    materialImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    materialImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    materialImage->flags = 0;
    materialImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    materialImage->compile(device);
    materialImage->allocateAndBindMemory(device);

    materialImageView = vsg::ImageView::create(materialImage, VK_IMAGE_ASPECT_COLOR_BIT);
    materialImageView->compile(device);

    // Shadow Write
    shadowWriteImage = vsg::Image::create();
    shadowWriteImage->imageType = VK_IMAGE_TYPE_2D;
    shadowWriteImage->format = VK_FORMAT_R32G32B32A32_SFLOAT;
    shadowWriteImage->extent.width = extent.width;
    shadowWriteImage->extent.height = extent.height;
    shadowWriteImage->extent.depth = 1;
    shadowWriteImage->mipLevels = 1;
    shadowWriteImage->arrayLayers = 1;
    shadowWriteImage->samples = samples;
    shadowWriteImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    shadowWriteImage->usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    shadowWriteImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    shadowWriteImage->flags = 0;
    shadowWriteImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    shadowWriteImage->compile(device);
    shadowWriteImage->allocateAndBindMemory(device);

    shadowWriteImageView = vsg::ImageView::create(shadowWriteImage, VK_IMAGE_ASPECT_COLOR_BIT);
    shadowWriteImageView->compile(device);

    // Shadow Sample
    shadowSampleImage = vsg::Image::create();
    shadowSampleImage->imageType = VK_IMAGE_TYPE_2D;
    shadowSampleImage->format = VK_FORMAT_R32G32B32A32_SFLOAT;
    shadowSampleImage->extent.width = extent.width;
    shadowSampleImage->extent.height = extent.height;
    shadowSampleImage->extent.depth = 1;
    shadowSampleImage->mipLevels = 1;
    shadowSampleImage->arrayLayers = 1;
    shadowSampleImage->samples = VK_SAMPLE_COUNT_1_BIT;
    shadowSampleImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    shadowSampleImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    shadowSampleImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    shadowSampleImage->flags = 0;
    shadowSampleImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    shadowSampleImage->compile(device);
    shadowSampleImage->allocateAndBindMemory(device);

    shadowSampleImageView = vsg::ImageView::create(shadowSampleImage, VK_IMAGE_ASPECT_COLOR_BIT);
    shadowSampleImageView->compile(device);

    // Depth buffer
    depthImage = vsg::Image::create();
    depthImage->imageType = VK_IMAGE_TYPE_2D;
    depthImage->extent.width = extent.width;
    depthImage->extent.height = extent.height;
    depthImage->extent.depth = 1;
    depthImage->mipLevels = 1;
    depthImage->arrayLayers = 1;
    depthImage->format = depthFormat;
    depthImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImage->samples = samples;
    depthImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImage->usage = depthImageUsage;

    depthImage->compile(device);
    depthImage->allocateAndBindMemory(device);

    depthImageView = vsg::ImageView::create(depthImage);
    depthImageView->compile(device);

    if (multisampling)
    {
        multisampleDepthImage = depthImage;
        multisampleDepthImageView = depthImageView;
    }

    // Transition depth image layout
    {
        auto physicalDevice = device->getPhysicalDevice();
        int graphicsFamily = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);

        auto commandPool = vsg::CommandPool::create(device, graphicsFamily);
        vsg::submitCommandsToQueue(commandPool, device->getQueue(graphicsFamily), [&](vsg::CommandBuffer& commandBuffer) {
            auto depthImageBarrier = vsg::ImageMemoryBarrier::create(
                0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                depthImage,
                depthImageView->subresourceRange);

            auto pipelineBarrier = vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0, depthImageBarrier);
            pipelineBarrier->record(commandBuffer);

            if (multisampling)
            {
                auto msImageBarrier = vsg::ImageMemoryBarrier::create(
                    0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                    multisampleImage,
                    VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
                auto msPipelineBarrier = vsg::PipelineBarrier::create(
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    0, msImageBarrier);
                msPipelineBarrier->record(commandBuffer);

                if (multisampleDepthImage && multisampleDepthImage != depthImage)
                {
                    auto msDepthBarrier = vsg::ImageMemoryBarrier::create(
                        0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                        multisampleDepthImage,
                        multisampleDepthImageView->subresourceRange);
                    auto msDepthPipelineBarrier = vsg::PipelineBarrier::create(
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                        0, msDepthBarrier);
                    msDepthPipelineBarrier->record(commandBuffer);
                }
            }

            // Shadow history is sampled on the very first frame, so initialize it explicitly.
            auto shadowSampleToTransferDst = vsg::ImageMemoryBarrier::create(
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                shadowSampleImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
            auto shadowSampleInitBarrier = vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                shadowSampleToTransferDst);
            shadowSampleInitBarrier->record(commandBuffer);

            auto clearShadowSample = vsg::ClearColorImage::create();
            clearShadowSample->image = shadowSampleImage;
            clearShadowSample->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            clearShadowSample->color = {{0.0f, 0.0f, 0.0f, 0.0f}};
            clearShadowSample->ranges = {VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
            clearShadowSample->record(commandBuffer);

            auto shadowSampleToShaderRead = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                shadowSampleImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
            auto shadowSampleReadyBarrier = vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                shadowSampleToShaderRead);
            shadowSampleReadyBarrier->record(commandBuffer);
        });
    }
}

void OffscreenRenderTarget::buildRenderPass(vsg::ref_ptr<vsg::Device> device, VkFormat imageFormat, VkFormat depthFormat)
{
    (void)imageFormat;
    bool multisampling = _samples != VK_SAMPLE_COUNT_1_BIT;

    if (multisampling)
    {
        vsg::AttachmentDescription resolveAttachment = {};
        resolveAttachment.format = colorImage->format;
        resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vsg::AttachmentDescription depthAttachment = {};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = _samples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        auto colorAttachmentColor = vsg::defaultGbufferColorAttachment(gbufferImage0->format);
        colorAttachmentColor.samples = _samples;
        auto colorAttachmentNormal = vsg::defaultGbufferColorAttachment(gbufferImage1->format);
        colorAttachmentNormal.samples = _samples;
        auto colorAttachmentWorldPos = vsg::defaultGbufferColorAttachment(gbufferImage2->format);
        colorAttachmentWorldPos.samples = _samples;
        auto colorAttachmentMask = vsg::defaultGbufferColorAttachment(maskImage->format);
        colorAttachmentMask.samples = _samples;
        auto colorAttachmentMaterial = vsg::defaultGbufferColorAttachment(materialImage->format);
        colorAttachmentMaterial.samples = _samples;
        auto colorAttachmentShadowWrite = vsg::defaultGbufferColorAttachment(shadowWriteImage->format);
        colorAttachmentShadowWrite.samples = _samples;
        colorAttachmentShadowWrite.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        // Framebuffer attachment order (matching buildFramebuffer):
        // [0] resolve color, [1] depth, [2] gbuffer0,
        // [3] gbuffer1, [4] gbuffer2, [5] shadowWrite, [6] material, [7] mask
        vsg::RenderPass::Attachments attachments{
            resolveAttachment,
            depthAttachment,
            colorAttachmentColor,
            colorAttachmentNormal,
            colorAttachmentWorldPos,
            colorAttachmentShadowWrite,
            colorAttachmentMaterial,
            colorAttachmentMask};

        // Attachment references
        vsg::AttachmentReference resolveAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference depthAttachmentRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefColor = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefNormal = {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefWorldPos = {4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefShadowWrite = {5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefMaterial = {6, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefMask = {7, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference unusedResolveAttachmentRef = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};

        vsg::SubpassDescription subpass;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachments.emplace_back(colorAttachmentRefColor);
        subpass.colorAttachments.emplace_back(colorAttachmentRefNormal);
        subpass.colorAttachments.emplace_back(colorAttachmentRefWorldPos);
        subpass.colorAttachments.emplace_back(colorAttachmentRefShadowWrite);
        subpass.colorAttachments.emplace_back(colorAttachmentRefMaterial);
        subpass.colorAttachments.emplace_back(colorAttachmentRefMask);
        subpass.resolveAttachments.emplace_back(resolveAttachmentRef);
        subpass.resolveAttachments.emplace_back(unusedResolveAttachmentRef);
        subpass.resolveAttachments.emplace_back(unusedResolveAttachmentRef);
        subpass.resolveAttachments.emplace_back(unusedResolveAttachmentRef);
        subpass.resolveAttachments.emplace_back(unusedResolveAttachmentRef);
        subpass.resolveAttachments.emplace_back(unusedResolveAttachmentRef);
        subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

        vsg::RenderPass::Subpasses subpasses{subpass};

        vsg::SubpassDependency colorDependency = {};
        colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        colorDependency.dstSubpass = 0;
        colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.srcAccessMask = 0;
        colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency.dependencyFlags = 0;

        vsg::SubpassDependency depthDependency = {};
        depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        depthDependency.dstSubpass = 0;
        depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dependencyFlags = 0;

        vsg::SubpassDependency colorOutputDependency = {};
        colorOutputDependency.srcSubpass = 0;
        colorOutputDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        colorOutputDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorOutputDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        colorOutputDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorOutputDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        colorOutputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        vsg::RenderPass::Dependencies dependencies{
            colorDependency, depthDependency,
            colorOutputDependency};

        renderPass = vsg::RenderPass::create(device, attachments, subpasses, dependencies);
    }
    else
    {
        auto colorAttachmentColor = vsg::defaultGbufferColorAttachment(gbufferImage0->format);
        auto colorAttachmentNormal = vsg::defaultGbufferColorAttachment(gbufferImage1->format);
        auto colorAttachmentWorldPos = vsg::defaultGbufferColorAttachment(gbufferImage2->format);
        auto colorAttachmentMask = vsg::defaultGbufferColorAttachment(maskImage->format);
        auto colorAttachmentMaterial = vsg::defaultGbufferColorAttachment(materialImage->format);
        auto colorAttachmentShadowWrite = vsg::defaultGbufferColorAttachment(shadowWriteImage->format);
        colorAttachmentShadowWrite.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        auto depthAttachment = vsg::defaultDepthAttachment(depthFormat);

        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        vsg::RenderPass::Attachments attachments{
            colorAttachmentColor,
            colorAttachmentNormal,
            colorAttachmentWorldPos,
            colorAttachmentShadowWrite,
            colorAttachmentMaterial,
            colorAttachmentMask,
            depthAttachment};

        vsg::AttachmentReference colorAttachmentRefColor = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefNormal = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefWorldPos = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefShadowWrite = {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefMaterial = {4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefMask = {5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference depthAttachmentRef = {6, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        vsg::SubpassDescription subpass;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachments.emplace_back(colorAttachmentRefColor);
        subpass.colorAttachments.emplace_back(colorAttachmentRefNormal);
        subpass.colorAttachments.emplace_back(colorAttachmentRefWorldPos);
        subpass.colorAttachments.emplace_back(colorAttachmentRefShadowWrite);
        subpass.colorAttachments.emplace_back(colorAttachmentRefMaterial);
        subpass.colorAttachments.emplace_back(colorAttachmentRefMask);
        subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

        vsg::RenderPass::Subpasses subpasses{subpass};

        vsg::SubpassDependency colorDependency = {};
        colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        colorDependency.dstSubpass = 0;
        colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.srcAccessMask = 0;
        colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency.dependencyFlags = 0;

        vsg::SubpassDependency depthDependency = {};
        depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        depthDependency.dstSubpass = 0;
        depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dependencyFlags = 0;

        vsg::SubpassDependency colorOutputDependency = {};
        colorOutputDependency.srcSubpass = 0;
        colorOutputDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        colorOutputDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorOutputDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        colorOutputDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorOutputDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        colorOutputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        vsg::RenderPass::Dependencies dependencies{
            colorDependency, depthDependency,
            colorOutputDependency};

        renderPass = vsg::RenderPass::create(device, attachments, subpasses, dependencies);
    }
}

void OffscreenRenderTarget::buildFramebuffer(VkExtent2D extent)
{
    vsg::ImageViews attachments;

    if (_samples != VK_SAMPLE_COUNT_1_BIT)
    {
        // Multisampled path - must match render pass attachment order:
        // [0] resolve color, [1] depth, [2] gbuffer0,
        // [3] gbuffer1, [4] gbuffer2, [5] shadowWrite, [6] material, [7] mask
        attachments.push_back(colorImageView);
        attachments.push_back(depthImageView);
        attachments.push_back(gbufferImageView0);
        attachments.push_back(gbufferImageView1);
        attachments.push_back(gbufferImageView2);
        attachments.push_back(shadowWriteImageView);
        attachments.push_back(materialImageView);
        attachments.push_back(maskImageView);
    }
    else
    {
        // Non-multisampled path - must match render pass attachment order:
        // [0] gbuffer0, [1] gbuffer1, [2] gbuffer2, [3] shadowWrite, [4] material, [5] mask, [6] depth
        attachments.push_back(gbufferImageView0);
        attachments.push_back(gbufferImageView1);
        attachments.push_back(gbufferImageView2);
        attachments.push_back(shadowWriteImageView);
        attachments.push_back(materialImageView);
        attachments.push_back(maskImageView);
        attachments.push_back(depthImageView);
    }

    framebuffer = vsg::Framebuffer::create(renderPass, attachments, extent.width, extent.height, 1);
}
