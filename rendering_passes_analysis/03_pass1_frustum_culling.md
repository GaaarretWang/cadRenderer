# Pass1: Frustum Culling（视锥体剔除）

> 分析分支：`encoder`

---

## 1. 概述

Pass1 是主渲染前的 Compute Shader Pass，对所有实例执行**视锥体 6 平面裁剪**，将通过裁剪的实例写入间接绘制命令缓冲区，减少后续渲染的 Draw Call 开销。

**详细分析参见**：[occlusion_culling_analysis.md](../occlusion_culling_analysis.md) 第 3.1 节

---

## 2. 位置

- **着色器**：`asset/data/shaders/computevertex.comp`
- **管线构建**：`asset/src/OcclusionCullingPasses.cpp` — `buildFirstComputePass()`
- **调度**：`asset/src/vsgRendererServer.cpp` — `commandGraph->addChild(depth_cull_command_graph1)`

---

## 3. 核心逻辑

### 3.1 视锥体平面提取

从相机参数推导 6 个视锥体平面（左、右、上、下、近、远），存储在 `camera_plane.planes[6]` 中。

### 3.2 实例剔除

```glsl
// 对每个实例的包围盒 8 个顶点
bool isFullyCulled(vec4[8] points, vec4 plane) {
    for (int i = 0; i < 8; i++) {
        if (dot(points[i].xyz, plane.xyz) + plane.w >= 0.0)
            return false;  // 至少一个顶点在内侧
    }
    return true;  // 所有顶点都在外侧 → 完全剔除
}

// 6 个平面逐一检测
for (int p = 0; p < 6; p++) {
    if (isFullyCulled(viewPoints, camera_plane.planes[p])) {
        culled = true;
        break;
    }
}
```

### 3.3 输出

- 更新 `DrawIndexedIndirect` 的 `instanceCount`
- 将通过剔除的实例矩阵从 `input_instance_buffer` 紧凑写入 `output_instance_buffer`

---

## 4. 数据流

```
input_instance_buffer_info → Pass1 Compute → output_instance_buffer_info
                                                    ↓
                                            Main Render Pass（DrawIndexedIndirect）
```

---

## 5. 同步

Pass1 完成后插入 `BufferMemoryBarrier`：
- `SHADER_WRITE_BIT` → `INDIRECT_COMMAND_READ_BIT`
- 确保 `instanceCount` 的写入对后续渲染 Pass 可见

---

## 6. 依赖关系

- **前置依赖**：Clear Pass（间接依赖，确保缓冲区状态）
- **后续依赖**：Main Render Pass（读取剔除后的实例列表）
- **注意**：Pass1 仅做视锥体粗剔除，不涉及深度信息；深度遮挡剔除由 Pass2 完成
