#include "IBL.h"
#include "Utils.h"
#include <random>

#ifndef VSGSSAOPASS_H
#define VSGSSAOPASS_H

/**
 * SSAOPass - 屏幕空间环境光遮蔽（Screen-Space Ambient Occlusion）
 *
 * 工作流程分两个 subpass（子通道）：
 *   Subpass 1: SSAO 生成（generate）
 *     - 读取 G-Buffer 中的颜色、法线、世界坐标
 *     - 使用随机噪声纹理做采样偏移，生成 AO 值
 *   Subpass 2: SSAO 去噪（denoise）
 *     - 将 SSAO 结果与 shadow factor（阴影因子）合并
 *     - 输出到最终 color attachment
 *
 * VSG 概念速查：
 *   ShaderSet: 描述一组 shader（vertex + fragment）及其 attribute/descriptor 绑定
 *   GraphicsPipelineConfigurator: 帮助配置 Vulkan graphics pipeline 的工具类
 *   Input Attachment: subpass 内直接读取上一个 subpass 的输出（无需采样器）
 *   Combined Image Sampler: 通过采样器从纹理读取（需要 sampler + image view）
 */
namespace SSAOPass{
    // ---- Subpass 1: SSAO 生成 ----

    // 创建 SSAO 生成用的 ShaderSet，绑定 G-Buffer 的 input attachment 和噪声纹理
    vsg::ref_ptr<vsg::ShaderSet> customSSAOShaderSet(vsg::ref_ptr<const vsg::Options> options);

    // 构建 SSAO 生成的渲染数据（全屏四边形 + 随机噪声纹理），添加到 scene graph
    // GBufferView0/1/2 分别对应 G-Buffer 的 color、normal、worldPos 附件
    void buildSSAOData(vsg::ref_ptr<vsg::Options> options, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ImageView> GBufferView0, vsg::ref_ptr<vsg::ImageView> GBufferView1, vsg::ref_ptr<vsg::ImageView> GBufferView2, VkExtent2D extent);

    // ---- Subpass 2: SSAO 去噪 ----

    // 创建 SSAO 去噪用的 ShaderSet，绑定 G-Buffer color、shadow 写入附件、SSAO 结果采样
    // 包含 alpha blending 配置，用于混合 SSAO 和 shadow factor
    vsg::ref_ptr<vsg::ShaderSet> customSSAODenoiseShaderSet(vsg::ref_ptr<const vsg::Options> options);

    // 构建 SSAO 去噪的渲染数据（全屏四边形），添加到 scene graph
    // ShadowWriteView 是 shadow pass 的输出，SSAOResultImageView 是 subpass 1 的 SSAO 输出
    void buildSSAODenoiseData(vsg::ref_ptr<vsg::Options> options, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ImageView> GBufferView0, vsg::ref_ptr<vsg::ImageView> ShadowWriteView, vsg::ref_ptr<vsg::ImageView> SSAOResultImageView);
}
#endif