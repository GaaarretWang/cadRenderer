#pragma once

/**
 * CustomViewDependentState1 - View1（合成渲染 pass）专用的轻量级 ViewDependentState
 *
 * 这是 CustomViewDependentState 的一个轻量级变体（variant），用于第二个渲染 pass（synthesis pass）。
 * 它的核心设计思路是：不独立管理灯光和阴影，而是复用主 View 的 descriptor set，
 * 确保两个 view 使用完全相同的 shadow map 和灯光数据。
 *
 * VSG 知识：ViewDependentState 是 VulkanSceneGraph 中管理"视图相关状态"的基类，
 * 典型用途包括灯光数据（lightData）、阴影贴图（shadow maps）、视口数据（viewportData）等。
 * 它通过 descriptor set 与 GPU shader 交互。
 */

#include <vsg/all.h>
#include "MyMask.h"
#include "CustomViewDependentState.h"

#ifndef CUSTOMVIEWDEPENDENTSTATE1_H
#define CUSTOMVIEWDEPENDENTSTATE1_H

class CustomViewDependentState1 : public vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState1>
{
public:
    // 场景包围盒（虚拟坐标系下 / 真实世界坐标系下）
    vsg::dbox scene_bound_ws_virtual;
    vsg::dbox scene_bound_ws_real;

    // 阴影贴图采样器（用于无 compare 操作的采样，shader 中手动做阴影比较时使用）
    vsg::ref_ptr<vsg::DescriptorImage> shadowMapSamplerImages;

    CustomViewDependentState1(vsg::View* in_view) :
        vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState1>(in_view){}

    // 初始化 descriptor set：先创建自己的布局，然后覆盖为主 View（pre_depth_pass）的 descriptor set
    virtual void init(vsg::ResourceRequirements& requirements) override;

    // traverse() 为空实现——这个 View 不独立管理灯光/阴影，仅依赖主 View 的数据
    void traverse(vsg::RecordTraversal& rt) const override;

    // 创建自定义的 shadow map Image（2D array，用于多级阴影贴图）
    vsg::ref_ptr<vsg::Image> createCustomShadowImage(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage);
};

#endif // CUSTOMVIEWDEPENDENTSTATE1_H