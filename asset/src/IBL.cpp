// ============================================
// IBL.cpp - Image-Based Lighting (基于图像的光照) 资源创建与 PBR 渲染系统
//
// 本文件实现 IBL 全套离线 GPU 生成管线, 核心流程:
//   1. generateBRDFLUT:          生成 BRDF 积分查找表 (2D LUT)
//   2. generateEnvmap:           将 equirectangular HDR 图转换为 cubemap
//   3. generateIrradianceCube:   对 cubemap 做漫反射卷积, 生成辐照度贴图
//   4. generatePrefilteredEnvmapCube: GGX 重要性采样, 生成多级粗糙度的镜面反射预滤波贴图
//   5. drawSkyboxVSGNode:        创建天空盒渲染节点, 支持相机图像叠加和深度测试
//   6. customPbrShaderSet:       构建 PBR ShaderSet, 绑定所有 descriptor
//   7. updateHDRTextures:        运行时切换 HDR 环境贴图
//
// VSG (VulkanSceneGraph) 概念速查:
//   - Image:       Vulkan 图像对象的封装, 存储像素数据
//   - ImageView:   图像视图, 定义如何解释 Image (2D/cube/层数/mip 级)
//   - Sampler:     采样器, 定义纹理过滤和寻址模式
//   - ImageInfo:   将 Image + ImageView + Sampler 打包, 供 descriptor 绑定
//   - RenderPass:  Vulkan 渲染通道, 描述附件和子通道
//   - CommandGraph: 命令图, VSG 中组织 GPU 命令的根节点
//   - StateGroup:  状态组, 管线绑定/描述符集等渲染状态的容器
//   - ref_ptr<T>:  VSG 智能指针, 类似 shared_ptr, 基于引用计数管理 GPU 资源
// ============================================

#include "IBL.h"

#define _USE_MATH_DEFINES

#include <math.h>
#include <iostream>
#include <type_traits>
#include <stb_image.h>

using namespace vsg;

namespace vsg
{
    // ============================================
    // MyCustomPushConstants: 自定义 push constant 封装
    // VSG 内置的 PushConstants 不够灵活, 这里自定义一个 StateCommand
    // 用于在渲染时通过 vkCmdPushConstants 向 shader 传递自定义数据
    //
    // VSG StateCommand 概念: 每个 StateCommand 对应一个 Vulkan 命令
    // (如 vkCmdBindPipeline, vkCmdPushConstants 等), 在场景图遍历时自动执行
    // slot=2 表示该命令在渲染状态栈中的优先级位置
    // ============================================
    class VSG_DECLSPEC MyCustomPushConstants : public Inherit<StateCommand, MyCustomPushConstants>
    {
    public:
        MyCustomPushConstants();
        MyCustomPushConstants(VkShaderStageFlags in_shaderFlags, uint32_t in_offset, uint32_t in_data_size, void* in_data);

        VkShaderStageFlags stageFlags = 0;
        uint32_t offset = 0;
        uint32_t data_size;
        void* data;

        void read(Input& input) override;
        void write(Output& output) const override;

        void record(CommandBuffer& commandBuffer) const override;

    protected:
        virtual ~MyCustomPushConstants();
    };
    VSG_type_name(vsg::MyCustomPushConstants);

    MyCustomPushConstants::MyCustomPushConstants() :
        Inherit(2) // slot 0
    {
    }

    MyCustomPushConstants::MyCustomPushConstants(VkShaderStageFlags in_stageFlags, uint32_t in_offset, uint32_t in_data_size, void* in_data) :
        Inherit(2), // slot 0
        stageFlags(in_stageFlags),
        offset(in_offset),
        data_size(in_data_size),
        data(in_data)
    {
    }

    MyCustomPushConstants::~MyCustomPushConstants()
    {
    }

    void MyCustomPushConstants::read(Input& input)
    {
        StateCommand::read(input);

        input.readValue<uint32_t>("stageFlags", stageFlags);
        input.read("offset", offset);
        input.read("data_size", data_size);
        input.read("data", data);
    }

    void MyCustomPushConstants::write(Output& output) const
    {
        StateCommand::write(output);

        output.writeValue<uint32_t>("stageFlags", stageFlags);
        output.write("offset", offset);
        output.write("data_size", data_size);
        output.write("data", data);
    }

    void MyCustomPushConstants::record(CommandBuffer& commandBuffer) const
    {
        vkCmdPushConstants(commandBuffer, commandBuffer.getCurrentPipelineLayout(), stageFlags, offset, data_size, data);
    }

    // ============================================
    // MyViewMatrix: 基于 mat4 的简单视图矩阵封装
    // 用于离线 cubemap 渲染 (IBL 贴图生成时不需要完整的 Camera 对象)
    // VSG 中 ViewMatrix 是抽象基类, transform()/inverse() 提供视图矩阵变换
    // 这里直接存储 mat4, 无需继承 VSG 的 LookAt 等复杂视图矩阵类
    // ============================================
    class VSG_DECLSPEC MyViewMatrix : public Inherit<ViewMatrix, MyViewMatrix>
    {
    public:
        MyViewMatrix() :
            matrix() {}
        MyViewMatrix(const mat4& m) :
            matrix(m) {}

        dmat4 transform() const override{ return dmat4(matrix); }

        dmat4 inverse() const override{ return dmat4(matrix); }

        mat4 matrix;
    };
    VSG_type_name(vsg::MyViewMatrix);
} // namespace vsg

namespace IBL
{

// ============================================
// 静态变量: IBL 系统的全局 GPU 资源和状态
// ============================================

Textures textures;  // 所有 IBL 纹理资源 (envmap/irradiance/prefilter/brdfLut)

AppData appData = {};  // 应用程序数据 (options, debugOutputPath 等)

std::vector<ptr<ShaderSet>> shaderSets;  // 自定义 PBR ShaderSet 缓存 (用于测试场景)

// gEnvmapRect: 加载的 equirectangular HDR 矩形图像 (中间结果, 用于转换到 cubemap)
static struct _EnvmapRect
{
    ptr<Data> image;           // HDR 像素数据 (vec4Array2D, format=R32G32B32A32_SFLOAT)
    ptr<ImageView> imageView;  // 图像视图
    ptr<Sampler> sampler;      // 采样器 (CLAMP_TO_EDGE)
    ptr<ImageInfo> imageInfo;  // 绑定信息 (供 shader 采样)

    uint32_t width, height;    // HDR 图像分辨率
} gEnvmapRect;

// gSkyboxCube: 天空盒立方体的几何数据
// 6 面 x 4 顶点 = 24 个顶点 + 36 个索引 (每面 2 个三角形)
static struct skyboxCube
{
    ptr<vsg::vec3Array> vertices;   // 顶点位置 (x,y,z)
    ptr<vsg::ushortArray> indices;  // 索引 (ushort, 6面 * 6索引 = 36)

} gSkyboxCube;

// gVkEvents: GPU 同步事件 (VkEvent) 和内存屏障 (ImageMemoryBarrier)
// 用于确保 envmap cubemap 生成完成后再执行后续的 irradiance/prefilter 生成
// VSG Event 封装了 Vulkan VkEvent, 可在 GPU 命令流中设置/等待信号
static struct IBLVkEvents {
    ptr<Event> envmapCubeRenderedEvent;    // envmap cubemap 渲染完成事件 (未使用)
    ptr<Event> envmapCubeGeneratedEvent;   // envmap cubemap 生成完成事件
    ptr<ImageMemoryBarrier> envmapCubeRenderedBarrier;   // 渲染完成后的图像内存屏障 (未使用)
    ptr<ImageMemoryBarrier> envmapCubeGeneratedBarrier;  // 生成完成后的图像内存屏障
} gVkEvents;

// createImage2D: 创建 2D 图像及其 ImageView (用于 BRDF LUT / 离线 framebuffer 等)
// VSG 中 Image 和 ImageView 是分离的: Image 是 GPU 内存中的像素数据, ImageView 定义如何读取
void createImage2D(vsg::Context& context, VkFormat format, VkImageUsageFlags usage, VkExtent2D extent, ptr<vsg::Image>& image, ptr<vsg::ImageView>& imageView)
{
    // TODO: 内存分配延迟到RenderGraph的编译
    // Image
    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = format;
    image->extent = VkExtent3D{extent.width, extent.height, 1};
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = usage;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE; // new in vsg?

    // Image view
    imageView = vsg::createImageView(context, image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageView->viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageView->format = format;
    imageView->subresourceRange = {};
    imageView->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageView->subresourceRange.levelCount = 1;
    imageView->subresourceRange.layerCount = 1;
    imageView->image = image;
}

// createImageCube: 创建 cubemap 图像 (6 面, 用于 envmap/irradiance/prefilter)
// 关键标志: VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT 告诉 Vulkan 这是一个 cubemap
// arrayLayers=6 表示 6 个面, numMips 控制 mipmap 级数
// 注意: VSG 的 createImageView 默认创建 2D 视图, 所以这里手动设置 viewType=VK_IMAGE_VIEW_TYPE_CUBE
void createImageCube(vsg::Context& context,
    VkFormat format, 
    VkImageUsageFlags usage, 
    VkExtent2D extent, uint32_t numMips, 
    ptr<vsg::Image>& image, 
    ptr<vsg::ImageView>& imageView)
{
    // Image
    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = format;
    image->extent = VkExtent3D{extent.width, extent.height, 1};
    image->mipLevels = numMips;
    image->arrayLayers = 6; // 6 faces
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = usage;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;     // new in vsg?
    image->flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // cube map flag

    // Image view
    //imageView = vsg::createImageView(context, image, VK_IMAGE_ASPECT_COLOR_BIT);
    //imageView->viewType = VK_IMAGE_VIEW_TYPE_CUBE; // cube
    //imageView->format = format;
    //imageView->subresourceRange = {};
    //imageView->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //imageView->subresourceRange.levelCount = numMips;
    //imageView->subresourceRange.layerCount = 6;
    //imageView->image = image;
    
    // !! vsg::createImageView 用了 ImageView::create(image, aspect)，compile()之后再指定viewType就没用了。
    // 调用前手动修改ImageView的type

    auto aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    vsg::Device* device = context.device;

    image->compile(device);
    // get memory requirements
    VkMemoryRequirements memRequirements = image->getMemoryRequirements(device->deviceID);
    // allocate memory with out export memory info extension
    auto [deviceMemory, offset] = context.deviceMemoryBufferPools->reserveMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    image->bind(deviceMemory, offset);

    imageView = ImageView::create(image, aspectFlags);
    imageView->viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    imageView->format = format;
    imageView->subresourceRange = {};
    imageView->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageView->subresourceRange.levelCount = numMips;
    imageView->subresourceRange.layerCount = 6;    
    imageView->compile(device);
}

// createSampler: 创建 2D 纹理采样器 (用于 BRDF LUT 等 2D 纹理)
// 线性过滤 + CLAMP_TO_EDGE 寻址, maxLod = numMips (控制 mipmap 采样范围)
void createSampler(uint32_t numMips, ptr<vsg::Sampler>& sampler)
{
    sampler = vsg::Sampler::create();
    sampler->magFilter = VK_FILTER_LINEAR;
    sampler->minFilter = VK_FILTER_LINEAR;
    sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->minLod = 0.0f;
    sampler->maxLod = static_cast<float>(numMips); // mip lods
    sampler->borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler->flags = 0; // new in vsg
}

// createSamplerCube: 创建 cubemap 采样器 (用于 envmap/irradiance/prefilter cubemap)
// 与 createSampler 几乎相同, 区别在于供 cubemap ImageView 使用
void createSamplerCube(uint32_t numMips, ptr<vsg::Sampler>& sampler)
{
    sampler = vsg::Sampler::create();
    sampler->magFilter = VK_FILTER_LINEAR;
    sampler->minFilter = VK_FILTER_LINEAR;
    sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->minLod = 0.0f;
    sampler->maxLod = static_cast<float>(numMips); // mip lods
    sampler->borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
}

// createImageInfo: 创建 ImageInfo (将 ImageView + layout + Sampler 打包在一起)
// ImageInfo 是 VSG 中 descriptor 绑定的最小单位, shader 通过它采样纹理
void createImageInfo(const ptr<vsg::ImageView> imageView, VkImageLayout layout, const ptr<vsg::Sampler> sampler, ptr<vsg::ImageInfo> &imageInfo)
{
    imageInfo = vsg::ImageInfo::create();
    imageInfo->imageView = imageView;
    imageInfo->imageLayout = layout;
    imageInfo->sampler = sampler;
}

// createRTTRenderPass: 创建 Render-to-Texture (RTT) 渲染通道
// 用于离线 GPU 渲染 (BRDF LUT / envmap / irradiance / prefilter 生成)
// 只有颜色附件, 无深度附件; 通过 subpass dependency 控制 layout 转换
// VSG RenderPass 概念: Vulkan RenderPass 封装, 描述渲染所需的附件和子通道
void createRTTRenderPass(ptr<vsg::Context> context, VkFormat format, VkImageLayout finalLayout, ptr<vsg::RenderPass>& renderPass)
{
    vsg::AttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = finalLayout;
    // attachment descriptions
    vsg::RenderPass::Attachments attachments{attDesc};
    vsg::AttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    vsg::SubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachments.emplace_back(colorReference);
    vsg::RenderPass::Subpasses subpasses = {subpassDescription};

    // Use subpass dependencies for layout transitions
    vsg::RenderPass::Dependencies dependencies(2);
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    renderPass = vsg::RenderPass::create(context->device.get(), attachments, subpasses, dependencies);
}

// createImageMemoryBarrier: 创建图像布局转换的内存屏障 (Image Memory Barrier)
// 在 Vulkan 中, 切换图像 layout (如 TRANSFER_DST -> SHADER_READ_ONLY) 需要插入 barrier
// 以确保之前的写操作完成, 后续的读操作能看到正确的数据
// srcAccessMask: 旧 layout 下需要完成的访问类型
// dstAccessMask: 新 layout 下将要进行的访问类型
ptr<vsg::ImageMemoryBarrier> createImageMemoryBarrier(//Layout转换图像缓冲区(关于图像的内存屏障)
    ptr<vsg::Image> image,
    VkImageSubresourceRange subresourceRange,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout)
{
    VkAccessFlags srcAccessMask, dstAccessMask;

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (oldImageLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // Image layout is undefined (or does not matter)
        // Only valid as initial layout
        // No flags required, listed only for completeness
        srcAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        // Image is preinitialized
        // Only valid as initial layout for linear images, preserves memory contents
        // Make sure host writes have been finished
        srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image is a color attachment
        // Make sure any writes to the color buffer have been finished
        srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image is a depth/stencil attachment
        // Make sure any writes to the depth/stencil buffer have been finished
        srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image is a transfer source
        // Make sure any reads from the image have been finished
        srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image is a transfer destination
        // Make sure any writes to the image have been finished
        srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image is read by a shader
        // Make sure any shader reads from the image have been finished
        srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (newImageLayout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image will be used as a transfer destination
        // Make sure any writes to the image have been finished
        dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image will be used as a transfer source
        // Make sure any reads from the image have been finished
        dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image will be used as a color attachment
        // Make sure any writes to the color buffer have been finished
        dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image layout will be used as a depth/stencil attachment
        // Make sure any writes to depth/stencil buffer have been finished
        dstAccessMask = dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image will be read in a shader (sampler, input attachment)
        // Make sure any writes to the image have been finished
        if (srcAccessMask == 0)
        {
            srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    return ImageMemoryBarrier::create(srcAccessMask, dstAccessMask, oldImageLayout, newImageLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, subresourceRange);
}

// createImageLayoutPipelineBarrier: 创建包含图像 layout 转换的管线屏障 (Pipeline Barrier)
// PipelineBarrier 封装了 vkCmdPipelineBarrier, 用于 GPU 命令流中的同步
ptr<vsg::PipelineBarrier> createImageLayoutPipelineBarrier(//Layout转换管线屏障
    ptr<vsg::Image> image,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout,
    VkImageSubresourceRange subresourceRange,
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
    auto imageMemoryBarrier = createImageMemoryBarrier(image, subresourceRange, oldImageLayout, newImageLayout);
    return vsg::PipelineBarrier::create(srcStageMask, dstStageMask, VkDependencyFlags(), imageMemoryBarrier);
}

ptr<vsg::PipelineBarrier> createImageLayoutPipelineBarrier(
    ptr<vsg::Image> image,
    VkImageAspectFlags aspectMask,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout,
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = aspectMask;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;
    return createImageLayoutPipelineBarrier(image, oldImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);
}

// createResources: IBL 系统主初始化函数, 创建所有 GPU 纹理资源
//
// 创建的纹理资源:
//   - envmapCube:      环境贴图 cubemap (equirectangular 转换后的结果)
//   - testMap[]:       按 HDR 图片编号索引的环境贴图 cubemap 集合
//   - irraMap[]:       按 HDR 图片编号索引的辐照度 cubemap 集合
//   - prefMap[]:       按 HDR 图片编号索引的预滤波 cubemap 集合
//   - brdfLut:         BRDF 积分查找表 (2D, 512x512, R16G16_SFLOAT)
//   - irradianceCube:  辐照度 cubemap (64x64, 用于漫反射 IBL)
//   - prefilterCube:   预滤波 cubemap (512x512, 用于镜面反射 IBL)
//   - gSkyboxCube:     天空盒立方体几何数据 (24 顶点 + 36 索引)
//   - params:          传递给 shader 的参数 buffer
//
// hdr_image_max_num: 支持的 HDR 图片最大数量 (从 1 开始编号)
void createResources(VsgContext& vsgContext, int hdr_image_max_num)
{
    auto& context = vsgContext.context;
    auto& viewer = vsgContext.viewer;

    // ===================== GPU 同步事件初始化 =====================
    // VkEvent: Vulkan 事件, 用于 GPU 命令流之间的同步
    // envmapCubeGeneratedEvent: envmap cubemap 生成完成事件
    // envmapCubeRenderedEvent: envmap cubemap 渲染完成事件 (未使用)
    gVkEvents.envmapCubeGeneratedEvent = Event::create(context->device);
    gVkEvents.envmapCubeRenderedEvent = Event::create(context->device);

    // ===================== 主环境贴图 cubemap 创建 =====================
    // envmapCube: 从 equirectangular HDR 转换而来的 cubemap
    // 用于天空盒渲染和后续的 irradiance/prefilter 生成
    {
        // createImageCube: 创建 cubemap 图像 (6 面, 支持 mipmap)
        // 参数: context, format, usage, extent, numMips, image, imageView
        createImageCube(*context, 
            Constants::EnvmapCube::format, 
            // VK_IMAGE_USAGE_SAMPLED_BIT: 可被 shader 采样
            // VK_IMAGE_USAGE_TRANSFER_DST_BIT: 可作为传输目标 (用于 mipmap 生成)
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // DST for mipmap generation
            Constants::EnvmapCube::extent,
            Constants::EnvmapCube::numMips, 
            textures.envmapCube, 
            textures.envmapCubeView
        );
        
        // createSamplerCube: 创建 cubemap 采样器
        // 线性过滤 + CLAMP_TO_EDGE 寻址模式
        createSamplerCube(
            Constants::EnvmapCube::numMips, // 1 for no mips
            textures.envmapCubeSmapler
        );

        // createImageInfo: 打包 ImageView + Sampler + Layout
        // 供 descriptor set 绑定使用
        createImageInfo(
            textures.envmapCubeView, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            textures.envmapCubeSmapler, 
            textures.envmapCubeInfo
        );
    }

    // ===================== HDR 图片集合 cubemap 创建 =====================
    // testMap/irraMap/prefMap: 按 HDR 图片编号索引的 cubemap 集合
    // 支持多 HDR 环境贴图切换 (从 1 开始编号)
    for(int i = 1; i <= hdr_image_max_num; i++){
        // --- testMap: 环境贴图 cubemap ---
        {
            _ImageLine tempLine;
            createImageCube(*context, 
                Constants::EnvmapCube::format, 
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // DST for mipmap generation
                Constants::EnvmapCube::extent,
                Constants::EnvmapCube::numMips, 
                tempLine.cube, 
                tempLine.cubeView
            );
            createSamplerCube(
                Constants::EnvmapCube::numMips, // 1 for no mips
                tempLine.cubeSmapler
            );

            createImageInfo(
                tempLine.cubeView, 
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
                tempLine.cubeSmapler, 
                tempLine.cubeInfo
            );

            textures.testMap.insert({i, tempLine});
        }

        // --- irraMap: 辐照度 cubemap (漫反射 IBL) ---
        {
            _ImageLine tempLine;
            createImageCube(*context, 
                Constants::IrradianceCube::format, 
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                Constants::IrradianceCube::extent, 
                Constants::IrradianceCube::numMips, 
                tempLine.cube, 
                tempLine.cubeView
            );
            createSamplerCube(
                Constants::IrradianceCube::numMips, // 7
                tempLine.cubeSmapler
            );
            createImageInfo(
                tempLine.cubeView, 
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                tempLine.cubeSmapler, 
                tempLine.cubeInfo
            );

            textures.irraMap.insert({i, tempLine});
        }

        // --- prefMap: 预滤波 cubemap (镜面反射 IBL) ---
        {
            _ImageLine tempLine;
            createImageCube(*context,
                Constants::PrefilteredEnvmapCube::format,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                Constants::PrefilteredEnvmapCube::extent,
                Constants::PrefilteredEnvmapCube::numMips,
                tempLine.cube, 
                tempLine.cubeView
            );
            createSamplerCube(
                Constants::PrefilteredEnvmapCube::numMips, // 10
                tempLine.cubeSmapler
            );
            createImageInfo(
                tempLine.cubeView, 
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                tempLine.cubeSmapler, 
                tempLine.cubeInfo
            );

            textures.prefMap.insert({i, tempLine});
        }
    }

    // ===================== BRDF LUT 创建 =====================
    // BRDF LUT: BRDF 积分查找表, Split-Sum 近似的一部分
    // 纹理格式: R16G16_SFLOAT (512x512)
    // R 通道: scale (镜面反射缩放因子)
    // G 通道: bias  (镜面反射偏移因子)
    // 仅依赖 roughness 和 NdotV, 一次生成永久复用
    {
        createImage2D(*context, 
            Constants::BrdfLUT::format, 
            // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT: 作为渲染目标 (离线生成)
            // VK_IMAGE_USAGE_SAMPLED_BIT: 可被 shader 采样
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            Constants::BrdfLUT::extent, 
            textures.brdfLut, 
            textures.brdfLutView);
        createSampler(1, 
            textures.brdfLutSampler);

        createImageInfo(
            textures.brdfLutView, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            textures.brdfLutSampler, 
            textures.brdfLutInfo
        );
    }
    
    // ===================== 主辐照度 cubemap 创建 =====================
    // irradianceCube: 辐照度 cubemap (64x64, 7 级 mipmap)
    // 用于漫反射 IBL (Diffuse Indirect Lighting)
    // 漫反射光照变化平缓, 低分辨率即可
    {
        createImageCube(*context, 
            Constants::IrradianceCube::format, 
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            Constants::IrradianceCube::extent, 
            Constants::IrradianceCube::numMips, 
            textures.irradianceCube, 
            textures.irradianceCubeView
        );
        createSamplerCube(
            Constants::IrradianceCube::numMips, // 7
            textures.irradianceCubeSampler
        );
        createImageInfo(
            textures.irradianceCubeView, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            textures.irradianceCubeSampler, 
            textures.irradianceCubeInfo
        );
    }

    // ===================== 主预滤波 cubemap 创建 =====================
    // prefilterCube: 预滤波 cubemap (512x512, 10 级 mipmap)
    // 用于镜面反射 IBL (Specular Indirect Lighting)
    // 每个 mip level 对应一个 roughness 级别 (mip 0 = 光滑, mip 9 = 粗糙)
    {
        createImageCube(*context,
            Constants::PrefilteredEnvmapCube::format,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            Constants::PrefilteredEnvmapCube::extent,
            Constants::PrefilteredEnvmapCube::numMips,
            textures.prefilterCube,
            textures.prefilterCubeView);
        createSamplerCube(
            Constants::PrefilteredEnvmapCube::numMips, // 10
            textures.prefilterCubeSampler);
        createImageInfo(
            textures.prefilterCubeView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            textures.prefilterCubeSampler,
            textures.prefilterCubeInfo);
    }

    // ===================== 天空盒立方体几何数据创建 =====================
    // gSkyboxCube: 天空盒立方体的几何数据
    // 6 面 x 4 顶点 = 24 个顶点, 每面 2 个三角形 = 36 个索引
    // 顶点顺序: Right, Left, Front, Back, Bottom, Top
    // 用于 irradiance/prefilter 生成时的天空盒渲染
    gSkyboxCube.vertices = vsg::vec3Array::create({

        //---------------------------------------------------
        
        // Right
        {1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, 1.0f},
        {1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, 1.0f},

        // Left
        {-1.0f, -1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, 1.0f},

        // Front
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, 1.0},

        // Back
        {-1.0f, -1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, 1.0f},

        // Bottom
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},

        // Top
        {-1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, -1.0f},
        {-1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f}}
    );

    // ===================== 天空盒索引数据创建 =====================
    // indices: 索引数组, 定义 6 个面的三角形绘制顺序
    // 每面 2 个三角形, 共 36 个索引
    // 顶点顺序: Right(0-3), Left(4-7), Front(8-11), Back(12-15), Bottom(16-19), Top(20-23)
    gSkyboxCube.indices = vsg::ushortArray::create({
        // Back (+Z 方向, 观察者背对)
        0, 2, 1,
        1, 2, 3,

        // Front (-Z 方向, 观察者面向)
        6, 4, 5,
        7, 6, 5,

        // Left (-X 方向)
        10, 8, 9,
        11, 10, 9,

        // Right (+X 方向)
        14, 13, 12,
        15, 13, 14,

        // Bottom (-Y 方向, 地面)
        17, 16, 19,
        19, 16, 18,

        // Top (+Y 方向, 天花板)
        23, 20, 21,
        22, 20, 23}
    );

    // ===================== IBL 参数缓冲区创建 =====================
    // textures.params: 传递给 shader 的参数 buffer
    // 用于运行时动态调整 IBL 效果 (如环境光强度)
    // DYNAMIC_DATA_TRANSFER_AFTER_RECORD: 标记数据在记录后传输
    textures.params = vec4Array::create(1, vec4(0, 0, 0, 1.0f));
    textures.params->properties.dataVariance = vsg::DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    textures.paramsInfo = BufferInfo::create(textures.params.get());
}

// clearResources: 清理 IBL 系统的所有 GPU 资源
// 重置全局状态 (textures, appData, events, skybox geometry, shaderSets)
// 资源释放策略: 注释掉的代码保留原始引用, 避免野指针
void clearResources()
{

    // ===================== 重置应用程序数据 =====================
    appData = {};

    // ===================== 重置纹理资源 =====================
    // {}: 使用 VSG 的默认构造函数, 自动释放 ref_ptr
    textures = {};
    gVkEvents = {};
    gSkyboxCube = {};
    gEnvmapRect = {};
    
    shaderSets.clear();
}

// LoadHdrImageSTBI: 使用 stb_image 库加载 HDR (.hdr) 图像
// VSG 没有内置的 HDR 图像加载支持, 所以用 stbi_loadf 读取 32 位浮点 HDR 数据
// 通过 VSG Visitor 模式将数据填充到不同类型的 2D 数组 (floatArray2D / vec3Array2D / vec4Array2D)
class LoadHdrImageSTBI : public vsg::Visitor
{
private:
    std::string mpFilepath;
    float* mpData;
    uint32_t mpWidth, mpHeight, mpNumChannels;

    template<class A>
    void update(A& image)
    {
        float* pData = mpData;
        if (pData == nullptr)
        {
            std::cerr << "Image not loaded, use LoadHdriImageSTBI::readImage() first" << std::endl;
            return;
        }

        using value_type = typename A::value_type;

        for (size_t r = 0; r < image.height(); ++r)
        {
            value_type* ptr = &image.at(0, r);
            for (size_t c = 0; c < image.width(); ++c)
            {

                // floatArray2D
                if constexpr (std::is_same_v<value_type, float>)
                {
                    (*ptr) = pData[0];
                }
                // vec3Array2D or vec4Array2D
                else
                {
                    ptr->r = pData[0];
                    ptr->g = pData[1];
                    ptr->b = pData[2];
                    // vec4Array2D
                    if constexpr (std::is_same_v<value_type, vsg::vec4>) ptr->a = 1.0f;
                }
                ++ptr;
                pData += mpNumChannels;
            }
        }
        image.dirty();
    }

public:
   
    LoadHdrImageSTBI() :
        mpFilepath(""), mpData(nullptr), mpWidth(0), mpHeight(0), mpNumChannels(0){}

    LoadHdrImageSTBI(const std::string& filepath) :
        mpFilepath(filepath), mpData(nullptr), mpWidth(0), mpHeight(0), mpNumChannels(0) {}

    ~LoadHdrImageSTBI() {
        freeImage();
    }

    // use the vsg::Visitor to safely cast to types handled by the UpdateImage class
    void apply(vsg::floatArray2D& image) override { update(image); }
    void apply(vsg::vec3Array2D& image) override { update(image); }
    void apply(vsg::vec4Array2D& image) override { update(image); }

    void updateBuffer(vsg::Data *image) {
        image->accept(*this);
    }

    void readImage(const std::string& filepath)
    {
        this->mpFilepath = filepath;
        //stbi_set_flip_vertically_on_load(true);
        mpData = stbi_loadf(filepath.c_str(), (int*)&mpWidth, (int*)&mpHeight, (int*)&mpNumChannels, 3);
    }

    vsg::ivec3 getDimensions() {
        return vsg::ivec3(mpWidth, mpHeight, mpNumChannels);
    }

    void freeImage() 
    {
        if (mpData != nullptr)
        {
            stbi_image_free(mpData);
            mpData = nullptr;
        }
    }
};

// loadEnvmapRect: 加载 equirectangular HDR 图像到 gEnvmapRect
// 使用 stb_image (stbi_loadf) 读取 .hdr 文件, 结果存入 gEnvmapRect.image (vec4Array2D)
// equirectangular (等距矩形) 是一种将球面映射到矩形的投影方式, 常用于 HDR 环境贴图
void loadEnvmapRect(VsgContext& context, const std::string& filePath)
{
    //auto evnmapFilepath = vsg::findFile("textures/test_park.hdr", appData.options->paths);
    // auto evnmapFilepath = vsg::findFile(filePath, appData.options->paths);
    LoadHdrImageSTBI loader;
    loader.readImage(filePath);
    auto dimensions = loader.getDimensions();
    gEnvmapRect.width = dimensions.x;
    gEnvmapRect.height = dimensions.y;
    gEnvmapRect.image = vsg::vec4Array2D::create(dimensions.x, dimensions.y);
    gEnvmapRect.image->properties.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    loader.updateBuffer(gEnvmapRect.image);

    gEnvmapRect.sampler = Sampler::create();
    gEnvmapRect.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    gEnvmapRect.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    gEnvmapRect.imageInfo = ImageInfo::create(gEnvmapRect.sampler, gEnvmapRect.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// loadHdrFile: 加载 HDR 文件, 返回浮点像素数据指针 (调用方负责释放)
float *loadHdrFile(const std::string& filepath, int& width, int& height, int& channels)
{
    float* data = stbi_loadf(filepath.c_str(), &width, &height, &channels, 4);

    return data;
}

// generateBRDFLUT: 生成 BRDF (Bidirectional Reflectance Distribution Function) 积分查找表
//
// BRDF LUT 是 Split-Sum 近似方法的一部分:
//   - 纹理格式: R16G16_SFLOAT (512x512)
//   - R 通道: scale (镜面反射的缩放因子)
//   - G 通道: bias  (镜面反射的偏移因子)
//   - 仅依赖 roughness (纵轴) 和 NdotV (横轴), 与环境无关, 一次生成永久复用
//
// 使用 fullscreen quad 渲染 (无顶点输入, 3 个顶点由 vertex shader 硬编码)
// 无需 descriptor set, 无需 push constant, 最简单的离线渲染
void generateBRDFLUT(VsgContext &vsgContext)
{
    // TODO: actually create shaders
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreenquad.vert", appData.options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/genbrdflut.frag", appData.options->paths);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create fullscreenguad.vert genbrdflut.frag shaders." << std::endl;
        return;
    }

    ptr<vsg::Context>& context = vsgContext.context;

    //auto tStart = std::chrono::high_resolution_clock::now();
    const VkFormat format = VK_FORMAT_R16G16_SFLOAT; // R16G16 is supported pretty much everywhere
    const int32_t dim = 512;
    const VkExtent3D extent3d = {dim, dim, 1};
    const VkExtent2D extent = {dim, dim};

    ptr<vsg::Image>& lutImage = textures.brdfLut;
    ptr<vsg::ImageView>& lutImageView = textures.brdfLutView;
    createImage2D(*context, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        extent, lutImage, lutImageView);
    ptr<vsg::Sampler>& lutSampler = textures.brdfLutSampler;
    createSampler(1, lutSampler);

    ptr<vsg::ImageInfo>& lutImageInfo = textures.brdfLutInfo;
    createImageInfo(lutImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, lutSampler, lutImageInfo);

    ptr<vsg::RenderPass> renderPass;
    // attachments / renderPass (subpass for layout trainsition)
    createRTTRenderPass(context, format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, renderPass);
    // frame buffer
    auto fbuf = vsg::Framebuffer::create(renderPass, vsg::ImageViews{lutImageView}, dim, dim, 1);

    auto rtt_rendergraph = vsg::RenderGraph::create();
    rtt_rendergraph->renderArea.offset = VkOffset2D{0, 0};
    rtt_rendergraph->renderArea.extent = extent;
    rtt_rendergraph->clearValues = {{0.0f, 0.0f, 0.0f, 1.0f}};
    rtt_rendergraph->framebuffer = fbuf;

    // create RenderGraph

    // pushConstantRange: empty
    vsg::PushConstantRanges pushConstantRanges{};
    // pipelineLayout
    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{}, vsg::PushConstantRanges{});

    // shader and pipeline object
    const vsg::ShaderStages shaders{vertexShader, fragmentShader};
    auto vertexInputState = vsg::VertexInputState::create(); // empty for no input
    auto inputAssemblyState = vsg::InputAssemblyState::create();
    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyState->primitiveRestartEnable = VK_FALSE;
    auto rasterState = vsg::RasterizationState::create();
    rasterState->cullMode = VK_CULL_MODE_NONE;
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorWriteMask = 0xf;
    auto blendState = vsg::ColorBlendState::create(vsg::ColorBlendState::ColorBlendAttachments{colorBlendAttachment});
    auto multisampleState = vsg::MultisampleState::create(VK_SAMPLE_COUNT_1_BIT);
    auto depthState = vsg::DepthStencilState::create();
    depthState->depthTestEnable = VK_FALSE;
    depthState->depthWriteEnable = VK_FALSE;
    auto pipelineStates = vsg::GraphicsPipelineStates{
        vertexInputState,
        inputAssemblyState,
        rasterState,
        multisampleState,
        blendState,
        depthState};

    auto pipeline = vsg::GraphicsPipeline::create(pipelineLayout.get(), shaders, pipelineStates, /*subpass=*/0);
    // the acutall vkCmdBindPipeline command
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(pipeline);
    auto pipelineNode = vsg::StateGroup::create();
    pipelineNode->add(bindGraphicsPipeline);
    //auto bindDescriptor = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, descriptorSet);
    //pipelineNode->add(bindDescriptor);
    auto draw = vsg::Draw::create(3, 1, 0, 0);
    pipelineNode->addChild(draw);

    // 应该做一个Dummy Scene中，进行对应的Graphic States管理，加入Descriptor Pipeline绑定和全屏Quad几何绑定
    // Scene节点挂在rtt_RenderGraph下面, like this
    // literally vsg::createRenderGraphForView() below
    auto dummyCamera = vsg::Camera::create(); // or reuse camera from main render loop.
    auto rtt_view = vsg::View::create(dummyCamera, pipelineNode);
    //rtt_rendergraph->addChild(rtt_view);
    rtt_rendergraph->addChild(pipelineNode);

    // 先创建各种贴图和RenderPass （rtt_rendergraph），然后创建Descriptor和（rtt_view/dummyScene）
    auto commandGraph = vsg::CommandGraph::create(vsgContext.device, vsgContext.queueFamily); // done with
    //rtt_commandGraph->submitOrder = -1; // render before the main_commandGraph, or nest in main_commandGraph
    commandGraph->addChild(rtt_rendergraph);

    //vsg::write(rtt_rendergraph, appData.debugOutputPath);
    auto& viewer = vsgContext.viewer;
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
}

// generateEnvmap: 将 equirectangular HDR 图像转换为 cubemap
//
// 处理流程:
//   1. 加载 equirectangular HDR 图像 (loadEnvmapRect, 使用 stb_image)
//   2. 创建离线 framebuffer (512x512)
//   3. 对 6 个面各渲染一次 fullscreen quad, 将 equirectangular 映射到每个面
//   4. 将 framebuffer 拷贝到 cubemap 对应面, 然后 blit 生成 mipmap
//   5. 设置 VkEvent 信号, 通知后续的 irradiance/prefilter 生成可以开始
//
// hdr 参数: -1 表示主 envmap, 其他值表示 HDR 图片编号 (存入 testMap)
// shader: fullscreenquad.vert + equirect2cube.frag
void generateEnvmap(VsgContext& vsgContext, std::string& envmapFilepath, int hdr)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreenquad.vert", appData.options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/equirect2cube.frag", appData.options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create fullscreenguad equirect2cude shaders." << std::endl;
        return;
    }

    loadEnvmapRect(vsgContext, envmapFilepath);
    auto& context = vsgContext.context;
    auto& viewer = vsgContext.viewer;

    //const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
    //const int32_t dim = 512;
    //const VkExtent2D extent = {dim, dim};

    ptr<vsg::RenderPass> renderPass;
    createRTTRenderPass(context, Constants::EnvmapCube::format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, renderPass);

    // offscreen frame buffer (single 2D framebuffer reused for 6 faces)
    ptr<vsg::Image> pFBImage;
    ptr<vsg::ImageView> pFBImageView;
    auto fbUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createImage2D(*context, Constants::EnvmapCube::format, fbUsageFlags, Constants::EnvmapCube::extent, pFBImage, pFBImageView);
    pFBImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    pFBImageView->subresourceRange.baseMipLevel = 0;
    pFBImageView->subresourceRange.baseArrayLayer = 0;

    auto offscreenFB = vsg::Framebuffer::create(renderPass, vsg::ImageViews{pFBImageView}, Constants::EnvmapCube::dim, Constants::EnvmapCube::dim, 1);

     // Create render graph
    // DescriptorSetLayout
    vsg::DescriptorSetLayoutBindings descriptorSetLayoutBindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorSetLayoutBindings);
    // And actual Descriptor for cubemap texture
    auto envmapRectDescriptor = vsg::DescriptorImage::create(gEnvmapRect.imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{envmapRectDescriptor});

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128 + 4}};

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);

    const vsg::ShaderStages shaders{vertexShader, fragmentShader};
    
    //vsg::VertexInputState::Bindings vertexBindingsDescriptions{
    //    VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}};

    //vsg::VertexInputState::Attributes vertexAttributeDescriptions{
    //    VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}};

    //auto vertexInputState = vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions);
    auto vertexInputState = vsg::VertexInputState::create();
    auto inputAssemblyState = vsg::InputAssemblyState::create();
    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyState->primitiveRestartEnable = VK_FALSE;
    auto rasterState = vsg::RasterizationState::create();
    rasterState->cullMode = VK_CULL_MODE_NONE; // none to render cube inner face
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorWriteMask = 0xf;
    auto blendState = vsg::ColorBlendState::create(vsg::ColorBlendState::ColorBlendAttachments{colorBlendAttachment});
    auto multisampleState = vsg::MultisampleState::create(VK_SAMPLE_COUNT_1_BIT);
    auto depthState = vsg::DepthStencilState::create();
    depthState->depthTestEnable = VK_FALSE;
    depthState->depthWriteEnable = VK_FALSE;
    auto pipelineStates = vsg::GraphicsPipelineStates{
        vertexInputState,
        inputAssemblyState,
        rasterState,
        multisampleState,
        blendState,
        depthState};
    auto pipeline = vsg::GraphicsPipeline::create(pipelineLayout.get(), shaders, pipelineStates, /*subpass=*/0);

    // CommandGraph to hold the different RenderGraphs used to render each view
    auto commandGraph = vsg::CommandGraph::create(vsgContext.device, vsgContext.queueFamily);
    VkImageSubresourceRange fbSubresRange = {};
    fbSubresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    fbSubresRange.baseArrayLayer = 0;
    fbSubresRange.baseMipLevel = 0;
    fbSubresRange.layerCount = 1;
    fbSubresRange.levelCount = 0;
    VkImageSubresourceLayers fbSubresLayers = {};
    fbSubresLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    fbSubresLayers.baseArrayLayer = 0;
    fbSubresLayers.layerCount = 1;
    fbSubresLayers.mipLevel = 0;
    VkOffset3D fbSubresourceBound = {Constants::EnvmapCube::dim, Constants::EnvmapCube::dim, 1};

    // set envmapCube (all mips) to TRANSFER_DST before every render pass
    VkImageSubresourceRange cubeAllMipSubresRange = {};
    cubeAllMipSubresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cubeAllMipSubresRange.baseArrayLayer = 0;
    cubeAllMipSubresRange.baseMipLevel = 0;
    cubeAllMipSubresRange.layerCount = 6;
    cubeAllMipSubresRange.levelCount = Constants::EnvmapCube::numMips;
    IBL::ptr<vsg::PipelineBarrier> setCubeLayoutTransferDst;
    if(hdr==-1){
        setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.envmapCube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cubeAllMipSubresRange);
    }
    else{
        setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.testMap.at(hdr).cube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cubeAllMipSubresRange);
    }
    commandGraph->addChild(setCubeLayoutTransferDst);
  
    //auto projMatValue = vsg::mat4Value::create(vsg::perspective((M_PI / 2.0), 1.0, 0.1, 512.0));
    //std::vector<ptr<vsg::mat4Value>> viewMatValues= {
    //    // POSITIVE_X
    //    vsg::mat4Value::create(rotate(radians(90.0f), vec3(0.0f, 1.0f, 0.0f)) * rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f))),
    //    // NEGATIVE_X
    //    vsg::mat4Value::create(rotate(radians(-90.0f), vec3(0.0f, 1.0f, 0.0f)) * rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f))),
    //    // POSITIVE_Y
    //    vsg::mat4Value::create(rotate(radians(-90.0f), vec3(1.0f, 0.0f, 0.0f))),
    //    // NEGATIVE_Y
    //    vsg::mat4Value::create(rotate(radians(90.0f), vec3(1.0f, 0.0f, 0.0f))),
    //    // POSITIVE_Z
    //    vsg::mat4Value::create(rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f))),
    //    // NEGATIVE_Z
    //    vsg::mat4Value::create(rotate(radians(180.0f), vec3(0.0f, 0.0f, 1.0f))),
    //};

    for (uint32_t f = 0; f < 6; f++)
    {
        auto viewportState = vsg::ViewportState::create(0, 0, Constants::EnvmapCube::dim, Constants::EnvmapCube::dim);
        //auto camera = vsg::Camera::create(dummyProjMatrix, dummyViewMatrix, viewportState);
        //auto dummyViewMatrix = vsg::RelativeViewMatrix(matrices[f], )
        auto camera = vsg::Camera::create();

        // bind & draw by vsgScene traversal
        auto bindStates = vsg::StateGroup::create();
        bindStates->add(vsg::BindGraphicsPipeline::create(pipeline));
        bindStates->add(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, descriptorSet));
        auto faceIdx = vsg::uintValue::create(f);
        // sit behind two matrix4x4 pushed by vsg::Camera here
        //bindStates->add(PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, projMatValue));
        //bindStates->add(PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 64, viewMatValues[f]));
        vsg::ref_ptr<vsg::mat4Array> newmatrix = vsg::mat4Array::create(2);
        vsg::ref_ptr<vsg::Camera> tmp_camera = vsg::Camera::create();
        newmatrix->set(0, (vsg::mat4)tmp_camera->projectionMatrix->transform());
        newmatrix->set(1, (vsg::mat4)tmp_camera->viewMatrix->transform());
        vsg::ref_ptr<vsg::PushConstants> pc = vsg::PushConstants::create(
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, newmatrix);
        bindStates->add(pc);

        bindStates->add(PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 128, faceIdx));
        // fullscreen quad
        auto drawFullscreenQuad = vsg::Draw::create(3, 1, 0, 0);
        bindStates->addChild(drawFullscreenQuad);
        //bindStates->addChild(BindVertexBuffers::create(0, vsg::DataList{gSkyboxCube.vertices}));
        //bindStates->addChild(BindIndexBuffer::create(gSkyboxCube.indices));
        //bindStates->addChild(DrawIndexed::create(gSkyboxCube.indices->size(), 1, 0, 0, 0));

        // dummy camera for MVP push constants, dummyScene for bind & draw
        auto view = vsg::View::create(camera, bindStates);
        // warp begin/end renderpass to every bind & draw
        auto rendergraph = vsg::RenderGraph::create();
        rendergraph->renderArea.offset = VkOffset2D{0, 0};
        rendergraph->renderArea.extent = Constants::EnvmapCube::extent;
        rendergraph->clearValues = {{0.0f, 0.0f, 0.0f, 1.0f}};
        rendergraph->framebuffer = offscreenFB;
        rendergraph->addChild(view);

        // renderpass to offscreen framebuffer
        commandGraph->addChild(rendergraph);

        // setup barrier and copy framebuffer to cubemap face.
        // renderpass之后，fb的layout为VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL，需要再转换到VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        auto setFBLayoutTransferSrc = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        commandGraph->addChild(setFBLayoutTransferSrc);

        auto copyFBToCubeFace = vsg::CopyImage::create();
        VkImageCopy copyRegion = {};
        copyRegion.srcSubresource = fbSubresLayers;
        copyRegion.srcOffset = {0, 0, 0};
        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.baseArrayLayer = f;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.dstSubresource.mipLevel = 0;
        copyRegion.dstOffset = {0, 0, 0};
        copyRegion.extent.width = static_cast<uint32_t>(Constants::EnvmapCube::dim);
        copyRegion.extent.height = static_cast<uint32_t>(Constants::EnvmapCube::dim);
        copyRegion.extent.depth = 1;
        copyFBToCubeFace->regions = {copyRegion};
        copyFBToCubeFace->srcImage = pFBImage;
        copyFBToCubeFace->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        if(hdr==-1){
            copyFBToCubeFace->dstImage = textures.envmapCube;
        }
        else{
            copyFBToCubeFace->dstImage = textures.testMap.at(hdr).cube;
        }
        copyFBToCubeFace->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        commandGraph->addChild(copyFBToCubeFace);

        // blit to higher mips
        for(uint32_t targetMipLevel = 1; targetMipLevel < Constants::EnvmapCube::numMips; targetMipLevel++) 
        {
            int32_t mipDim = (int32_t) Constants::EnvmapCube::dim >> targetMipLevel;
            VkImageBlit blitRegion = {};
            blitRegion.srcSubresource = fbSubresLayers;
            blitRegion.srcOffsets[1] = fbSubresourceBound;
            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.baseArrayLayer = f;
            blitRegion.dstSubresource.layerCount = 1;
            blitRegion.dstSubresource.mipLevel = targetMipLevel;
            blitRegion.dstOffsets[1] = {mipDim, mipDim, 1};
            
            auto blitFBToCubeFaceMip = vsg::BlitImage::create();
            blitFBToCubeFaceMip->regions = {blitRegion};
            blitFBToCubeFaceMip->srcImage = pFBImage;
            blitFBToCubeFaceMip->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            if(hdr==-1){
                blitFBToCubeFaceMip->dstImage = textures.envmapCube;
            }
            else{
                blitFBToCubeFaceMip->dstImage = textures.testMap.at(hdr).cube;
            }
            blitFBToCubeFaceMip->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            commandGraph->addChild(blitFBToCubeFaceMip);
        }
        //ptr<PipelineBarrier> setFBLayoutAttachment = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT,
        //    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        //commandGraph->addChild(setFBLayoutAttachment);
    }

    // set for shader use layout after mipmap generaton. & signal event for further lut generation
    auto setEvent = vsg::SetEvent::create(gVkEvents.envmapCubeGeneratedEvent, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    commandGraph->addChild(setEvent);

    IBL::ptr<vsg::PipelineBarrier> setCubeLayoutShaderRead;
    if(hdr==-1){
        setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.envmapCube, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cubeAllMipSubresRange);
    }
    else{
        setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.testMap.at(hdr).cube, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cubeAllMipSubresRange);
    
    }
    commandGraph->addChild(setCubeLayoutShaderRead);
    gVkEvents.envmapCubeGeneratedBarrier = setCubeLayoutShaderRead->imageMemoryBarriers.at(0);
    viewer->addRecordAndSubmitTaskAndPresentation({commandGraph});
}

// generateIrradianceCube: 生成漫反射辐照度 cubemap (Irradiance Convolution)
//
// 对环境贴图 cubemap 的每个 texel 方向, 在半球上采样并累加所有入射光
// 输出: 64x64 cubemap (低分辨率即可, 因为漫反射光照变化非常平缓)
//
// 处理流程:
//   1. 等待 envmap 生成完成 (WaitEvents on envmapCubeGeneratedEvent)
//   2. 创建离线 framebuffer, 6 面 x numMips 次渲染
//   3. 每次渲染一个天空盒, shader 中对 envmap 做半球积分
//   4. 将 framebuffer 逐面逐 mip 拷贝到 irradianceCube
//
// shader: skyboxCubegen.vert + irradiancecubeMesh.frag
// hdr 参数: -1 表示主 irradianceCube, 其他值存入 irraMap
void generateIrradianceCube(VsgContext& vsgContext, int hdr)
{
    //auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreenquad.vert", appData.options->paths);
    //auto fragShaderFilepath = vsg::findFile("shaders/IBL/irradianceCube.frag", appData.options->paths);
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/skyboxCubegen.vert", appData.options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/irradiancecubeMesh.frag", appData.options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create skyboxCubegen irradianceCubeMesh shaders." << std::endl;
        return;
    }

    auto& context = vsgContext.context;
    auto& viewer = vsgContext.viewer;

    // CommandGraph to hold the different RenderGraphs used to render each view
    auto commandGraph = vsg::CommandGraph::create(vsgContext.device, vsgContext.queueFamily);

    auto waitEnvmapCubeGenerated = vsg::WaitEvents::create(
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        gVkEvents.envmapCubeGeneratedEvent,
        gVkEvents.envmapCubeGeneratedBarrier);
    commandGraph->addChild(waitEnvmapCubeGenerated);

    ptr<vsg::RenderPass> renderPass;
    createRTTRenderPass(context, Constants::IrradianceCube::format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, renderPass);

    // offscreen frame buffer (single 2D framebuffer reused for 6 faces)
    ptr<vsg::Image> pFBImage;
    ptr<vsg::ImageView> pFBImageView;
    auto fbUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createImage2D(*context, Constants::IrradianceCube::format, fbUsageFlags, Constants::IrradianceCube::extent, pFBImage, pFBImageView);
    pFBImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    pFBImageView->subresourceRange.baseMipLevel = 0;
    pFBImageView->subresourceRange.baseArrayLayer = 0;

    auto offscreenFB = vsg::Framebuffer::create(renderPass, vsg::ImageViews{pFBImageView}, Constants::IrradianceCube::dim, Constants::IrradianceCube::dim, 1);

    // Create render graph
    // DescriptorSetLayout
    vsg::DescriptorSetLayoutBindings descriptorSetLayoutBindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorSetLayoutBindings);
    // And actual Descriptor for cubemap texture
    
    vsg::ref_ptr<vsg::DescriptorImage> envmapRectDescriptor;
    if(hdr==-1){
        envmapRectDescriptor = vsg::DescriptorImage::create(textures.envmapCubeInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }
    else{
        envmapRectDescriptor = vsg::DescriptorImage::create(textures.testMap.at(hdr).cubeInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{envmapRectDescriptor});

    vsg::PushConstantRanges pushConstantRanges = {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128}
    };

    // pipeline layout
    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);

    // create pipeline
    const vsg::ShaderStages shaders{vertexShader, fragmentShader};
    
    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}};

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}};
    auto vertexInputState = vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions);
    auto inputAssemblyState = vsg::InputAssemblyState::create();
    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyState->primitiveRestartEnable = VK_FALSE;
    auto rasterState = vsg::RasterizationState::create();
    rasterState->cullMode = VK_CULL_MODE_NONE; // none to render cube inner face
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorWriteMask = 0xf;
    auto blendState = vsg::ColorBlendState::create(vsg::ColorBlendState::ColorBlendAttachments{colorBlendAttachment});
    auto multisampleState = vsg::MultisampleState::create(VK_SAMPLE_COUNT_1_BIT);
    auto depthState = vsg::DepthStencilState::create();
    depthState->depthTestEnable = VK_FALSE;
    depthState->depthWriteEnable = VK_FALSE;
    auto pipelineStates = vsg::GraphicsPipelineStates{
        vertexInputState,
        inputAssemblyState,
        rasterState,
        multisampleState,
        blendState,
        depthState};
    auto pipeline = vsg::GraphicsPipeline::create(pipelineLayout.get(), shaders, pipelineStates, /*subpass=*/0);
    
    auto setFBLayoutAttachment = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    commandGraph->addChild(setFBLayoutAttachment);

    // set irradianceCube to TRANSFER_DST before every render pass
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = Constants::IrradianceCube::numMips;
    subresourceRange.layerCount = 6;

    IBL::ptr<vsg::PipelineBarrier> setCubeLayoutTransferDst;
    if(hdr==-1){
        setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.irradianceCube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
    }
    else{
        setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.irraMap.at(hdr).cube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
    }
    commandGraph->addChild(setCubeLayoutTransferDst);
    
    //VkViewport viewport = {};    
    //viewport.width = viewport.height = Constants::IrradianceCube::dim;
    //viewport.minDepth = 0.0f;
    //viewport.maxDepth = 1.0f;

    //VkRect2D scissor = {};
    //scissor.extent = Constants::IrradianceCube::extent;
    //scissor.offset = {0, 0};
    //auto setScissor = vsg::SetScissor::create();
    //setScissor->firstScissor = 0;
    //setScissor->scissors = {scissor};
    //commandGraph->addChild(setScissor);


    std::vector<vsg::mat4> viewMats = {
        // POSITIVE_X
        rotate(radians(90.0f), vec3(0.0f, 1.0f, 0.0f)) * rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_X
        rotate(radians(-90.0f), vec3(0.0f, 1.0f, 0.0f)) * rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Y
        rotate(radians(-90.0f), vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Y
        rotate(radians(90.0f), vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Z
        rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Z
        rotate(radians(180.0f), vec3(0.0f, 0.0f, 1.0f)),
    };

    auto dummyProjMatrix = vsg::Perspective::create(vsg::degrees(M_PI/2), 1.0, 0.1, 512.0);
    std::vector<ptr<ViewMatrix>> myViews(6);
    for(int i = 0; i < 6; i++) {
        myViews[i] = MyViewMatrix::create(viewMats[i]);
    }

    for (uint32_t m = 0; m < Constants::IrradianceCube::numMips; m++)
    {
        uint32_t mipDim = Constants::IrradianceCube::dim >> m;
        auto viewportState = vsg::ViewportState::create(0, 0, mipDim, mipDim);
        for (uint32_t f = 0; f < 6; f++)
        {
            auto camera = vsg::Camera::create(dummyProjMatrix, myViews[f], viewportState);
            // bind & draw by vsgScene traversal
            auto bindStates = vsg::StateGroup::create();
            bindStates->add(vsg::BindGraphicsPipeline::create(pipeline));
            bindStates->add(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, descriptorSet));

            // fullscreen quad
            //auto drawFullscreenQuad = vsg::Draw::create(3, 1, 0, 0);
            //bindStates->addChild(drawFullscreenQuad);
            auto drawSkybox = vsg::Group::create();
            drawSkybox->addChild(BindVertexBuffers::create(0, DataList{gSkyboxCube.vertices}));
            drawSkybox->addChild(BindIndexBuffer::create(gSkyboxCube.indices));
            drawSkybox->addChild(DrawIndexed::create(gSkyboxCube.indices->size(), 1, 0, 0, 0));
            bindStates->addChild(drawSkybox);
            auto view = vsg::View::create(camera, bindStates);

            // warp begin/end renderpass to every bind & draw
            auto rendergraph = vsg::RenderGraph::create();
            rendergraph->renderArea.offset = VkOffset2D{0, 0};
            rendergraph->renderArea.extent = VkExtent2D{mipDim, mipDim};
            rendergraph->clearValues = {{0.0f, 0.0f, 0.0f, 1.0f}};
            rendergraph->framebuffer = offscreenFB;
            rendergraph->addChild(view);

            // renderpass to offscreen framebuffer
            commandGraph->addChild(rendergraph);
            // renderpass之后，fb的layout为VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL，需要再转换到VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            auto setFBLayoutTransfeSrc = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            commandGraph->addChild(setFBLayoutTransfeSrc);

            auto copyFBToCubeMap = vsg::CopyImage::create();
            VkImageCopy copyRegion = {};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset = {0, 0, 0};
            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = f;
            copyRegion.dstSubresource.mipLevel = m;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstOffset = {0, 0, 0};
            copyRegion.extent.width = mipDim;
            copyRegion.extent.height = mipDim;
            copyRegion.extent.depth = 1;
            copyFBToCubeMap->regions = {copyRegion};
            copyFBToCubeMap->srcImage = pFBImage;
            copyFBToCubeMap->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            if(hdr==-1){
                copyFBToCubeMap->dstImage = textures.irradianceCube;
            }
            else{
                copyFBToCubeMap->dstImage = textures.irraMap.at(hdr).cube;
            }
            copyFBToCubeMap->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            commandGraph->addChild(copyFBToCubeMap);
            //ptr<PipelineBarrier> setFBLayoutAttachment = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT,
            //                                                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
            commandGraph->addChild(setFBLayoutAttachment);
        }
    }

    IBL::ptr<vsg::PipelineBarrier> setCubeLayoutShaderRead;
    if(hdr==-1){
        setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.irradianceCube, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        subresourceRange);
    }
    else{
        setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.irraMap.at(hdr).cube, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        subresourceRange);
    }
    commandGraph->addChild(setCubeLayoutShaderRead);

    viewer->addRecordAndSubmitTaskAndPresentation({commandGraph});
}

// generatePrefilteredEnvmapCube: 生成镜面反射预滤波环境贴图 (Specular Prefilter)
//
// 使用 GGX 重要性采样 (Importance Sampling) 对环境贴图进行预卷积:
//   - 每个 mip level 对应一个 roughness 级别 (mip 0 = 最光滑, mip 9 = 最粗糙)
//   - 较高的 mip 分辨率较低 (512 -> 256 -> 128 -> ... -> 1)
//   - shader 中根据 roughness 和随机方向采样, 模拟微表面 BRDF 的镜面反射
//
// 处理流程:
//   1. 等待 envmap 生成完成 (WaitEvents)
//   2. 对 6 面 x 10 个 mip 各渲染一次天空盒
//   3. 通过 push constant 传递 numMips, mipLevel, faceIdx 给 fragment shader
//   4. 将 framebuffer 逐面逐 mip 拷贝到 prefilterCube
//
// shader: skyboxCubegen.vert + prefilterenvmapMesh.frag
// hdr 参数: -1 表示主 prefilterCube, 其他值存入 prefMap
void generatePrefilteredEnvmapCube(VsgContext& vsgContext, int hdr)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/skyboxCubegen.vert", appData.options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/prefilterenvmapMesh.frag", appData.options->paths);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create skyboxCubegen prefilterenvmapMesh shaders." << std::endl;
        return;
    }

    auto& context = vsgContext.context;
    auto& viewer = vsgContext.viewer;

    auto commandGraph = vsg::CommandGraph::create(vsgContext.device, vsgContext.queueFamily);

    auto waitEnvmapCubeGenerated = vsg::WaitEvents::create(
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        gVkEvents.envmapCubeGeneratedEvent,
        gVkEvents.envmapCubeGeneratedBarrier);
    commandGraph->addChild(waitEnvmapCubeGenerated);

    ptr<vsg::RenderPass> renderPass;
    createRTTRenderPass(context, Constants::PrefilteredEnvmapCube::format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, renderPass);

    // offscreen frame buffer (single 2D framebuffer reused for 6 faces)
    ptr<vsg::Image> pFBImage;
    ptr<vsg::ImageView> pFBImageView;
    auto fbUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createImage2D(*context, Constants::PrefilteredEnvmapCube::format, fbUsageFlags, Constants::PrefilteredEnvmapCube::extent, pFBImage, pFBImageView);
    pFBImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    pFBImageView->subresourceRange.baseMipLevel = 0;
    pFBImageView->subresourceRange.baseArrayLayer = 0;

    auto offscreenFB = vsg::Framebuffer::create(renderPass, vsg::ImageViews{pFBImageView}, Constants::PrefilteredEnvmapCube::dim, Constants::PrefilteredEnvmapCube::dim, 1);

    // Create render graph
    // DescriptorSetLayout
    vsg::DescriptorSetLayoutBindings descriptorSetLayoutBindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorSetLayoutBindings);
    // And actual Descriptor for cubemap texture

    vsg::ref_ptr<vsg::DescriptorImage> envmapRectDescriptor;
    if(hdr==-1){
        envmapRectDescriptor = vsg::DescriptorImage::create(textures.envmapCubeInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }
    else{
        envmapRectDescriptor = vsg::DescriptorImage::create(textures.testMap.at(hdr).cubeInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }
    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{envmapRectDescriptor});

    vsg::PushConstantRanges pushConstantRanges = {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128},
        {VK_SHADER_STAGE_FRAGMENT_BIT, 128, 12},
    }; // faceIdx (u32=4) + roughness(float=4)

    // pipeline layout
    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);

    // create pipeline
    const vsg::ShaderStages shaders{vertexShader, fragmentShader};

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}};

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}};
    auto vertexInputState = vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions);
    auto inputAssemblyState = vsg::InputAssemblyState::create();
    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyState->primitiveRestartEnable = VK_FALSE;
    auto rasterState = vsg::RasterizationState::create();
    rasterState->cullMode = VK_CULL_MODE_NONE; // none to render cube inner face
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorWriteMask = 0xf;
    auto blendState = vsg::ColorBlendState::create(vsg::ColorBlendState::ColorBlendAttachments{colorBlendAttachment});
    auto multisampleState = vsg::MultisampleState::create(VK_SAMPLE_COUNT_1_BIT);
    auto depthState = vsg::DepthStencilState::create();
    depthState->depthTestEnable = VK_FALSE;
    depthState->depthWriteEnable = VK_FALSE;
    auto pipelineStates = vsg::GraphicsPipelineStates{
        vertexInputState,
        inputAssemblyState,
        rasterState,
        multisampleState,
        blendState,
        depthState};
    auto pipeline = vsg::GraphicsPipeline::create(pipelineLayout.get(), shaders, pipelineStates, /*subpass=*/0);

    auto setFBLayoutAttachment = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    commandGraph->addChild(setFBLayoutAttachment);

    // set irradianceCube to TRANSFER_DST before every render pass
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = Constants::PrefilteredEnvmapCube::numMips;
    subresourceRange.layerCount = 6;

    IBL::ptr<vsg::PipelineBarrier> setCubeLayoutTransferDst;
    if(hdr==-1){
        setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.prefilterCube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
    }
    else{
        setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.prefMap.at(hdr).cube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
    }
    commandGraph->addChild(setCubeLayoutTransferDst);

    auto dummyProjMatrix = vsg::Perspective::create(degrees(M_PI / 2.0), 1.0, 0.1, 512.0);
    std::vector<vsg::mat4> viewMats = {
        // POSITIVE_X
        rotate(radians(90.0f), vec3(0.0f, 1.0f, 0.0f)) * rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_X
        rotate(radians(-90.0f), vec3(0.0f, 1.0f, 0.0f)) * rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Y
        rotate(radians(-90.0f), vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Y
        rotate(radians(90.0f), vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Z
        rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Z
        rotate(radians(180.0f), vec3(0.0f, 0.0f, 1.0f)),
    };
    std::vector<ptr<ViewMatrix>> myViews(6);
    for (int i = 0; i < 6; i++)
    {
        myViews[i] = MyViewMatrix::create(viewMats[i]);
    }

    for (uint32_t m = 0; m < Constants::PrefilteredEnvmapCube::numMips; m++)
    {
        uint32_t mipDim = Constants::PrefilteredEnvmapCube::dim >> m;
        auto viewportState = vsg::ViewportState::create(0, 0, mipDim, mipDim);
        for (uint32_t f = 0; f < 6; f++)
        {
            auto camera = vsg::Camera::create(dummyProjMatrix, myViews[f], viewportState);

            // bind & draw by vsgScene traversal
            auto bindStates = vsg::StateGroup::create();
            bindStates->add(vsg::BindGraphicsPipeline::create(pipeline));
            bindStates->add(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, descriptorSet));

            auto uintBlock = vsg::uintArray::create(3);
            uintBlock->at(0) = Constants::PrefilteredEnvmapCube::numMips;
            uintBlock->at(1) = m;
            uintBlock->at(2) = f;
            bindStates->add(PushConstants::create(VK_SHADER_STAGE_FRAGMENT_BIT, 128, uintBlock));

            // fullscreen quad
            //auto drawFullscreenQuad = vsg::Draw::create(3, 1, 0, 0);
            //bindStates->addChild(drawFullscreenQuad);
            auto drawSkybox = vsg::Group::create();
            drawSkybox->addChild(BindVertexBuffers::create(0, DataList{gSkyboxCube.vertices}));
            drawSkybox->addChild(BindIndexBuffer::create(gSkyboxCube.indices));
            drawSkybox->addChild(DrawIndexed::create(gSkyboxCube.indices->size(), 1, 0, 0, 0));
            bindStates->addChild(drawSkybox);

            // dummy camera for MVP push constants, dummyScene for bind & draw
            auto view = vsg::View::create(camera, bindStates);
            // warp begin/end renderpass to every bind & draw
            auto rendergraph = vsg::RenderGraph::create();
            rendergraph->renderArea.offset = VkOffset2D{0, 0};
            rendergraph->renderArea.extent = VkExtent2D{mipDim, mipDim};
            rendergraph->clearValues = {{0.0f, 0.0f, 0.0f, 1.0f}};
            rendergraph->framebuffer = offscreenFB;
            rendergraph->addChild(view);

            // renderpass to offscreen framebuffer
            commandGraph->addChild(rendergraph);
            // renderpass之后，fb的layout为VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL，需要再转换到VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            auto setFBLayoutTransfeSrc = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            commandGraph->addChild(setFBLayoutTransfeSrc);

            auto copyFBToCubeMap = vsg::CopyImage::create();
            VkImageCopy copyRegion = {};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset = {0, 0, 0};
            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = f;
            copyRegion.dstSubresource.mipLevel = m;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstOffset = {0, 0, 0};
            copyRegion.extent.width = mipDim;
            copyRegion.extent.height = mipDim;
            copyRegion.extent.depth = 1;
            copyFBToCubeMap->regions = {copyRegion};
            copyFBToCubeMap->srcImage = pFBImage;
            copyFBToCubeMap->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            if(hdr==-1){
                copyFBToCubeMap->dstImage = textures.prefilterCube;
            }
            else{
                copyFBToCubeMap->dstImage = textures.prefMap.at(hdr).cube;
            }
            copyFBToCubeMap->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            commandGraph->addChild(copyFBToCubeMap);
            ptr<PipelineBarrier> setFBLayoutAttachment = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                                                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
            commandGraph->addChild(setFBLayoutAttachment);
        }
    }

    IBL::ptr<vsg::PipelineBarrier> setCubeLayoutShaderRead;
    if(hdr==-1){
        setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.prefilterCube,
                                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                    subresourceRange);
    }
    else{
        setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.prefMap.at(hdr).cube,
                                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                    subresourceRange);
    }
    commandGraph->addChild(setCubeLayoutShaderRead);

    //vsg::write(commandGraph, appData.debugOutputPath);

    viewer->addRecordAndSubmitTaskAndPresentation({commandGraph});
}

// drawSkyboxVSGNode: 创建天空盒渲染节点, 返回可挂载到场景图的 StateGroup
//
// 功能:
//   - 渲染 HDR 环境贴图 cubemap 作为天空盒背景
//   - 可选叠加相机图像 (camera_data): 真实相机画面作为前景, HDR 天空盒作为背景
//   - 可选深度测试 (depth_data): 根据相机深度信息判断 skybox/相机图像的混合
//   - 支持阴影参数 (shadow_pc_data): 在 skybox shader 中计算阴影
//   - 支持 tone mapping (exposure + gamma)
//
// 参数:
//   - root: 父 StateGroup, 渲染命令将添加到此节点
//   - width/height: 视口尺寸 (用于 tone mapping 参数)
//   - camera_data: 相机图像纹理 (空则不叠加)
//   - depth_data:  相机深度纹理 (空则不启用深度测试)
//   - shadow_pc_data: 阴影 push constant 数据 (空则不计算阴影)
//
// VSG StateGroup 概念: 场景图节点, 管理渲染状态 (管线/描述符/顶点缓冲等)
// VSG Commands 概念: 封装 Vulkan 绑定和绘制命令 (BindVertexBuffers/DrawIndexed 等)
ptr<StateGroup> drawSkyboxVSGNode(VsgContext& context, vsg::ref_ptr<vsg::StateGroup> root, int width, int height, vsg::ImageInfoList camera_data, vsg::ImageInfoList depth_data, vsg::ref_ptr<vsg::Data> shadow_pc_data)
{
    // ===================== 第 1 步：加载 skybox shader =====================
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/skybox.vert", appData.options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/skybox.frag", appData.options->paths);
    // 从文件加载 vertex shader (VK_SHADER_STAGE_VERTEX_BIT, 入口函数 "main")
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    // 从文件加载 fragment shader (VK_SHADER_STAGE_FRAGMENT_BIT, 入口函数 "main")
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
    // 检查 shader 是否加载成功
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create skybox shaders." << std::endl;
        return ptr<StateGroup>();
    }

    // ===================== 第 2 步：创建 tone mapping 参数 =====================
    auto shaderCompileSettings = ShaderCompileSettings::create();
    auto shaderStages = ShaderStages{vertexShader, fragmentShader};
    // tonemapParams: tone mapping 参数 [exposure, gamma, width, height]
    auto tonemapParams = floatArray::create(4);
    tonemapParams->set(0, 3.0f);   // exposure: 曝光值 (控制 HDR 亮度)
    tonemapParams->set(1, 2.2f);   // gamma: Gamma 校正值 (sRGB = 2.2)
    tonemapParams->set(2, width * 1.f);  // width: 视口宽度 (用于 shader 计算)
    tonemapParams->set(3, height * 1.f); // height: 视口高度 (用于 shader 计算)

    // ===================== 第 3 步：判断渲染模式 =====================
    // hasShadowInSkybox: 是否启用深度测试 + 阴影模式
    // 条件: 同时存在 depth_data (深度图) 和 shadow_pc_data (阴影参数)
    bool hasShadowInSkybox = (depth_data.size() > 0 && shadow_pc_data);

    // ===================== 第 4 步：创建 ShaderSet (shader 程序集合) =====================
    // ShaderSet: VSG 中一组 shader 程序的集合, 封装顶点输入布局和 uniform 描述
    auto skyBoxShaderSet = ShaderSet::create(shaderStages, shaderCompileSettings);

    // 绑定顶点属性: inPos (position, location 0, R32G32B32_SFLOAT)
    skyBoxShaderSet->addAttributeBinding("inPos", "", 0, VK_FORMAT_R32G32B32_SFLOAT, gSkyboxCube.vertices);

    // 绑定 descriptor: envmap (环境贴图 cubemap, set 0, binding 0)
    skyBoxShaderSet->addDescriptorBinding("envmap", "", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::EnvmapCube::format}));

    // 绑定 descriptor: tonemapParams (tone mapping 参数, set 0, binding 1, uniform buffer)
    skyBoxShaderSet->addDescriptorBinding("tonemapParams", "", 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, tonemapParams);

    // 可选: 绑定 descriptor: cameraImage (相机图像, set 0, binding 2, CAMERA_IMAGE 语义)
    if(camera_data.size() > 0) skyBoxShaderSet->addDescriptorBinding("cameraImage", "CAMERA_IMAGE", 0, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));

    // 可选: 绑定 descriptor: depthImage (深度图, set 0, binding 3, CAMERA_DEPTH 语义)
    if(depth_data.size() > 0) skyBoxShaderSet->addDescriptorBinding("depthImage", "CAMERA_DEPTH", 0, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ushortArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R16_UNORM}));

    // ===================== 第 5 步：扩展绑定 (深度+阴影模式) =====================
    if(hasShadowInSkybox) {
        // 扩展 push constant 范围: 0~256 字节 (vertex + fragment 共享)
        skyBoxShaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 256);

        // 定义 VIEW_DESCRIPTOR_SET = 1 (视图相关 descriptor set)
        #define VIEW_DESCRIPTOR_SET 1

        // 绑定 descriptor: lightData (光源数据, set 1, binding 0, uniform buffer)
        skyBoxShaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));

        // 绑定 descriptor: viewportData (视口数据, set 1, binding 1, uniform buffer)
        skyBoxShaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0,0, 1280, 1024));

        // 绑定 descriptor: shadowMaps (阴影贴图, set 1, binding 2, combined image sampler)
        skyBoxShaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));

        // 绑定 descriptor: shadowMapsSampler (阴影贴图采样器, set 1, binding 3)
        skyBoxShaderSet->addDescriptorBinding("shadowMapsSampler", "", VIEW_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));

        // 添加 ViewDependentStateBinding: 视图相关的 descriptor set 绑定器
        skyBoxShaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEW_DESCRIPTOR_SET));

        // 扩展 color blend state: 支持 4 个 attachment 输出 (MRT: outColor + location 1,2,3)
        auto colorBlendState = vsg::ColorBlendState::create();
        colorBlendState->attachments.resize(4, colorBlendState->attachments[0]);
        skyBoxShaderSet->defaultGraphicsPipelineStates.push_back(colorBlendState);
    } else {
        // 普通模式: push constant 范围 0~128 字节
        skyBoxShaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128);
    }

    // ===================== 第 6 步：创建 GraphicsPipelineConfigurator =====================
    // DataList: 顶点数据列表 (用于绑定顶点缓冲)
    vsg::DataList pipelineInputs;

    // GraphicsPipelineConfigurator: VSG 图形管线配置器, 自动创建 Pipeline + DescriptorSet
    auto pplcfg = vsg::GraphicsPipelineConfigurator::create(skyBoxShaderSet);

    // RasterizationState: 光栅化状态 (背面剔除设置)
    auto rasterState = RasterizationState::create();
    rasterState->cullMode = VK_CULL_MODE_NONE;  // 关闭背面剔除 (渲染立方体内面)
    pplcfg->pipelineStates.push_back(rasterState);

    // DepthStencilState: 深度/模板状态
    auto depthState = vsg::DepthStencilState::create();
    if(depth_data.size() > 0) {
        // 有深度数据时，启用深度写入（用于相机深度前置渲染）
        depthState->depthTestEnable = VK_TRUE;    // 启用深度测试
        depthState->depthWriteEnable = VK_TRUE;   // 启用深度写入
        depthState->depthCompareOp = VK_COMPARE_OP_ALWAYS; // 始终通过深度测试（skybox始终写入）
    } else {
        // 无深度数据时，禁用深度测试（天空盒背景）
        depthState->depthTestEnable = VK_FALSE;
        depthState->depthWriteEnable = VK_FALSE;
    }
    pplcfg->pipelineStates.push_back(depthState);

    // ===================== 第 7 步：绑定顶点数据 =====================
    // 绑定顶点属性 inPos: vertex input rate, 使用 gSkyboxCube.vertices
    pplcfg->assignArray(pipelineInputs, "inPos", VK_VERTEX_INPUT_RATE_VERTEX, gSkyboxCube.vertices);

    // 绑定纹理: envmap (环境贴图 cubemap)
    pplcfg->assignTexture("envmap", ImageInfoList{textures.envmapCubeInfo});

    // 绑定 uniform: tonemapParams (tone mapping 参数)
    pplcfg->assignDescriptor("tonemapParams", tonemapParams);

    // 可选: 绑定纹理 cameraImage (相机图像)
    if(camera_data.size() > 0) pplcfg->assignTexture("cameraImage", camera_data);

    // 可选: 绑定纹理 depthImage (深度图)
    if(depth_data.size() > 0) pplcfg->assignTexture("depthImage", depth_data);

    // 初始化管线配置 (创建 Vulkan Pipeline + DescriptorSet)
    pplcfg->init();

    // ===================== 第 8 步：创建绘制命令 =====================
    // Commands: 封装绘制命令的容器
    auto drawCmds = Commands::create();

    // BindVertexBuffers: 绑定顶点缓冲 (location 0 → pipelineInputs)
    drawCmds->addChild(BindVertexBuffers::create(pplcfg->baseAttributeBinding, pipelineInputs));

    // BindIndexBuffer: 绑定索引缓冲 (使用天空盒立方体索引)
    drawCmds->addChild(BindIndexBuffer::create(gSkyboxCube.indices));

    // DrawIndexed: 绘制 indexed 几何体 (36 顶点, 1 instance, 索引偏移 0)
    drawCmds->addChild(DrawIndexed::create(gSkyboxCube.indices->size(), 1, 0, 0, 0));

    // ===================== 第 9 步：复制管线状态到根节点 =====================
    // copyTo: 将 GraphicsPipelineConfigurator 的状态命令复制到 root StateGroup
    // 包括: BindGraphicsPipeline, BindDescriptorSet, SetViewport, SetScissor 等
    pplcfg->copyTo(root);

    // ===================== 第 10 步：添加阴影 push constant (可选) =====================
    // 在 CAMERA_DEPTH + shadow 模式下, 添加 push constant 绑定 shadow 参数
    if(hasShadowInSkybox) {
        // PushConstants: 每帧传入 shader 的轻量级 uniform 数据
        // 偏移 128 字节: vertex (0-127) 之后是 fragment 区域
        auto pc = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 128, shadow_pc_data);
        root->stateCommands.push_back(pc);
    }

    // ===================== 第 11 步：添加绘制命令到根节点 =====================
    // 将绘制命令添加到 root StateGroup
    root->addChild(drawCmds);

    // 返回 root (已填充渲染状态和绘制命令)
    return root;
}

// IBLDescriptorSetBinding: IBL 资源的 DescriptorSet 布局定义
//
// 封装 IBL 所需的 descriptor set (set 0), 包含 4 个 binding:
//   binding 0: brdfLut      (combined image sampler) - BRDF 查找表
//   binding 1: irradiance   (combined image sampler) - 辐照度 cubemap
//   binding 2: prefilter    (combined image sampler) - 预滤波 cubemap
//   binding 3: params       (uniform buffer)         - 渲染参数
//
// VSG CustomDescriptorSetBinding 概念:
//   允许 ShaderSet 自动管理 descriptor set 的创建和绑定
//   createDescriptorSetLayout() 返回布局, createStateCommand() 返回绑定命令
struct IBLDescriptorSetBinding : vsg::Inherit<CustomDescriptorSetBinding, IBLDescriptorSetBinding>
{
    uint32_t set;
    ptr<DescriptorSet> descriptorSet;
    ptr<DescriptorSetLayout> descriptorSetLayout;

    IBLDescriptorSetBinding(const uint32_t& in_set, const IBL::Textures& _textures) :
        set(in_set)
    {
        vsg::DescriptorSetLayoutBindings customSetLayoutBindings = {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
        };
        descriptorSetLayout = DescriptorSetLayout::create(customSetLayoutBindings);
        auto brdfLutDescriptor = vsg::DescriptorImage::create(_textures.brdfLutInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        auto irradianceDescriptor = vsg::DescriptorImage::create(_textures.irradianceCubeInfo, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        auto prefilterDescriptor = vsg::DescriptorImage::create(_textures.prefilterCubeInfo, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        auto paramsDescriptor = vsg::DescriptorBuffer::create(vsg::BufferInfoList{_textures.paramsInfo}, 3, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        //auto paramsDescriptor = vsg::DescriptorBuffer::create(BufferInfoList{_textures.params}, 3, 0);
        descriptorSet = DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{brdfLutDescriptor, irradianceDescriptor, prefilterDescriptor, paramsDescriptor});
    };

    //int compare(const Object& rhs) const override;

    void read(Input& input) override {}
    void write(Output& output) const override {}

    bool compatibleDescriptorSetLayout(const DescriptorSetLayout& dsl) const {return descriptorSetLayout->compare(dsl) == 0; }

    ref_ptr<DescriptorSetLayout> createDescriptorSetLayout() override {
        return descriptorSetLayout;
    }
    ref_ptr<StateCommand> createStateCommand(ref_ptr<PipelineLayout> layout) override {
        return BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, set, descriptorSet);
    }
};
//
//struct CustomViewDependentStateBinding : vsg::Inherit<ViewDependentStateBinding, CustomViewDependentStateBinding>
//{
//    uint32_t set;
//    ptr<DescriptorSet> descriptorSet;
//    ptr<DescriptorSetLayout> descriptorSetLayout;
//
//    CustomViewDependentStateBinding(const uint32_t& in_set, ptr<BufferInfo> dataBufferInfo) :
//        set(in_set)
//    {
//        DescriptorSetLayoutBindings descriptorBindings{
//            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // lightData
//            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // viewportData
//            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},                      // shadow map 2D texture array
//            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
//        };
//
//        descriptorSetLayout = DescriptorSetLayout::create(descriptorBindings);
//        auto brdfLutDescriptor = vsg::DescriptorImage::create(_textures.brdfLutInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//        auto irradianceDescriptor = vsg::DescriptorImage::create(_textures.irradianceCubeInfo, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//        auto prefilterDescriptor = vsg::DescriptorImage::create(_textures.prefilterCubeInfo, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//
//        descriptorSet = DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{brdfLutDescriptor, irradianceDescriptor, prefilterDescriptor});
//    };
//
//    //int compare(const Object& rhs) const override;
//
//    void read(Input& input) override {}
//    void write(Output& output) const override {}
//
//    bool compatibleDescriptorSetLayout(const DescriptorSetLayout& dsl) const { return descriptorSetLayout->compare(dsl) == 0; }
//
//    ref_ptr<DescriptorSetLayout> createDescriptorSetLayout() override
//    {
//        return descriptorSetLayout;
//    }
//    ref_ptr<StateCommand> createStateCommand(ref_ptr<PipelineLayout> layout) override
//    {
//        return BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, set, descriptorSet);
//    }
//};


// customPbrShaderSet: 创建自定义 PBR (Physically Based Rendering) ShaderSet
//
// 包含 3 个 descriptor set:
//   set 0 (CUSTOM_DESCRIPTOR_SET): IBL 资源
//     - brdfLut, irradiance, prefilter, params
//   set 1 (VIEW_DESCRIPTOR_SET): 视图相关 (灯光/阴影)
//     - lightData, viewportData, shadowMaps, shadowMapsSampler
//   set 2 (MATERIAL_DESCRIPTOR_SET): 材质贴图
//     - diffuseMap, mrMap, normalMap, aoMap, emissiveMap, specularMap, displacementMap
//     - instanceModelMatrix, ConstantBuffer, materialArray, shadowsampler
//
// 顶点属性:
//   vsg_Vertex (pos 0), vsg_Normal (pos 1), vsg_TexCoord0 (pos 2), vsg_Color (pos 3), vsg_InstanceID (pos 4)
//
// Push constant: 256 字节 (vertex + fragment 共享)
// 混合模式: Alpha blending (SRC_ALPHA, ONE_MINUS_SRC_ALPHA)
// 支持 4 个 color attachment 输出 (MRT)
vsg::ref_ptr<vsg::ShaderSet> customPbrShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    vsg::info("Local pbr_ShaderSet(", options, ")");

    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/standard.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/custom_pbr.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("pbr_ShaderSet(...) could not find shaders.");
        return {};
    }

#define CUSTOM_DESCRIPTOR_SET 0
#define VIEW_DESCRIPTOR_SET 1
#define MATERIAL_DESCRIPTOR_SET 2

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord0", "", 2, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_Color", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
    shaderSet->addAttributeBinding("vsg_InstanceID", "", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    shaderSet->addAttributeBinding("vsg_position", "VSG_INSTANCE_POSITIONS", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_position_scaleDistance", "VSG_BILLBOARD", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    shaderSet->addDescriptorBinding("diffuseMap", "VSG_DIFFUSE_MAP", MATERIAL_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("mrMap", "VSG_METALLROUGHNESS_MAP", MATERIAL_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec2Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8_UNORM}));
    shaderSet->addDescriptorBinding("normalMap", "VSG_NORMAL_MAP", MATERIAL_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("aoMap", "VSG_LIGHTMAP_MAP", MATERIAL_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    shaderSet->addDescriptorBinding("emissiveMap", "VSG_EMISSIVE_MAP", MATERIAL_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("specularMap", "VSG_SPECULAR_MAP", MATERIAL_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("displacementMap", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("instanceModelMatrix", "", MATERIAL_DESCRIPTOR_SET, 11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::floatArray::create());
    shaderSet->addDescriptorBinding("ConstantBuffer", "", MATERIAL_DESCRIPTOR_SET, 12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray::create());
    shaderSet->addDescriptorBinding("shadowsampler", "", MATERIAL_DESCRIPTOR_SET, 13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("materialArray", "", MATERIAL_DESCRIPTOR_SET, 14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PbrMaterialArray::create());

    shaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
    shaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0,0, 1280, 1024));
    shaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    shaderSet->addDescriptorBinding("shadowMapsSampler", "", VIEW_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));

    shaderSet->addDescriptorBinding("brdfLut", "", CUSTOM_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::BrdfLUT::format}));
    shaderSet->addDescriptorBinding("irradiance", "", CUSTOM_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::IrradianceCube::format}));
    shaderSet->addDescriptorBinding("prefilter", "", CUSTOM_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::PrefilteredEnvmapCube::format}));
    shaderSet->addDescriptorBinding("params", "", CUSTOM_DESCRIPTOR_SET, 3,
                                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
                                    textures.params);
    //
    auto iblDSBinding = IBLDescriptorSetBinding::create(CUSTOM_DESCRIPTOR_SET, IBL::textures);
    shaderSet->customDescriptorSetBindings.push_back(iblDSBinding);

    // additional defines
    shaderSet->optionalDefines = {"VSG_GREYSCALE_DIFFUSE_MAP", "VSG_TWO_SIDED_LIGHTING"};

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 256);

    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS", "VSG_DISPLACEMENT_MAP"}, vsg::PositionAndDisplacementMapArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS"}, vsg::PositionArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_DISPLACEMENT_MAP"}, vsg::DisplacementMapArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_BILLBOARD"}, vsg::BillboardArrayState::create()});

    shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEW_DESCRIPTOR_SET));
    
    auto colorBlendState = vsg::ColorBlendState::create();
    colorBlendState->attachments[0] = {
        VK_TRUE,                                      // 开启混合
        VK_BLEND_FACTOR_SRC_ALPHA,                    // 源颜色因子：取当前片元的 Alpha 值
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,          // 目标颜色因子：1 - 源 Alpha（经典半透公式）
        VK_BLEND_OP_ADD,                              // 颜色混合：源×源Alpha + 目标×(1-源Alpha)
        VK_BLEND_FACTOR_ONE,                          // 源 Alpha 因子：1
        VK_BLEND_FACTOR_ZERO,                         // 目标 Alpha 因子：0
        VK_BLEND_OP_ADD,                              // Alpha 混合：源Alpha×1 + 目标Alpha×0
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    colorBlendState->attachments.resize(4, colorBlendState->attachments[0]); 
    shaderSet->defaultGraphicsPipelineStates.push_back(colorBlendState);

    return shaderSet;
}

// createTestScene: 创建 PBR 测试场景 (11x11 球体网格, 覆盖 roughness x metallic 参数空间)
// 每个球体使用独立的 customPbrShaderSet, roughness 从 0.0~1.0, metallic 从 0.0~1.0
// 用于验证 PBR 渲染和 IBL 光照效果
vsg::ref_ptr<vsg::Node> createTestScene(vsg::ref_ptr<vsg::Options> options, bool requiresBase = true)
{
    auto builder = vsg::Builder::create();
    builder->options = options;

    auto scene = vsg::Group::create();

    //std::vector<ptr<ShaderSet>> shaderSets;
    shaderSets.clear();
    shaderSets.reserve(144);
    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;
    geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);

    for(int row = 0; row < 11; row++) {
        for(int col = 0; col < 11; col++) {
            auto shaderSet = customPbrShaderSet(options);
            shaderSets.push_back(shaderSet);

            PbrMaterial& pbrMaterial = dynamic_cast<PbrMaterialValue*>(shaderSet->getDescriptorBinding("material").data.get())->value();
            pbrMaterial.roughnessFactor = row * 0.1f;
            pbrMaterial.metallicFactor = col * 0.1f;

            
            builder->shaderSet = shaderSet;
            //geomInfo.cullNode = insertCullNode;
            //if (textureFile) stateInfo.image = vsg::read_cast<vsg::Data>(textureFile, options);

            auto transformNode = vsg::MatrixTransform::create(translate(3 * row - 15.0, 3 * col - 15.0, 0.0) * scale(2.5));
            auto sphereNode = builder->createSphere(geomInfo, stateInfo);
            transformNode->addChild(sphereNode);
            scene->addChild(transformNode);
        }
    }
    //auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
    //if (requiresBase)
    //{
    //    double diameter = vsg::length(bounds.max - bounds.min);
    //    geomInfo.position.set((bounds.min.x + bounds.max.x) * 0.5, (bounds.min.y + bounds.max.y) * 0.5, bounds.min.z);
    //    geomInfo.dx.set(diameter, 0.0, 0.0);
    //    geomInfo.dy.set(0.0, diameter, 0.0);
    //    geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);

    //    stateInfo.two_sided = true;

    //    scene->addChild(builder->createQuad(geomInfo, stateInfo));
    //}
    //vsg::info("createTestScene() extents = ", bounds);
    return scene;
}

// iblDemoSceneGraph: 创建 IBL 演示场景图
// 将 customPbrShaderSet 注册到 options->shaderSets["pbribl"], 然后调用 createTestScene
ptr<Node> iblDemoSceneGraph(VsgContext& context)
{
    auto options = vsg::Options::create(*appData.options);
    auto shaderSet = customPbrShaderSet(options);

    //vsg::DescriptorSetLayoutBindings descriptorSetLayoutBindings = {
    //    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    //    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    //    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    //};

    //auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorSetLayoutBindings);
    //// And actual Descriptor for cubemap texture
    //auto envmapRectDescriptor = vsg::DescriptorImage::create(gEnvmapRect.imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    //auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{envmapRectDescriptor});

    //auto iblSamplerDescriptorSetLayout = shaderSet->createDescriptorSetLayout({""}, 2);
    //for(auto &binding : iblSamplerDescriptorSetLayout->bindings) {
    //    binding.pImmutableSamplers = nullptr;
    //}

    //auto pplLayout = shaderSet->createPipelineLayout({}, {0, 2});
    //auto stateGroup = vsg::StateGroup::create();
    //stateGroup->add(IBLDescriptorSetBinding::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pplLayout, iblDescriptorSet));
    
    //auto sharedObjectsFromSceneBuilder = SharedObjects::create();
    options->shaderSets["pbribl"] = shaderSet;
    //options->inheritedState = stateGroup->stateCommands;
    //options->sharedObjects = sharedObjectsFromSceneBuilder;
    auto scene = createTestScene(options, false);
    //vsg::write(scene, appData.debugOutputPath);
    return scene;
}

// updateHDRTextures: 运行时切换 HDR 环境贴图
//
// 将预生成的 HDR 纹理 (testMap/irraMap/prefMap) 拷贝到当前激活的 IBL 纹理:
//   1. 拷贝 testMap[hdr] -> envmapCube (含 mipmap blit)
//   2. 拷贝 irraMap[hdr] -> irradianceCube (逐面逐 mip)
//   3. 拷贝 prefMap[hdr] -> prefilterCube  (逐面逐 mip)
//
// 所有拷贝命令追加到传入的 command 对象中, 由调用者提交到 GPU
// 这样可以在不重新运行离线生成管线的情况下, 实时切换不同的 HDR 环境
void updateHDRTextures(vsg::ref_ptr<vsg::Commands>& command, int hdr)
{
    
    // CommandGraph to hold the different RenderGraphs used to render each view
    // auto commandGraph = vsg::CommandGraph::create(vsgContext.device, vsgContext.queueFamily);

    {

        VkImageSubresourceRange cubeAllMipSubresRange = {};
        cubeAllMipSubresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cubeAllMipSubresRange.baseArrayLayer = 0;
        cubeAllMipSubresRange.baseMipLevel = 0;
        cubeAllMipSubresRange.layerCount = 6;
        cubeAllMipSubresRange.levelCount = Constants::EnvmapCube::numMips;
        auto setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.testMap.at(hdr).cube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cubeAllMipSubresRange);
        command->addChild(setCubeLayoutTransferDst);
        for (uint32_t f = 0; f< 6; f++)
        {
            auto copyFBToCubeFace = vsg::CopyImage::create();
            VkImageCopy copyRegion = {};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = f;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcOffset = {0, 0, 0};
            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = f;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstSubresource.mipLevel = 0;
            copyRegion.dstOffset = {0, 0, 0};
            copyRegion.extent.width = static_cast<uint32_t>(Constants::EnvmapCube::dim);
            copyRegion.extent.height = static_cast<uint32_t>(Constants::EnvmapCube::dim);
            copyRegion.extent.depth = 1;
            copyFBToCubeFace->regions = {copyRegion};
            copyFBToCubeFace->srcImage = textures.testMap.at(hdr).cube;
            copyFBToCubeFace->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            copyFBToCubeFace->dstImage = textures.envmapCube;
            copyFBToCubeFace->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            command->addChild(copyFBToCubeFace);

            // blit to higher mips
            for(uint32_t targetMipLevel = 1; targetMipLevel < Constants::EnvmapCube::numMips; targetMipLevel++) 
            {
                int32_t mipDim = (int32_t) Constants::EnvmapCube::dim >> targetMipLevel;
                VkImageBlit blitRegion = {};
                blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blitRegion.srcSubresource.baseArrayLayer = f;
                blitRegion.srcSubresource.layerCount = 1;
                blitRegion.srcSubresource.mipLevel = targetMipLevel;
                blitRegion.srcOffsets[1] = {mipDim, mipDim, 1};
                blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blitRegion.dstSubresource.baseArrayLayer = f;
                blitRegion.dstSubresource.layerCount = 1;
                blitRegion.dstSubresource.mipLevel = targetMipLevel;
                blitRegion.dstOffsets[1] = {mipDim, mipDim, 1};
                
                auto blitFBToCubeFaceMip = vsg::BlitImage::create();
                blitFBToCubeFaceMip->regions = {blitRegion};
                blitFBToCubeFaceMip->srcImage = textures.testMap.at(hdr).cube;
                blitFBToCubeFaceMip->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                blitFBToCubeFaceMip->dstImage = textures.envmapCube;
                blitFBToCubeFaceMip->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                command->addChild(blitFBToCubeFaceMip);
            }
        }
        auto setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(
            textures.envmapCube, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            cubeAllMipSubresRange);
        command->addChild(setCubeLayoutShaderRead);
    }
    
    {
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = Constants::IrradianceCube::numMips;
        subresourceRange.layerCount = 6;
        for (uint32_t m = 0; m < Constants::IrradianceCube::numMips; m++)
        {
            uint32_t mipDim = Constants::IrradianceCube::dim >> m;
            auto viewportState = vsg::ViewportState::create(0, 0, mipDim, mipDim);
            for (uint32_t f = 0; f < 6; f++)
            {
                auto copyFBToCubeMap = vsg::CopyImage::create();
                VkImageCopy copyRegion = {};
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.baseArrayLayer = f;
                copyRegion.srcSubresource.mipLevel = m;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.srcOffset = {0, 0, 0};
                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.dstSubresource.baseArrayLayer = f;
                copyRegion.dstSubresource.mipLevel = m;
                copyRegion.dstSubresource.layerCount = 1;
                copyRegion.dstOffset = {0, 0, 0};
                copyRegion.extent.width = mipDim;
                copyRegion.extent.height = mipDim;
                copyRegion.extent.depth = 1;
                copyFBToCubeMap->regions = {copyRegion};
                copyFBToCubeMap->srcImage = textures.irraMap.at(hdr).cube;
                copyFBToCubeMap->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                copyFBToCubeMap->dstImage = textures.irradianceCube;
                copyFBToCubeMap->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

                command->addChild(copyFBToCubeMap);
            }
        }
        auto setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.irraMap.at(hdr).cube, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            subresourceRange);
        command->addChild(setCubeLayoutShaderRead);
    }
    
    {
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = Constants::PrefilteredEnvmapCube::numMips;
        subresourceRange.layerCount = 6;
        for (uint32_t m = 0; m < Constants::PrefilteredEnvmapCube::numMips; m++)
        {
            uint32_t mipDim = Constants::PrefilteredEnvmapCube::dim >> m;
            for (uint32_t f = 0; f < 6; f++)
            {
                auto copyFBToCubeMap = vsg::CopyImage::create();
                VkImageCopy copyRegion = {};
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.baseArrayLayer = f;
                copyRegion.srcSubresource.mipLevel = m;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.srcOffset = {0, 0, 0};
                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.dstSubresource.baseArrayLayer = f;
                copyRegion.dstSubresource.mipLevel = m;
                copyRegion.dstSubresource.layerCount = 1;
                copyRegion.dstOffset = {0, 0, 0};
                copyRegion.extent.width = mipDim;
                copyRegion.extent.height = mipDim;
                copyRegion.extent.depth = 1;
                copyFBToCubeMap->regions = {copyRegion};
                copyFBToCubeMap->srcImage = textures.prefMap.at(hdr).cube;
                copyFBToCubeMap->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                copyFBToCubeMap->dstImage = textures.prefilterCube;
                copyFBToCubeMap->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

                command->addChild(copyFBToCubeMap);
            }
        }
        auto setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.prefMap.at(hdr).cube,
                                                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                        subresourceRange);
        command->addChild(setCubeLayoutShaderRead);
    }
}

} // namespace IBL
