
# 环境光贴图接口文档

## 目录

1. 图像和采样器创建
2. 内存屏障和管线屏障
3. 资源管理
4. 环境贴图生成
5. BRDF查找表生成
6. 辐照度立方体贴图生成
7. 预过滤环境立方体贴图生成
8. 天空盒渲染
9. PBR着色器设置
10. 测试场景创建

## 1. 图像和采样器创建

### 1.1 createImage2D

创建一个2D图像和对应的图像视图。

**声明：**

```cpp
void createImage2D(vsg::Context& context, VkFormat format, VkImageUsageFlags usage, VkExtent2D extent, ptr<vsg::Image>& image, ptr<vsg::ImageView>& imageView)
```

**参数：**

- context：vsg::Context& - Vulkan上下文
- format：VkFormat - 图像格式
- usage：VkImageUsageFlags - 图像用途标志
- extent：VkExtent2D - 图像尺寸
- image：ptr\<vsg::Image>& - 输出参数，创建的图像对象
- imageView：ptr\<vsg::ImageView>& - 输出参数，创建的图像视图对象

**返回值：** void

**注意事项：**

- 此函数创建的图像为2D类型，单层，无mipmap
- 图像的内存分配被注释掉，可能需要在RenderGraph编译时进行

### 1.2 createImageCube

创建一个立方体贴图图像和对应的图像视图。

**声明：**

```cpp
void createImageCube(vsg::Context& context, VkFormat format, VkImageUsageFlags usage, VkExtent2D extent, uint32_t numMips, ptr<vsg::Image>& image, ptr<vsg::ImageView>& imageView)
```

**参数：**

- context：vsg::Context& - Vulkan上下文
- format：VkFormat - 图像格式
- usage：VkImageUsageFlags - 图像用途标志
- extent：VkExtent2D - 图像尺寸
- numMips：uint32_t - mipmap级别数量
- image：ptr\<vsg::Image>& - 输出参数，创建的图像对象
- imageView：ptr\<vsg::ImageView>& - 输出参数，创建的图像视图对象

**返回值：** void

**注意事项：**

- 此函数创建的是立方体贴图，包含6个面
- 图像视图的类型被设置为VK_IMAGE_VIEW_TYPE_CUBE
- 函数手动编译图像和分配内存，而不是使用vsg::createImageView

### 1.3 createSampler

创建一个图像采样器对象。

**声明：**

```cpp
void createSampler(uint32_t numMips, ptr<vsg::Sampler>& sampler)
```

**参数：**

- numMips：uint32_t - mipmap级别数量
- sampler：ptr\<vsg::Sampler>& - 输出参数，创建的采样器对象

**返回值：** void

**注意事项：**

- 采样器使用线性过滤和线性mipmap模式
- 地址模式设置为VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
- maxLod设置为mipmap级别数量

### 1.4 createSamplerCube

创建一个适用于立方体贴图的图像采样器对象。

**声明：**

```cpp
void createSamplerCube(uint32_t numMips, ptr<vsg::Sampler>& sampler)
```

**参数：**

- numMips：uint32_t - mipmap级别数量
- sampler：ptr\<vsg::Sampler>& - 输出参数，创建的采样器对象

**返回值：** void

**注意事项：**

- 采样器使用线性过滤和线性mipmap模式
- 地址模式设置为VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
- maxLod设置为mipmap级别数量

### 1.5 createImageInfo

创建一个vsg::ImageInfo对象，包含图像视图、图像布局和采样器信息。

**声明：**

```cpp
void createImageInfo(const ptr<vsg::ImageView> imageView, VkImageLayout layout, const ptr<vsg::Sampler> sampler, ptr<vsg::ImageInfo> &imageInfo)
```

**参数：**

- imageView：const ptr\<vsg::ImageView> - 图像视图对象
- layout：VkImageLayout - 图像布局
- sampler：const ptr\<vsg::Sampler> - 采样器对象
- imageInfo：ptr\<vsg::ImageInfo>& - 输出参数，创建的ImageInfo对象

**返回值：** void

**注意事项：**

- 此函数将图像视图、布局和采样器信息组合到一个ImageInfo对象中

## 2. 内存屏障和管线屏障

### 2.1 createImageMemoryBarrier

创建一个图像内存屏障，用于图像布局转换。

**声明：**

```cpp
ptr<vsg::ImageMemoryBarrier> createImageMemoryBarrier(ptr<vsg::Image> image, VkImageSubresourceRange subresourceRange, VkImageLayout oldImageLayout, VkImageLayout newImageLayout)
```

**参数：**

- image：ptr\<vsg::Image> - 需要进行布局转换的图像
- subresourceRange：VkImageSubresourceRange - 图像的子资源范围
- oldImageLayout：VkImageLayout - 图像的旧布局
- newImageLayout：VkImageLayout - 图像的新布局

**返回值：** ptr\<vsg::ImageMemoryBarrier> - 创建的图像内存屏障对象

**注意事项：**

- 函数会根据旧布局和新布局自动设置适当的访问掩码
- 支持多种常见的布局转换场景

### 2.2 createImageLayoutPipelineBarrier

创建一个用于图像布局转换的管线屏障。

**声明：**

```cpp
ptr<vsg::PipelineBarrier> createImageLayoutPipelineBarrier(ptr<vsg::Image> image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
```

**参数：**

- image：ptr\<vsg::Image> - 需要进行布局转换的图像
- oldImageLayout：VkImageLayout - 图像的旧布局
- newImageLayout：VkImageLayout - 图像的新布局
- subresourceRange：VkImageSubresourceRange - 图像的子资源范围
- srcStageMask：VkPipelineStageFlags - 源管线阶段掩码（默认为ALL_COMMANDS_BIT）
- dstStageMask：VkPipelineStageFlags - 目标管线阶段掩码（默认为ALL_COMMANDS_BIT）

**返回值：** ptr\<vsg::PipelineBarrier> - 创建的管线屏障对象

**注意事项：**

- 此函数使用createImageMemoryBarrier创建图像内存屏障
- 默认的管线阶段掩码为ALL_COMMANDS_BIT，可以根据需要进行调整

### 2.3 createImageLayoutPipelineBarrier（重载版本）

创建一个用于图像布局转换的管线屏障，使用简化的参数。

**声明：**

```cpp
ptr<vsg::PipelineBarrier> createImageLayoutPipelineBarrier(ptr<vsg::Image> image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
```

**参数：**

- image：ptr\<vsg::Image> - 需要进行布局转换的图像
- aspectMask：VkImageAspectFlags - 图像的方面掩码
- oldImageLayout：VkImageLayout - 图像的旧布局
- newImageLayout：VkImageLayout - 图像的新布局
- srcStageMask：VkPipelineStageFlags - 源管线阶段掩码（默认为ALL_COMMANDS_BIT）
- dstStageMask：VkPipelineStageFlags - 目标管线阶段掩码（默认为ALL_COMMANDS_BIT）

**返回值：** ptr\<vsg::PipelineBarrier> - 创建的管线屏障对象

**注意事项：**

- 此函数是createImageLayoutPipelineBarrier的重载版本，简化了子资源范围的设置
- 默认子资源范围设置为单个mip级别和单个数组层

## 3. 资源管理

### 3.1 createResources

创建各种图像资源，包括环境立方体贴图、BRDF查找表、辐照度立方体贴图和预过滤环境立方体贴图。

**声明：**

```cpp
void createResources(VsgContext& vsgContext)
```

**参数：**

- vsgContext：VsgContext& - VSG上下文对象

**返回值：** void

**注意事项：**

- 函数创建多个图像资源和相关的采样器
- 设置了同步用的VkEvent对象
- 创建了天空盒的顶点和索引数据
- 初始化了一些参数缓冲

### 3.2 clearResources

清理和重置所有创建的资源。

**声明：**

```cpp
void clearResources()
```

**参数：** 无

**返回值：** void

**注意事项：**

- 函数主要通过将指针设置为nullptr来清理资源
- 大部分清理代码被注释掉了，可能是为了优化性能或保留某些资源
- 清空了appData、textures、gVkEvents、gSkyboxCube和gEnvmapRect等全局变量
- 清空了shaderSets容器

## 4. 环境贴图生成

### 4.1 loadEnvmapRect

加载HDR环境贴图并创建相应的图像、采样器和图像信息。

**声明：**

```cpp
void loadEnvmapRect(VsgContext& context, const std::string& filePath)
```

**参数：**

- context：VsgContext& - VSG上下文
- filePath：const std::string& - HDR文件路径

**返回值：** void

**注意事项：**

- 使用STBI加载器读取HDR图像
- 创建一个2D数组来存储图像数据
- 设置采样器的地址模式为CLAMP_TO_EDGE
- 创建ImageInfo对象，包含采样器、图像和布局信息

### 4.2 loadHdrFile

使用stb_image库加载HDR文件。

**声明：**

```cpp
float *loadHdrFile(const std::string& filepath, int& width, int& height, int& channels)
```

**参数：**

- filepath：const std::string& - HDR文件路径
- width：int& - 输出参数，图像宽度
- height：int& - 输出参数，图像高度
- channels：int& - 输出参数，图像通道数

**返回值：** float* - 加载的图像数据指针

**注意事项：**

- 使用stbi_loadf函数加载HDR文件
- 强制加载为4通道（RGBA）格式

### 4.3 generateEnvmap

从等距矩形HDR环境贴图生成立方体贴图。

**声明：**

```cpp
void generateEnvmap(VsgContext& vsgContext)
```

**参数：**

- vsgContext：VsgContext& - VSG上下文

**返回值：** void

**注意事项：**

- 加载等距矩形HDR环境贴图
- 创建离屏帧缓冲用于渲染立方体贴图的每个面
- 设置图形管线，包括顶点和片段着色器
- 为立方体贴图的每个面创建渲染通道
- 使用CopyImage和BlitImage命令将渲染结果复制到立方体贴图
- 生成立方体贴图的mipmap级别
- 设置内存屏障和事件，以便后续处理

## 5. BRDF查找表生成

### 5.1 generateBRDFLUT

生成BRDF查找表（Look-Up Table）。

**声明：**

```cpp
void generateBRDFLUT(VsgContext &vsgContext)
```

**参数：**

- vsgContext：VsgContext& - VSG上下文

**返回值：** void

**注意事项：**

- 创建全屏四边形顶点着色器和BRDF LUT片段着色器
- 设置渲染目标为R16G16格式的512x512图像
- 创建渲染通道、帧缓冲和渲染图
- 设置图形管线状态，包括顶点输入、光栅化、混合等
- 创建命令图并添加到查看器中执行

## 6. 辐照度立方体贴图生成

### 6.1 generateIrradianceCube

生成辐照度立方体贴图。

**声明：**

```cpp
void generateIrradianceCube(VsgContext& vsgContext)
```

**参数：**

- vsgContext：VsgContext& - VSG上下文

**返回值：** void

**注意事项：**

- 使用天空盒网格着色器生成辐照度立方体贴图
- 为每个面和每个mip级别创建渲染通道
- 使用CopyImage命令将渲染结果复制到辐照度立方体贴图
- 设置内存屏障以确保正确的图像布局转换
- 等待环境立方体贴图生成完成后再开始处理

## 7. 预过滤环境立方体贴图生成

### 7.1 generatePrefilteredEnvmapCube

生成预过滤环境立方体贴图。

**声明：**

```cpp
void generatePrefilteredEnvmapCube(VsgContext& vsgContext)
```

**参数：**

- vsgContext：VsgContext& - VSG上下文

**返回值：** void

**注意事项：**

- 使用天空盒网格着色器生成预过滤环境立方体贴图
- 为每个面和每个mip级别创建渲染通道
- 使用推送常量传递面索引和粗糙度信息
- 使用CopyImage命令将渲染结果复制到预过滤环境立方体贴图
- 设置内存屏障以确保正确的图像布局转换
- 等待环境立方体贴图生成完成后再开始处理

## 8. 天空盒渲染

### 8.1 drawSkyboxVSGNode

创建并返回用于渲染天空盒的VSG节点。

**声明：**

```cpp
ptr<StateGroup> drawSkyboxVSGNode(VsgContext& context)
```

**参数：**

- context：VsgContext& - VSG上下文对象，包含渲染所需的各种资源和状态

**返回值：** ptr\<StateGroup> - 包含天空盒渲染所需的所有状态和命令的StateGroup节点

**注意事项：**

- 使用自定义的顶点和片段着色器
- 设置了无面剔除和禁用深度测试的渲染状态
- 使用环境立方体贴图作为天空盒纹理
- 包含色调映射参数（曝光和伽玛）的uniform buffer

## 9. PBR着色器设置

### 9.1 customPbrShaderSet

创建并返回一个自定义的基于物理的渲染(PBR)着色器集。

**声明：**

```cpp
vsg::ref_ptr<vsg::ShaderSet> customPbrShaderSet(vsg::ref_ptr<const vsg::Options> options)
```

**参数：**

- options：vsg::ref_ptr\<const vsg::Options> - VSG选项对象，包含着色器文件路径等配置信息

**返回值：** vsg::ref_ptr<vsg::ShaderSet> - 包含PBR渲染所需的所有着色器、属性绑定和描述符绑定的着色器集

**注意事项：**

- 使用自定义的顶点和片段着色器
- 设置了多个描述符集，包括材质、视图和自定义IBL相关的描述符
- 包含多个可选的预处理器定义
- 支持实例化、位移贴图和广告牌等特性

## 10. 测试场景创建

### 10.1 createTestScene

创建一个测试场景，包含一个11x11的球体网格，每个球体具有不同的粗糙度和金属度。

**声明：**

```cpp
vsg::ref_ptr<vsg::Node> createTestScene(vsg::ref_ptr<vsg::Options> options, bool requiresBase = true)
```

**参数：**

- options：vsg::ref_ptr\<vsg::Options> - VSG选项对象，包含场景创建所需的配置信息
- requiresBase：bool - 是否需要创建底座（默认为true，但在当前实现中未使用）

**返回值：** vsg::ref_ptr\<vsg::Node> - 包含整个测试场景的根节点

**注意事项：**

- 使用customPbrShaderSet创建的着色器集
- 每个球体的粗糙度和金属度根据其在网格中的位置进行设置
- 球体被放置在一个3x3的网格中，中心位于原点

### 10.2 iblDemoSceneGraph

创建并返回用于IBL（基于图像的照明）演示的完整场景图。

**声明：**

```cpp
ptr<Node> iblDemoSceneGraph(VsgContext& context)
```

**参数：**

- context：VsgContext& - VSG上下文对象，包含场景创建所需的各种资源和状态

**返回值：** ptr\<Node> - 包含整个IBL演示场景的根节点

**注意事项：**

- 使用customPbrShaderSet创建自定义的PBR着色器集
- 调用createTestScene创建测试场景
- 当前实现中注释掉了一些可能用于设置IBL相关描述符集的代码
