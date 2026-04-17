#include "SSAOPass.h"

namespace
{
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

    auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
    indirectBuffer->set(0, VkDrawIndexedIndirectCommand{6, 1, 0, 0, 0});
    drawCommands->addChild(vsg::DrawIndexedIndirect::create(
        indirectBuffer,
        1,
        sizeof(VkDrawIndexedIndirectCommand)));

    graphicsPipelineConfig->init();

    auto stateGroup = vsg::StateGroup::create();
    graphicsPipelineConfig->copyTo(stateGroup);
    stateGroup->addChild(drawCommands);
    return stateGroup;
}
}

vsg::ref_ptr<vsg::ShaderSet> SSAOPass::customStandaloneSSAOShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreen_quad.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/ssao_standalone.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("customStandaloneSSAOShaderSet(...) could not find shaders.");
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addDescriptorBinding("normalSampler", "", kMaterialDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    shaderSet->addDescriptorBinding("worldPosSampler", "", kMaterialDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    shaderSet->addDescriptorBinding("samplerNoise", "", kMaterialDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    shaderSet->addDescriptorBinding("GlobalBuffer", "", kMaterialDescriptorSet, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubyteArray::create(sizeof(GlobalConstantData)));
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128);
    return shaderSet;
}

void SSAOPass::buildStandaloneSSAOData(vsg::ref_ptr<vsg::Options> options,
                                       vsg::ref_ptr<vsg::Group> scene,
                                       vsg::ref_ptr<vsg::ImageView> normalView,
                                       vsg::ref_ptr<vsg::ImageView> worldPosView,
                                       VkExtent2D outputExtent,
                                       vsg::BufferInfoList global_buffer_info_list)
{
    auto shaderSet = SSAOPass::customStandaloneSSAOShaderSet(options);
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    shaderSet->defaultGraphicsPipelineStates.push_back(rasterizationState);
    shaderSet->defaultGraphicsPipelineStates.push_back(createFullscreenDepthState());

    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shaderSet);
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

vsg::ref_ptr<vsg::ShaderSet> SSAOPass::customStandaloneSSAOCompositeShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreen_quad.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/ssao_composite.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("customStandaloneSSAOCompositeShaderSet(...) could not find shaders.");
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addDescriptorBinding("colorSampler", "", kMaterialDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    shaderSet->addDescriptorBinding("ssaoSampler", "", kMaterialDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    shaderSet->addDescriptorBinding("GlobalBuffer", "", kMaterialDescriptorSet, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubyteArray::create(sizeof(GlobalConstantData)));

    return shaderSet;
}

void SSAOPass::buildStandaloneSSAOCompositeData(vsg::ref_ptr<vsg::Options> options,
                                                vsg::ref_ptr<vsg::Group> scene,
                                                vsg::ref_ptr<vsg::ImageView> colorView,
                                                vsg::ref_ptr<vsg::ImageView> ssaoView,
                                                vsg::BufferInfoList global_buffer_info_list)
{
    auto shaderSet = SSAOPass::customStandaloneSSAOCompositeShaderSet(options);
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    shaderSet->defaultGraphicsPipelineStates.push_back(rasterizationState);
    shaderSet->defaultGraphicsPipelineStates.push_back(createFullscreenDepthState());

    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shaderSet);
    auto colorSampler = Utils::createNearestClampSampler();
    auto ssaoSampler = Utils::createLinearSampler();
    vsg::ImageInfoList colorImageInfoList = {
        vsg::ImageInfo::create(colorSampler, colorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList ssaoImageInfoList = {
        vsg::ImageInfo::create(ssaoSampler, ssaoView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};

    graphicsPipelineConfig->assignTexture("colorSampler", colorImageInfoList);
    graphicsPipelineConfig->assignTexture("ssaoSampler", ssaoImageInfoList);
    graphicsPipelineConfig->assignDescriptor("GlobalBuffer", global_buffer_info_list);

    scene->addChild(createFullscreenStateGroup(graphicsPipelineConfig));
}
