#include "IBL.h"

#define _USE_MATH_DEFINES

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <math.h>
#include <iostream>
#include <type_traits>
#include <stb_image.h>

using namespace vsg;

namespace vsg
{
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

// static variables
Textures textures;
//VsgContext vsgContext;
AppData appData = {};

std::vector<ptr<ShaderSet>> shaderSets;

// static data for local .obj use only (no extern in header)
static struct _EnvmapRect
{
    ptr<Data> image;
    ptr<ImageView> imageView;
    ptr<Sampler> sampler;
    ptr<ImageInfo> imageInfo;

    uint32_t width, height;
} gEnvmapRect;

static struct skyboxCube
{
    ptr<vsg::vec3Array> vertices;
    ptr<vsg::ushortArray> indices;

} gSkyboxCube;

static struct IBLVkEvents {
    ptr<Event> envmapCubeRenderedEvent;
    ptr<Event> envmapCubeGeneratedEvent;
    ptr<ImageMemoryBarrier> envmapCubeRenderedBarrier;
    ptr<ImageMemoryBarrier> envmapCubeGeneratedBarrier;
} gVkEvents;

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

void createImageCube(vsg::Context& context, VkFormat format, VkImageUsageFlags usage, VkExtent2D extent, uint32_t numMips, ptr<vsg::Image>& image, ptr<vsg::ImageView>& imageView)
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

void createImageInfo(const ptr<vsg::ImageView> imageView, VkImageLayout layout, const ptr<vsg::Sampler> sampler, ptr<vsg::ImageInfo> &imageInfo)
{
    imageInfo = vsg::ImageInfo::create();
    imageInfo->imageView = imageView;
    imageInfo->imageLayout = layout;
    imageInfo->sampler = sampler;
}

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

void createResources(VsgContext& vsgContext)
{
    auto& context = vsgContext.context;
    auto& viewer = vsgContext.viewer;

    // VkEvents for synchronaziation;
    gVkEvents.envmapCubeGeneratedEvent = Event::create(context->device);
    gVkEvents.envmapCubeRenderedEvent = Event::create(context->device);

    // environment cubemap (only used by skybox and texture generations)
    {
        createImageCube(*context, 
            Constants::EnvmapCube::format, 
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // DST for mipmap generation
            Constants::EnvmapCube::extent,
            Constants::EnvmapCube::numMips, 
            textures.envmapCube, 
            textures.envmapCubeView
        );
        createSamplerCube(
            Constants::EnvmapCube::numMips, // 1 for no mips
            textures.envmapCubeSmapler
        );

        createImageInfo(
            textures.envmapCubeView, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            textures.envmapCubeSmapler, 
            textures.envmapCubeInfo
        );
    }

    // move brdf lut here
    {
        createImage2D(*context, 
            Constants::BrdfLUT::format, 
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
    // irradiance cubemap
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

    gSkyboxCube.vertices = vsg::vec3Array::create({// Back
        {-1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, -1.0f},
        {-1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f},

        // Front
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},

        // Left
        {-1.0f, -1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, 1.0f},

        // Right
        {1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, 1.0f},
        {1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, 1.0f},

        // Bottom
        {-1.0f, -1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, 1.0f},

        // Top
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, 1.0}}
    );

    gSkyboxCube.indices = vsg::ushortArray::create({// Back
        0, 2, 1,
        1, 2, 3,

        // Front
        6, 4, 5,
        7, 6, 5,

        // Left
        10, 8, 9,
        11, 10, 9,

        // Right
        14, 13, 12,
        15, 13, 14,

        // Bottom
        17, 16, 19,
        19, 16, 18,

        // Top
        23, 20, 21,
        22, 20, 23}
    );

    textures.params = vec4Array::create(1, vec4(0, 0, 0, 1.0f));
    textures.params->properties.dataVariance = vsg::DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    textures.paramsInfo = BufferInfo::create(textures.params.get());
}

void clearResources()
{
    //textures.envmapCube = nullptr;
    //textures.envmapCubeView = nullptr;
    //textures.envmapCubeSmapler = nullptr;
    //textures.envmapCubeInfo = nullptr;

    //textures.brdfLut = nullptr;
    //textures.brdfLutView = nullptr;
    //textures.brdfLutSampler = nullptr;
    //textures.brdfLutInfo = nullptr;

    //textures.irradianceCube = nullptr;
    //textures.irradianceCubeView = nullptr;
    //textures.irradianceCubeSampler = nullptr;
    //textures.irradianceCubeInfo = nullptr;

    //textures.prefilterCube = nullptr;
    //textures.prefilterCubeView = nullptr;
    //textures.prefilterCubeSampler = nullptr;
    //textures.prefilterCubeInfo = nullptr;

    appData = {};

    textures = {};
    gVkEvents = {};
    gSkyboxCube = {};
    gEnvmapRect = {};
    //if (gVkEvents.envmapCubeRenderedEvent)
    //    gVkEvents.envmapCubeRenderedEvent = nullptr;
    //if (gVkEvents.envmapCubeGeneratedEvent)
    //    gVkEvents.envmapCubeGeneratedEvent = nullptr;
    //if (gVkEvents.envmapCubeRenderedBarrier)
    //    gVkEvents.envmapCubeRenderedBarrier = nullptr;
    //if (gVkEvents.envmapCubeGeneratedBarrier)
    //    gVkEvents.envmapCubeGeneratedBarrier = nullptr;

    //if (gSkyboxCube.vertices)
    //    gSkyboxCube.vertices =nullptr;
    //if (gSkyboxCube.indices)
    //    gSkyboxCube.indices = nullptr;
    shaderSets.clear();
}

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

void loadEnvmapRect(VsgContext& context, const std::string& filePath)
{
    auto searchPaths = appData.options->paths;
    searchPaths.push_back("./data");
    //auto evnmapFilepath = vsg::findFile("textures/test_park.hdr", searchPaths);
    // auto evnmapFilepath = vsg::findFile(filePath, searchPaths);
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

float *loadHdrFile(const std::string& filepath, int& width, int& height, int& channels)
{
    float* data = stbi_loadf(filepath.c_str(), &width, &height, &channels, 4);

    return data;
}

void generateBRDFLUT(VsgContext &vsgContext)
{
    // TODO: actually create shaders
    auto searchPaths = appData.options->paths;
    searchPaths.push_back("./data");
    //std::cout << searchPaths.size() << std::endl;
    //for (auto path : searchPaths)
    //    std::cout << path << std::endl;

    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreenquad.vert", searchPaths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/genbrdflut.frag", searchPaths);
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
    // descriptors: empty
    //vsg::DescriptorSetLayoutBindings debugDescriptorBindings = {
    //    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    //};
    ////auto descriptorSetLayout = vsg::DescriptorSetLayout::create(vsg::DescriptorSetLayoutBindings{});
    //auto descriptorSetLayout = vsg::DescriptorSetLayout::create(debugDescriptorBindings);
    //auto texDescriptor = vsg::DescriptorImage::create(gEnvmapRect.sampler, gEnvmapRect.image, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    //auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texDescriptor});

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

void generateEnvmap(VsgContext& vsgContext, std::string& envmapFilepath)
{
    auto searchPaths = appData.options->paths;
    searchPaths.push_back("./data");
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreenquad.vert", searchPaths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/equirect2cube.frag", searchPaths);
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

    auto setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.envmapCube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cubeAllMipSubresRange);
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
        copyFBToCubeFace->dstImage = textures.envmapCube;
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
            blitFBToCubeFaceMip->dstImage = textures.envmapCube;
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
    auto setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.envmapCube, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cubeAllMipSubresRange);
    commandGraph->addChild(setCubeLayoutShaderRead);
    gVkEvents.envmapCubeGeneratedBarrier = setCubeLayoutShaderRead->imageMemoryBarriers.at(0);
    viewer->addRecordAndSubmitTaskAndPresentation({commandGraph});
}

void generateIrradianceCube(VsgContext& vsgContext)//生成辐照度贴图
{
    auto searchPaths = appData.options->paths;
    searchPaths.push_back("./data");
    //auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreenquad.vert", searchPaths);
    //auto fragShaderFilepath = vsg::findFile("shaders/IBL/irradianceCube.frag", searchPaths);
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/skyboxCubegen.vert", searchPaths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/irradiancecubeMesh.frag", searchPaths);
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
    auto envmapRectDescriptor = vsg::DescriptorImage::create(textures.envmapCubeInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
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
    auto setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.irradianceCube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
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
            copyFBToCubeMap->dstImage = textures.irradianceCube;
            copyFBToCubeMap->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            commandGraph->addChild(copyFBToCubeMap);
            //ptr<PipelineBarrier> setFBLayoutAttachment = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT,
            //                                                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
            commandGraph->addChild(setFBLayoutAttachment);
        }
    }
    auto setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.irradianceCube, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        subresourceRange);
    commandGraph->addChild(setCubeLayoutShaderRead);

    viewer->addRecordAndSubmitTaskAndPresentation({commandGraph});
}

void generatePrefilteredEnvmapCube(VsgContext& vsgContext)
{
    auto searchPaths = appData.options->paths;
    searchPaths.push_back("./data");
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/skyboxCubegen.vert", searchPaths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/prefilterenvmapMesh.frag", searchPaths);
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
    auto envmapRectDescriptor = vsg::DescriptorImage::create(textures.envmapCubeInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
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
    auto setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.prefilterCube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
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
            copyFBToCubeMap->dstImage = textures.prefilterCube;
            copyFBToCubeMap->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            commandGraph->addChild(copyFBToCubeMap);
            ptr<PipelineBarrier> setFBLayoutAttachment = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                                                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
            commandGraph->addChild(setFBLayoutAttachment);
        }
    }
    auto setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.prefilterCube,
                                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                    subresourceRange);
    commandGraph->addChild(setCubeLayoutShaderRead);

    //vsg::write(commandGraph, appData.debugOutputPath);

    viewer->addRecordAndSubmitTaskAndPresentation({commandGraph});
}

ptr<StateGroup> drawSkyboxVSGNode(VsgContext& context, vsg::ref_ptr<vsg::StateGroup> root, int width, int height, vsg::ref_ptr<vsg::Data> camera_data)
{
    auto searchPaths = appData.options->paths;
    searchPaths.push_back("./data");
    //auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreenquad.vert", searchPaths);
    //auto fragShaderFilepath = vsg::findFile("shaders/IBL/irradianceCube.frag", searchPaths);
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/skybox.vert", searchPaths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/skybox.frag", searchPaths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create skybox shaders." << std::endl;
        return ptr<StateGroup>();
    }

    auto shaderCompileSettings = ShaderCompileSettings::create();
    auto shaderStages = ShaderStages{vertexShader, fragmentShader};
    auto tonemapParams = floatArray::create(4);
    tonemapParams->set(0, 3.0f); //exposure
    tonemapParams->set(1, 2.2f); //gamma
    tonemapParams->set(2, width * 1.f); //gamma
    tonemapParams->set(3, height * 1.f); //gamma

    auto skyBoxShaderSet = ShaderSet::create(shaderStages, shaderCompileSettings);
    skyBoxShaderSet->addAttributeBinding("inPos", "", 0, VK_FORMAT_R32G32B32_SFLOAT, gSkyboxCube.vertices);
    // vsg这块的API不支持绑定一个DescriptorImage（vsg::Data和vsg::Image是兄弟关系，这里只接收Data）
    //skyBoxShaderSet->addDescriptorBinding("envmap", "", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, textures.envmapCube);
    // 但是Data在创建Layout和做绑定的时候没有真正使用，所以这里先随便用个Data代替一下
    skyBoxShaderSet->addDescriptorBinding("envmap", "", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::EnvmapCube::format}));
    skyBoxShaderSet->addDescriptorBinding("tonemapParams", "", 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, tonemapParams);
    if(camera_data) skyBoxShaderSet->addDescriptorBinding("cameraImage", "CAMERA_IMAGE", 0, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    skyBoxShaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    vsg::DataList pipelineInputs;

    auto pplcfg = vsg::GraphicsPipelineConfigurator::create(skyBoxShaderSet);   
    auto rasterState = RasterizationState::create();
    rasterState->cullMode = VK_CULL_MODE_NONE;
    pplcfg->pipelineStates.push_back(rasterState);
    auto depthState = vsg::DepthStencilState::create();
    depthState->depthTestEnable = VK_FALSE;
    depthState->depthWriteEnable = VK_FALSE;
    pplcfg->pipelineStates.push_back(depthState);
    pplcfg->assignArray(pipelineInputs, "inPos", VK_VERTEX_INPUT_RATE_VERTEX, gSkyboxCube.vertices);
    pplcfg->assignTexture("envmap", ImageInfoList{textures.envmapCubeInfo});
    pplcfg->assignDescriptor("tonemapParams", tonemapParams);
    if(camera_data) pplcfg->assignTexture("cameraImage", camera_data);
    pplcfg->init();

    auto drawCmds = Commands::create();
    drawCmds->addChild(BindVertexBuffers::create(pplcfg->baseAttributeBinding, pipelineInputs));
    drawCmds->addChild(BindIndexBuffer::create(gSkyboxCube.indices));
    drawCmds->addChild(DrawIndexed::create(gSkyboxCube.indices->size(), 1, 0, 0, 0));
    pplcfg->copyTo(root);
    root->addChild(drawCmds);
    return root;
}

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

    shaderSet->addAttributeBinding("vsg_position", "VSG_INSTANCE_POSITIONS", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_position_scaleDistance", "VSG_BILLBOARD", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    shaderSet->addDescriptorBinding("displacementMap", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("diffuseMap", "VSG_DIFFUSE_MAP", MATERIAL_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("mrMap", "VSG_METALLROUGHNESS_MAP", MATERIAL_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec2Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8_UNORM}));
    shaderSet->addDescriptorBinding("normalMap", "VSG_NORMAL_MAP", MATERIAL_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("aoMap", "VSG_LIGHTMAP_MAP", MATERIAL_DESCRIPTOR_SET, 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    shaderSet->addDescriptorBinding("emissiveMap", "VSG_EMISSIVE_MAP", MATERIAL_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("specularMap", "VSG_SPECULAR_MAP", MATERIAL_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("material", "", MATERIAL_DESCRIPTOR_SET, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PbrMaterialValue::create());
    shaderSet->addDescriptorBinding("transparent", "", MATERIAL_DESCRIPTOR_SET, 9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::intValue::create(0));

    shaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
    //shaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0, 0, 1280, 1024));
    shaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    shaderSet->addDescriptorBinding("viewMatrixData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::mat4Array::create(4));

    shaderSet->addDescriptorBinding("brdfLut", "", CUSTOM_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::BrdfLUT::format}));
    shaderSet->addDescriptorBinding("irradiance", "", CUSTOM_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::IrradianceCube::format}));
    shaderSet->addDescriptorBinding("prefilter", "", CUSTOM_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::PrefilteredEnvmapCube::format}));
    shaderSet->addDescriptorBinding("params", "", CUSTOM_DESCRIPTOR_SET, 3,
                                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
                                    textures.params);

    auto iblDSBinding = IBLDescriptorSetBinding::create(CUSTOM_DESCRIPTOR_SET, IBL::textures);
    shaderSet->customDescriptorSetBindings.push_back(iblDSBinding);

    // additional defines
    shaderSet->optionalDefines = {"VSG_GREYSCALE_DIFFUSE_MAP", "VSG_TWO_SIDED_LIGHTING", "VSG_WORKFLOW_SPECGLOSS"};

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128);

    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS", "VSG_DISPLACEMENT_MAP"}, vsg::PositionAndDisplacementMapArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS"}, vsg::PositionArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_DISPLACEMENT_MAP"}, vsg::DisplacementMapArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_BILLBOARD"}, vsg::BillboardArrayState::create()});

    shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEW_DESCRIPTOR_SET));

    return shaderSet;
}

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

} // namespace IBL

#endif