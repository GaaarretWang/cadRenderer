#include "SSAOPass.h"

namespace
{
constexpr uint32_t kCustomDescriptorSet = 0;
constexpr uint32_t kViewDescriptorSet = 1;
constexpr uint32_t kMaterialDescriptorSet = 2;

vsg::ref_ptr<vsg::DepthStencilState> createFullscreenDepthState()
{
    auto depthStencilState = vsg::DepthStencilState::create();
    depthStencilState->depthTestEnable = VK_FALSE;
    depthStencilState->depthWriteEnable = VK_FALSE;
    depthStencilState->depthCompareOp = VK_COMPARE_OP_ALWAYS;
    return depthStencilState;
}

vsg::ImageInfoList createNoiseImageInfo(VkExtent2D extent)
{
    unsigned int seed = 100;
    std::mt19937 generator(seed);
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

    auto samplerNoiseData = vsg::ubvec4Array2D::create(extent.width, extent.height);
    for (uint32_t x = 0; x < extent.width; ++x)
    {
        for (uint32_t y = 0; y < extent.height; ++y)
        {
            float randX = distribution(generator);
            float randY = distribution(generator);
            samplerNoiseData->set(
                x,
                y,
                vsg::ubvec4(
                    static_cast<uint8_t>(randX * 255.0f),
                    static_cast<uint8_t>(randY * 255.0f),
                    static_cast<uint8_t>(0),
                    static_cast<uint8_t>(0)));
        }
    }
    samplerNoiseData->properties.format = VK_FORMAT_R8G8B8A8_UNORM;
    samplerNoiseData->properties.dataVariance = vsg::DYNAMIC_DATA;

    return {vsg::ImageInfo::create(Utils::createNearestSampler(), samplerNoiseData)};
}

vsg::ref_ptr<vsg::StateGroup> createFullscreenStateGroup(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> graphicsPipelineConfig)
{
    auto vertices = vsg::vec3Array::create({
        vsg::vec3(-1.0f, -1.0f, 1.0f),
        vsg::vec3(1.0f, -1.0f, 1.0f),
        vsg::vec3(1.0f, 1.0f, 1.0f),
        vsg::vec3(-1.0f, 1.0f, 1.0f)});

    auto indices = vsg::ushortArray::create({0, 1, 2, 2, 3, 0});

    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);

    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

    graphicsPipelineConfig->init();

    auto stateGroup = vsg::StateGroup::create();
    graphicsPipelineConfig->copyTo(stateGroup);
    stateGroup->addChild(drawCommands);
    return stateGroup;
}

vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> createFullscreenPipelineConfig(vsg::ref_ptr<vsg::ShaderSet> shaderSet,
                                                                               VkSampleCountFlagBits outputSamples = VK_SAMPLE_COUNT_1_BIT,
                                                                               bool sampleRateShading = false)
{
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    shaderSet->defaultGraphicsPipelineStates.push_back(rasterizationState);
    shaderSet->defaultGraphicsPipelineStates.push_back(createFullscreenDepthState());
    auto multisampleState = vsg::MultisampleState::create(outputSamples);
    if (sampleRateShading)
    {
        multisampleState->sampleShadingEnable = VK_TRUE;
        multisampleState->minSampleShading = 1.0f;
    }
    shaderSet->defaultGraphicsPipelineStates.push_back(multisampleState);
    return vsg::GraphicsPipelineConfigurator::create(shaderSet);
}

struct DeferredIblDescriptorSetBinding : vsg::Inherit<vsg::CustomDescriptorSetBinding, DeferredIblDescriptorSetBinding>
{
    uint32_t set = 0;
    vsg::ref_ptr<vsg::DescriptorSet> descriptorSet;
    vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout;

    explicit DeferredIblDescriptorSetBinding(uint32_t in_set) :
        set(in_set)
    {
        vsg::DescriptorSetLayoutBindings descriptorBindings = {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
        descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

        auto brdfLutDescriptor = vsg::DescriptorImage::create(IBL::textures.brdfLutInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        auto irradianceDescriptor = vsg::DescriptorImage::create(IBL::textures.irradianceCubeInfo, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        auto prefilterDescriptor = vsg::DescriptorImage::create(IBL::textures.prefilterCubeInfo, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        auto paramsDescriptor = vsg::DescriptorBuffer::create(vsg::BufferInfoList{IBL::textures.paramsInfo}, 3, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        descriptorSet = vsg::DescriptorSet::create(
            descriptorSetLayout,
            vsg::Descriptors{brdfLutDescriptor, irradianceDescriptor, prefilterDescriptor, paramsDescriptor});
    }

    void read(vsg::Input&) override {}
    void write(vsg::Output&) const override {}

    bool compatibleDescriptorSetLayout(const vsg::DescriptorSetLayout& dsl) const override
    {
        return descriptorSetLayout->compare(dsl) == 0;
    }

    vsg::ref_ptr<vsg::DescriptorSetLayout> createDescriptorSetLayout() override
    {
        return descriptorSetLayout;
    }

    vsg::ref_ptr<vsg::StateCommand> createStateCommand(vsg::ref_ptr<vsg::PipelineLayout> layout) override
    {
        return vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, set, descriptorSet);
    }
};

vsg::ref_ptr<vsg::ShaderSet> createStandaloneSSAOShaderSet(vsg::ref_ptr<const vsg::Options> options,
                                                           bool useResolvedInputs)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreen_quad.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile(
        useResolvedInputs ? "shaders/IBL/ssao_standalone_resolved.frag" : "shaders/IBL/ssao_standalone.frag",
        options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("createStandaloneSSAOShaderSet(...) could not find shaders.");
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    vsg::ref_ptr<vsg::Data> gbufferPlaceholder;
    if (useResolvedInputs)
    {
        gbufferPlaceholder = vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT});
    }
    else
    {
        gbufferPlaceholder = vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM});
    }
    shaderSet->addDescriptorBinding("normalSampler", "", kMaterialDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, gbufferPlaceholder);
    shaderSet->addDescriptorBinding("worldPosSampler", "", kMaterialDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, gbufferPlaceholder);
    shaderSet->addDescriptorBinding("samplerNoise", "", kMaterialDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    shaderSet->addDescriptorBinding("GlobalBuffer", "", kMaterialDescriptorSet, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubyteArray::create(sizeof(GlobalConstantData)));
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128);
    return shaderSet;
}

vsg::ref_ptr<vsg::ShaderSet> createStandaloneSSAOCompositeShaderSet(vsg::ref_ptr<const vsg::Options> options,
                                                                    bool useMsaaColorInput)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreen_quad.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile(
        useMsaaColorInput ? "shaders/IBL/ssao_composite_msaa.frag" : "shaders/IBL/ssao_composite.frag",
        options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("createStandaloneSSAOCompositeShaderSet(...) could not find shaders.");
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addDescriptorBinding(
        "colorSampler",
        "",
        kMaterialDescriptorSet,
        0,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    shaderSet->addDescriptorBinding("ssaoSampler", "", kMaterialDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    shaderSet->addDescriptorBinding("realSceneSampler", "", kMaterialDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    shaderSet->addDescriptorBinding("GlobalBuffer", "", kMaterialDescriptorSet, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubyteArray::create(sizeof(GlobalConstantData)));
    return shaderSet;
}

vsg::ref_ptr<vsg::ShaderSet> createStandaloneRealSceneShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreen_quad.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/real_scene_standalone.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("createStandaloneRealSceneShaderSet(...) could not find shaders.");
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addDescriptorBinding("cameraImageSampler", "", kMaterialDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("realDepthSampler", "", kMaterialDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ushortArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R16_UNORM}));
    shaderSet->addDescriptorBinding("sceneDepthSampler", "", kMaterialDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_D32_SFLOAT}));
    shaderSet->addDescriptorBinding("GlobalBuffer", "", kMaterialDescriptorSet, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubyteArray::create(sizeof(GlobalConstantData)));
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 256);
    shaderSet->addDescriptorBinding("lightData", "", kViewDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
    shaderSet->addDescriptorBinding("viewportData", "", kViewDescriptorSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0, 0, 1280, 1024));
    shaderSet->addDescriptorBinding("shadowMaps", "", kViewDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    shaderSet->addDescriptorBinding("shadowMapsSampler", "", kViewDescriptorSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(kViewDescriptorSet));
    return shaderSet;
}

vsg::ref_ptr<vsg::ShaderSet> createStandaloneResolvedDepthShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreen_quad.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/gbuffer_resolve_depth.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("createStandaloneResolvedDepthShaderSet(...) could not find shaders.");
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addDescriptorBinding("depthSampler", "", kMaterialDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_D32_SFLOAT}));
    return shaderSet;
}

vsg::ref_ptr<vsg::ShaderSet> createStandaloneResolvedNormalShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreen_quad.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/gbuffer_resolve_normal.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("createStandaloneResolvedNormalShaderSet(...) could not find shaders.");
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addDescriptorBinding("depthSampler", "", kMaterialDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_D32_SFLOAT}));
    shaderSet->addDescriptorBinding("normalSampler", "", kMaterialDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT}));
    return shaderSet;
}

vsg::ref_ptr<vsg::ShaderSet> createStandaloneResolvedWorldPosShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreen_quad.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/gbuffer_resolve_world_pos.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("createStandaloneResolvedWorldPosShaderSet(...) could not find shaders.");
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addDescriptorBinding("depthSampler", "", kMaterialDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_D32_SFLOAT}));
    shaderSet->addDescriptorBinding("worldPosSampler", "", kMaterialDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT}));
    return shaderSet;
}

vsg::ref_ptr<vsg::ShaderSet> createStandaloneResolvedMaterialShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreen_quad.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/gbuffer_resolve_material.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("createStandaloneResolvedMaterialShaderSet(...) could not find shaders.");
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addDescriptorBinding("depthSampler", "", kMaterialDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_D32_SFLOAT}));
    shaderSet->addDescriptorBinding("materialSampler", "", kMaterialDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT}));
    return shaderSet;
}

vsg::ref_ptr<vsg::ShaderSet> createStandaloneDeferredDebugShaderSet(vsg::ref_ptr<const vsg::Options> options,
                                                                    bool useMsaaInputs)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreen_quad.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile(
        useMsaaInputs ? "shaders/IBL/deferred_debug_standalone_msaa.frag" : "shaders/IBL/deferred_debug_standalone.frag",
        options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("createStandaloneDeferredDebugShaderSet(...) could not find shaders.");
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    auto gbufferPlaceholder = vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT});
    shaderSet->addDescriptorBinding("brdfLut", "", kCustomDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{IBL::Constants::BrdfLUT::format}));
    shaderSet->addDescriptorBinding("irradiance", "", kCustomDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{IBL::Constants::IrradianceCube::format}));
    shaderSet->addDescriptorBinding("prefilter", "", kCustomDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{IBL::Constants::PrefilteredEnvmapCube::format}));
    shaderSet->addDescriptorBinding("params", "", kCustomDescriptorSet, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, IBL::textures.params);
    shaderSet->addDescriptorBinding("normalSampler", "", kMaterialDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, gbufferPlaceholder);
    shaderSet->addDescriptorBinding("worldPosSampler", "", kMaterialDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, gbufferPlaceholder);
    shaderSet->addDescriptorBinding("materialSampler", "", kMaterialDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, gbufferPlaceholder);
    shaderSet->addDescriptorBinding("ssaoSampler", "", kMaterialDescriptorSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, gbufferPlaceholder);
    shaderSet->addDescriptorBinding("GlobalBuffer", "", kMaterialDescriptorSet, 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubyteArray::create(sizeof(GlobalConstantData)));
    shaderSet->addDescriptorBinding("lightData", "", kViewDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
    shaderSet->addDescriptorBinding("viewportData", "", kViewDescriptorSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0, 0, 1280, 1024));
    shaderSet->addDescriptorBinding("shadowMaps", "", kViewDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    shaderSet->addDescriptorBinding("shadowMapsSampler", "", kViewDescriptorSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(kViewDescriptorSet));
    shaderSet->customDescriptorSetBindings.push_back(DeferredIblDescriptorSetBinding::create(kCustomDescriptorSet));
    return shaderSet;
}
}

void SSAOPass::buildStandaloneSSAOData(vsg::ref_ptr<vsg::Options> options,
                                       vsg::ref_ptr<vsg::Group> scene,
                                       vsg::ref_ptr<vsg::ImageView> normalView,
                                       vsg::ref_ptr<vsg::ImageView> worldPosView,
                                       VkExtent2D outputExtent,
                                       vsg::BufferInfoList global_buffer_info_list,
                                       bool useResolvedInputs)
{
    auto graphicsPipelineConfig = createFullscreenPipelineConfig(createStandaloneSSAOShaderSet(options, useResolvedInputs));
    auto sampledInputSampler = Utils::createNearestClampSampler();
    vsg::ImageInfoList normalImageInfoList = {
        vsg::ImageInfo::create(sampledInputSampler, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList worldPosImageInfoList = {
        vsg::ImageInfo::create(sampledInputSampler, worldPosView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};

    graphicsPipelineConfig->assignTexture("normalSampler", normalImageInfoList);
    graphicsPipelineConfig->assignTexture("worldPosSampler", worldPosImageInfoList);
    graphicsPipelineConfig->assignTexture("samplerNoise", createNoiseImageInfo(outputExtent));
    graphicsPipelineConfig->assignDescriptor("GlobalBuffer", global_buffer_info_list);

    scene->addChild(createFullscreenStateGroup(graphicsPipelineConfig));
}

void SSAOPass::buildStandaloneSSAOCompositeData(vsg::ref_ptr<vsg::Options> options,
                                                vsg::ref_ptr<vsg::Group> scene,
                                                vsg::ref_ptr<vsg::ImageView> colorView,
                                                vsg::ref_ptr<vsg::ImageView> ssaoView,
                                                vsg::ref_ptr<vsg::ImageView> realSceneView,
                                                vsg::BufferInfoList global_buffer_info_list,
                                                bool useMsaaColorInput,
                                                VkSampleCountFlagBits outputSamples)
{
    auto graphicsPipelineConfig = createFullscreenPipelineConfig(
        createStandaloneSSAOCompositeShaderSet(options, useMsaaColorInput),
        outputSamples);
    auto colorSampler = Utils::createNearestClampSampler();
    auto ssaoSampler = Utils::createLinearSampler();
    vsg::ImageInfoList colorImageInfoList = {
        vsg::ImageInfo::create(colorSampler, colorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList ssaoImageInfoList = {
        vsg::ImageInfo::create(ssaoSampler, ssaoView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList realSceneImageInfoList = {
        vsg::ImageInfo::create(colorSampler, realSceneView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};

    graphicsPipelineConfig->assignTexture("colorSampler", colorImageInfoList);
    graphicsPipelineConfig->assignTexture("ssaoSampler", ssaoImageInfoList);
    graphicsPipelineConfig->assignTexture("realSceneSampler", realSceneImageInfoList);
    graphicsPipelineConfig->assignDescriptor("GlobalBuffer", global_buffer_info_list);

    scene->addChild(createFullscreenStateGroup(graphicsPipelineConfig));
}

void SSAOPass::buildStandaloneRealSceneData(vsg::ref_ptr<vsg::Options> options,
                                            vsg::ref_ptr<vsg::Group> scene,
                                            const vsg::ImageInfoList& cameraImageInfoList,
                                            const vsg::ImageInfoList& realDepthInfoList,
                                            vsg::ref_ptr<vsg::ImageView> sceneDepthView,
                                            vsg::BufferInfoList global_buffer_info_list,
                                            vsg::ref_ptr<vsg::Data> shadow_pc_data)
{
    auto graphicsPipelineConfig = createFullscreenPipelineConfig(createStandaloneRealSceneShaderSet(options));
    auto sceneDepthSampler = Utils::createNearestClampSampler();
    vsg::ImageInfoList sceneDepthInfoList = {
        vsg::ImageInfo::create(sceneDepthSampler, sceneDepthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)};

    graphicsPipelineConfig->assignTexture("cameraImageSampler", cameraImageInfoList);
    graphicsPipelineConfig->assignTexture("realDepthSampler", realDepthInfoList);
    graphicsPipelineConfig->assignTexture("sceneDepthSampler", sceneDepthInfoList);
    graphicsPipelineConfig->assignDescriptor("GlobalBuffer", global_buffer_info_list);

    auto stateGroup = createFullscreenStateGroup(graphicsPipelineConfig);
    stateGroup->stateCommands.push_back(vsg::PushConstants::create(
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        128,
        shadow_pc_data));
    scene->addChild(stateGroup);
}

void SSAOPass::buildStandaloneResolvedDepthData(vsg::ref_ptr<vsg::Options> options,
                                                vsg::ref_ptr<vsg::Group> scene,
                                                vsg::ref_ptr<vsg::ImageView> depthView)
{
    auto graphicsPipelineConfig = createFullscreenPipelineConfig(createStandaloneResolvedDepthShaderSet(options));
    auto depthSampler = Utils::createNearestClampSampler();
    vsg::ImageInfoList depthInfoList = {
        vsg::ImageInfo::create(depthSampler, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)};

    graphicsPipelineConfig->assignTexture("depthSampler", depthInfoList);
    scene->addChild(createFullscreenStateGroup(graphicsPipelineConfig));
}

void SSAOPass::buildStandaloneResolvedNormalData(vsg::ref_ptr<vsg::Options> options,
                                                 vsg::ref_ptr<vsg::Group> scene,
                                                 vsg::ref_ptr<vsg::ImageView> depthView,
                                                 vsg::ref_ptr<vsg::ImageView> normalView)
{
    auto graphicsPipelineConfig = createFullscreenPipelineConfig(createStandaloneResolvedNormalShaderSet(options));
    auto nearestSampler = Utils::createNearestClampSampler();
    vsg::ImageInfoList depthInfoList = {
        vsg::ImageInfo::create(nearestSampler, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList normalInfoList = {
        vsg::ImageInfo::create(nearestSampler, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};

    graphicsPipelineConfig->assignTexture("depthSampler", depthInfoList);
    graphicsPipelineConfig->assignTexture("normalSampler", normalInfoList);
    scene->addChild(createFullscreenStateGroup(graphicsPipelineConfig));
}

void SSAOPass::buildStandaloneResolvedWorldPosData(vsg::ref_ptr<vsg::Options> options,
                                                   vsg::ref_ptr<vsg::Group> scene,
                                                   vsg::ref_ptr<vsg::ImageView> depthView,
                                                   vsg::ref_ptr<vsg::ImageView> worldPosView)
{
    auto graphicsPipelineConfig = createFullscreenPipelineConfig(createStandaloneResolvedWorldPosShaderSet(options));
    auto nearestSampler = Utils::createNearestClampSampler();
    vsg::ImageInfoList depthInfoList = {
        vsg::ImageInfo::create(nearestSampler, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList worldPosInfoList = {
        vsg::ImageInfo::create(nearestSampler, worldPosView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};

    graphicsPipelineConfig->assignTexture("depthSampler", depthInfoList);
    graphicsPipelineConfig->assignTexture("worldPosSampler", worldPosInfoList);
    scene->addChild(createFullscreenStateGroup(graphicsPipelineConfig));
}

void SSAOPass::buildStandaloneResolvedMaterialData(vsg::ref_ptr<vsg::Options> options,
                                                   vsg::ref_ptr<vsg::Group> scene,
                                                   vsg::ref_ptr<vsg::ImageView> depthView,
                                                   vsg::ref_ptr<vsg::ImageView> materialView)
{
    auto graphicsPipelineConfig = createFullscreenPipelineConfig(createStandaloneResolvedMaterialShaderSet(options));
    auto nearestSampler = Utils::createNearestClampSampler();
    vsg::ImageInfoList depthInfoList = {
        vsg::ImageInfo::create(nearestSampler, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList materialInfoList = {
        vsg::ImageInfo::create(nearestSampler, materialView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};

    graphicsPipelineConfig->assignTexture("depthSampler", depthInfoList);
    graphicsPipelineConfig->assignTexture("materialSampler", materialInfoList);
    scene->addChild(createFullscreenStateGroup(graphicsPipelineConfig));
}

void SSAOPass::buildStandaloneDeferredDebugData(vsg::ref_ptr<vsg::Options> options,
                                                vsg::ref_ptr<vsg::Group> scene,
                                                vsg::ref_ptr<vsg::ImageView> normalView,
                                                vsg::ref_ptr<vsg::ImageView> worldPosView,
                                                vsg::ref_ptr<vsg::ImageView> materialView,
                                                vsg::ref_ptr<vsg::ImageView> ssaoView,
                                                vsg::BufferInfoList global_buffer_info_list,
                                                bool useMsaaInputs,
                                                VkSampleCountFlagBits outputSamples)
{
    auto graphicsPipelineConfig = createFullscreenPipelineConfig(
        createStandaloneDeferredDebugShaderSet(options, useMsaaInputs),
        outputSamples,
        useMsaaInputs);
    auto nearestSampler = Utils::createNearestClampSampler();
    auto linearSampler = Utils::createLinearSampler();
    vsg::ImageInfoList normalInfoList = {
        vsg::ImageInfo::create(nearestSampler, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList worldPosInfoList = {
        vsg::ImageInfo::create(nearestSampler, worldPosView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList materialInfoList = {
        vsg::ImageInfo::create(nearestSampler, materialView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList ssaoInfoList = {
        vsg::ImageInfo::create(linearSampler, ssaoView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};

    graphicsPipelineConfig->assignTexture("normalSampler", normalInfoList);
    graphicsPipelineConfig->assignTexture("worldPosSampler", worldPosInfoList);
    graphicsPipelineConfig->assignTexture("materialSampler", materialInfoList);
    graphicsPipelineConfig->assignTexture("ssaoSampler", ssaoInfoList);
    graphicsPipelineConfig->assignDescriptor("GlobalBuffer", global_buffer_info_list);

    scene->addChild(createFullscreenStateGroup(graphicsPipelineConfig));
}
