// 搁置

//#include "IBLv2.h"
//#include "vkhelper.h"
//
//#define _USE_MATH_DEFINES
//#define STB_IMAGE_IMPLEMENTATION
//
//#include <math.h>
//#include <iostream>
//#include <type_traits>
//#include <vsg/utils/ShaderSet.h>
//#include <stb_image.h>
//
//using namespace vsg;
//
//namespace IBLv2
//{
//
//// static variables
////Textures textures;
////VsgContext vsgContext;
//AppData appData = {};
//
//std::vector<ptr<ShaderSet>> shaderSets;
//
//// static data for local .obj use only (no extern in header)
//static struct _EnvmapRect
//{
//    ptr<Data> image;
//    ptr<ImageView> imageView;
//    ptr<Sampler> sampler;
//    ptr<ImageInfo> imageInfo;
//
//    uint32_t width, height;
//} gEnvmapRect;
//
//static struct skyboxCube
//{
//    ptr<vsg::vec3Array> vertices;
//    ptr<vsg::ushortArray> indices;
//
//} gSkyboxCube;
//
//static struct IBLVkEvents {
//    ptr<Event> envmapCubeRenderedEvent;
//    ptr<Event> envmapCubeGeneratedEvent;
//    ptr<ImageMemoryBarrier> envmapCubeRenderedBarrier;
//    ptr<ImageMemoryBarrier> envmapCubeGeneratedBarrier;
//} gVkEvents;
//
//void createRTTRenderPass(ptr<vsg::Context> context, VkFormat format, VkImageLayout finalLayout, ptr<vsg::RenderPass>& renderPass)
//{
//    vsg::AttachmentDescription attDesc = {};
//    // Color attachment
//    attDesc.format = format;
//    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
//    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
//    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
//    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//    attDesc.finalLayout = finalLayout;
//    // attachment descriptions
//    vsg::RenderPass::Attachments attachments{attDesc};
//    vsg::AttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
//
//    vsg::SubpassDescription subpassDescription = {};
//    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
//    subpassDescription.colorAttachments.emplace_back(colorReference);
//    vsg::RenderPass::Subpasses subpasses = {subpassDescription};
//
//    // Use subpass dependencies for layout transitions
//    vsg::RenderPass::Dependencies dependencies(2);
//    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
//    dependencies[0].dstSubpass = 0;
//    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
//    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
//    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
//    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
//    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
//    dependencies[1].srcSubpass = 0;
//    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
//    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
//    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
//    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
//    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
//    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
//
//    // Create the actual renderpass
//    renderPass = vsg::RenderPass::create(context->device.get(), attachments, subpasses, dependencies);
//}
//
//
//
//void generateBRDFLUT(VsgContext &vsgContext)
//{
//    // TODO: actually create shaders
//    auto searchPaths = appData.options->paths;
//    searchPaths.push_back("D:\\Project\\VSGCAD\\CADVSG_Cloud\\data");
//    //std::cout << searchPaths.size() << std::endl;
//    //for (auto path : searchPaths)
//    //    std::cout << path << std::endl;
//
//    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreenquad.vert", searchPaths);
//    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
//    auto fragShaderFilepath = vsg::findFile("shaders/IBL/genbrdflut.frag", searchPaths);
//    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
//    if (!vertexShader || !fragmentShader)
//    {
//        std::cout << "Could not create shaders." << std::endl;
//        return;
//    }
//
//    ptr<vsg::Window>& window = vsgContext.window;
//    ptr<vsg::Context>& context = vsgContext.context;
//
//    //auto tStart = std::chrono::high_resolution_clock::now();
//    const VkFormat format = VK_FORMAT_R16G16_SFLOAT; // R16G16 is supported pretty much everywhere
//    const int32_t dim = 512;
//    const VkExtent3D extent3d = {dim, dim, 1};
//    const VkExtent2D extent = {dim, dim};
//
//    ptr<vsg::Image>& lutImage = textures.brdfLut;
//    ptr<vsg::ImageView>& lutImageView = textures.brdfLutView;
//    createImage2D(*context, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
//        extent, lutImage, lutImageView);
//    ptr<vsg::Sampler>& lutSampler = textures.brdfLutSampler;
//    createSampler(1, lutSampler);
//
//    ptr<vsg::ImageInfo>& lutImageInfo = textures.brdfLutInfo;
//    createImageInfo(lutImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, lutSampler, lutImageInfo);
//
//    ptr<vsg::RenderPass> renderPass;
//    // attachments / renderPass (subpass for layout trainsition)
//    createRTTRenderPass(context, format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, renderPass);
//    // frame buffer
//    auto fbuf = vsg::Framebuffer::create(renderPass, vsg::ImageViews{lutImageView}, dim, dim, 1);
//
//    auto rtt_rendergraph = vsg::RenderGraph::create();
//    rtt_rendergraph->renderArea.offset = VkOffset2D{0, 0};
//    rtt_rendergraph->renderArea.extent = extent;
//    rtt_rendergraph->clearValues = {{0.0f, 0.0f, 0.0f, 1.0f}};
//    rtt_rendergraph->framebuffer = fbuf;
//
//    // create RenderGraph
//    // descriptors: empty
//    //vsg::DescriptorSetLayoutBindings debugDescriptorBindings = {
//    //    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
//    //};
//    ////auto descriptorSetLayout = vsg::DescriptorSetLayout::create(vsg::DescriptorSetLayoutBindings{});
//    //auto descriptorSetLayout = vsg::DescriptorSetLayout::create(debugDescriptorBindings);
//    //auto texDescriptor = vsg::DescriptorImage::create(gEnvmapRect.sampler, gEnvmapRect.image, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//    //auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texDescriptor});
//
//    // pushConstantRange: empty
//    vsg::PushConstantRanges pushConstantRanges{};
//    // pipelineLayout
//    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{}, vsg::PushConstantRanges{});
//
//    // shader and pipeline object
//    const vsg::ShaderStages shaders{vertexShader, fragmentShader};
//    auto vertexInputState = vsg::VertexInputState::create(); // empty for no input
//    auto inputAssemblyState = vsg::InputAssemblyState::create();
//    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
//    inputAssemblyState->primitiveRestartEnable = VK_FALSE;
//    auto rasterState = vsg::RasterizationState::create();
//    rasterState->cullMode = VK_CULL_MODE_NONE;
//    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
//    colorBlendAttachment.blendEnable = VK_FALSE;
//    colorBlendAttachment.colorWriteMask = 0xf;
//    auto blendState = vsg::ColorBlendState::create(vsg::ColorBlendState::ColorBlendAttachments{colorBlendAttachment});
//    auto multisampleState = vsg::MultisampleState::create(VK_SAMPLE_COUNT_1_BIT);
//    auto depthState = vsg::DepthStencilState::create();
//    depthState->depthTestEnable = VK_FALSE;
//    depthState->depthWriteEnable = VK_FALSE;
//    auto pipelineStates = vsg::GraphicsPipelineStates{
//        vertexInputState,
//        inputAssemblyState,
//        rasterState,
//        multisampleState,
//        blendState,
//        depthState};
//
//    auto pipeline = vsg::GraphicsPipeline::create(pipelineLayout.get(), shaders, pipelineStates, /*subpass=*/0);
//    // the acutall vkCmdBindPipeline command
//    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(pipeline);
//    auto pipelineNode = vsg::StateGroup::create();
//    pipelineNode->add(bindGraphicsPipeline);
//    //auto bindDescriptor = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, descriptorSet);
//    //pipelineNode->add(bindDescriptor);
//    auto draw = vsg::Draw::create(3, 1, 0, 0);
//    pipelineNode->addChild(draw);
//
//    // 应该做一个Dummy Scene中，进行对应的Graphic States管理，加入Descriptor Pipeline绑定和全屏Quad几何绑定
//    // Scene节点挂在rtt_RenderGraph下面, like this
//    // literally vsg::createRenderGraphForView() below
//    auto dummyCamera = vsg::Camera::create(); // or reuse camera from main render loop.
//    auto rtt_view = vsg::View::create(dummyCamera, pipelineNode);
//    //rtt_rendergraph->addChild(rtt_view);
//    rtt_rendergraph->addChild(pipelineNode);
//
//    // 先创建各种贴图和RenderPass （rtt_rendergraph），然后创建Descriptor和（rtt_view/dummyScene）
//    auto commandGraph = vsg::CommandGraph::create(window); // done with
//    //rtt_commandGraph->submitOrder = -1; // render before the main_commandGraph, or nest in main_commandGraph
//    commandGraph->addChild(rtt_rendergraph);
//
//    //vsg::write(rtt_rendergraph, appData.debugOutputPath);
//    auto& viewer = vsgContext.viewer;
//    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
//}
//
//ptr<StateGroup> drawSkyboxVSGNode(VsgContext& context)
//{
//    auto searchPaths = appData.options->paths;
//    searchPaths.push_back("D:\\Project\\VSGCAD\\CADVSG_Cloud\\data");
//    //auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreenquad.vert", searchPaths);
//    //auto fragShaderFilepath = vsg::findFile("shaders/IBL/irradianceCube.frag", searchPaths);
//    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/skybox.vert", searchPaths);
//    auto fragShaderFilepath = vsg::findFile("shaders/IBL/skybox.frag", searchPaths);
//    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
//    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
//    if (!vertexShader || !fragmentShader)
//    {
//        std::cout << "Could not create shaders." << std::endl;
//        return ptr<StateGroup>();
//    }
//
//    auto shaderCompileSettings = ShaderCompileSettings::create();
//    auto shaderStages = ShaderStages{vertexShader, fragmentShader};
//    auto tonemapParams = floatArray::create(2);
//    tonemapParams->set(0, 3.0f); //exposure
//    tonemapParams->set(1, 2.2f); //gamma
//
//    auto skyBoxShaderSet = ShaderSet::create(shaderStages, shaderCompileSettings);
//    skyBoxShaderSet->addAttributeBinding("inPos", "", 0, VK_FORMAT_R32G32B32_SFLOAT, gSkyboxCube.vertices);
//    // vsg这块的API不支持绑定一个DescriptorImage（vsg::Data和vsg::Image是兄弟关系，这里只接收Data）
//    //skyBoxShaderSet->addDescriptorBinding("envmap", "", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, textures.envmapCube);
//    // 但是Data在创建Layout和做绑定的时候没有真正使用，所以这里先随便用个Data代替一下
//    skyBoxShaderSet->addDescriptorBinding("envmap", "", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::EnvmapCube::format}));
//    skyBoxShaderSet->addDescriptorBinding("tonemapParams", "", 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, tonemapParams);
//    skyBoxShaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);
//
//    vsg::DataList pipelineInputs;
//
//    auto pplcfg = vsg::GraphicsPipelineConfigurator::create(skyBoxShaderSet);   
//    auto rasterState = RasterizationState::create();
//    rasterState->cullMode = VK_CULL_MODE_NONE;
//    pplcfg->pipelineStates.push_back(rasterState);
//    auto depthState = vsg::DepthStencilState::create();
//    depthState->depthTestEnable = VK_FALSE;
//    depthState->depthWriteEnable = VK_FALSE;
//    pplcfg->pipelineStates.push_back(depthState);
//    pplcfg->assignArray(pipelineInputs, "inPos", VK_VERTEX_INPUT_RATE_VERTEX, gSkyboxCube.vertices);
//    pplcfg->assignTexture("envmap", ImageInfoList{textures.envmapCubeInfo});
//    pplcfg->assignDescriptor("tonemapParams", tonemapParams);
//    pplcfg->init();
//
//    auto drawCmds = Commands::create();
//    drawCmds->addChild(BindVertexBuffers::create(pplcfg->baseAttributeBinding, pipelineInputs));
//    drawCmds->addChild(BindIndexBuffer::create(gSkyboxCube.indices));
//    drawCmds->addChild(DrawIndexed::create(gSkyboxCube.indices->size(), 1, 0, 0, 0));
//    auto root = StateGroup::create();
//    pplcfg->copyTo(root);
//    root->addChild(drawCmds);
//    return root;
//}
//
//struct IBLDescriptorSetBinding : vsg::Inherit<CustomDescriptorSetBinding, IBLDescriptorSetBinding> 
//{
//    uint32_t set;
//    ptr<DescriptorSet> descriptorSet;
//    ptr<DescriptorSetLayout> descriptorSetLayout;
//
//    IBLDescriptorSetBinding(const uint32_t& in_set, const IBL::Textures& _textures) :
//        set(in_set)
//    {
//        vsg::DescriptorSetLayoutBindings customSetLayoutBindings = {
//            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
//            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
//            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
//        };
//        descriptorSetLayout = DescriptorSetLayout::create(customSetLayoutBindings);
//        auto brdfLutDescriptor = vsg::DescriptorImage::create(_textures.brdfLutInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//        auto irradianceDescriptor = vsg::DescriptorImage::create(_textures.irradianceCubeInfo, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//        auto prefilterDescriptor = vsg::DescriptorImage::create(_textures.prefilterCubeInfo, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//
//        //auto paramsDescriptor = vsg::DescriptorBuffer::create(BufferInfoList{_textures.params}, 3, 0);
//        descriptorSet = DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{brdfLutDescriptor, irradianceDescriptor, prefilterDescriptor/*, paramsDescriptor*/});
//    };
//
//    //int compare(const Object& rhs) const override;
//
//    void read(Input& input) override {}
//    void write(Output& output) const override {}
//
//    bool compatibleDescriptorSetLayout(const DescriptorSetLayout& dsl) const {return descriptorSetLayout->compare(dsl) == 0; }
//
//    ref_ptr<DescriptorSetLayout> createDescriptorSetLayout() override {
//        return descriptorSetLayout;
//    }
//    ref_ptr<StateCommand> createStateCommand(ref_ptr<PipelineLayout> layout) override {
//        return BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, set, descriptorSet);
//    }
//};
////
////struct CustomViewDependentStateBinding : vsg::Inherit<ViewDependentStateBinding, CustomViewDependentStateBinding>
////{
////    uint32_t set;
////    ptr<DescriptorSet> descriptorSet;
////    ptr<DescriptorSetLayout> descriptorSetLayout;
////
////    CustomViewDependentStateBinding(const uint32_t& in_set, ptr<BufferInfo> dataBufferInfo) :
////        set(in_set)
////    {
////        DescriptorSetLayoutBindings descriptorBindings{
////            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // lightData
////            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // viewportData
////            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},                      // shadow map 2D texture array
////            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
////        };
////
////        descriptorSetLayout = DescriptorSetLayout::create(descriptorBindings);
////        auto brdfLutDescriptor = vsg::DescriptorImage::create(_textures.brdfLutInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
////        auto irradianceDescriptor = vsg::DescriptorImage::create(_textures.irradianceCubeInfo, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
////        auto prefilterDescriptor = vsg::DescriptorImage::create(_textures.prefilterCubeInfo, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
////
////        descriptorSet = DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{brdfLutDescriptor, irradianceDescriptor, prefilterDescriptor});
////    };
////
////    //int compare(const Object& rhs) const override;
////
////    void read(Input& input) override {}
////    void write(Output& output) const override {}
////
////    bool compatibleDescriptorSetLayout(const DescriptorSetLayout& dsl) const { return descriptorSetLayout->compare(dsl) == 0; }
////
////    ref_ptr<DescriptorSetLayout> createDescriptorSetLayout() override
////    {
////        return descriptorSetLayout;
////    }
////    ref_ptr<StateCommand> createStateCommand(ref_ptr<PipelineLayout> layout) override
////    {
////        return BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, set, descriptorSet);
////    }
////};
//
//
//vsg::ref_ptr<vsg::ShaderSet> customPbrShaderSet(vsg::ref_ptr<const vsg::Options> options)
//{
//    vsg::info("Local pbr_ShaderSet(", options, ")");
//
//    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/standard.vert", options->paths);
//    auto fragShaderFilepath = vsg::findFile("shaders/IBL/custom_pbr.frag", options->paths);
//    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
//    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
//
//    if (!vertexShader || !fragmentShader)
//    {
//        vsg::error("pbr_ShaderSet(...) could not find shaders.");
//        return {};
//    }
//
//#define CUSTOM_DESCRIPTOR_SET 0
//#define VIEW_DESCRIPTOR_SET 1
//#define MATERIAL_DESCRIPTOR_SET 2
//
//    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
//
//    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
//    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
//    shaderSet->addAttributeBinding("vsg_TexCoord0", "", 2, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
//    shaderSet->addAttributeBinding("vsg_Color", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
//
//    shaderSet->addAttributeBinding("vsg_position", "VSG_INSTANCE_POSITIONS", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
//    shaderSet->addAttributeBinding("vsg_position_scaleDistance", "VSG_BILLBOARD", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
//
//    shaderSet->addDescriptorBinding("displacementMap", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
//    shaderSet->addDescriptorBinding("diffuseMap", "VSG_DIFFUSE_MAP", MATERIAL_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
//    shaderSet->addDescriptorBinding("mrMap", "VSG_METALLROUGHNESS_MAP", MATERIAL_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec2Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32_SFLOAT}));
//    shaderSet->addDescriptorBinding("normalMap", "VSG_NORMAL_MAP", MATERIAL_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32_SFLOAT}));
//    shaderSet->addDescriptorBinding("aoMap", "VSG_LIGHTMAP_MAP", MATERIAL_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
//    shaderSet->addDescriptorBinding("emissiveMap", "VSG_EMISSIVE_MAP", MATERIAL_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
//    shaderSet->addDescriptorBinding("specularMap", "VSG_SPECULAR_MAP", MATERIAL_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
//    shaderSet->addDescriptorBinding("material", "", MATERIAL_DESCRIPTOR_SET, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PbrMaterialValue::create());
//
//    shaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
//    //shaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0, 0, 1280, 1024));
//    shaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
//    shaderSet->addDescriptorBinding("viewMatrixData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::mat4Array::create(4));
//
//    shaderSet->addDescriptorBinding("brdfLut", "", CUSTOM_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::BrdfLUT::format}));
//    shaderSet->addDescriptorBinding("irradiance", "", CUSTOM_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::IrradianceCube::format}));
//    shaderSet->addDescriptorBinding("prefilter", "", CUSTOM_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{Constants::PrefilteredEnvmapCube::format}));
//    //shaderSet->addDescriptorBinding("params", "", CUSTOM_DESCRIPTOR_SET, 3,
//    //                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
//    //                                textures.params);
//
//    auto iblDSBinding = IBLDescriptorSetBinding::create(CUSTOM_DESCRIPTOR_SET, IBL::textures);
//    shaderSet->customDescriptorSetBindings.push_back(iblDSBinding);
//
//    // additional defines
//    shaderSet->optionalDefines = {"VSG_GREYSCALE_DIFFUSE_MAP", "VSG_TWO_SIDED_LIGHTING", "VSG_WORKFLOW_SPECGLOSS"};
//
//    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128);
//
//    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS", "VSG_DISPLACEMENT_MAP"}, vsg::PositionAndDisplacementMapArrayState::create()});
//    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS"}, vsg::PositionArrayState::create()});
//    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_DISPLACEMENT_MAP"}, vsg::DisplacementMapArrayState::create()});
//    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_BILLBOARD"}, vsg::BillboardArrayState::create()});
//
//    shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEW_DESCRIPTOR_SET));
//
//    return shaderSet;
//}
//
//vsg::ref_ptr<vsg::Node> createTestScene(vsg::ref_ptr<vsg::Options> options, bool requiresBase = true)
//{
//    auto builder = vsg::Builder::create();
//    builder->options = options;
//
//    auto scene = vsg::Group::create();
//
//    //std::vector<ptr<ShaderSet>> shaderSets;
//    shaderSets.clear();
//    shaderSets.reserve(144);
//    vsg::GeometryInfo geomInfo;
//    vsg::StateInfo stateInfo;
//    geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);
//
//    for(int row = 0; row < 11; row++) {
//        for(int col = 0; col < 11; col++) {
//            auto shaderSet = customPbrShaderSet(options);
//            shaderSets.push_back(shaderSet);
//
//            PbrMaterial& pbrMaterial = dynamic_cast<PbrMaterialValue*>(shaderSet->getDescriptorBinding("material").data.get())->value();
//            pbrMaterial.roughnessFactor = row * 0.1f;
//            pbrMaterial.metallicFactor = col * 0.1f;
//
//            
//            builder->shaderSet = shaderSet;
//            //geomInfo.cullNode = insertCullNode;
//            //if (textureFile) stateInfo.image = vsg::read_cast<vsg::Data>(textureFile, options);
//
//            auto transformNode = vsg::MatrixTransform::create(translate(3 * row - 15.0, 3 * col - 15.0, 0.0) * scale(2.5));
//            auto sphereNode = builder->createSphere(geomInfo, stateInfo);
//            transformNode->addChild(sphereNode);
//            scene->addChild(transformNode);
//        }
//    }
//    //auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
//    //if (requiresBase)
//    //{
//    //    double diameter = vsg::length(bounds.max - bounds.min);
//    //    geomInfo.position.set((bounds.min.x + bounds.max.x) * 0.5, (bounds.min.y + bounds.max.y) * 0.5, bounds.min.z);
//    //    geomInfo.dx.set(diameter, 0.0, 0.0);
//    //    geomInfo.dy.set(0.0, diameter, 0.0);
//    //    geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);
//
//    //    stateInfo.two_sided = true;
//
//    //    scene->addChild(builder->createQuad(geomInfo, stateInfo));
//    //}
//    //vsg::info("createTestScene() extents = ", bounds);
//    return scene;
//}
//
//ptr<Node> iblDemoSceneGraph(VsgContext& context)
//{
//    auto options = vsg::Options::create(*appData.options);
//    auto shaderSet = customPbrShaderSet(options);
//
//    //vsg::DescriptorSetLayoutBindings descriptorSetLayoutBindings = {
//    //    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
//    //    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
//    //    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
//    //};
//
//    //auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorSetLayoutBindings);
//    //// And actual Descriptor for cubemap texture
//    //auto envmapRectDescriptor = vsg::DescriptorImage::create(gEnvmapRect.imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//    //auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{envmapRectDescriptor});
//
//    //auto iblSamplerDescriptorSetLayout = shaderSet->createDescriptorSetLayout({""}, 2);
//    //for(auto &binding : iblSamplerDescriptorSetLayout->bindings) {
//    //    binding.pImmutableSamplers = nullptr;
//    //}
//
//    //auto pplLayout = shaderSet->createPipelineLayout({}, {0, 2});
//    //auto stateGroup = vsg::StateGroup::create();
//    //stateGroup->add(IBLDescriptorSetBinding::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pplLayout, iblDescriptorSet));
//    
//    //auto sharedObjectsFromSceneBuilder = SharedObjects::create();
//    options->shaderSets["pbribl"] = shaderSet;
//    //options->inheritedState = stateGroup->stateCommands;
//    //options->sharedObjects = sharedObjectsFromSceneBuilder;
//    auto scene = createTestScene(options, false);
//    //vsg::write(scene, appData.debugOutputPath);
//    return scene;
//}
//
//} // namespace IBL