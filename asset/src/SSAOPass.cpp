#include "SSAOPass.h"

vsg::ref_ptr<vsg::ShaderSet> SSAOPass::customSSAOShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    vsg::info("Local pbr_ShaderSet(", options, ")");

    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/ssao.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/ssao.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("ssao_ShaderSet(...) could not find shaders.");
        return {};
    }

#define MATERIAL_DESCRIPTOR_SET 2

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    shaderSet->addDescriptorBinding(
        "colorInputAttachment",          // Name; must match the GLSL declaration.
        "",                               // No preprocessor macro; subpass 1 always reads it.
        MATERIAL_DESCRIPTOR_SET,  // Descriptor set dedicated to input attachments.
        0,             // Binding index matching GLSL input_attachment_index.
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,  // Must use an input-attachment descriptor.
        1,                                // Single descriptor.
        VK_SHADER_STAGE_FRAGMENT_BIT,     // Read only in the fragment shader.
        vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM})
    );
    shaderSet->addDescriptorBinding(
        "normalInputAttachment",          // Name; must match the GLSL declaration.
        "",                               // No preprocessor macro; subpass 1 always reads it.
        MATERIAL_DESCRIPTOR_SET,  // Descriptor set dedicated to input attachments.
        1,             // Binding index matching GLSL input_attachment_index.
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  // Combined image sampler for this attachment.
        1,                                // Single descriptor.
        VK_SHADER_STAGE_FRAGMENT_BIT,     // Read only in the fragment shader.
        vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM})
    );
    shaderSet->addDescriptorBinding(
        "worldPosInputAttachment",        // Name; must match the GLSL declaration.
        "",                               // No preprocessor macro.
        MATERIAL_DESCRIPTOR_SET,  // Reuse the same input-attachment set.
        2,           // Binding index.
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM})
    );
    shaderSet->addDescriptorBinding("samplerNoise", "", MATERIAL_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));

    return shaderSet;
}

void SSAOPass::buildSSAOData(vsg::ref_ptr<vsg::Options> options, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ImageView> GBufferView0, vsg::ref_ptr<vsg::ImageView> GBufferView1, vsg::ref_ptr<vsg::ImageView> GBufferView2, VkExtent2D extent){
    vsg::ref_ptr<vsg::ShaderSet> model_shaderset = SSAOPass::customSSAOShaderSet(options);
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    model_shaderset->defaultGraphicsPipelineStates.push_back(rasterizationState);
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(model_shaderset);
    graphicsPipelineConfig->subpass = 1;

    unsigned int seed = 100;
    std::mt19937 generator(seed); // Mersenne Twister with good speed and randomness for this use case.
    std::uniform_real_distribution<float> distribution(0, 1.0f);

    auto samplerNoiseData = vsg::ubvec4Array2D::create(extent.width, extent.height); // VSG 2D texture container in RGBA8 format.
    for (uint32_t i = 0; i < extent.width; ++i){
        for (uint32_t j = 0; j < extent.height; ++j){
            float randX = distribution(generator); // Random x component.
            float randY = distribution(generator); // Random y component.
            samplerNoiseData->set(i, j, vsg::ubvec4(static_cast<uint8_t>(randX * 255.0f), 
                    static_cast<uint8_t>(randY * 255.0f), static_cast<uint8_t>(0), static_cast<uint8_t>(0)));
        }
    }
    samplerNoiseData->properties.format = VK_FORMAT_R8G8B8A8_UNORM;
    samplerNoiseData->properties.dataVariance = vsg::DYNAMIC_DATA;

    auto noiseSampler = Utils::createNearestSampler();
    
    vsg::ImageInfoList noiseImageInfoList = {vsg::ImageInfo::create(noiseSampler, samplerNoiseData)};
    graphicsPipelineConfig->assignTexture("samplerNoise", noiseImageInfoList);

    vsg::ref_ptr<vsg::Sampler> in_sampler{nullptr};
    vsg::ImageInfoList GBufferViewList0 = {vsg::ImageInfo::create(noiseSampler, GBufferView0, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList GBufferViewList1 = {vsg::ImageInfo::create(noiseSampler, GBufferView1, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList GBufferViewList2 = {vsg::ImageInfo::create(noiseSampler, GBufferView2, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    graphicsPipelineConfig->assignTexture("colorInputAttachment", GBufferViewList0);
    graphicsPipelineConfig->assignTexture("normalInputAttachment", GBufferViewList1);
    graphicsPipelineConfig->assignTexture("worldPosInputAttachment", GBufferViewList2);

    auto vertices = vsg::vec3Array::create({
        vsg::vec3(-1, -1, 1.0f),
        vsg::vec3(1, -1, 1.0f),
        vsg::vec3(1, 1, 1.0f),
        vsg::vec3(-1, 1, 1.0f)
        });

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
        2, 3, 0});
    
    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));

    VkDrawIndexedIndirectCommand cmd = {
        6,      // indexCount
        1,     // instanceCount
        0,         // firstIndex
        0,         // vertexOffset
        0          // firstInstance
    };

    auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
    indirectBuffer->set(0, cmd);
    auto draw_indirect = vsg::DrawIndexedIndirect::create(
        indirectBuffer,  // Indirect-command buffer.
        1,              // Number of draw commands.
        sizeof(VkDrawIndexedIndirectCommand) // Command stride.
    );
    drawCommands->addChild(draw_indirect);
    graphicsPipelineConfig->init();

    auto stateGroup = vsg::StateGroup::create();
    graphicsPipelineConfig->copyTo(stateGroup);
    stateGroup->addChild(drawCommands);
    scene->addChild(stateGroup);
}

vsg::ref_ptr<vsg::ShaderSet> SSAOPass::customSSAODenoiseShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    vsg::info("Local pbr_ShaderSet(", options, ")");

    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/ssao_denoise.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/ssao_denoise.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("ssao_ShaderSet(...) could not find shaders.");
        return {};
    }

#define MATERIAL_DESCRIPTOR_SET 2

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    shaderSet->addDescriptorBinding(
        "colorInputAttachment",          // Name; must match the GLSL declaration.
        "",                               // No preprocessor macro; subpass 1 always reads it.
        MATERIAL_DESCRIPTOR_SET,  // Descriptor set dedicated to input attachments.
        0,             // Binding index matching GLSL input_attachment_index.
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,  // Must use an input-attachment descriptor.
        1,                                // Single descriptor.
        VK_SHADER_STAGE_FRAGMENT_BIT,     // Read only in the fragment shader.
        vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM})
    );

    shaderSet->addDescriptorBinding(
        "shadowInputAttachment",          // Name; must match the GLSL declaration.
        "",                               // No preprocessor macro; subpass 1 always reads it.
        MATERIAL_DESCRIPTOR_SET,  // Descriptor set dedicated to input attachments.
        1,             // Binding index matching GLSL input_attachment_index.
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,  // Must use an input-attachment descriptor.
        1,                                // Single descriptor.
        VK_SHADER_STAGE_FRAGMENT_BIT,     // Read only in the fragment shader.
        vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM})
    );
    shaderSet->addDescriptorBinding("samplerSSAO", "", MATERIAL_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    
    
    auto colorBlendState = vsg::ColorBlendState::create();
    colorBlendState->attachments[0] = {
        VK_FALSE,                                      // Disable blending.
        VK_BLEND_FACTOR_SRC_ALPHA,                    // Source color factor: current fragment alpha.
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,          // Destination color factor: 1 - source alpha.
        VK_BLEND_OP_ADD,                              // Color blend: src * srcAlpha + dst * (1 - srcAlpha).
        VK_BLEND_FACTOR_ONE,                          // Source alpha factor: 1.
        VK_BLEND_FACTOR_ZERO,                         // Destination alpha factor: 0.
        VK_BLEND_OP_ADD,                              // Alpha blend: srcAlpha * 1 + dstAlpha * 0.
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    colorBlendState->attachments.resize(2, colorBlendState->attachments[0]); 
    shaderSet->defaultGraphicsPipelineStates.push_back(colorBlendState);

    return shaderSet;
}

void SSAOPass::buildSSAODenoiseData(vsg::ref_ptr<vsg::Options> options, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ImageView> GBufferView0, vsg::ref_ptr<vsg::ImageView> ShadowWriteView, vsg::ref_ptr<vsg::ImageView> SSAOResultImageView){
    vsg::ref_ptr<vsg::ShaderSet> model_shaderset = SSAOPass::customSSAODenoiseShaderSet(options);
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    model_shaderset->defaultGraphicsPipelineStates.push_back(rasterizationState);
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(model_shaderset);
    graphicsPipelineConfig->subpass = 2;

    auto noiseSampler = Utils::createNearestSampler();

    vsg::ImageInfoList GBufferViewList0 = {vsg::ImageInfo::create(noiseSampler, GBufferView0, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList GBufferViewListShadow = {vsg::ImageInfo::create(noiseSampler, ShadowWriteView, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList GBufferViewList1 = {vsg::ImageInfo::create(noiseSampler, SSAOResultImageView, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    graphicsPipelineConfig->assignTexture("colorInputAttachment", GBufferViewList0);
    graphicsPipelineConfig->assignTexture("shadowInputAttachment", GBufferViewListShadow);
    graphicsPipelineConfig->assignTexture("samplerSSAO", GBufferViewList1);

    auto vertices = vsg::vec3Array::create({
        vsg::vec3(-1, -1, 1.0f),
        vsg::vec3(1, -1, 1.0f),
        vsg::vec3(1, 1, 1.0f),
        vsg::vec3(-1, 1, 1.0f)
        });

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
        2, 3, 0});

    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);

    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));

    VkDrawIndexedIndirectCommand cmd = {
        6,      // indexCount
        1,     // instanceCount
        0,         // firstIndex
        0,         // vertexOffset
        0          // firstInstance
    };

    auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
    indirectBuffer->set(0, cmd);
    auto draw_indirect = vsg::DrawIndexedIndirect::create(
        indirectBuffer,  // Indirect-command buffer.
        1,              // Number of draw commands.
        sizeof(VkDrawIndexedIndirectCommand) // Command stride.
    );

    drawCommands->addChild(draw_indirect);
    graphicsPipelineConfig->init();

    auto stateGroup = vsg::StateGroup::create();
    graphicsPipelineConfig->copyTo(stateGroup);
    stateGroup->addChild(drawCommands);
    scene->addChild(stateGroup);
}
