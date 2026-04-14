/**
 * Utils.h — 渲染工具函数集合
 *
 * 本文件提供两类工具：
 *   1. Vulkan Sampler 工厂函数 — 创建不同过滤/寻址模式的采样器（sampler），
 *      用于在 shader 中以不同方式采样纹理（texture）。
 *   2. BuildClearCommandGraph — 构建每帧开始时的清除命令图（CommandGraph），
 *      负责清空 G-Buffer 和深度缓冲区（depth buffer）。
 *
 * VSG 背景知识：
 *   - vsg::Sampler 对应 Vulkan 中的 VkSampler，决定纹理采样时的插值方式（linear/nearest）
 *     和 UV 超出 [0,1] 范围时的处理方式（repeat/clamp）。
 *   - vsg::CommandGraph 是 VSG 的命令组织结构，VulkanSceneGraph 会将其中的命令
 *     录制到 command buffer 中并提交给 GPU 执行。
 */

#ifndef UTILS_H
#define UTILS_H

#include "vsg/all.h"
#include "OffscreenRenderTarget.h"

namespace Utils{
    /**
     * 创建双线性插值（bilinear）采样器
     *
     * 用途：采样需要平滑插值的纹理，例如环境贴图（environment map）、
     *       漫反射贴图（diffuse map）等。
     *
     * 过滤模式：magFilter = LINEAR, minFilter = LINEAR（放大/缩小时都使用线性插值）
     * Mipmap 模式：NEAREST（不使用 mipmap 层间插值）
     * 寻址模式：CLAMP_TO_EDGE（UV 超出 [0,1] 时取边缘像素，不做重复）
     *
     * 注意：使用 static 局部变量实现单例模式，多次调用返回同一个 sampler 对象。
     */
    inline vsg::ref_ptr<vsg::Sampler> createLinearSampler()
    {
        static vsg::ref_ptr<vsg::Sampler> sampler = []() {
            auto s = vsg::Sampler::create();
            
            s->magFilter = VK_FILTER_LINEAR;
            s->minFilter = VK_FILTER_LINEAR;
            s->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            
            s->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            
            return s;
        }();

        return sampler;
    }

    /**
     * 创建最近邻采样器（nearest-neighbor），UV 重复模式
     *
     * 用途：采样需要像素精确（pixel-perfect）的纹理，例如 G-Buffer 中存储的
     *       索引数据（ID buffer）或布尔标志位，不能使用插值否则会破坏数据。
     *
     * 过滤模式：magFilter = NEAREST, minFilter = NEAREST（取最近的 texel，不做插值）
     * Mipmap 模式：NEAREST
     * 寻址模式：REPEAT（UV 超出 [0,1] 时重复纹理，适用于平铺纹理如棋盘格）
     */
    inline vsg::ref_ptr<vsg::Sampler> createNearestSampler()
    {
        static vsg::ref_ptr<vsg::Sampler> sampler = []() {
            auto s = vsg::Sampler::create();

            s->magFilter = VK_FILTER_NEAREST;
            s->minFilter = VK_FILTER_NEAREST;
            s->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

            s->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s->addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

            return s;
        }();

        return sampler;
    }

    /**
     * 创建最近邻采样器（nearest-neighbor），UV 边缘钳制模式
     *
     * 用途：与 createNearestSampler 类似，但 UV 超出 [0,1] 时不做重复，
     *       而是钳制（clamp）到边缘像素。适用于屏幕空间后处理效果（post-processing），
     *       例如 SSAO、阴影贴图等需要精确采样且不允许 UV 溢出的场景。
     *
     * 过滤模式：magFilter = NEAREST, minFilter = NEAREST
     * Mipmap 模式：NEAREST
     * 寻址模式：CLAMP_TO_EDGE
     */
    inline vsg::ref_ptr<vsg::Sampler> createNearestClampSampler()
    {
        static vsg::ref_ptr<vsg::Sampler> sampler = []() {
            auto s = vsg::Sampler::create();

            s->magFilter = VK_FILTER_NEAREST;
            s->minFilter = VK_FILTER_NEAREST;
            s->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

            s->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

            return s;
        }();

        return sampler;
    }

    /**
     * 构建每帧开始时的清除命令图（Clear CommandGraph）
     *
     * 在 deferred shading（延迟渲染）中，每帧开始前需要清空 G-Buffer 和深度缓冲区。
     * 本函数将所有清除操作组织到一个 CommandGraph 中，确保它们在同一 command buffer 中执行。
     *
     * 清除操作包括：
     *   1. 深度缓冲区清除 — 清除值为 0.0f（反向深度 reversed depth）
     *      - 反向深度原理：本项目使用 reversed depth，深度值 0.0f 表示最远处（far plane），
     *        1.0f 表示最近处（near plane）。这与常规深度缓冲（0=近，1=远）相反。
     *      - 优势：reversed depth 在浮点数精度分配上更合理，近处精度更高，
     *        可有效减少远处的 z-fighting（深度冲突）现象。
     *   2. G-Buffer 颜色附件清除 — 清除 4 张 offscreen 颜色图像：
     *      - gbufferImage0: 通常存储反照率（albedo）或基础颜色
     *      - gbufferImage1: 通常存储世界空间法线（normal）
     *      - gbufferImage2: 通常存储其他材质属性
     *      - shadowWriteImage: 阴影写入相关的图像
     *
     * MSAA 处理：当启用 MSAA（msaaSamples != 1）时，额外清除 multisampleDepthImage
     *           （多重采样深度图像），这是 MSAA 的 resolve 之前使用的深度附件。
     *
     * VSG 背景知识：
     *   - vsg::ClearDepthStencilImage 对应 Vulkan 的 vkCmdClearDepthStencilImage
     *   - vsg::ClearColorImage 对应 Vulkan 的 vkCmdClearColorImage
     *   - VkImageSubresourceRange 指定要清除的图像子资源范围（mip level、array layer）
     */
    inline void BuildClearCommandGraph(vsg::ref_ptr<vsg::CommandGraph> clear_image_commandgraph, VkExtent2D extent, vsg::ref_ptr<OffscreenRenderTarget> offscreenTarget, VkSampleCountFlagBits msaaSamples){
        // --- 深度缓冲区清除 ---
        // clearDepth: MSAA 多重采样深度图像（仅 MSAA 模式下使用）
        vsg::ref_ptr<vsg::ClearDepthStencilImage> clearDepth = vsg::ClearDepthStencilImage::create();
        // clearDepth1: 最终深度图像（resolve 后的结果，始终需要清除）
        vsg::ref_ptr<vsg::ClearDepthStencilImage> clearDepth1 = vsg::ClearDepthStencilImage::create();
        
        clearDepth->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        clearDepth->depthStencil = {0.0f, 0};  // 反向深度：0.0f = 远平面，清空为最远处
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        clearDepth->ranges = {range};

        clearDepth1->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        clearDepth1->depthStencil = {0.0f, 0};  // 同样使用 0.0f 清除（反向深度）
        clearDepth1->ranges = {range};
        if(msaaSamples != VK_SAMPLE_COUNT_1_BIT)
            clearDepth->image = offscreenTarget->multisampleDepthImage;  // MSAA 深度附件
        clearDepth1->image = offscreenTarget->depthImage;  // resolve 后的最终深度

        // 将深度清除命令添加到命令图（MSAA 模式下需要清除两张深度图）
        if(msaaSamples != VK_SAMPLE_COUNT_1_BIT)
            clear_image_commandgraph->addChild(clearDepth);
        clear_image_commandgraph->addChild(clearDepth1);

        // --- G-Buffer 颜色附件清除 ---
        VkImageSubresourceRange range0{};
        range0.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  // 颜色附件（非深度）
        range0.baseMipLevel = 0;
        range0.levelCount = 1;
        range0.baseArrayLayer = 0;
        range0.layerCount = 1;

        // G-Buffer 0: 基础颜色/反照率（albedo），清除为黑色
        auto clearColor0 = vsg::ClearColorImage::create();
        clearColor0->image = offscreenTarget->gbufferImage0;
        clearColor0->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        clearColor0->color = {0.0f, 0.0f, 0.0f, 1.0f};
        clearColor0->ranges = {range0};

        // G-Buffer 1: 世界空间法线（world-space normal），清除为黑色（无几何体的区域）
        auto clearColor1 = vsg::ClearColorImage::create();
        clearColor1->image = offscreenTarget->gbufferImage1;
        clearColor1->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        clearColor1->color = {0.0f, 0.0f, 0.0f, 1.0f};
        clearColor1->ranges = {range0};

        // G-Buffer 2: 其他材质属性，清除为黑色（无几何体的区域）
        auto clearColor2 = vsg::ClearColorImage::create();
        clearColor2->image = offscreenTarget->gbufferImage2;
        clearColor2->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        clearColor2->color = {0.0f, 0.0f, 0.0f, 1.0f};
        clearColor2->ranges = {range0};

        // Shadow Write 图像：阴影写入相关的缓冲区，清除为特殊值
        auto clearColor3 = vsg::ClearColorImage::create();
        clearColor3->image = offscreenTarget->shadowWriteImage;
        clearColor3->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        clearColor3->color = {1, -2, 0.0f, 1.0f};  // 特殊清除值，用于阴影计算的初始状态
        clearColor3->ranges = {range0};

        // 将所有 G-Buffer 清除命令添加到命令图
        clear_image_commandgraph->addChild(clearColor0);
        clear_image_commandgraph->addChild(clearColor1);
        clear_image_commandgraph->addChild(clearColor2);
        clear_image_commandgraph->addChild(clearColor3);

        // auto clearCmd = vsg::ClearAttachments::create();
        // VkClearRect clearRect{};
        // clearRect.rect = {
        //     0,          // 左上角x
        //     0,          // 左上角y
        //     extent.width,      // 宽度
        //     extent.height      // 高度
        // };
        // clearRect.baseArrayLayer = 0;    // 起始图层
        // clearRect.layerCount = 1;       // 图层数量

        // for (uint32_t i = 1; i < 4; ++i) // 限制最多3张
        // {
        //     VkClearAttachment attachment{};
        //     attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // 颜色附件
        //     attachment.colorAttachment = i;                    // 附件索引（0/1/2）
        //     attachment.clearValue.color = {0, 0, 0, 0};      // 该附件的清除颜色
        //     clearCmd->attachments.push_back(attachment);
        //     clearCmd->rects.push_back(clearRect);
        // }

        // clear_image_commandgraph->addChild(clearCmd);
        // 3. 添加到命令图（和原有深度清除逻辑一致）

    }
}

#endif // UTILS_H