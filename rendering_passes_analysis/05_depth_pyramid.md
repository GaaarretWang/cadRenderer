# Depth Pyramid Generation（深度金字塔构建）

> 分析分支：`encoder`

---

## 1. 概述

深度金字塔（Hierarchical Z-Buffer / HiZ）是一个 10 层的 Mipmap 结构，每层分辨率减半，通过逐级降采样构建。用于后续 Pass2 的遮挡剔除。

**详细分析参见**：[occlusion_culling_analysis.md](../occlusion_culling_analysis.md) 第 2 节

---

## 2. 位置

- **Image 创建**：`asset/src/OcclusionCullingPasses.cpp` — `initOcclusionCullingPassesImageInfo()`
- **构建流程**：`asset/src/OcclusionCullingPasses.cpp` — `buildDepthPyramid()`
- **着色器**：
  - `asset/data/shaders/computevertex_depthimage.comp`（Level 0：深度拷贝）
  - `asset/data/shaders/computevertex_depthpyramid.comp`（Level 1-9：降采样）

---

## 3. 深度金字塔 Image

```cpp
depthPyramidImage = vsg::Image::create();
depthPyramidImage->imageType = VK_IMAGE_TYPE_2D;
depthPyramidImage->format = VK_FORMAT_R32_SFLOAT;    // 颜色格式，非深度格式
depthPyramidImage->mipLevels = 10;                    // 10 层 Mipmap
depthPyramidImage->usage = VK_IMAGE_USAGE_STORAGE_BIT |   // Compute 读写
                           VK_IMAGE_USAGE_SAMPLED_BIT;    // 片元着色器采样
```

**关键设计**：使用 `R32_SFLOAT` 而非深度格式，因为通过 Compute Shader 以 Storage Image 方式读写。

---

## 4. 构建流程

### Level 0：原始深度拷贝

```glsl
// computevertex_depthimage.comp
imageStore(depth_pyramid, coord, vec4(imageLoad(src_depth, coord).x, 0, 0, 0));
```

将 `depthImage` 逐像素拷贝到金字塔第 0 层。之前插入 Barrier 将深度图从 `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` 转为 `SHADER_READ_ONLY_OPTIMAL`。

### Level 1-9：2×2 → 1 降采样

```glsl
// computevertex_depthpyramid.comp
float d00 = imageLoad(prev_level, src_coord).x;
float d01 = imageLoad(prev_level, src_coord + ivec2(0, 1)).x;
float d10 = imageLoad(prev_level, src_coord + ivec2(1, 0)).x;
float d11 = imageLoad(prev_level, src_coord + ivec2(1, 1)).x;
float min_depth = min(min(d00, d01), min(d10, d11));
imageStore(depth_pyramid, coord, vec4(min_depth, 0, 0, 0));
```

每层取 2×2 区域的**最小值**（最远深度），写入当前层级。

### 层间同步

每层生成后插入 `ImageMemoryBarrier`：
```cpp
barrier->srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
barrier->subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1};  // 仅当前层
```

---

## 5. 金字塔结构

```
Level 0: 1920 × 1080  (原始深度)
Level 1:  960 ×  540   (min 2×2)
Level 2:  480 ×  270
Level 3:  240 ×  135
Level 4:  120 ×   68
Level 5:   60 ×   34
Level 6:   30 ×   17
Level 7:   15 ×    9
Level 8:    8 ×    5
Level 9:    4 ×    3
```

---

## 6. 依赖关系

- **前置依赖**：Main Render Pass（需要 depthImage）
- **后续依赖**：Pass2（采样深度金字塔进行遮挡剔除）
- **同步**：10 个 Compute Pass 串行执行，层间通过 PipelineBarrier 同步
