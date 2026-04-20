#include "PresentPass.h"

#include "Utils.h"

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

vsg::ref_ptr<vsg::ShaderSet> createPresentShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreen_quad.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/present_to_msaa.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("createPresentShaderSet(...) could not find shaders.");
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addDescriptorBinding("sourceSampler", "", kMaterialDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    return shaderSet;
}

vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> createPipelineConfig(vsg::ref_ptr<vsg::ShaderSet> shaderSet, VkSampleCountFlagBits samples)
{
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    shaderSet->defaultGraphicsPipelineStates.push_back(rasterizationState);
    shaderSet->defaultGraphicsPipelineStates.push_back(createFullscreenDepthState());
    shaderSet->defaultGraphicsPipelineStates.push_back(vsg::MultisampleState::create(samples));
    return vsg::GraphicsPipelineConfigurator::create(shaderSet);
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
}

void PresentPass::buildPresentToMsaaData(vsg::ref_ptr<vsg::Options> options,
                                         vsg::ref_ptr<vsg::Group> scene,
                                         vsg::ref_ptr<vsg::ImageView> sourceColorView,
                                         VkSampleCountFlagBits samples)
{
    auto graphicsPipelineConfig = createPipelineConfig(createPresentShaderSet(options), samples);
    auto sourceSampler = Utils::createNearestClampSampler();
    vsg::ImageInfoList sourceImageInfoList = {
        vsg::ImageInfo::create(sourceSampler, sourceColorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};

    graphicsPipelineConfig->assignTexture("sourceSampler", sourceImageInfoList);
    scene->addChild(createFullscreenStateGroup(graphicsPipelineConfig));
}
