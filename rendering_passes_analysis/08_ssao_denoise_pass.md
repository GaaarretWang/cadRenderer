# SSAO Denoise Pass（SSAO 降噪 + 阴影合并 — Subpass 2）

> 分析分支：`encoder`

---

## 1. 概述

SSAO Denoise Pass 在 Main Render Pass 的 Subpass 2 中执行，对 SSAO 结果进行降噪处理，并与阴影因子合并，输出最终的颜色和阴影采样结果。

---

## 2. 位置

- **实现**：`asset/src/SSAOPass.cpp` — `buildSSAODenoiseData()`
- **ShaderSet**：`SSAOPass::customSSAODenoiseShaderSet()`
- **着色器**：
  - `asset/data/shaders/IBL/ssao_denoise.vert`
  - `asset/data/shaders/IBL/ssao_denoise.frag`

---

## 3. 输入

| 输入 | 类型 | Binding | 说明 |
|------|------|---------|------|
| gbuffer0 | Input Attachment | set=2, binding=0 | 颜色（Subpass 内直接读取） |
| shadowWrite | Input Attachment | set=2, binding=1 | 阴影因子（Subpass 0 输出） |
| samplerSSAO | Combined Image Sampler | set=2, binding=3 | SSAO 结果（Subpass 1 输出） |

---

## 4. 输出

| 输出 | 格式 | 说明 |
|------|------|------|
| 最终颜色 | Resolve Attachment | 降噪后的颜色输出 |
| shadowSample | — | 阴影采样结果 |

---

## 5. Alpha Blending

启用 Alpha Blending 用于降噪混合：

```cpp
colorBlendState->attachments[0] = {
    VK_FALSE,                    // blending disabled for this pass
    VK_BLEND_FACTOR_SRC_ALPHA,
    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    VK_BLEND_OP_ADD,
    VK_BLEND_FACTOR_ONE,
    VK_BLEND_FACTOR_ZERO,
    VK_BLEND_OP_ADD,
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
};
```

附件 0 和附件 1 使用相同的混合状态。

---

## 6. 渲染方式

全屏四边形绘制（DrawIndexedIndirect），与 SSAO Pass 相同的顶点/索引数据。

---

## 7. 依赖关系

- **前置依赖**：
  - SSAO Subpass 1（需要 ssaoResult）
  - Main Render Subpass 0（需要 gbuffer0, shadowWrite）
- **后续依赖**：Synthesis Render Pass（使用最终颜色）
- **同步**：通过 Subpass 依赖自动同步
