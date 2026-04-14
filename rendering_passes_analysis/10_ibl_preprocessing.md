# IBL Preprocessing（图像基光照预处理）

> 分析分支：`encoder`

---

## 1. 概述

IBL Preprocessing 是**初始化阶段**的一次性处理，生成 PBR 渲染所需的环境光照资源：BRDF LUT、辐照度立方体贴图、预滤波环境贴图。这些资源在运行时被 Main Render Pass 的 PBR 着色器采样。

---

## 2. 位置

- **实现**：`asset/src/IBL.cpp`
- **ShaderSet**：`IBL::customPbrShaderSet()`
- **调用时机**：初始化阶段（`initRenderer()` 中）

---

## 3. 预处理资源

### 3.1 BRDF LUT（BRDF 查找表）

- **格式**：2D 纹理
- **用途**：存储菲涅尔项和几何遮蔽项的积分结果
- **生成方式**：离线计算或运行时全屏渲染生成

### 3.2 Irradiance Cubemap（辐照度立方体贴图）

- **格式**：Cube Map
- **用途**：漫反射 IBL，存储环境光的平均辐照度
- **生成方式**：对环境贴图进行半球积分卷积

### 3.3 Prefiltered Environment Map（预滤波环境贴图）

- **格式**：Cube Map，多级 Mipmap
- **用途**：镜面反射 IBL，按粗糙度分级的环境反射
- **生成方式**：`generatePrefilteredEnvmapCube()`，对每个 Mip Level 和每个 Cube Face 渲染

```cpp
// 对每个 mip level（粗糙度递增）
for (int mip = 0; mip < numMipLevels; mip++) {
    float roughness = (float)mip / (numMipLevels - 1);
    // 对每个 cube face
    for (int face = 0; face < 6; face++) {
        // 设置 push constant: roughness
        // 渲染 cube geometry
        // 采样环境贴图 + 重要性采样
    }
}
```

### 3.4 Environment Map（环境贴图）

- **格式**：Cube Map
- **用途**：天空盒渲染 + 生成 irradiance/prefilter
- **支持 HDR 纹理更新**：`updateHDRTextures()` 在多个 HDR 环境间切换

---

## 4. PBR ShaderSet

`customPbrShaderSet()` 定义了完整的 PBR 材质系统：

| Descriptor | Set | Binding | 类型 |
|-----------|-----|---------|------|
| brdfLut | 0 | 0 | Combined Image Sampler |
| irradiance | 0 | 1 | Combined Image Sampler |
| prefilteredEnvmap | 0 | 2 | Combined Image Sampler |
| params | 0 | 3 | Uniform Buffer |
| instanceModelMatrix | 1 | 0 | Storage Buffer |
| diffuseMap | 3 | 0 | Combined Image Sampler |
| mrMap | 3 | 1 | Combined Image Sampler |
| normalMap | 3 | 2 | Combined Image Sampler |
| aoMap | 3 | 3 | Combined Image Sampler |
| emissiveMap | 3 | 4 | Combined Image Sampler |
| specularMap | 3 | 5 | Combined Image Sampler |
| displacementMap | 3 | 6 | Combined Image Sampler |

---

## 5. 天空盒渲染

`drawSkyboxVSGNode()` 创建天空盒渲染节点：

- 使用环境贴图（envmap）作为立方体贴图纹理
- 支持相机图像叠加模式（`CAMERA_IMAGE`）
- 支持深度-based 天空盒（`CAMERA_DEPTH`）
- 支持阴影感知模式（通过 `ViewDependentState` 采样阴影贴图）

---

## 6. 依赖关系

- **前置依赖**：无（初始化阶段）
- **后续依赖**：Main Render Pass（PBR 材质采样 IBL 资源）
- **注意**：IBL 资源在初始化时生成，运行时仅通过 `updateHDRTextures()` 切换环境
