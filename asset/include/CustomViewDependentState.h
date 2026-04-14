/**
 * CustomViewDependentState.h
 *
 * 自定义的 ViewDependentState，用于管理级联阴影贴图（CSM, Cascaded Shadow Maps）和场景光照数据。
 *
 * VSG 概念说明：
 *   - vsg::ViewDependentState 是 VSG 中与每个 View 绑定的状态管理器，负责在渲染时
 *     提供光照、阴影贴图等 per-view 数据给 shader。每帧由 RecordTraversal 遍历调用。
 *   - 它通过 descriptor set（描述符集合）将光源数据和阴影贴图绑定到 GPU pipeline。
 *
 * 本类核心职责：
 *   1. 创建并管理 shadow map 相关的 Vulkan 资源（depth image、sampler、descriptor set）
 *   2. 每帧更新光源数据、计算 CSM 分割、设置 shadow camera 的投影和观察矩阵
 *   3. 管理一个 compute pass，用于 GPU 端的 shadow frustum culling（阴影视锥体剔除）
 */

#pragma once

#include <vsg/all.h>
#include "MyMask.h"
#include "CADMesh.h"

#ifndef CUSTOMVIEWDEPENDENTSTATE_H
#define CUSTOMVIEWDEPENDENTSTATE_H

class CustomViewDependentState : public vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>
{
public:
    // 虚拟场景的世界空间包围盒（用于计算 shadow camera 的正交投影范围）
    mutable vsg::dbox scene_bound_ws_virtual;
    // 真实场景的世界空间包围盒（用于限制 shadow far plane，确保真实物体被包含）
    vsg::dbox scene_bound_ws_real;

    mutable bool draw_shadow_light = true;  // 光照参数变化时设为 true，触发阴影重算
    mutable bool draw_shadow_pose = true;   // 模型位姿变化时设为 true，触发包围盒重算

    // shadow frustum culling 的 compute shader 命令图
    // VSG 概念：CommandGraph 是命令提交的顶层节点，每个 CommandGraph 可绑定到不同的 queue family
    // 这里绑定到 compute queue，用于 GPU 端剔除不在 shadow frustum 内的 draw call
    vsg::ref_ptr<vsg::CommandGraph> computeCommandGraphShadow;

    vsg::ref_ptr<vsg::Options> options;

    // 无 compare 功能的 shadow map sampler（用于手动 PCF 采样）
    vsg::ref_ptr<vsg::DescriptorImage> shadowMapSamplerImages;

    // 构造函数：传入 View、Device、compute queue family 和资源搜索选项
    CustomViewDependentState(vsg::View* in_view, vsg::ref_ptr<vsg::Device> device, int computeQueueFamily, vsg::ref_ptr<vsg::Options> in_options) :
        vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>(in_view), computeCommandGraphShadow(vsg::CommandGraph::create(device, computeQueueFamily)), options(in_options){}

    // 重写 init：创建 shadow map image、descriptor set layout、compute pipeline 等资源
    virtual void init(vsg::ResourceRequirements& requirements) override;

    // 重写 traverse：每帧由 RecordTraversal 调用，更新光源数据并计算 CSM 分割
    void traverse(vsg::RecordTraversal& rt) const override;

    // 创建自定义的 2D shadow map image（支持 array layers，即多级级联）
    vsg::ref_ptr<vsg::Image> createCustomShadowImage(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage);
};

#endif // CUSTOMVIEWDEPENDENTSTATE_H