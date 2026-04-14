# 渲染器遮挡剔除（Occlusion Culling）与深度金字塔分析

> 分析分支：`encoder`
> 分析日期：2026-04-03

---

## 1. 总体架构

本项目实现了一套完整的 **GPU 驱动的遮挡剔除（GPU-Driven Occlusion Culling）** 管线，基于深度金字塔（Hierarchical Z-Buffer / HiZ）技术。整个管线分为三个阶段，在两个 CommandGraph 中执行：

```
CommandGraph（每帧第一遍）:
  ├── Clear Pass（清屏）
  ├── Pass1: First Compute Pass（视锥体剔除 + 初始化）
  └── Render Pass（虚拟 CAD 模型渲染到 Offscreen FBO）

CommandGraph1（每帧第二遍）:
  ├── Depth Pyramid Generation（深度金字塔构建）
  ├── Pass2: Second Compute Pass（深度遮挡剔除）
  ├── Render Pass1（合成渲染）
  ├── Barrier（颜色布局转换）
  └── Copy to Window（输出到屏幕）
```

**执行顺序**：Pass1 视锥剔除 → 虚拟场景渲染（生成深度图） → 深度金字塔构建 → Pass2 深度遮挡剔除 → 合成渲染

---

## 2. 深度金字塔生成过程（详细）

### 2.1 深度金字塔 Image 创建

位置：`asset/src/OcclusionCullingPasses.cpp:11-40`，`initOcclusionCullingPassesImageInfo()`

```cpp
depthPyramidImage = vsg::Image::create();
depthPyramidImage->imageType = VK_IMAGE_TYPE_2D;
depthPyramidImage->format = VK_FORMAT_R32_SFLOAT;       // 注意：不是深度格式，是 R32 浮点颜色格式
depthPyramidImage->mipLevels = 10;                       // 共 10 个 Mipmap 层级
depthPyramidImage->arrayLayers = 1;
depthPyramidImage->usage = VK_IMAGE_USAGE_STORAGE_BIT |  // Compute Shader 读写
                           VK_IMAGE_USAGE_SAMPLED_BIT |  // 片元着色器采样
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT;
depthPyramidImage->initialLayout = VK_IMAGE_LAYOUT_GENERAL;
depthPyramidImage->extent.width = extent.width;          // 原始屏幕分辨率
depthPyramidImage->extent.height = extent.height;
```

**关键设计**：
- 使用 `VK_FORMAT_R32_SFLOAT`（32位浮点颜色格式），而非深度格式（如 `D32_SFLOAT`）。这是因为深度金字塔通过 Compute Shader 以 Storage Image 方式读写，不走深度测试管线
- 10 层 Mipmap，每层分辨率减半（如 1920x1080 → 960x540 → ... → 4x2 → 2x1 → 1x1）
- 采样器使用 `VK_FILTER_NEAREST`，避免插值污染最大深度值

### 2.2 深度金字塔构建流程

位置：`asset/src/OcclusionCullingPasses.cpp:147-295`，`buildDepthPyramid()`

整个构建过程通过 10 个 Compute Shader Pass 串行执行，每个 Pass 生成一层 Mipmap。Pass 之间通过 `PipelineBarrier` 进行同步。

#### 第 0 层：原始深度图拷贝

使用 `computevertex_depthimage.comp`（`asset/data/shaders/computevertex_depthimage.comp`）：

```glsl
layout(r32f, set = 0, binding = 0) uniform writeonly image2D depth_pyramid;
layout(r32f, set = 0, binding = 1) uniform readonly image2D src_depth;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    imageStore(depth_pyramid, coord, vec4(imageLoad(src_depth, coord).x, 0, 0, 0));
}
```

- 将 `OffscreenRenderTarget` 的深度附件（`depthImage`）原始值逐像素拷贝到金字塔第 0 层
- Dispatch 尺寸：`(width+31)/32 × (height+31)/32 × 1`
- 在此之前有一个 PipelineBarrier 将深度图布局从 `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` 转换为 `SHADER_READ_ONLY_OPTIMAL`

#### 第 1-9 层：逐级降采样

使用 `computevertex_depthpyramid.comp`（`asset/data/shaders/computevertex_depthpyramid.comp`）：

```glsl
layout(r32f, set = 0, binding = 0) uniform writeonly image2D depth_pyramid;
layout(r32f, set = 0, binding = 1) uniform readonly image2D prev_level;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= pc.size.x || coord.y >= pc.size.y) return;

    ivec2 src_coord = coord * 2;
    float d00 = imageLoad(prev_level, src_coord).x;
    float d01 = imageLoad(prev_level, src_coord + ivec2(0, 1)).x;
    float d10 = imageLoad(prev_level, src_coord + ivec2(1, 0)).x;
    float d11 = imageLoad(prev_level, src_coord + ivec2(1, 1)).x;

    float min_depth = min(min(d00, d01), min(d10, d11));
    imageStore(depth_pyramid, coord, vec4(min_depth, 0, 0, 0));
}
```

**降采样逻辑**：
1. 从上一层级（分辨率 2W × 2H）读取 2×2 区域的 4 个深度值
2. 取这 4 个值的**最小值**（注意：在反转深度或特定深度范围下，min 对应"最远"的深度）
3. 写入当前层级（分辨率 W × H）

每层的 PushConstants 传入当前层级的分辨率：`{width >> i, height >> i}`

#### 层间同步

每层生成后插入 `ImageMemoryBarrier`：
```cpp
auto barrier = vsg::ImageMemoryBarrier::create(
    VK_ACCESS_SHADER_WRITE_BIT,
    VK_ACCESS_SHADER_READ_BIT,
    VK_IMAGE_LAYOUT_GENERAL,
    VK_IMAGE_LAYOUT_GENERAL,
    ...
    depthPyramidImage,
    VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1}  // 仅同步当前层级
);
```

最终所有层级生成完成后，插入一个全层级 Barrier 将金字塔从写入状态转为读取状态，供后续 Pass2 的遮挡剔除使用。

### 2.3 深度金字塔结构示意

```
Level 0: 原始深度图    1920 × 1080  (从 FBO 深度附件拷贝)
Level 1: 降采样        960  × 540   (2×2 → 1, min)
Level 2: 降采样        480  × 270
Level 3: 降采样        240  × 135
Level 4: 降采样        120  × 68
Level 5: 降采样        60   × 34
Level 6: 降采样        30   × 17
Level 7: 降采样        15   × 9
Level 8: 降采样        8    × 5
Level 9: 降采样        4    × 3
```

---

## 3. 遮挡剔除 Compute Shader 详解

### 3.1 Pass1：视锥体剔除（`computevertex.comp`）

位置：`asset/data/shaders/computevertex.comp`
调度：`OcclusionCullingPasses::buildFirstComputePass()`，在渲染之前执行

**功能**：
- 对每个实例做视锥体 6 平面裁剪
- 将通过裁剪的实例写入 `outIndirect` 和 `outIndirectMatrix`
- 不涉及深度金字塔，纯粹是视锥体级别的粗剔除

**核心逻辑**：
```glsl
// 6 个视锥平面（从相机参数推导）
bool isFullyCulled(vec4[8] points, vec4 plane) {
    for (int i = 0; i < 8; i++) {
        if (dot(points[i].xyz, plane.xyz) + plane.w >= 0.0)
            return false;  // 至少一个顶点在内侧
    }
    return true;  // 所有顶点都在外侧
}

// 对 6 个平面逐一检测
for (int p = 0; p < 6; p++) {
    if (isFullyCulled(viewPoints, camera_plane.planes[p])) {
        culled = true;
        break;
    }
}
```

**输出**：更新 `DrawIndexedIndirect` 的 `instanceCount`，以及将可见实例的矩阵从 `input_instance_buffer` 拷贝到 `output_instance_buffer`。

### 3.2 Pass2：深度遮挡剔除（`computevertex1.comp`）

位置：`asset/data/shaders/computevertex1.comp`
调度：`OcclusionCullingPasses::buildSecondComputePass()`，在深度金字塔生成之后执行

这是整个管线的**核心**——利用深度金字塔进行遮挡剔除。`computevertex1.comp`（`local_size_x = 32`）和 `computevertex1_seat.comp`（`local_size_x = 700`）逻辑完全一致，仅 workgroup 大小不同，根据实例数量选择使用。

**剔除流程**：

#### Step 1：视锥体剔除（同 Pass1）

```glsl
bool culled = false;
for (int p = 0; p < 6; p++) {
    if (isFullyCulled(viewPoints, camera_plane.planes[p])) {
        culled = true;
        break;
    }
}
```

#### Step 2：计算屏幕空间包围盒和 Mip Level

```glsl
// 将包围盒 8 个顶点从 Clip 空间转到 NDC，再转到 UV 空间 [0,1]
vec3 ndcs[8];
for (int i = 0; i < 8; i++) {
    ndcs[i] = clipPoints[i].xyz / clipPoints[i].w;
    ndcs[i].xy = clamp(ndcs[i].xy, -1, 1) * 0.5 + 0.5;
    ndcs[i].z = clamp(ndcs[i].z, 0, 1);  // 深度值归一化到 [0,1]
    // 更新 uv_min / uv_max
}

// 计算屏幕覆盖范围（像素）
float uvWidth  = (uv_max.x - uv_min.x) * imageSize.x;
float uvHeight = (uv_max.y - uv_min.y) * imageSize.y;

// 选择合适的 Mip Level
float maxCoverage = max(uvWidth, uvHeight);
int mipLevel = int(ceil(log2(maxCoverage / compare_image_size)));
mipLevel = clamp(mipLevel, 0, 9);
```

**Mip Level 选择策略**：物体屏幕覆盖范围越大，使用越高分辨率的 Mip Level（更低层级）。`compare_image_size = 4` 是比较区域的固定大小（4×4 像素网格）。

#### Step 3：从深度金字塔采样场景深度

```glsl
#define compare_image_size 4
float render_depths[compare_image_size_square];  // 4×4 = 16 个值
for (int i = 0; i < compare_image_size; i++) {
    for (int j = 0; j < compare_image_size; j++) {
        vec2 mipUV = vec2(begin_pixel.x + 0.5f + j, begin_pixel.y + 0.5f + i) / pixel_scale;
        render_depths[i * compare_image_size + j] = textureLod(mipPyramid, mipUV, mipLevel).x;
    }
}
```

在选中的 Mip Level 上，采样一个 4×4 区域的深度值作为"场景已有深度"。

#### Step 4：光栅化包围盒到 4×4 深度缓冲（软件光栅化）

```glsl
// 将 8 个 NDC 顶点转换到 Mip Level 对应的像素空间
vec3 screenPoints[8];
for (int i = 0; i < 8; i++) {
    screenPoints[i].xy = ndcs[i].xy * mip_scale * imageSize - begin_pixel;
    screenPoints[i].z = ndcs[i].z;
}

// 对包围盒 12 个三角形面逐一光栅化
rasterize(screenPoints[0], screenPoints[2], screenPoints[6], out_surface); // 面1
rasterize(screenPoints[0], screenPoints[6], screenPoints[4], out_surface); // 面2
// ... 共 12 个三角形面
rasterize(screenPoints[2], screenPoints[7], screenPoints[6], out_surface); // 面12
```

`rasterize()` 函数在 `compare_image_size+1 = 5` 的网格上进行软件三角形光栅化：
- 使用重心坐标遍历覆盖的像素
- 通过深度插值计算每个像素的深度
- 取 max 保留最远深度（处理多个面覆盖同一像素的情况）

之后对 5×5 网格取 2×2 邻域的最大值，压缩为 4×4 的 `final_depth[]` 数组。

#### Step 5：深度比较与剔除决策

```glsl
bool depth_culled = true;
for(int i = 0; i < compare_image_size_square; ++i){
    if(final_depth[i] + 0.01 > render_depths[i])
        depth_culled = false;  // 包围盒的深度比场景深度更近 → 可见
}
```

- `final_depth[i]`：包围盒在该像素的最近深度（软件光栅化得到）
- `render_depths[i]`：场景在该像素的最远深度（从深度金字塔采样）
- 如果包围盒**所有**像素都被场景遮挡（`depth_culled = true`），则剔除该实例
- `+ 0.01` 是深度偏移（bias），防止 z-fighting

**特殊情况**：如果包围盒有顶点穿过近平面（`clipsNearPlane = true`），则始终可见，不做深度剔除。

#### Step 6：写入可见实例

```glsl
if(!depth_culled || clipsNearPlane){
    inHighlightData.data[instanceIndex].z = 1;  // 标记为可见
    if(inHighlightData.data[instanceIndex].y == 0){  // 未被隐藏
        uint prevCount = atomicAdd(outIndirect.instanceCount, 1);
        // 写入实例数据到 output_instance_buffer
        outIndirectMatrix.instanceModelMatrix[prevCount] = out_data;
    }
}
```

通过 `atomicAdd` 实现无锁并行的可见实例计数，将通过剔除的实例紧凑写入输出缓冲。

---

## 4. ProtoData 中与剔除相关的缓冲区

位置：`asset/include/CADMesh.h`

```cpp
struct ProtoData {
    // ...
    vsg::ref_ptr<vsg::DrawIndexedIndirect> draw_indirect;       // 间接绘制命令
    vsg::ref_ptr<vsg::BufferInfo> indirect_full_buffer_info;    // 完整的间接绘制命令（备份）
    vsg::ref_ptr<vsg::mat4Array> instance_buffer;               // 输入实例矩阵
    vsg::ref_ptr<vsg::mat4Array> last_instance_buffer;          // 上一帧的 proto 矩阵
    vsg::ref_ptr<vsg::BufferInfo> input_instance_buffer_info;   // 输入实例矩阵 Buffer
    vsg::ref_ptr<vsg::BufferInfo> last_instance_buffer_info;    // 上一帧 Buffer
    vsg::ref_ptr<vsg::BufferInfo> output_instance_buffer_info;  // 输出实例矩阵 Buffer
    vsg::ref_ptr<vsg::BufferInfo> bounds_buffer_info;           // 包围盒 Buffer
    vsg::ref_ptr<vsg::vec4Array> bounds_data;                   // 包围盒数据 (min, max, draw_state)
    // ...
};

// 全局变换矩阵
static vsg::ref_ptr<vsg::mat4Array> global_model_matrix_buffer;
static vsg::ref_ptr<vsg::BufferInfo> global_model_matrix_buffer_info;
static vsg::ref_ptr<vsg::mat4Array> last_global_model_matrix_buffer;
static vsg::ref_ptr<vsg::BufferInfo> last_global_model_matrix_buffer_info;
```

**数据流**：
- Pass1 读取 `input_instance_buffer_info`，写入 `output_instance_buffer_info`
- Pass2 读取 Pass1 的输出，进一步深度剔除后写入 `output_instance_buffer_info`
- 图形渲染 Pass 读取最终的 `output_instance_buffer_info` 进行绘制

---

## 5. 同步与管线屏障

整个管线的同步通过 Vulkan PipelineBarrier 实现：

```
Pass1（Compute）:
  └─ BufferMemoryBarrier: SHADER_WRITE → INDIRECT_COMMAND_READ
     确保 Pass1 的 instanceCount 写入对后续渲染可见

渲染（Graphics）:
  └─ ImageMemoryBarrier: DEPTH_STENCIL_ATTACHMENT → SHADER_READ_ONLY
     确保深度图写入完成，可被金字塔生成读取

金字塔生成（Compute）:
  └─ 层间 ImageMemoryBarrier: 每层生成后同步
  └─ 最终 ImageMemoryBarrier: 全 10 层 SHADER_WRITE → SHADER_READ

Pass2（Compute）:
  └─ BufferMemoryBarrier: SHADER_WRITE → INDIRECT_COMMAND_READ
     确保 Pass2 的 instanceCount 对最终渲染可见
```

---

## 6. 相关文件清单

| 文件 | 功能 |
|------|------|
| `asset/include/OcclusionCullingPasses.h` | 遮挡剔除模块头文件，声明接口和全局状态 |
| `asset/src/OcclusionCullingPasses.cpp` | 深度金字塔 Image 创建、Pass1/金字塔构建/Pass2 的管线搭建 |
| `asset/data/shaders/computevertex.comp` | Pass1 计算着色器：视锥体剔除 + 初始化 |
| `asset/data/shaders/computevertex_depthimage.comp` | 金字塔第 0 层：原始深度图拷贝 |
| `asset/data/shaders/computevertex_depthpyramid.comp` | 金字塔第 1-9 层：2×2 → 1 降采样（取 min） |
| `asset/data/shaders/computevertex1.comp` | Pass2 计算着色器：深度遮挡剔除（local_size_x = 32） |
| `asset/data/shaders/computevertex1_seat.comp` | Pass2 计算着色器：深度遮挡剔除（local_size_x = 700，大数据量变体） |
| `asset/src/vsgRendererServer.cpp` | 渲染器主循环，编排两个 CommandGraph 的执行顺序 |
| `asset/include/CADMesh.h` | ProtoData 结构，包含剔除所需的缓冲区定义 |
| `asset/include/OffscreenRenderTarget.h` | 离屏渲染目标，提供深度附件供金字塔生成 |

---

## 7. 性能相关设计

1. **Mip Level 自适应**：根据物体屏幕覆盖面积自动选择金字塔层级，大物体用高层级（低分辨率快速判断），小物体用低层级（高分辨率精确判断）
2. **固定比较网格**：`compare_image_size = 4`，始终在 4×4 网格上做深度比较，控制计算量
3. **双 Workgroup 变体**：`computevertex1.comp`（32 线程）和 `computevertex1_seat.comp`（700 线程），根据实例数选择，避免小数据量的线程浪费
4. **Pass 分离**：视锥剔除（Pass1）在渲染前执行，深度剔除（Pass2）在渲染后执行。这是因为深度金字塔需要上一帧或当前帧的深度图作为输入
5. **原子计数**：使用 `atomicAdd` 实现可见实例的紧凑写入，避免额外的排序步骤
