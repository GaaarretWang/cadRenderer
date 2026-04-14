# Barrier & Copy to Window Pass（屏障与拷贝到窗口）

> 分析分支：`encoder`

---

## 1. 概述

Barrier & Copy to Window Pass 是每帧的最后两个 Pass，负责将离屏 FBO 的渲染结果通过布局转换和图像拷贝输出到 Swapchain，完成最终的屏幕显示。

---

## 2. 位置

- **Barrier**：`asset/src/vsgRendererServer.cpp` — `commandGraph1->addChild(barrierCommandGraph)`
- **Copy**：`asset/src/vsgRendererServer.cpp` — `commandGraph1->addChild(copyCommandGraph)`

---

## 3. Barrier Pass（布局转换）

### 3.1 颜色图像布局转换

将离屏渲染的颜色附件从 `COLOR_ATTACHMENT_OPTIMAL` 转换为 `GENERAL`：

```cpp
// Vkimgmembarrier 工具类（encoder.cpp）
VkImageMemoryBarrier barrier = {};
barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
barrier.image = offscreenColorImage->vk(deviceID);
barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
barrier.subresourceRange.baseMipLevel = 0;
barrier.subresourceRange.levelCount = 1;
barrier.subresourceRange.baseArrayLayer = 0;
barrier.subresourceRange.layerCount = 1;
```

### 3.2 为什么需要 GENERAL 布局

- Swapchain 图像通常使用 `GENERAL` 或 `PRESENT_SRC_KHR` 布局
- `vkCmdCopyImage` 要求源图像至少在 `TRANSFER_SRC_OPTIMAL` 或 `GENERAL` 布局
- `GENERAL` 布局兼容所有操作，适合简单的拷贝场景

---

## 4. Copy to Window Pass（图像拷贝）

### 4.1 离屏 → Swapchain 拷贝

```cpp
// 将离屏渲染结果拷贝到 swapchain image
VkImageCopy copyRegion = {};
copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
copyRegion.extent = {width, height, 1};

vkCmdCopyImage(commandBuffer,
    offscreenColorImage->vk(), VK_IMAGE_LAYOUT_GENERAL,
    swapchainImage->vk(), VK_IMAGE_LAYOUT_GENERAL,
    1, &copyRegion);
```

### 4.2 Swapchain 布局转换

在拷贝前后需要处理 Swapchain 图像的布局：
- 拷贝前：`UNDEFINED` → `GENERAL`（或 `TRANSFER_DST_OPTIMAL`）
- 拷贝后：`GENERAL` → `PRESENT_SRC_KHR`（用于显示）

---

## 5. 执行顺序

```
Synthesis Render Pass
  ↓
Barrier CommandGraph
  ├── offscreen color: COLOR_ATTACHMENT → GENERAL
  └── (可选) swapchain: PRESENT_SRC → GENERAL/TRANSFER_DST
  ↓
Copy CommandGraph
  ├── vkCmdCopyImage: offscreen → swapchain
  └── (可选) swapchain: GENERAL → PRESENT_SRC
  ↓
Present（显示）
```

---

## 6. 依赖关系

- **前置依赖**：Synthesis Render Pass（离屏渲染完成）
- **后续依赖**：Present（显示到屏幕）
- **同步**：Barrier 确保渲染完成后再进行拷贝；拷贝完成后才能 Present
