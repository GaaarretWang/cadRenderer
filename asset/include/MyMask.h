/**
 * MyMask.h — 视图掩码（View Mask）定义，用于选择性渲染
 *
 * 本文件定义了一系列位掩码常量（bit mask），配合 VSG 的 vsg::Switch 节点实现选择性遍历。
 *
 * 工作原理：
 *   - 每个场景节点（Node）可以设置一个 mask 值（vsg::Mask 类型，本质是 uint64_t）。
 *   - 在渲染遍历（render traversal）时，VSG 会将当前活动的 mask 与节点的 mask 进行
 *     位与运算（bitwise AND）。只有结果非零时，该节点才会被渲染。
 *   - 这使得可以通过切换 mask 状态来控制哪些物体可见，例如：只显示 PBR 物体、
 *     隐藏 wireframe 等。
 *
 *   与 vsg::Switch 的配合：
 *   - vsg::Switch 节点可以包含多个子节点组（children），每个子节点关联一个 mask 值。
 *   - 通过设置 Switch 的 active mask，可以选择性地遍历（traverse）哪些子节点。
 *   - 例如：Switch 包含 [skybox, wireframe_overlay, camera_image] 三个子节点，
 *     设置 mask 为 MASK_SKYBOX | MASK_WIREFRAME 时，只遍历 skybox 和 wireframe_overlay。
 *
 * 使用示例：
 *   // 创建一个 PBR 物体节点，设置其 mask
 *   object->mask = MASK_PBR_FULL;
 *
 *   // 在 Switch 中选择性显示
 *   auto sw = vsg::Switch::create();
 *   sw->addChild(MASK_SKYBOX, skyboxNode);
 *   sw->addChild(MASK_WIREFRAME, wireframeNode);
 *   sw->setActiveMask(MASK_SKYBOX | MASK_WIREFRAME);  // 两个都显示
 */
#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <cstdint>
#include <vsg/core/Mask.h>

//constexpr vsg::Mask MASK_SHADOW_CASTER = 1ul;
//constexpr vsg::Mask MASK_SHADOW_RECEIVER = 2ul;
//constexpr vsg::Mask MASK_BACKGROUND = 4ul;
//constexpr vsg::Mask MASK_MODEL = MASK_SHADOW_CASTER | MASK_SHADOW_RECEIVER;
//constexpr vsg::Mask MASK_GEOMETRY = MASK_SHADOW_RECEIVER | MASK_BACKGROUND;

#ifndef MYMASK_H
#define MYMASK_H

// --- 渲染通道掩码定义 ---
// 每个常量是一个独立的位（bit），用于标识物体属于哪个渲染通道

// PBR 完整渲染通道：包含 PBR 材质的物体，参与完整的光照计算（漫反射+镜面反射+IBL）
constexpr vsg::Mask MASK_PBR_FULL = 1ul;

// 阴影投射者（shadow caster）：该物体会向其他表面投射阴影，参与 shadow map 生成 pass
constexpr vsg::Mask MASK_SHADOW_CASTER = 2ul;

// 阴影接收者（shadow receiver）：该物体会接收来自其他物体的阴影，参与阴影采样 pass
constexpr vsg::Mask MASK_SHADOW_RECEIVER = 4ul;

// 天空盒（skybox）：背景环境渲染，通常是一个覆盖全屏的立方体贴图
constexpr vsg::Mask MASK_SKYBOX = 8ul;

// 线框叠加层（wireframe overlay）：以线框模式显示的物体，用于调试或 CAD 模型可视化
constexpr vsg::Mask MASK_WIREFRAME = 16ul;

// 相机图像层（camera image）：AR/MR 场景中的真实相机画面背景层
constexpr vsg::Mask MASK_CAMERA_IMAGE = 32ul;

// 文字渲染层（text overlay）：屏幕空间或世界空间的文字标注
constexpr vsg::Mask MASK_TEXT = 64ul;

// SSAO 通道：屏幕空间环境光遮蔽（Screen-Space Ambient Occlusion），
// 参与 SSAO 计算 pass，但不参与主渲染
constexpr vsg::Mask MASK_SSAO = 128ul;

// 默认绘制掩码：排除 SSAO 和天空盒，包含所有常规渲染物体
// 表达式解析：~(MASK_SSAO | MASK_SKYBOX) = ~(128 | 8) = ~136
// 效果：所有非 SSAO 且非天空盒的物体都会被绘制
constexpr vsg::Mask MASK_DRAW = ~(MASK_SSAO | MASK_SKYBOX);

/**
 * 合并 shader 类型枚举
 * 用于指定最终合成阶段使用哪种 shader 模式来混合虚拟与现实图像
 */
enum mergeShaderType{
    FULL_MODEL,     // 完整模型渲染模式：虚拟物体 + 光照 + 后处理
    CAMERA_DEPTH,   // 相机深度模式：使用真实相机的深度信息进行混合
    CAD_DAPTH       // CAD 深度模式：使用 CAD 模型的深度信息进行混合
};
#endif // MYMASK_H
