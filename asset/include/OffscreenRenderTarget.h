#pragma once

#include "vsg/all.h"

/**
 * OffscreenRenderTarget - 离屏渲染目标（Offscreen Render Target）
 *
 * 封装了一个完整的离屏 MRT（Multiple Render Target）渲染管线，包含：
 * - 3 张 GBuffer 附件（color / normal / worldPos）
 * - SSAO 结果附件、shadow write/sample 附件
 * - 深度附件（支持 multisample resolve）
 * - 一个 3-subpass 的 RenderPass：
 *     Subpass 0: 主渲染（GBuffer MRT 输出 + shadow write）
 *     Subpass 1: SSAO 计算（读取 GBuffer0 作为 input attachment）
 *     Subpass 2: Denoise + merge（读取 SSAO + shadowWrite 作为 input，输出最终颜色 + shadowSample）
 *
 * 与窗口 swapchain 的 on-screen 渲染不同，此 render target 的 attachment[0]（colorImage）
 * 始终保持 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL，用于后续 blit 到窗口。
 * 这是 VSG（VulkanSceneGraph）中 Object 的子类，使用 ref_ptr 管理生命周期。
 */
class OffscreenRenderTarget : public vsg::Inherit<vsg::Object, OffscreenRenderTarget>
{
public:
    // 初始化：创建所有 GBuffer 图像、SSAO/shadow 图像、深度图像，并将 depth image 转换到 DEPTH_STENCIL_ATTACHMENT_OPTIMAL layout
    // VSG 中 Image 需要先 compile（编译 GPU 资源）再 allocateAndBindMemory（分配显存）
    void init(vsg::ref_ptr<vsg::Device> device, VkExtent2D extent, VkSampleCountFlagBits samples, VkFormat depthFormat, VkImageUsageFlags depthImageUsage);

    // 构建 3-subpass 的 RenderPass，定义 attachment 描述、subpass 配置和 subpass 间的依赖关系
    // 与 on-screen renderPass 的区别：attachment[0] 的 finalLayout 是 COLOR_ATTACHMENT_OPTIMAL（非 PRESENT_SRC_KHR）
    void buildRenderPass(vsg::ref_ptr<vsg::Device> device, VkFormat imageFormat, VkFormat depthFormat, bool requiresDepthRead);

    // 使用 renderPass 和所有 ImageView 创建 Framebuffer
    // attachment 的顺序必须与 buildRenderPass 中的 AttachmentDescription 顺序严格一致
    void buildFramebuffer(VkExtent2D extent);

    VkExtent2D getExtent() const { return _extent; }

    // ---- GBuffer 附件（Geometry Buffer）----
    // GBuffer0: 存储颜色（RGBA），Subpass 1 的 SSAO shader 通过 input attachment 读取
    // 格式 R32G32B32A32_SFLOAT，usage 含 INPUT_ATTACHMENT_BIT（供 subpass 内部读取）
    vsg::ref_ptr<vsg::Image> gbufferImage0;
    vsg::ref_ptr<vsg::ImageView> gbufferImageView0;

    // GBuffer1: 存储法线（Normal），Subpass 2 的 denoise shader 可通过 sampled 方式读取
    // 格式 R32G32B32A32_SFLOAT，usage 含 SAMPLED_BIT（供跨 subpass 的 shader 采样）
    vsg::ref_ptr<vsg::Image> gbufferImage1;
    vsg::ref_ptr<vsg::ImageView> gbufferImageView1;

    // GBuffer2: 存储世界坐标（World Position），同上
    vsg::ref_ptr<vsg::Image> gbufferImage2;
    vsg::ref_ptr<vsg::ImageView> gbufferImageView2;

    // ---- SSAO 结果附件 ----
    // Subpass 1 输出的 SSAO 遮蔽值，后续可被其他 pass 采样使用
    // 格式 R32G32B32A32_SFLOAT，usage 含 SAMPLED_BIT
    vsg::ref_ptr<vsg::Image> ssaoResultImage;
    vsg::ref_ptr<vsg::ImageView> ssaoResultImageView;

    // ---- Shadow 附件 ----
    // Shadow Write: Subpass 0 写入的阴影数据，Subpass 2 通过 input attachment 读取
    // usage 含 INPUT_ATTACHMENT_BIT | SAMPLED_BIT
    vsg::ref_ptr<vsg::Image> shadowWriteImage;
    vsg::ref_ptr<vsg::ImageView> shadowWriteImageView;

    // Shadow Sample: Subpass 2 输出的去噪后阴影结果，供后续光照 pass 采样
    // usage 含 SAMPLED_BIT
    vsg::ref_ptr<vsg::Image> shadowSampleImage;
    vsg::ref_ptr<vsg::ImageView> shadowSampleImageView;

    // ---- 深度附件 ----
    // 主深度缓冲，所有 subpass 共享同一深度附件
    // 当开启 multisampling 且 requiresDepthRead 时，此为 resolve 后的单样本深度
    vsg::ref_ptr<vsg::Image> depthImage;
    vsg::ref_ptr<vsg::ImageView> depthImageView;

    // Multisample 深度附件（仅在 multisampling + requiresDepthRead 时创建）
    // 保存原始的多样本深度，Vulkan 在 subpass 0 结束时自动 resolve 到 depthImage（单样本）
    vsg::ref_ptr<vsg::Image> multisampleDepthImage;
    vsg::ref_ptr<vsg::ImageView> multisampleDepthImageView;

    // ---- Multisample 颜色附件 ----
    // 多样本颜色附件（仅在开启 multisampling 时创建）
    // Subpass 0 渲染到此附件，Vulkan 自动 resolve 到 colorImageView（单样本）
    // 类似 VSG 的 Window::buildSwapchain 中的 multisample 路径
    vsg::ref_ptr<vsg::Image> multisampleImage;
    vsg::ref_ptr<vsg::ImageView> multisampleImageView;

    // ---- 离屏颜色附件（最终输出）----
    // attachment[0]，最终合成的颜色输出
    // 与 on-screen renderPass 的区别：layout 始终保持 COLOR_ATTACHMENT_OPTIMAL
    // 用于后续 blit/transfer 到窗口 swapchain
    vsg::ref_ptr<vsg::Image> colorImage;
    vsg::ref_ptr<vsg::ImageView> colorImageView;

    // ---- RenderPass 和 Framebuffer ----
    // 3-subpass 的 RenderPass，定义了所有 attachment、subpass 和依赖关系
    vsg::ref_ptr<vsg::RenderPass> renderPass;

    // Framebuffer，将所有 ImageView 按 renderPass 定义的顺序绑定
    vsg::ref_ptr<vsg::Framebuffer> framebuffer;

private:
    VkExtent2D _extent;               // 渲染目标的像素尺寸（宽 x 高）
    VkSampleCountFlagBits _samples;   // 多重采样数，VK_SAMPLE_COUNT_1_BIT 表示不开启 MSAA
};
