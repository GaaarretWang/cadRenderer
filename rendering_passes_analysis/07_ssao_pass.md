# SSAO Pass（屏幕空间环境光遮蔽 — Subpass 1）

> 分析分支：`encoder`

---

## 1. 概述

SSAO Pass 在 Main Render Pass 的 Subpass 1 中执行，利用 GBuffer 数据计算屏幕空间环境光遮蔽因子。输出一张 SSAO 遮蔽图，供后续 Denoise Pass 使用。

---

## 2. 位置

- **实现**：`asset/src/SSAOPass.cpp` — `buildSSAOData()`
- **ShaderSet**：`SSAOPass::customSSAOShaderSet()`
- **着色器**：
  - `asset/data/shaders/IBL/ssao.vert`
  - `asset/data/shaders/IBL/ssao.frag`

---

## 3. 输入

| 输入 | 类型 | Binding | 说明 |
|------|------|---------|------|
| gbuffer0 | Input Attachment | set=2, binding=0 | 颜色（Subpass 内直接读取） |
| gbuffer1 | Combined Image Sampler | set=2, binding=1 | 法线（世界空间） |
| gbuffer2 | Combined Image Sampler | set=2, binding=2 | 世界坐标 |
| samplerNoise | Combined Image Sampler | set=2, binding=3 | 随机噪声纹理 |

### 噪声纹理生成

```cpp
// 在 buildSSAOData() 中生成
auto samplerNoiseData = vsg::ubvec4Array2D::create(extent.width, extent.height);
for (uint32_t i = 0; i < extent.width; ++i){
    for (uint32_t j = 0; j < extent.height; ++j){
        float randX = distribution(generator);
        float randY = distribution(generator);
        samplerNoiseData->set(i, j, vsg::ubvec4(
            randX * 255, randY * 255, 0, 0));
    }
}
```

使用固定种子（`seed = 100`）的梅森旋转算法生成随机方向向量，用于 SSAO 采样核的旋转。

---

## 4. 输出

| 输出 | 格式 | 说明 |
|------|------|------|
| ssaoResultImage | — | SSAO 遮蔽因子图 |

写入 `ssaoResultImage`，作为 Subpass 2 的输入。

---

## 5. 渲染方式

全屏四边形绘制（DrawIndexedIndirect）：

```cpp
auto vertices = vsg::vec3Array::create({
    vsg::vec3(-1, -1, 1.0f),
    vsg::vec3( 1, -1, 1.0f),
    vsg::vec3( 1,  1, 1.0f),
    vsg::vec3(-1,  1, 1.0f)
});
auto indices = vsg::ushortArray::create({0, 1, 2, 2, 3, 0});
```

- 6 个索引，1 个实例
- 无顶点缓冲绑定（位置硬编码在顶点着色器中）

---

## 6. 依赖关系

- **前置依赖**：Main Render Subpass 0（需要 GBuffer 数据）
- **后续依赖**：SSAO Denoise Subpass 2（读取 ssaoResult）
- **同步**：通过 Subpass 依赖自动同步（COLOR_ATTACHMENT_OUTPUT → FRAGMENT_SHADER_INPUT）
