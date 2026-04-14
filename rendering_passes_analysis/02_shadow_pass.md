# Shadow Pass（阴影 Pass — CSM 级联阴影贴图）

> 分析分支：`encoder`

---

## 1. 概述

Shadow Pass 实现了 **Cascaded Shadow Maps (CSM)**，支持最多 8 个级联阴影贴图，每个 2048×2048 像素。该 Pass 由 `CustomViewDependentState` 管理，在主 CommandGraph 之前通过 `preRenderCommandGraph` 执行。

---

## 2. 位置

- **头文件**：`asset/include/CustomViewDependentState.h`
- **实现**：`asset/src/CustomViewDependentState.cpp`
- **着色器**：`asset/data/shaders/computevertex_shadow.comp`（阴影视锥体剔除）

---

## 3. 初始化（`init()`）

### 3.1 阴影深度图像创建

```cpp
shadowDepthImage = vsg::Image::create();
shadowDepthImage->imageType = VK_IMAGE_TYPE_2D;
shadowDepthImage->format = VK_FORMAT_D32_SFLOAT;        // 32位浮点深度
shadowDepthImage->extent = {2048, 2048, 1};              // 2048×2048
shadowDepthImage->mipLevels = 1;
shadowDepthImage->arrayLayers = 8;                       // 最多 8 层（2D Array）
shadowDepthImage->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT;    // 可被主渲染采样
```

### 3.2 Compute Pipeline（阴影视锥体剔除）

使用 `computevertex_shadow.comp` 对每个阴影贴图做视锥体剔除：

```glsl
// 对每个实例检测是否在当前阴影贴图的视锥体内
// 通过的实例写入 shadow indirect buffer
layout(local_size_x = 32) in;
```

- 每个阴影贴图有独立的视锥体平面
- 剔除结果写入对应的间接绘制命令缓冲

---

## 4. 遍历逻辑（`traverse()`）

### 4.1 光源空间矩阵计算

使用 **Cpractical 分割方案**（Practical Split Scheme）计算每个级联的投影矩阵：

```cpp
// 计算相机视锥体的分割点（对数/线性混合）
for (int cascade = 0; cascade < numShadowMaps; cascade++) {
    // 对数分割 + 线性分割的混合
    float splitLog = near * pow(far / near, (cascade + 1.0f) / numShadowMaps);
    float splitLin = near + (far - near) * (cascade + 1.0f) / numShadowMaps;
    float splitDist = lambda * splitLog + (1.0f - lambda) * splitLin;

    // 计算该级联的视锥体 8 个角点
    // 将角点变换到光源空间
    // 计算 AABB 确定正交投影参数
    // 构建光源空间的 View-Projection 矩阵
}
```

### 4.2 场景包围盒计算

分别计算两类几何体的包围盒：
- **虚拟几何体**（`MASK_PBR_FULL`）：需要投射和接收阴影
- **真实几何体**（`MASK_SHADOW_RECEIVER`）：仅接收阴影（相机图像）

### 4.3 每级联渲染

每个阴影贴图创建独立的：
- `vsg::View` — 包含光源空间的相机矩阵
- `vsg::RenderGraph` — 渲染到 shadowDepthImage 的对应层
- 绑定剔除后的间接绘制命令

---

## 5. 输出

| 输出 | 格式 | 用途 |
|------|------|------|
| `shadowDepthImage` | D32_SFLOAT, 2048×2048, ≤8 层 | 阴影深度贴图（2D Array） |
| `shadowDescriptorSet` | — | 包含阴影贴图采样器的 Descriptor Set |

阴影贴图在 Main Render Pass（Subpass 0）中通过 `ViewDependentState` 的 Descriptor Set 被采样，用于 PCF/PCSS 阴影计算。

---

## 6. 依赖关系

- **前置依赖**：无（独立于主渲染管线）
- **后续依赖**：
  - Main Render Pass（Subpass 0）采样阴影贴图
  - SSAO Denoise Pass（Subpass 2）读取 shadowWriteImage
- **同步**：通过 `preRenderCommandGraph` 在主 CommandGraph 之前执行，确保阴影贴图在主渲染时可用
