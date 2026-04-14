# 渲染管线总览 — 执行顺序与依赖关系

> 分析分支：`encoder`
> 分析日期：2026-04-03

---

## 1. 双 CommandGraph 架构

本渲染器采用 **双 CommandGraph** 帧结构，每帧分两遍执行：

```
CommandGraph（每帧第一遍 — 阴影 + 剔除 + 主渲染 + SSAO）:
  ├── Clear Pass（清屏：GBuffer + 深度）
  ├── Pass1: Frustum Culling Compute（视锥体剔除）
  └── Render Pass（离屏 FBO，3 个 Subpass）
      ├── Subpass 0: Main Render（虚拟 CAD 模型 → GBuffer MRT）
      ├── Subpass 1: SSAO（屏幕空间环境光遮蔽）
      └── Subpass 2: SSAO Denoise + Shadow Merge

CommandGraph1（每帧第二遍 — 深度金字塔 + 遮挡剔除 + 合成 + 输出）:
  ├── Depth Pyramid Generation（10 层深度金字塔构建）
  ├── Pass2: Depth Occlusion Culling（深度遮挡剔除）
  ├── Render Pass1（view1 合成渲染：天空盒/线框/文字）
  ├── Barrier（颜色布局转换：COLOR_ATTACHMENT → GENERAL）
  └── Copy to Window（离屏 FBO → Swapchain）

Shadow（由 CustomViewDependentState 管理，在 CommandGraph 之前执行）:
  ├── Compute: Shadow Frustum Culling（阴影视锥体剔除）
  └── Per-Shadow-Map Render Passes（最多 8 × 2048×2048）
```

---

## 2. 执行流程时序

```
每帧开始
  │
  ▼
[Shadow Pass] — CustomViewDependentState::preRenderCommandGraph
  │  Compute: 阴影视锥体剔除
  │  Render: 生成 CSM 阴影贴图（D32_SFLOAT, 2048×2048, ≤8 层）
  │
  ▼
[CommandGraph]
  │
  ├── [Clear Pass] — BuildClearCommandGraph
  │     清除深度（0.0f，反转深度）+ 4 个 GBuffer 颜色附件（黑色）
  │
  ├── [Pass1] — Frustum Culling Compute
  │     视锥体 6 平面裁剪 → 更新 instanceCount + 输出可见实例矩阵
  │
  └── [Render Pass] — OffscreenRenderTarget FBO
        │
        ├── Subpass 0: Main Render
        │     虚拟 CAD 模型渲染到 GBuffer（MRT: 3 × R32G32B32A32_SFLOAT）
        │     输出: gbuffer0（颜色）, gbuffer1（法线）, gbuffer2（世界坐标）, shadowWrite（阴影）
        │     深度附件: depthImage（反转深度，GREATER 比较）
        │
        ├── Subpass 1: SSAO
        │     读取 gbuffer0（input attachment）, gbuffer1/2（sampled）
        │     输出: ssaoResultImage
        │
        └── Subpass 2: SSAO Denoise + Shadow Merge
              读取 gbuffer0（input）, shadowWrite（input）, ssaoResult（sampled）
              输出: 最终颜色（resolve）, shadowSample
  │
  ▼
[fix_depth_interop] — CUDA-Vulkan 深度互操作
  │  修复深度互操作图像
  │
  ▼
[copyInteropToDepthImage] — 拷贝互操作深度到 depthImage
  │
  ▼
[CommandGraph1]
  │
  ├── [Depth Pyramid Generation] — 10 个 Compute Pass
  │     Level 0: 原始深度图拷贝（depthImage → pyramid Level 0）
  │     Level 1-9: 2×2 → 1 降采样（取 min）
  │
  ├── [Pass2] — Depth Occlusion Culling Compute
  │     视锥体剔除 + 深度金字塔采样 + 软件光栅化 + 深度比较
  │     → 更新 instanceCount + 输出可见实例矩阵
  │
  ├── [Render Pass1] — view1 合成渲染
  │     合并虚拟 CAD + 天空盒 + 线框 + 文字到 swapchain
  │
  ├── [Barrier] — 颜色布局转换
  │     offscreen color: COLOR_ATTACHMENT_OPTIMAL → GENERAL
  │
  └── [Copy to Window] — 离屏 → Swapchain
        将离屏渲染结果拷贝到 swapchain image
```

---

## 3. Pass 依赖关系

| Pass | 前置依赖 | 后续 Pass |
|------|----------|-----------|
| Shadow Pass | 无（独立） | Main Render（Subpass 0 采样阴影贴图） |
| Clear Pass | 无（帧起始） | Main Render（需要干净的 GBuffer/深度） |
| Pass1 Frustum Cull | Clear Pass（间接依赖，确保缓冲区状态） | Main Render（提供可见实例列表） |
| Main Render (Subpass 0) | Clear + Pass1 + Shadow | SSAO（读取 GBuffer） |
| SSAO (Subpass 1) | Main Render | SSAO Denoise（读取 SSAO 结果） |
| SSAO Denoise (Subpass 2) | SSAO + Main Render（shadowWrite） | Synthesis Render |
| Depth Pyramid | Main Render（需要 depthImage） | Pass2（采样深度金字塔） |
| Pass2 Depth Cull | Depth Pyramid + Pass1（输入实例列表） | Synthesis Render（提供最终可见实例） |
| Synthesis Render | Pass2 + SSAO Denoise | Barrier |
| Barrier | Synthesis Render | Copy to Window |
| Copy to Window | Barrier | 帧结束（Present） |
| CUDA Depth Interop | 外部 CUDA 管线 | Main Render（提供深度数据） |
| IBL Preprocessing | 初始化阶段（仅一次） | Main Render（PBR 材质采样） |

---

## 4. 关键设计决策

### 4.1 反转深度缓冲
- 清除深度值为 `0.0f`（最近），深度比较使用 `VK_COMPARE_OP_GREATER`
- 优势：远平面精度更高，适合大范围 CAD 场景

### 4.2 三 Subpass 单 Render Pass
- Subpass 0/1/2 在同一个 RenderPass 内，通过 Input Attachment 传递数据
- 避免 GBuffer 数据写出到内存再读入，节省带宽
- Subpass 依赖确保正确的执行顺序

### 4.3 GPU-Driven 渲染
- 使用 `DrawIndexedIndirect` 实现 GPU 驱动的实例化渲染
- Compute Shader 通过 `atomicAdd` 紧凑写入可见实例列表
- 双 Pass 剔除：先视锥体粗剔除，再深度精确剔除

### 4.4 CUDA-Vulkan 互操作
- 深度图像通过外部内存导出到 CUDA
- `fix_depth_interop` 直接在互操作内存上操作，无需 D2H 传输
- 使用外部信号量同步 Vulkan 和 CUDA

---

## 5. View Mask 系统

| View | Mask | 包含内容 |
|------|------|----------|
| `view` (0) | MASK_CAMERA_IMAGE \| MASK_PBR_FULL \| MASK_SHADOW_RECEIVER | 相机图像 + 虚拟 CAD + 阴影接收 |
| `view1` (1) | MASK_PBR_FULL \| MASK_WIREFRAME \| MASK_TEXT \| MASK_SHADOW_RECEIVER \| MASK_SSAO | 虚拟 CAD + 线框 + 文字 + 阴影 + SSAO |

---

## 6. 相关文件清单

| 文件 | 功能 |
|------|------|
| `asset/src/vsgRendererServer.cpp` | 渲染器主循环，编排 CommandGraph |
| `asset/include/Utils.h` | BuildClearCommandGraph，辅助工具 |
| `asset/src/CustomViewDependentState.cpp` | 阴影贴图生成，CSM 管理 |
| `asset/src/CustomViewDependentState1.cpp` | 第二视图状态 |
| `asset/src/OffscreenRenderTarget.cpp` | GBuffer 创建，3-Subpass RenderPass |
| `asset/src/OcclusionCullingPasses.cpp` | 深度金字塔 + 遮挡剔除 Pass |
| `asset/src/SSAOPass.cpp` | SSAO 和 SSAO Denoise Pass |
| `asset/src/IBL.cpp` | IBL 资源创建，PBR ShaderSet，天空盒 |
| `asset/src/encoder.cpp` | CUDA 上下文，互操作处理 |
| `asset/include/fixDepth.h` | CUDA-Vulkan 深度互操作声明 |

---

## 7. 各 Pass 详细文档索引

| 编号 | Pass | 文档 |
|------|------|------|
| 01 | Clear Pass | [01_clear_pass.md](01_clear_pass.md) |
| 02 | Shadow Pass (CSM) | [02_shadow_pass.md](02_shadow_pass.md) |
| 03 | Pass1 Frustum Culling | [03_pass1_frustum_culling.md](03_pass1_frustum_culling.md) |
| 04 | Main Render Pass | [04_main_render_pass.md](04_main_render_pass.md) |
| 05 | Depth Pyramid | [05_depth_pyramid.md](05_depth_pyramid.md) |
| 06 | Pass2 Depth Culling | [06_pass2_depth_culling.md](06_pass2_depth_culling.md) |
| 07 | SSAO Pass | [07_ssao_pass.md](07_ssao_pass.md) |
| 08 | SSAO Denoise Pass | [08_ssao_denoise_pass.md](08_ssao_denoise_pass.md) |
| 09 | Synthesis Render Pass | [09_synthesis_render_pass.md](09_synthesis_render_pass.md) |
| 10 | IBL Preprocessing | [10_ibl_preprocessing.md](10_ibl_preprocessing.md) |
| 11 | CUDA-Vulkan Depth Interop | [11_cuda_vulkan_depth_interop.md](11_cuda_vulkan_depth_interop.md) |
| 12 | Barrier & Copy to Window | [12_barrier_and_copy_pass.md](12_barrier_and_copy_pass.md) |
