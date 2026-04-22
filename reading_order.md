# 代码阅读顺序指南

> 基于渲染管线执行流程，由表及里、由 C++ 到 Shader 组织阅读路线。
> 建议按 Phase 顺序阅读，每 Phase 内先读 C++ 再读对应的 Shader。

---

## Phase 0: 项目入口与整体架构

> 先理解程序启动流程和双 CommandGraph 架构全貌。

| 顺序 | 文件 | 阅读重点 |
|------|------|----------|
| 0.1 | `src/main.cpp` | 程序入口：加载相机位姿 → 初始化 RenderingServer → 主循环 `Update()` |
| 0.2 | `include/RenderingServer.h` + `src/RenderingServer.cpp` | 客户端接口层：封装 `vsgRendererServer`，对外暴露 `Init/Update` 接口 |
| 0.3 | `asset/include/vsgRendererServer.h` | **核心类定义**：理解所有成员变量的用途（device, viewer, view, offscreenTarget, vsgContext 等） |
| 0.4 | `rendering_passes_analysis/00_summary.md` | 双 CommandGraph 架构总览、执行时序、Pass 依赖关系 |

**读完后应理解**：
- 程序如何启动，主循环做什么
- 双 CommandGraph 架构：CommandGraph（阴影+剔除+主渲染）→ CommandGraph1（深度金字塔+剔除+合成）
- 各核心模块的职责划分

---

## Phase 1: 初始化流程 — initRenderer

> 阅读 `vsgRendererServer::initRenderer()`，这是最重的初始化函数。

| 顺序 | 文件 | 阅读重点 |
|------|------|----------|
| 1.1 | `asset/src/vsgRendererServer.cpp` — `initRenderer()` | 通读整个函数，理解初始化步骤：Vulkan Instance → Device → IBL 资源 → 模型加载 → 场景图构建 → CommandGraph 构建 → compile |
| 1.2 | `asset/include/MyMask.h` | View Mask 定义：`MASK_PBR_FULL`, `MASK_SHADOW_CASTER`, `MASK_CAMERA_IMAGE` 等位掩码常量 |
| 1.3 | `asset/include/Utils.h` | `BuildClearCommandGraph`：每帧清除 GBuffer + 深度的命令构建（reversed depth clear 值 0.0f） |

**读完后应理解**：
- initRenderer 的完整步骤和调用顺序
- View Mask 系统如何控制不同渲染通道的可见性
- Clear Pass 的构建方式

---

## Phase 2: 离屏渲染目标 — OffscreenRenderTarget

> 理解 GBuffer 和 3-Subpass RenderPass 的设计。

| 顺序 | 文件 | 阅读重点 |
|------|------|----------|
| 2.1 | `asset/include/OffscreenRenderTarget.h` | 类定义：所有 GBuffer 附件（gbuffer0/1/2, ssaoResult, shadowWrite, shadowSample, depth），附件格式和用途 |
| 2.2 | `asset/src/OffscreenRenderTarget.cpp` | 实现：`init()` 创建 Image → `buildRenderPass()` 定义 3-Subpass → `buildFramebuffer()` |

**读完后应理解**：
- 3 个 GBuffer 分别存储什么数据（颜色/法线/世界坐标）
- Subpass 0/1/2 各自的输入输出附件
- Subpass 之间的依赖关系和 Input Attachment 机制
- MSAA 下的 depth resolve 处理

---

## Phase 3: 阴影系统 — CSM + PCSS

> 阴影在 CommandGraph 之前执行，由 CustomViewDependentState 管理。

| 顺序 | 文件 | 阅读重点 |
|------|------|----------|
| 3.1 | `asset/include/CustomViewDependentState.h` | CSM 管理类：shadow map 资源、CSM 分割、compute shadow frustum culling |
| 3.2 | `asset/src/CustomViewDependentState.cpp` | `init()` 创建 shadow map 资源 → `traverse()` 每帧更新光源+CSM 分割 → shadow frustum culling compute pass |
| 3.3 | `asset/include/CustomViewDependentState1.h` | 第二个 View 的 ViewDependentState（descriptor set 共享策略） |
| 3.4 | `asset/src/CustomViewDependentState1.cpp` | 实现细节 |
| 3.5 | `asset/data/shaders/shadow.vert` | **Shader**: 阴影贴图的顶点变换 |
| 3.6 | `asset/data/shaders/shadow.frag` | **Shader**: 阴影贴图的片段输出 |
| 3.7 | `asset/data/shaders/computevertex_shadow.comp` | **Shader**: 阴影视锥体剔除 compute shader |

**读完后应理解**：
- CSM 的 Cpractical 分割方案
- Shadow map 的生成流程（depth-only render pass）
- 阴影贴图使用 reversed depth + VK_COMPARE_OP_GREATER 的原因
- GPU 端 shadow frustum culling 的工作方式

---

## Phase 4: IBL 预处理管线

> IBL 资源在初始化阶段预计算，运行时只做采样。

| 顺序 | 文件 | 阅读重点 |
|------|------|----------|
| 4.1 | `asset/include/IBL.h` | IBL 模块定义：`Textures` 结构体（envmap/irradiance/prefilter/brdfLut）、`VsgContext`、常量定义（格式/尺寸/mip 级数） |
| 4.2 | `asset/src/IBL.cpp` — `createResources()` | 创建所有 IBL GPU 资源 |
| 4.3 | `asset/src/IBL.cpp` — `generateBRDFLUT()` | 生成 BRDF LUT |
| 4.4 | `asset/src/IBL.cpp` — `generateEnvmap()` | Equirectangular → Cubemap 转换 |
| 4.5 | `asset/src/IBL.cpp` — `generateIrradianceCube()` | 辐照度卷积 |
| 4.6 | `asset/src/IBL.cpp` — `generatePrefilteredEnvmapCube()` | 预滤波环境贴图（GGX 重要性采样） |
| 4.7 | `asset/src/IBL.cpp` — `drawSkyboxVSGNode()` | 天空盒场景图节点构建 |
| 4.8 | `asset/src/IBL.cpp` — `customPbrShaderSet()` | PBR ShaderSet 创建（绑定 IBL 纹理） |
| 4.9 | `asset/data/shaders/IBL/genbrdflut.vert` + `genbrdflut.frag` | **Shader**: BRDF LUT 生成 |
| 4.10 | `asset/data/shaders/IBL/equirect2cube.frag` | **Shader**: HDR 等距柱状投影 → Cubemap |
| 4.11 | `asset/data/shaders/IBL/irradiancecubeMesh.frag` | **Shader**: 辐照度卷积（半球积分） |
| 4.12 | `asset/data/shaders/IBL/prefilterenvmapMesh.frag` | **Shader**: 预滤波环境贴图（GGX 重要性采样） |
| 4.13 | `asset/data/shaders/IBL/skybox.vert` + `skybox.frag` | **Shader**: 天空盒渲染 |
| 4.14 | `asset/data/shaders/IBL/skybox_background.frag` | **Shader**: IBL 背景渲染（相机图像叠加） |
| 4.15 | `asset/data/shaders/IBL/skyboxCubegen.vert` | **Shader**: Cubemap 面生成的顶点着色器 |
| 4.16 | `asset/data/shaders/IBL/fullscreenquad.vert` | **Shader**: 全屏四边形顶点着色器（后处理用） |

**读完后应理解**：
- Split-Sum 近似的三个组件：BRDF LUT + Irradiance + Prefiltered Envmap
- 各 IBL 纹理的格式选择和 mip 级数设计
- 为什么 Irradiance 分辨率低（64），Prefilter 分辨率高（512）
- 天空盒节点如何集成到场景图中

---

## Phase 5: 主渲染 — CommandGraph 第一遍

> 理解 CommandGraph 的构建和第一遍渲染流程。

| 顺序 | 文件 | 阅读重点 |
|------|------|----------|
| 5.1 | `asset/src/vsgRendererServer.cpp` — `initRenderer()` 中构建 CommandGraph 的部分 | 理解 CommandGraph 如何串联 Clear → Pass1 → RenderPass |
| 5.2 | `asset/data/shaders/IBL/standard.vert` | **Shader**: PBR 主顶点着色器（实例化渲染、双矩阵 current/last） |
| 5.3 | `asset/data/shaders/IBL/custom_pbr.frag` | **Shader**: PBR 主片段着色器（多 BRDF 模型、IBL 采样、阴影计算、SSAO 融合） |

**读完后应理解**：
- PBR 片段着色器中完整的光照计算流程
- 实例化渲染的双矩阵（current/last model matrix）用途
- Subpass 0 如何输出到 MRT GBuffer

---

## Phase 6: SSAO — 子通道 1 和 2

| 顺序 | 文件 | 阅读重点 |
|------|------|----------|
| 6.1 | `asset/include/SSAOPass.h` | SSAO 模块接口 |
| 6.2 | `asset/src/SSAOPass.cpp` | SSAO + SSAO Denoise 的 CommandGraph 构建 |
| 6.3 | `asset/data/shaders/IBL/ssao.vert` + `ssao.frag` | **Shader**: SSAO 计算（128 采样核、TBN 矩阵、深度比较） |
| 6.4 | `asset/data/shaders/IBL/ssao_denoise.vert` + `ssao_denoise.frag` | **Shader**: SSAO 降噪（高斯模糊） |

**读完后应理解**：
- SSAO 如何通过 Input Attachment 读取 GBuffer 数据
- 128 采样核的半球分布策略
- Denoise 为什么用 subpass 而不是独立 pass（tile-based GPU 带宽优化）

---

## Phase 7: GPU-Driven 遮挡剔除

> CommandGraph1 的核心：深度金字塔 + Pass2 遮挡剔除。

| 顺序 | 文件 | 阅读重点 |
|------|------|----------|
| 7.1 | `asset/include/OcclusionCullingPasses.h` | 模块定义：`CameraPlaneInfo`、`ComputePushConstants`、深度金字塔资源声明 |
| 7.2 | `asset/src/OcclusionCullingPasses.cpp` — `initOcclusionCullingPassesImageInfo()` | 深度金字塔 Image/Sampler/ImageView 创建 |
| 7.3 | `asset/src/OcclusionCullingPasses.cpp` — `generateCameraData()` | 视锥体平面方程计算 |
| 7.4 | `asset/src/OcclusionCullingPasses.cpp` — `buildFirstComputePass()` | Pass1: 视锥体剔除 compute pipeline 构建 |
| 7.5 | `asset/src/OcclusionCullingPasses.cpp` — `buildDepthPyramid()` | 深度金字塔 10 级 min-downsample 构建 |
| 7.6 | `asset/src/OcclusionCullingPasses.cpp` — `buildSecondComputePass()` | Pass2: 深度遮挡剔除 compute pipeline 构建 |
| 7.7 | `asset/data/shaders/computevertex_depthimage.comp` | **Shader**: 深度金字塔 Level 0 原始深度拷贝 |
| 7.8 | `asset/data/shaders/computevertex_depthpyramid.comp` | **Shader**: 深度金字塔 Level 1-9 的 min-downsample |
| 7.9 | `asset/data/shaders/computevertex.comp` | **Shader**: Pass2 遮挡剔除（软件光栅化 + 深度比较 + atomic 可见实例收集） |
| 7.10 | `asset/data/shaders/computevertex1.comp` | **Shader**: Pass2 变体 — 大实例 batch 版本 |
| 7.11 | `asset/data/shaders/computevertex1_seat.comp` | **Shader**: Pass2 变体 — 小实例 batch 版本 |

**读完后应理解**：
- 双 Pass 剔除：Pass1 视锥粗剔除 → Pass2 深度精确剔除
- 深度金字塔为什么用 min（而非 max）做 downsample
- Pass2 中软件光栅化的原理：compute shader 内做 AABB 投影 + 深度比较
- `DrawIndexedIndirect` 如何由 compute shader 控制
- compute → graphics 的 barrier 同步策略

---

## Phase 8: 合成渲染与输出 — CommandGraph1 第二遍

| 顺序 | 文件 | 阅读重点 |
|------|------|----------|
| 8.1 | `asset/src/vsgRendererServer.cpp` — `render()` | 完整的每帧渲染流程：更新相机 → CUDA 互操作 → CommandGraph 执行 → CommandGraph1 执行 → 像素读回 |
| 8.2 | `asset/data/shaders/line.vert` + `line.frag` | **Shader**: 线框叠加渲染 |
| 8.3 | `asset/data/shaders/point.vert` + `point.frag` | **Shader**: 点云渲染 |

**读完后应理解**：
- render() 的完整执行顺序
- 离屏 FBO 到 Swapchain 的拷贝（Barrier + vkCmdCopyImage）
- 线框/点云的动态更新机制

---

## Phase 9: CUDA-Vulkan 深度互操作

> 虚实融合的核心：CUDA 处理真实深度图，通过共享显存传给 Vulkan。

| 顺序 | 文件 | 阅读重点 |
|------|------|----------|
| 9.1 | `asset/include/fixDepth.h` | CUDA 深度修正函数声明 |
| 9.2 | `asset/include/encoder.h` | CUDA 编码器接口、`Cudaimage` 结构体定义 |
| 9.3 | `asset/src/encoder.cpp` | CUDA 上下文初始化、Vulkan 外部内存导入、`Cudaimage::get()` 获取设备指针 |
| 9.4 | `asset/src/vsgRendererServer.cpp` — `copyInteropToDepthImage()` | GPU 端深度图拷贝（interop image → depth image） |

**读完后应理解**：
- CUDA-Vulkan 互操作的完整数据通路
- `VkExternalMemoryImageCreateInfo` 和 `VkExportMemoryAllocateInfo` 的作用
- 为什么必须通过 UUID 匹配 CUDA 和 Vulkan 设备
- 外部信号量（external semaphore）的 wait/signal 同步机制

---

## Phase 10: 辅助模块

| 顺序 | 文件 | 阅读重点 |
|------|------|----------|
| 10.1 | `asset/include/CADMesh.h` + `asset/src/CADMesh.cpp` | CAD 模型数据管理：`ProtoData`（原型数据）、实例 buffer、全局矩阵 buffer |
| 10.2 | `asset/include/ConfigShader.h` + `asset/src/ConfigShader.cpp` | ShaderSet 工厂函数（shadow/line/point shader 的构建） |
| 10.3 | `asset/include/ImGui.h` + `asset/src/ImGui.cpp` | GUI 参数面板（运行时调参） |
| 10.4 | `asset/include/ImageUtils.h` + `asset/src/ImageUtils.cpp` | 图像加载/保存/对比工具 |
| 10.5 | `asset/include/screenshot.h` | 截图/编码功能 |
| 10.6 | `asset/include/OBJLoader.h` + `asset/src/OBJLoader.cpp` | OBJ 模型加载器 |
| 10.7 | `asset/include/PlaneLoader.h` + `asset/src/PlaneLoader.cpp` | 平面几何加载器 |

---

## 渲染管线数据流速查

```
CPU 端: main.cpp
  │
  ├── 渲染服务器初始化 (vsgRendererServer::initRenderer)
  │     ├── Vulkan Instance/Device 创建
  │     ├── IBL 资源预计算 (Phase 4)
  │     ├── CAD 模型加载 → 场景图构建
  │     ├── OffscreenRenderTarget 创建 (Phase 2)
  │     ├── CommandGraph 构建
  │     └── viewer->compile()
  │
  └── 主循环 (vsgRendererServer::render)
        │
        ├── Shadow Pass (Phase 3)
        │     └── CSM shadow map 生成
        │
        ├── CommandGraph (Phase 5-6)
        │     ├── Clear Pass (GBuffer + Depth)
        │     ├── Pass1: Frustum Culling (compute)
        │     └── Render Pass (3 Subpasses)
        │           ├── Subpass 0: PBR → GBuffer MRT
        │           ├── Subpass 1: SSAO
        │           └── Subpass 2: Denoise + Shadow Merge
        │
        ├── CUDA Depth Interop (Phase 9)
        │     └── fix_depth_interop → copyInteropToDepthImage
        │
        └── CommandGraph1 (Phase 7-8)
              ├── Depth Pyramid (10 levels)
              ├── Pass2: Occlusion Culling (compute)
              ├── Synthesis Render (skybox + wireframe + text)
              └── Copy to Window (offscreen → swapchain)
```

---

## Shader 文件索引

按功能分组，方便查阅：

### Compute Shaders (GPU-Driven 渲染)
| 文件 | 功能 |
|------|------|
| `computevertex.comp` | Pass2 遮挡剔除（主版本） |
| `computevertex1.comp` | Pass2 遮挡剔除（大实例 batch） |
| `computevertex1_seat.comp` | Pass2 遮挡剔除（小实例 batch） |
| `computevertex_depthimage.comp` | 深度金字塔 Level 0 拷贝 |
| `computevertex_depthpyramid.comp` | 深度金字塔 Level 1-9 min-downsample |
| `computevertex_shadow.comp` | 阴影视锥体剔除 |

### IBL Shaders (基于图像的光照)
| 文件 | 功能 |
|------|------|
| `IBL/custom_pbr.frag` | **核心**: PBR 主片段着色器 |
| `IBL/standard.vert` | PBR 主顶点着色器（实例化） |
| `IBL/genbrdflut.frag` + `genbrdflut.vert` | BRDF LUT 生成 |
| `IBL/equirect2cube.frag` | HDR → Cubemap 转换 |
| `IBL/irradiancecubeMesh.frag` | 辐照度卷积 |
| `IBL/prefilterenvmapMesh.frag` | 预滤波环境贴图 |
| `IBL/skybox.vert` + `skybox.frag` | 天空盒渲染 |
| `IBL/skybox_background.frag` | 相机图像背景叠加 |
| `IBL/skyboxCubegen.vert` | Cubemap 面生成 |
| `IBL/fullscreenquad.vert` | 全屏四边形 |

### SSAO Shaders (屏幕空间环境光遮蔽)
| 文件 | 功能 |
|------|------|
| `IBL/ssao.vert` + `IBL/ssao.frag` | SSAO 计算 |
| `IBL/ssao_denoise.vert` + `IBL/ssao_denoise.frag` | SSAO 降噪 |

### Shadow Shaders (阴影)
| 文件 | 功能 |
|------|------|
| `shadow.vert` + `shadow.frag` | Shadow map 生成 |

### Overlay Shaders (叠加层)
| 文件 | 功能 |
|------|------|
| `line.vert` + `line.frag` | 线框渲染 |
| `point.vert` + `point.frag` | 点云渲染 |

---

## 建议的阅读策略

1. **第一遍（全局了解）**: 只读 Phase 0 + 1，建立整体架构概念
2. **第二遍（管线深入）**: 读 Phase 2-3 + 5-8，跟着渲染管线走一遍
3. **第三遍（Shader 精读）**: 读 Phase 4 的 IBL shader + Phase 6 的 SSAO shader + Phase 7 的 compute shader
4. **第四遍（工程细节）**: 读 Phase 9 的 CUDA 互操作 + Phase 10 的辅助模块

遇到不理解的 VSG API，可以查阅：
- VSG 官方文档: https://vsg-dev.github.io/VulkanSceneGraph/
- 本项目的 `rendering_passes_analysis/` 目录下有各 Pass 的详细分析文档
