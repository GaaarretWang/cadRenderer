# Pass2: Depth Occlusion Culling（深度遮挡剔除）

> 分析分支：`encoder`

---

## 1. 概述

Pass2 是遮挡剔除管线的核心，利用深度金字塔进行**精确的深度遮挡剔除**。对每个实例做视锥体剔除后，再通过软件光栅化包围盒并与深度金字塔比较，剔除被场景遮挡的实例。

**详细分析参见**：[occlusion_culling_analysis.md](../occlusion_culling_analysis.md) 第 3.2 节

---

## 2. 位置

- **着色器**：
  - `asset/data/shaders/computevertex1.comp`（local_size_x = 32）
  - `asset/data/shaders/computevertex1_seat.comp`（local_size_x = 700，大数据量变体）
- **管线构建**：`asset/src/OcclusionCullingPasses.cpp` — `buildSecondComputePass()`
- **调度**：`asset/src/vsgRendererServer.cpp` — `commandGraph1->addChild(depth_pyramid_CommandGraph)`

---

## 3. 剔除流程

### Step 1：视锥体剔除（同 Pass1）
对 6 个视锥体平面逐一检测，完全在外侧的实例直接剔除。

### Step 2：计算屏幕空间包围盒和 Mip Level
```glsl
// NDC → UV 空间，计算屏幕覆盖范围
float maxCoverage = max(uvWidth, uvHeight);
int mipLevel = int(ceil(log2(maxCoverage / compare_image_size)));
mipLevel = clamp(mipLevel, 0, 9);
```

### Step 3：从深度金字塔采样场景深度
在选中的 Mip Level 上采样 4×4 区域的深度值。

### Step 4：软件光栅化包围盒到 4×4 深度缓冲
对包围盒 12 个三角形面逐一光栅化，通过重心坐标插值得到每个像素的深度。

### Step 5：深度比较与剔除决策
```glsl
if(final_depth[i] + 0.01 > render_depths[i])
    depth_culled = false;  // 包围盒深度比场景更近 → 可见
```
如果所有像素都被遮挡，则剔除该实例。

### Step 6：写入可见实例
通过 `atomicAdd` 紧凑写入可见实例列表。

---

## 4. 双 Workgroup 变体

| 变体 | local_size | 适用场景 |
|------|-----------|----------|
| `computevertex1.comp` | 32 | 小数据量（≤32 实例/Workgroup） |
| `computevertex1_seat.comp` | 700 | 大数据量（>32 实例/Workgroup） |

---

## 5. 依赖关系

- **前置依赖**：
  - Depth Pyramid（需要深度金字塔）
  - Pass1（输入实例列表）
- **后续依赖**：Synthesis Render Pass（使用最终可见实例绘制）
- **同步**：完成后插入 BufferMemoryBarrier（SHADER_WRITE → INDIRECT_COMMAND_READ）
