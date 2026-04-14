# Synthesis Render Pass（合成渲染 Pass — view1）

> 分析分支：`encoder`

---

## 1. 概述

Synthesis Render Pass 在 CommandGraph1 中执行，将虚拟 CAD 渲染结果与天空盒、线框、文字等叠加元素合并，输出最终的合成图像到离屏 FBO。

---

## 2. 位置

- **调度**：`asset/src/vsgRendererServer.cpp` — `commandGraph1->addChild(renderGraph1)`
- **天空盒**：`asset/src/IBL.cpp` — `drawSkyboxVSGNode()`
- **View**：`view1`，Mask = `MASK_PBR_FULL | MASK_WIREFRAME | MASK_TEXT | MASK_SHADOW_RECEIVER | MASK_SSAO`

---

## 3. 渲染内容

### 3.1 天空盒

通过 `drawSkyboxVSGNode()` 创建的天空盒节点，支持多种模式：

| 模式 | 说明 |
|------|------|
| `CAMERA_IMAGE` | 相机图像叠加（虚实融合） |
| `CAMERA_DEPTH` | 基于深度的天空盒 |
| `SHADOW_AWARE` | 支持阴影的天空盒 |

天空盒使用环境贴图（envmap cubemap）作为纹理。

### 3.2 虚拟 CAD 模型

通过 Pass2 剔除后的可见实例列表，使用 `DrawIndexedIndirect` 绘制。

### 3.3 线框叠加

`MASK_WIREFRAME` 标记的几何体以线框模式渲染。

### 3.4 文字叠加

`MASK_TEXT` 标记的文字元素叠加到最终图像上。

---

## 4. View Mask

```cpp
view1->mask = MASK_PBR_FULL | MASK_WIREFRAME | MASK_TEXT |
              MASK_SHADOW_RECEIVER | MASK_SSAO;
```

- `MASK_PBR_FULL`：虚拟 CAD 模型
- `MASK_WIREFRAME`：线框
- `MASK_TEXT`：文字
- `MASK_SHADOW_RECEIVER`：阴影接收
- `MASK_SSAO`：SSAO 效果

---

## 5. 输出

写入离屏 FBO 的颜色附件，后续通过 Barrier + Copy 传递到 Swapchain。

---

## 6. 依赖关系

- **前置依赖**：
  - Pass2（可见实例列表）
  - SSAO Denoise（Subpass 2 输出的最终颜色）
  - Shadow Pass（阴影贴图）
- **后续依赖**：Barrier Pass（布局转换）
