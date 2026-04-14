# Main Render Pass（主渲染 Pass — GBuffer MRT）

> 分析分支：`encoder`

---

## 1. 概述

Main Render Pass 是渲染器的核心图形 Pass，使用 **Multiple Render Target (MRT)** 将虚拟 CAD 模型渲染到 GBuffer，同时在同一个 RenderPass 内完成 SSAO 和 Denoise。该 Pass 包含 3 个 Subpass。

---

## 2. 位置

- **FBO/RenderPass 创建**：`asset/src/OffscreenRenderTarget.cpp` — `buildRenderPass()`
- **PBR ShaderSet**：`asset/src/IBL.cpp` — `customPbrShaderSet()`
- **调度**：`asset/src/vsgRendererServer.cpp` — `commandGraph->addChild(renderGraph)`

---

## 3. RenderPass 结构（3 Subpasses）

### Subpass 0: Main Render（GBuffer 输出）

**颜色附件**（MRT，4 个输出）：

| Attachment | 格式 | 内容 |
|-----------|------|------|
| gbuffer0 | R32G32B32A32_SFLOAT | 颜色 + alpha |
| gbuffer1 | R32G32B32A32_SFLOAT | 法线（世界空间） |
| gbuffer2 | R32G32B32A32_SFLOAT | 世界坐标 |
| shadowWrite | — | 阴影因子 |

**深度附件**：
- `depthImage`（单采样）或 `multisampleDepthImage`（MSAA）
- 格式：D32_SFLOAT
- 比较操作：`VK_COMPARE_OP_GREATER`（反转深度）

**渲染内容**：
- 通过 `DrawIndexedIndirect` 绘制通过 Pass1 剔除的虚拟 CAD 实例
- PBR 材质系统：支持 diffuseMap, normalMap, mrMap, aoMap, emissiveMap, specularMap, displacementMap
- IBL 光照：brdfLut, irradiance, prefilteredEnvmap
- 阴影采样：通过 `ViewDependentState` 的 CSM 阴影贴图
- 实例化渲染：每个实例有独立的 model matrix

### Subpass 1: SSAO（屏幕空间环境光遮蔽）

**输入**：
- gbuffer0 — Input Attachment（subpass 内直接读取）
- gbuffer1 — Combined Image Sampler（法线）
- gbuffer2 — Combined Image Sampler（世界坐标）

**输出**：
- ssaoResultImage — SSAO 遮蔽因子

**实现**：全屏四边形绘制，详见 [07_ssao_pass.md](07_ssao_pass.md)

### Subpass 2: SSAO Denoise + Shadow Merge

**输入**：
- gbuffer0 — Input Attachment
- shadowWrite — Input Attachment（Subpass 0 输出的阴影）
- ssaoResult — Combined Image Sampler（Subpass 1 输出的 SSAO）

**输出**：
- 最终颜色（resolve attachment）
- shadowSample — 阴影采样结果

**实现**：全屏四边形绘制，带 alpha blending，详见 [08_ssao_denoise_pass.md](08_ssao_denoise_pass.md)

---

## 4. Subpass 依赖

```cpp
// Subpass 0 → Subpass 1
dependency.srcSubpass = 0; dstSubpass = 1;
dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

// Subpass 1 → Subpass 2
dependency.srcSubpass = 1; dstSubpass = 2;
// 同样的 stage/access 配置
```

确保每个 Subpass 的输出在下一个 Subpass 读取时已完成写入。

---

## 5. MSAA 支持

当启用 MSAA 时：
- 颜色附件使用多采样图像（`VK_SAMPLE_COUNT_4_BIT`）
- 深度附件使用 `multisampleDepthImage`
- 最终 Subpass 2 输出 resolve 到单采样图像

---

## 6. 依赖关系

- **前置依赖**：
  - Clear Pass（干净的 GBuffer/深度）
  - Pass1（可见实例列表）
  - Shadow Pass（阴影贴图）
  - IBL Preprocessing（PBR 材质资源）
- **后续依赖**：
  - Depth Pyramid（需要 depthImage）
  - SSAO Denoise 结果供 Synthesis Render 使用
