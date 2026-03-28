#include "OffscreenRenderTarget.h"

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

    // Offscreen color attachment (attachment[0] in render pass)
    colorImage = vsg::Image::create();
    colorImage->imageType = VK_IMAGE_TYPE_2D;
    colorImage->format = VK_FORMAT_B8G8R8A8_UNORM;
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
    gbufferImage0->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

    // SSAO Result
    ssaoResultImage = vsg::Image::create();
    ssaoResultImage->imageType = VK_IMAGE_TYPE_2D;
    ssaoResultImage->format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ssaoResultImage->extent.width = extent.width;
    ssaoResultImage->extent.height = extent.height;
    ssaoResultImage->extent.depth = 1;
    ssaoResultImage->mipLevels = 1;
    ssaoResultImage->arrayLayers = 1;
    ssaoResultImage->samples = samples;
    ssaoResultImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    ssaoResultImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ssaoResultImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ssaoResultImage->flags = 0;
    ssaoResultImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    ssaoResultImage->compile(device);
    ssaoResultImage->allocateAndBindMemory(device);

    ssaoResultImageView = vsg::ImageView::create(ssaoResultImage, VK_IMAGE_ASPECT_COLOR_BIT);
    ssaoResultImageView->compile(device);

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
    shadowWriteImage->usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
    shadowSampleImage->samples = samples;
    shadowSampleImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    shadowSampleImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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

    // Multisample depth (when multisampling + requiresDepthRead)
    bool requiresDepthRead = (depthImageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    if (multisampling && requiresDepthRead)
    {
        multisampleDepthImage = depthImage;
        multisampleDepthImageView = depthImageView;

        // Create resolved depth buffer (single sample)
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
        depthImage->usage = depthImageUsage;
        depthImage->samples = VK_SAMPLE_COUNT_1_BIT;
        depthImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        depthImage->compile(device);
        depthImage->allocateAndBindMemory(device);

        depthImageView = vsg::ImageView::create(depthImage);
        depthImageView->compile(device);
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

                if (multisampleDepthImage)
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
        });
    }
}

void OffscreenRenderTarget::buildRenderPass(vsg::ref_ptr<vsg::Device> device, VkFormat imageFormat, VkFormat depthFormat, bool requiresDepthRead)
{
    bool multisampling = _samples != VK_SAMPLE_COUNT_1_BIT;

    if (multisampling)
    {
        // Multisampled render pass - similar to createMRTMultisampledRenderPass
        // Multisampled color attachment
        vsg::AttachmentDescription colorAttachment = {};
        colorAttachment.format = imageFormat;
        colorAttachment.samples = _samples;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Resolve attachment (offscreen, not present)
        vsg::AttachmentDescription resolveAttachment = {};
        resolveAttachment.format = imageFormat;
        resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Multisampled depth
        vsg::AttachmentDescription depthAttachment = {};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = _samples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        auto colorAttachmentColor = vsg::defaultGbufferColorAttachment(imageFormat);
        colorAttachmentColor.samples = _samples;
        auto colorAttachmentNormal = vsg::defaultGbufferColorAttachment(imageFormat);
        colorAttachmentNormal.samples = _samples;
        auto colorAttachmentWorldPos = vsg::defaultGbufferColorAttachment(imageFormat);
        colorAttachmentWorldPos.samples = _samples;
        auto colorAttachmentSSAONoise = vsg::defaultGbufferColorAttachment(imageFormat);
        colorAttachmentSSAONoise.samples = _samples;
        auto colorAttachmentShadowWrite = vsg::defaultGbufferColorAttachment(imageFormat);
        colorAttachmentShadowWrite.samples = _samples;
        auto colorAttachmentShadowSample = vsg::defaultGbufferColorAttachment(imageFormat);
        colorAttachmentShadowSample.samples = _samples;

        // Framebuffer attachment order (matching buildFramebuffer):
        // [0] multisample color, [1] resolve color, [2] depth (multisample),
        // [3] gbuffer0, [4] gbuffer1, [5] gbuffer2,
        // [6] ssao, [7] shadowWrite, [8] shadowSample
        // [9] depth resolve (if requiresDepthRead)

        vsg::RenderPass::Attachments attachments{colorAttachment, resolveAttachment, depthAttachment,
            colorAttachmentColor, colorAttachmentNormal, colorAttachmentWorldPos,
            colorAttachmentSSAONoise, colorAttachmentShadowWrite, colorAttachmentShadowSample};

        if (requiresDepthRead)
        {
            vsg::AttachmentDescription depthResolveAttachment = {};
            depthResolveAttachment.format = depthFormat;
            depthResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            depthResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.push_back(depthResolveAttachment);
        }

        // Attachment references
        vsg::AttachmentReference colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference resolveAttachmentRef = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference depthAttachmentRef = {2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        vsg::AttachmentReference colorAttachmentRefColor = {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefNormal = {4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefWorldPos = {5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefSSAONoise = {6, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefShadowWrite = {7, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefShadowSample = {8, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        // Subpass 0: Main rendering (no color resolve - matches VSG createMRTMultisampledRenderPass pattern)
        vsg::SubpassDescription subpass;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachments.emplace_back(colorAttachmentRefColor);
        subpass.colorAttachments.emplace_back(colorAttachmentRefNormal);
        subpass.colorAttachments.emplace_back(colorAttachmentRefWorldPos);
        subpass.colorAttachments.emplace_back(colorAttachmentRefShadowWrite);
        subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

        if (requiresDepthRead)
        {
            vsg::AttachmentReference depthResolveAttachmentRef = {9, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            subpass.depthResolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
            subpass.stencilResolveMode = VK_RESOLVE_MODE_NONE;
            subpass.depthStencilResolveAttachments.emplace_back(depthResolveAttachmentRef);
        }

        // Subpass 1: SSAO (no resolve)
        vsg::SubpassDescription subpass1;
        subpass1.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass1.colorAttachments.emplace_back(colorAttachmentRefSSAONoise);
        subpass1.depthStencilAttachments.emplace_back(depthAttachmentRef);
        vsg::AttachmentReference colorRef_Read = {3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        subpass1.inputAttachments = {colorRef_Read};

        // Subpass 2: Denoise (resolve color [0] to [1], shadowSample [8] unresolved)
        vsg::SubpassDescription subpass2;
        subpass2.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass2.colorAttachments.emplace_back(colorAttachmentRef);
        subpass2.colorAttachments.emplace_back(colorAttachmentRefShadowSample);
        subpass2.resolveAttachments.emplace_back(resolveAttachmentRef);
        subpass2.depthStencilAttachments.emplace_back(depthAttachmentRef);
        vsg::AttachmentReference colorRef_ShadowWrite = {7, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        subpass2.inputAttachments = {colorRef_Read, colorRef_ShadowWrite};

        vsg::RenderPass::Subpasses subpasses{subpass, subpass1, subpass2};

        // Dependencies - same as createMRTMultisampledRenderPass
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

        vsg::SubpassDependency colorDependency_ssao = {};
        colorDependency_ssao.srcSubpass = 0;
        colorDependency_ssao.dstSubpass = 1;
        colorDependency_ssao.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency_ssao.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        colorDependency_ssao.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency_ssao.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency_ssao.dependencyFlags = 0;

        vsg::SubpassDependency depthDependency_ssao = {};
        depthDependency_ssao.srcSubpass = 0;
        depthDependency_ssao.dstSubpass = 1;
        depthDependency_ssao.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency_ssao.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        depthDependency_ssao.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency_ssao.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency_ssao.dependencyFlags = 0;

        vsg::SubpassDependency ssaoToDenoiseDependency = {};
        ssaoToDenoiseDependency.srcSubpass = 1;
        ssaoToDenoiseDependency.dstSubpass = 2;
        ssaoToDenoiseDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        ssaoToDenoiseDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        ssaoToDenoiseDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        ssaoToDenoiseDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        ssaoToDenoiseDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        vsg::SubpassDependency depthToDenoiseDependency = {};
        depthToDenoiseDependency.srcSubpass = 1;
        depthToDenoiseDependency.dstSubpass = 2;
        depthToDenoiseDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthToDenoiseDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        depthToDenoiseDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        depthToDenoiseDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        depthToDenoiseDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        vsg::RenderPass::Dependencies dependencies{
            colorDependency, depthDependency,
            colorDependency_ssao, depthDependency_ssao,
            ssaoToDenoiseDependency, depthToDenoiseDependency};

        renderPass = vsg::RenderPass::create(device, attachments, subpasses, dependencies);
    }
    else
    {
        // Non-multisampled render pass - based on createMRTRenderPass
        auto colorAttachment = vsg::defaultColorAttachment(imageFormat);
        // Key change: finalLayout is COLOR_ATTACHMENT_OPTIMAL instead of PRESENT_SRC_KHR
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        auto colorAttachmentColor = vsg::defaultGbufferColorAttachment(imageFormat);
        auto colorAttachmentNormal = vsg::defaultGbufferColorAttachment(imageFormat);
        auto colorAttachmentWorldPos = vsg::defaultGbufferColorAttachment(imageFormat);
        auto colorAttachmentSSAONoise = vsg::defaultGbufferColorAttachment(imageFormat);
        auto colorAttachmentShadowWrite = vsg::defaultGbufferColorAttachment(imageFormat);
        auto colorAttachmentShadowSample = vsg::defaultGbufferColorAttachment(imageFormat);
        auto depthAttachment = vsg::defaultDepthAttachment(depthFormat);

        if (requiresDepthRead)
        {
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        }

        vsg::RenderPass::Attachments attachments{colorAttachment, colorAttachmentColor, colorAttachmentNormal,
            colorAttachmentWorldPos, colorAttachmentSSAONoise, colorAttachmentShadowWrite,
            colorAttachmentShadowSample, depthAttachment};

        vsg::AttachmentReference colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefColor = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefNormal = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefWorldPos = {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefSSAONoise = {4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefShadowWrite = {5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference colorAttachmentRefShadowSample = {6, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        vsg::AttachmentReference depthAttachmentRef = {7, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        // Subpass 0: Main rendering
        vsg::SubpassDescription subpass;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachments.emplace_back(colorAttachmentRefColor);
        subpass.colorAttachments.emplace_back(colorAttachmentRefNormal);
        subpass.colorAttachments.emplace_back(colorAttachmentRefWorldPos);
        subpass.colorAttachments.emplace_back(colorAttachmentRefShadowWrite);
        subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

        // Subpass 1: SSAO
        vsg::SubpassDescription subpass1;
        subpass1.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass1.colorAttachments.emplace_back(colorAttachmentRefSSAONoise);
        subpass1.depthStencilAttachments.emplace_back(depthAttachmentRef);
        vsg::AttachmentReference colorRef_Read = {1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        subpass1.inputAttachments = {colorRef_Read};

        // Subpass 2: Denoise
        vsg::SubpassDescription subpass2;
        subpass2.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass2.colorAttachments.emplace_back(colorAttachmentRef);
        subpass2.colorAttachments.emplace_back(colorAttachmentRefShadowSample);
        subpass2.depthStencilAttachments.emplace_back(depthAttachmentRef);
        vsg::AttachmentReference colorRef_ShadowWrite = {5, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        subpass2.inputAttachments = {colorRef_Read, colorRef_ShadowWrite};

        vsg::RenderPass::Subpasses subpasses{subpass, subpass1, subpass2};

        // Dependencies - same as createMRTRenderPass
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

        vsg::SubpassDependency colorDependency_ssao = {};
        colorDependency_ssao.srcSubpass = 0;
        colorDependency_ssao.dstSubpass = 1;
        colorDependency_ssao.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency_ssao.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        colorDependency_ssao.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency_ssao.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency_ssao.dependencyFlags = 0;

        vsg::SubpassDependency depthDependency_ssao = {};
        depthDependency_ssao.srcSubpass = 0;
        depthDependency_ssao.dstSubpass = 1;
        depthDependency_ssao.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency_ssao.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        depthDependency_ssao.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency_ssao.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency_ssao.dependencyFlags = 0;

        vsg::SubpassDependency ssaoToDenoiseDependency = {};
        ssaoToDenoiseDependency.srcSubpass = 1;
        ssaoToDenoiseDependency.dstSubpass = 2;
        ssaoToDenoiseDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        ssaoToDenoiseDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        ssaoToDenoiseDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        ssaoToDenoiseDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        ssaoToDenoiseDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        vsg::SubpassDependency depthToDenoiseDependency = {};
        depthToDenoiseDependency.srcSubpass = 1;
        depthToDenoiseDependency.dstSubpass = 2;
        depthToDenoiseDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthToDenoiseDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        depthToDenoiseDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        depthToDenoiseDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        depthToDenoiseDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        vsg::RenderPass::Dependencies dependencies{
            colorDependency, depthDependency,
            colorDependency_ssao, depthDependency_ssao,
            ssaoToDenoiseDependency, depthToDenoiseDependency};

        renderPass = vsg::RenderPass::create(device, attachments, subpasses, dependencies);
    }
}

void OffscreenRenderTarget::buildFramebuffer(VkExtent2D extent)
{
    vsg::ImageViews attachments;

    if (multisampleImageView)
    {
        // Multisampled path - must match render pass attachment order:
        // [0]multisample color, [1]resolve color, [2]depth (multisample),
        // [3]gbuffer0, [4]gbuffer1, [5]gbuffer2,
        // [6]ssao, [7]shadowWrite, [8]shadowSample
        // [9]depth resolve (single-sample, if requiresDepthRead)
        attachments.push_back(multisampleImageView);
        attachments.push_back(colorImageView);
        if (multisampleDepthImageView)
        {
            attachments.push_back(multisampleDepthImageView); // [2] multisample depth
        }
        else
        {
            attachments.push_back(depthImageView); // [2] depth (no multisample depth)
        }
        attachments.push_back(gbufferImageView0);
        attachments.push_back(gbufferImageView1);
        attachments.push_back(gbufferImageView2);
        attachments.push_back(ssaoResultImageView);
        attachments.push_back(shadowWriteImageView);
        attachments.push_back(shadowSampleImageView);
        if (multisampleDepthImageView)
        {
            attachments.push_back(depthImageView); // [9] resolved depth (single sample)
        }
    }
    else
    {
        // Non-multisampled path - must match render pass attachment order:
        // [0]color, [1]gbuffer0, [2]gbuffer1, [3]gbuffer2,
        // [4]ssao, [5]shadowWrite, [6]shadowSample, [7]depth
        attachments.push_back(colorImageView);
        attachments.push_back(gbufferImageView0);
        attachments.push_back(gbufferImageView1);
        attachments.push_back(gbufferImageView2);
        attachments.push_back(ssaoResultImageView);
        attachments.push_back(shadowWriteImageView);
        attachments.push_back(shadowSampleImageView);
        attachments.push_back(depthImageView);
    }

    framebuffer = vsg::Framebuffer::create(renderPass, attachments, extent.width, extent.height, 1);
}
