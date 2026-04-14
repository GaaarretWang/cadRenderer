# Clear Pass（清屏 Pass）

> 分析分支：`encoder`

---

## 1. 概述

Clear Pass 是每帧的第一个 Pass，负责清除离屏渲染目标的所有附件，为后续渲染提供干净的初始状态。

---

## 2. 位置

- **入口**：`asset/include/Utils.h` — `BuildClearCommandGraph()`
- **调用**：`asset/src/vsgRendererServer.cpp` — `commandGraph->addChild(clear_image_commandgraph)`

---

## 3. 清除目标

### 3.1 深度附件（反转深度）

```cpp
// 清除值：depth = 0.0f（最近），stencil = 0
VkClearDepthStencilValue clearDepthStencilValue{0.0f, 0};
```

**注意**：本渲染器使用**反转深度缓冲**（Reversed Depth Buffer）：
- 清除深度为 `0.0f`（最近平面）
- 深度比较操作为 `VK_COMPARE_OP_GREATER`（更远的深度值通过测试）
- 优势：远平面精度更高，适合大范围 CAD 场景

清除两个深度图像：
1. `multisampleDepthImage` — 多采样深度（MSAA 启用时）
2. `depthImage` — 单采样深度

### 3.2 颜色附件（GBuffer）

```cpp
VkClearColorValue clearClearColorValue{{0.0f, 0.0f, 0.0f, 0.0f}};
```

清除 4 个颜色附件：

| 附件 | 格式 | 用途 |
|------|------|------|
| `gbufferImage0` | R32G32B32A32_SFLOAT | GBuffer 0：颜色 + 其他 |
| `gbufferImage1` | R32G32B32A32_SFLOAT | GBuffer 1：法线 |
| `gbufferImage2` | R32G32B32A32_SFLOAT | GBuffer 2：世界坐标 |
| `shadowWriteImage` | — | 阴影写入 |

---

## 4. 实现方式

通过 VSG 的 `clearAttachments` 命令构建一个 CommandGraph，插入到 `commandGraph` 的最前面：

```cpp
auto clearAttachments = vsg::ClearAttachments::create();
// 添加深度清除
clearAttachments->attachments.push_back(VkClearAttachment{
    VK_IMAGE_ASPECT_DEPTH_BIT, 0, VkClearValue{clearDepthStencilValue}
});
// 添加颜色清除（gbuffer0, gbuffer1, gbuffer2, shadowWrite）
for (int i = 0; i < 4; i++) {
    clearAttachments->attachments.push_back(VkClearAttachment{
        VK_IMAGE_ASPECT_COLOR_BIT, i, VkClearValue{clearClearColorValue}
    });
}
```

---

## 5. 依赖关系

- **前置依赖**：无（帧起始）
- **后续依赖**：Main Render Pass（Subpass 0）需要干净的 GBuffer 和深度缓冲
- **同步**：作为 CommandGraph 的第一个节点，天然在所有后续 Pass 之前执行
