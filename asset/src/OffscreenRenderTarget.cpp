/**
 * OffscreenRenderTarget.cpp - 离屏渲染目标实现
 *
 * 职责：
 * 1. init(): 创建所有 GPU 图像资源（GBuffer、SSAO、Shadow、Depth），并做 layout transition
 * 2. buildRenderPass(): 构建 3-subpass 的 Vulkan RenderPass
 *    - Subpass 0: 主渲染 / GBuffer MRT 输出
 *    - Subpass 1: SSAO 计算（读取 GBuffer0 作为 input attachment）
 *    - Subpass 2: Denoise + 合并（读取 SSAO + shadowWrite 作为 input，输出最终颜色 + shadowSample）
 * 3. buildFramebuffer(): 将所有 ImageView 按 RenderPass 的 attachment 顺序绑定到 Framebuffer
 *
 * 关键概念：
 * - VSG（VulkanSceneGraph）中，Image 需要 compile + allocateAndBindMemory 后才能使用
 * - Subpass 间的 input attachment 允许在同一 RenderPass 内读取前一个 subpass 的输出
 * - Multisampling 时，resolved（单样本）附件和 multisample 附件是分开的
 * - 与 on-screen renderPass 的核心区别：attachment[0] 保持 COLOR_ATTACHMENT_OPTIMAL 而非 PRESENT_SRC_KHR
 */
#include "OffscreenRenderTarget.h"

void OffscreenRenderTarget::init(vsg::ref_ptr<vsg::Device> device, VkExtent2D extent, VkSampleCountFlagBits samples, VkFormat depthFormat, VkImageUsageFlags depthImageUsage)
{
    _extent = extent;
    _samples = samples;

    bool multisampling = samples != VK_SAMPLE_COUNT_1_BIT;

    // --- 创建 Multisample 颜色附件 ---
    // 仅在开启 MSAA 时创建。Vulkan 的 multisample 渲染需要两个附件：
    //   1. multisample image（多样本）：实际渲染目标
    //   2. resolve image（单样本）：Vulkan 自动将多样本结果 resolve 到此附件
    // 此处创建的是多样本附件，colorImage 作为 resolve 附件
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

    // --- 创建离屏颜色附件（attachment[0]）---
    // 此附件是最终合成的输出颜色，后续通过 blit/transfer 拷贝到窗口 swapchain
    // 格式 B8G8R8A8_UNORM（Vulkan 标准 BGRA 顺序）
    // samples = 1（始终单样本，即使开启 MSAA，multisample image 也会 resolve 到此）
    // usage 含 TRANSFER_SRC_BIT，允许后续从 GPU 读取
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

    // --- GBuffer0: 颜色 GBuffer ---
    // 格式 R32G32B32A32_SFLOAT（高精度浮点，适合存储颜色和位置等数据）
    // samples = _samples（跟随 MSAA 设置，multisample 时为多样本）
    // usage 含 INPUT_ATTACHMENT_BIT：Subpass 1 的 SSAO shader 通过 input attachment 读取
    // usage 含 TRANSFER_DST_BIT：允许外部拷贝数据到此 attachment
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

    // --- GBuffer1: 法线 GBuffer ---
    // usage 含 SAMPLED_BIT（而非 INPUT_ATTACHMENT_BIT）：Subpass 2 的 denoise shader 以 sampled 方式读取
    // 为什么用 SAMPLED 而非 INPUT？因为 GBuffer1 在 Subpass 0 输出，Subpass 2 使用（跨了一个 subpass）
    // VSG 的 input attachment 只能在同一 subpass 内读取；跨 subpass 采样需要用 sampler
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

    // --- GBuffer2: 世界坐标 GBuffer ---
    // 同 GBuffer1，使用 SAMPLED_BIT 供后续 shader 采样
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

    // --- SSAO 结果附件 ---
    // Subpass 1 输出的 SSAO 遮蔽值（ambient occlusion factor）
    // usage 含 SAMPLED_BIT：后续光照 pass 采样此纹理来应用 SSAO 效果
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

    // --- Shadow Write 附件 ---
    // Subpass 0 写入的阴影原始数据（未去噪）
    // usage 含 INPUT_ATTACHMENT_BIT：Subpass 2 的 denoise shader 通过 input attachment 读取
    // usage 含 SAMPLED_BIT：也可被其他 pass 采样
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

    // --- Shadow Sample 附件 ---
    // Subpass 2 输出的去噪后阴影结果
    // 后续光照 pass 通过 SAMPLED_BIT 采样此纹理来应用阴影效果
    // 与 shadowWrite 的区别：shadowWrite 是原始数据，shadowSample 是去噪后的最终结果
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

    // --- 深度附件 ---
    // 所有 subpass 共享同一深度附件（用于深度测试和 Early-Z 剔除）
    // samples = _samples（跟随 MSAA 设置）
    // depthImageUsage 由调用者指定，通常包含 DEPTH_STENCIL_ATTACHMENT_BIT
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

    // --- Multisample 深度处理 ---
    // 当同时开启 MSAA 和 requiresDepthRead 时，需要额外的 resolve 深度附件
    // 原理：Vulkan 在 subpass 结束时自动将 multisample depth resolve 到单样本 depth
    // requiresDepthRead 的判断：depthImageUsage 包含 TRANSFER_SRC_BIT 表示需要读取深度
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

    // --- 图像 Layout Transition（布局转换）---
    // Vulkan 要求图像在使用前必须处于正确的 layout
    // 这里通过 PipelineBarrier 将 depth / multisample 图像从 UNDEFINED 转换到：
    //   - depth: DEPTH_STENCIL_ATTACHMENT_OPTIMAL（供 depth test 使用）
    //   - multisample color: COLOR_ATTACHMENT_OPTIMAL（供 color write 使用）
    // Pipeline stage 从 TOP_OF_PIPE 到 EARLY_FRAGMENT_TESTS / COLOR_ATTACHMENT_OUTPUT
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
        // ===== Multisample 路径的 RenderPass =====
        // attachment 布局（必须与 buildFramebuffer 中 ImageView 的添加顺序一致）：
        // [0] multisample color（多样本颜色，Subpass 2 resolve 到 [1]）
        // [1] resolve color（单样本颜色，最终输出）
        // [2] depth（multisample 深度，如有 resolve 则 resolve 到 [9]）
        // [3] gbuffer0（颜色）
        // [4] gbuffer1（法线）
        // [5] gbuffer2（世界坐标）
        // [6] ssao（SSAO 结果）
        // [7] shadowWrite（阴影写入）
        // [8] shadowSample（阴影采样）
        // [9] depth resolve（单样本深度，仅 requiresDepthRead 时存在）

        // Multisampled render pass - similar to createMRTMultisampledRenderPass
        // Multisample color attachment
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

        // ===== Subpass 0: 主渲染 / GBuffer MRT 输出 =====
        // 使用 MRT（Multiple Render Target）同时写入 4 个 color attachment：
        //   [3] gbuffer0 (color), [4] gbuffer1 (normal), [5] gbuffer2 (worldPos), [7] shadowWrite
        // 深度附件 [2] 用于 depth test
        // Multisample 模式下不在此处做 resolve（Vulkan 在 subpass 结束时自动 resolve）
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
            // 深度 resolve：Vulkan 在 Subpass 0 结束时将 multisample depth [2] resolve 到单样本 depth [9]
            // VK_RESOLVE_MODE_AVERAGE_BIT：取多样本深度的平均值
            vsg::AttachmentReference depthResolveAttachmentRef = {9, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            subpass.depthResolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
            subpass.stencilResolveMode = VK_RESOLVE_MODE_NONE;
            subpass.depthStencilResolveAttachments.emplace_back(depthResolveAttachmentRef);
        }

        // ===== Subpass 1: SSAO 计算 =====
        // 写入 [6] ssao（SSAO 遮蔽值）
        // 读取 [1] gbuffer0 作为 input attachment（同一 subpass 内读取前一个 subpass 的输出）
        // input attachment 是 Vulkan 的 subpass 内部读取机制，无需 sampler
        // Subpass 1: SSAO (no resolve)
        vsg::SubpassDescription subpass1;
        subpass1.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass1.colorAttachments.emplace_back(colorAttachmentRefSSAONoise);
        subpass1.depthStencilAttachments.emplace_back(depthAttachmentRef);
        vsg::AttachmentReference colorRef_Read = {3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        subpass1.inputAttachments = {colorRef_Read};

        // ===== Subpass 2: Denoise + 合并 =====
        // 写入 [0] multisample color（resolve 到 [1]）和 [8] shadowSample
        // 读取 input attachments：
        //   - gbuffer0 [3]：通过 input attachment 读取颜色数据
        //   - shadowWrite [7]：通过 input attachment 读取阴影原始数据
        // resolveAttachments 将 [0] 的多样本颜色 resolve 到 [1] 的单样本颜色
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

        // ===== Subpass 依赖关系（Dependency）=====
        // Vulkan 的 subpass 依赖定义了 subpass 之间的执行顺序和内存可见性
        // 没有正确的依赖关系，Vulkan 可能乱序执行 subpass，导致读取到未写入的数据

        // 依赖 1: External -> Subpass 0（颜色输出）
        // 确保外部操作完成后，Subpass 0 才开始写入 color attachment
        // Dependencies - same as createMRTMultisampledRenderPass
        vsg::SubpassDependency colorDependency = {};
        colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        colorDependency.dstSubpass = 0;
        colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.srcAccessMask = 0;
        colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency.dependencyFlags = 0;

        // 依赖 2: External -> Subpass 0（深度）
        // 确保外部 depth write 完成后，Subpass 0 才开始 depth test
        vsg::SubpassDependency depthDependency = {};
        depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        depthDependency.dstSubpass = 0;
        depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dependencyFlags = 0;

        // 依赖 3: Subpass 0 -> Subpass 1（颜色输出 -> SSAO shader 读取）
        // Subpass 0 写入 gbuffer0 后，Subpass 1 的 fragment shader 才能通过 input attachment 读取
        vsg::SubpassDependency colorDependency_ssao = {};
        colorDependency_ssao.srcSubpass = 0;
        colorDependency_ssao.dstSubpass = 1;
        colorDependency_ssao.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency_ssao.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        colorDependency_ssao.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency_ssao.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency_ssao.dependencyFlags = 0;

        // 依赖 4: Subpass 0 -> Subpass 1（深度）
        // 确保 Subpass 0 的 depth 操作完成后，Subpass 1 才能访问深度附件
        vsg::SubpassDependency depthDependency_ssao = {};
        depthDependency_ssao.srcSubpass = 0;
        depthDependency_ssao.dstSubpass = 1;
        depthDependency_ssao.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency_ssao.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        depthDependency_ssao.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency_ssao.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency_ssao.dependencyFlags = 0;

        // 依赖 5: Subpass 1 -> Subpass 2（SSAO 输出 -> Denoise 读取）
        // Subpass 1 写入 ssaoResult 后，Subpass 2 才能读取
        // VK_DEPENDENCY_BY_REGION_BIT：允许 tile-based GPU 按屏幕区域并行执行（性能优化）
        vsg::SubpassDependency ssaoToDenoiseDependency = {};
        ssaoToDenoiseDependency.srcSubpass = 1;
        ssaoToDenoiseDependency.dstSubpass = 2;
        ssaoToDenoiseDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        ssaoToDenoiseDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        ssaoToDenoiseDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        ssaoToDenoiseDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        ssaoToDenoiseDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // 依赖 6: Subpass 1 -> Subpass 2（深度）
        // 确保 Subpass 1 的深度读取完成后再让 Subpass 2 读取
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
        // ===== Non-multisample 路径的 RenderPass =====
        // 无 MSAA 时不需要 resolve 附件，attachment 布局更简单：
        // [0] color, [1] gbuffer0, [2] gbuffer1, [3] gbuffer2,
        // [4] ssao, [5] shadowWrite, [6] shadowSample, [7] depth
        // Non-multisampled render pass - based on createMRTRenderPass
        auto colorAttachment = vsg::defaultColorAttachment(imageFormat);
        // 关键区别：finalLayout 是 COLOR_ATTACHMENT_OPTIMAL 而非 PRESENT_SRC_KHR
        // 因为是离屏渲染，不需要 presentation，保持 COLOR_ATTACHMENT 以便后续 blit
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

        // ===== Subpass 0: 主渲染 / GBuffer MRT 输出 =====
        // 与 multisample 版本逻辑相同，但 attachment 索引不同（无 resolve 附件）
        // Subpass 0: Main rendering
        vsg::SubpassDescription subpass;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachments.emplace_back(colorAttachmentRefColor);
        subpass.colorAttachments.emplace_back(colorAttachmentRefNormal);
        subpass.colorAttachments.emplace_back(colorAttachmentRefWorldPos);
        subpass.colorAttachments.emplace_back(colorAttachmentRefShadowWrite);
        subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

        // ===== Subpass 1: SSAO 计算 =====
        // 写入 [4] ssaoResult，读取 [1] gbuffer0 作为 input attachment
        // Subpass 1: SSAO
        vsg::SubpassDescription subpass1;
        subpass1.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass1.colorAttachments.emplace_back(colorAttachmentRefSSAONoise);
        subpass1.depthStencilAttachments.emplace_back(depthAttachmentRef);
        vsg::AttachmentReference colorRef_Read = {1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        subpass1.inputAttachments = {colorRef_Read};

        // ===== Subpass 2: Denoise + 合并 =====
        // 写入 [0] color（最终输出）和 [6] shadowSample
        // 读取 input attachments：[1] gbuffer0 和 [5] shadowWrite
        // 无 resolve（非 multisample 模式）
        // Subpass 2: Denoise
        vsg::SubpassDescription subpass2;
        subpass2.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass2.colorAttachments.emplace_back(colorAttachmentRef);
        subpass2.colorAttachments.emplace_back(colorAttachmentRefShadowSample);
        subpass2.depthStencilAttachments.emplace_back(depthAttachmentRef);
        vsg::AttachmentReference colorRef_ShadowWrite = {5, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        subpass2.inputAttachments = {colorRef_Read, colorRef_ShadowWrite};

        vsg::RenderPass::Subpasses subpasses{subpass, subpass1, subpass2};

        // ===== Subpass 依赖关系（非 multisample 路径）=====
        // 逻辑与 multisample 路径相同，但 attachment 索引不同
        // Dependencies - same as createMRTRenderPass
        vsg::SubpassDependency colorDependency = {};
        colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        colorDependency.dstSubpass = 0;
        colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.srcAccessMask = 0;
        colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency.dependencyFlags = 0;

        // 依赖 2: External -> Subpass 0（深度）
        vsg::SubpassDependency depthDependency = {};
        depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        depthDependency.dstSubpass = 0;
        depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dependencyFlags = 0;

        // 依赖 3: Subpass 0 -> Subpass 1（颜色 -> SSAO）
        vsg::SubpassDependency colorDependency_ssao = {};
        colorDependency_ssao.srcSubpass = 0;
        colorDependency_ssao.dstSubpass = 1;
        colorDependency_ssao.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency_ssao.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        colorDependency_ssao.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency_ssao.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency_ssao.dependencyFlags = 0;

        // 依赖 4: Subpass 0 -> Subpass 1（深度）
        vsg::SubpassDependency depthDependency_ssao = {};
        depthDependency_ssao.srcSubpass = 0;
        depthDependency_ssao.dstSubpass = 1;
        depthDependency_ssao.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency_ssao.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        depthDependency_ssao.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency_ssao.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency_ssao.dependencyFlags = 0;

        // 依赖 5: Subpass 1 -> Subpass 2（SSAO -> Denoise）
        vsg::SubpassDependency ssaoToDenoiseDependency = {};
        ssaoToDenoiseDependency.srcSubpass = 1;
        ssaoToDenoiseDependency.dstSubpass = 2;
        ssaoToDenoiseDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        ssaoToDenoiseDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        ssaoToDenoiseDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        ssaoToDenoiseDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        ssaoToDenoiseDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // 依赖 6: Subpass 1 -> Subpass 2（深度）
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

/**
 * buildFramebuffer - 创建 Framebuffer，将所有 ImageView 按 RenderPass 的 attachment 顺序绑定
 *
 * 关键：ImageView 的添加顺序必须与 buildRenderPass 中 AttachmentDescription 的定义顺序严格一致！
 * Vulkan 通过索引来匹配 attachment，顺序错误会导致渲染结果错乱或验证层报错。
 */
void OffscreenRenderTarget::buildFramebuffer(VkExtent2D extent)
{
    vsg::ImageViews attachments;

    if (multisampleImageView)
    {
        // ===== Multisample 路径的 Framebuffer 附件顺序 =====
        // 必须与 buildRenderPass 中的 AttachmentDescription 顺序一致！
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
        // ===== Non-multisample 路径的 Framebuffer 附件顺序 =====
        // 必须与 buildRenderPass 中的 AttachmentDescription 顺序一致！
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
