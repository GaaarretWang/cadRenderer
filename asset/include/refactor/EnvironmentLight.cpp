// 搁置

//#include "IBLv2.h"
//
//class LoadHdrImageSTBI : public vsg::Visitor
//{
//private:
//    std::string mpFilepath;
//    float* mpData;
//    uint32_t mpWidth, mpHeight, mpNumChannels;
//
//    template<class A>
//    void update(A& image)
//    {
//        float* pData = mpData;
//        if (pData == nullptr)
//        {
//            std::cerr << "Image not loaded, use LoadHdriImageSTBI::readImage() first" << std::endl;
//            return;
//        }
//
//        using value_type = typename A::value_type;
//
//        for (size_t r = 0; r < image.height(); ++r)
//        {
//            value_type* ptr = &image.at(0, r);
//            for (size_t c = 0; c < image.width(); ++c)
//            {
//
//                // floatArray2D
//                if constexpr (std::is_same_v<value_type, float>)
//                {
//                    (*ptr) = pData[0];
//                }
//                // vec3Array2D or vec4Array2D
//                else
//                {
//                    ptr->r = pData[0];
//                    ptr->g = pData[1];
//                    ptr->b = pData[2];
//                    // vec4Array2D
//                    if constexpr (std::is_same_v<value_type, vsg::vec4>) ptr->a = 1.0f;
//                }
//                ++ptr;
//                pData += mpNumChannels;
//            }
//        }
//        image.dirty();
//    }
//
//public:
//    LoadHdrImageSTBI() :
//        mpFilepath(""), mpData(nullptr), mpWidth(0), mpHeight(0), mpNumChannels(0) {}
//
//    LoadHdrImageSTBI(const std::string& filepath) :
//        mpFilepath(filepath), mpData(nullptr), mpWidth(0), mpHeight(0), mpNumChannels(0) {}
//
//    ~LoadHdrImageSTBI()
//    {
//        freeImage();
//    }
//
//    // use the vsg::Visitor to safely cast to types handled by the UpdateImage class
//    void apply(vsg::floatArray2D& image) override { update(image); }
//    void apply(vsg::vec3Array2D& image) override { update(image); }
//    void apply(vsg::vec4Array2D& image) override { update(image); }
//
//    void updateBuffer(vsg::Data* image)
//    {
//        image->accept(*this);
//    }
//
//    void readImage(const std::string& filepath)
//    {
//        this->mpFilepath = filepath;
//        //stbi_set_flip_vertically_on_load(true);
//        mpData = stbi_loadf(filepath.c_str(), (int*)&mpWidth, (int*)&mpHeight, (int*)&mpNumChannels, 3);
//    }
//
//    vsg::ivec3 getDimensions()
//    {
//        return vsg::ivec3(mpWidth, mpHeight, mpNumChannels);
//    }
//
//    void freeImage()
//    {
//        if (mpData != nullptr)
//        {
//            stbi_image_free(mpData);
//            mpData = nullptr;
//        }
//    }
//};
//
//void EnvironmentLight::createResources()
//{
//    // environment cubemap
//    createImageCube(
//        Constants::EnvmapCube::format,
//        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // DST for mipmap generation
//        Constants::EnvmapCube::extent,
//        Constants::EnvmapCube::numMips,
//        mEnvmapCube);
//    createImageViewCube(context, mEnvmapCube, VK_IMAGE_ASPECT_COLOR_BIT, mEnvmapCubeView);
//    createSampler(Constants::EnvmapCube::numMips, mEnvmapCubeSampler);
//    createImageInfo(
//        mEnvmapCubeView,
//        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//        mEnvmapCubeSampler,
//        mEnvmapCubeInfo);
//    // irradiance cubemap
//    createImageCube(
//        Constants::IrradianceCube::format,
//        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
//        Constants::IrradianceCube::extent,
//        Constants::IrradianceCube::numMips,
//        mIrradianceCube);
//    createImageViewCube(context, mIrradianceCube, VK_IMAGE_ASPECT_COLOR_BIT, mIrradianceCubeView);
//    createSampler(Constants::IrradianceCube::numMips, mIrradianceCubeSampler);
//    createImageInfo(
//        mIrradianceCubeView,
//        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//        mIrradianceCubeSampler,
//        mIrradianceCubeInfo);
//    // prefiltered environment cubemap
//    createImageCube(
//        Constants::PrefilteredEnvmapCube::format,
//        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
//        Constants::PrefilteredEnvmapCube::extent,
//        Constants::PrefilteredEnvmapCube::numMips,
//        mPrefilterCube);
//    createImageViewCube(context, mPrefilterCube, VK_IMAGE_ASPECT_COLOR_BIT, mPrefilterCubeView);
//    createSampler(Constants::PrefilteredEnvmapCube::numMips, mPrefilterCubeSampler);
//    createImageInfo(
//        mPrefilterCubeView,
//        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//        mPrefilterCubeSampler,
//        mPrefilterCubeInfo);
//}
//
//
//void EnvironmentLight::loadEnvmapRect()
//{
//    auto searchPaths = appData.options->paths;
//    searchPaths.push_back("D:\\Project\\VSGCAD\\CADVSG_Cloud\\data");
//
//    auto evnmapFilepath = vsg::findFile("textures/test.hdr", searchPaths);
//    LoadHdrImageSTBI loader;
//    loader.readImage(evnmapFilepath.string());
//    auto dimensions = loader.getDimensions();
//    gEnvmapRect.width = dimensions.x;
//    gEnvmapRect.height = dimensions.y;
//    gEnvmapRect.image = vsg::vec4Array2D::create(dimensions.x, dimensions.y);
//    gEnvmapRect.image->properties.format = VK_FORMAT_R32G32B32A32_SFLOAT;
//    loader.updateBuffer(gEnvmapRect.image);
//
//    gEnvmapRect.sampler = Sampler::create();
//    gEnvmapRect.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
//    gEnvmapRect.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
//
//    gEnvmapRect.imageInfo = ImageInfo::create(gEnvmapRect.sampler, gEnvmapRect.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//}
//
//void EnvironmentLight::generateEnvmap()
//{
//    auto searchPaths = appData.options->paths;
//    searchPaths.push_back("D:\\Project\\VSGCAD\\CADVSG_Cloud\\data");
//    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreenquad.vert", searchPaths);
//    auto fragShaderFilepath = vsg::findFile("shaders/IBL/equirect2cube.frag", searchPaths);
//    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
//    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
//    if (!vertexShader || !fragmentShader)
//    {
//        std::cout << "Could not create shaders." << std::endl;
//        return;
//    }
//
//    loadEnvmapRect();
//
//    ptr<vsg::RenderPass> renderPass;
//    createRTTRenderPass(context, Constants::EnvmapCube::format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, renderPass);
//
//    // offscreen frame buffer (single 2D framebuffer reused for 6 faces)
//    ptr<vsg::Image> pFBImage;
//    ptr<vsg::ImageView> pFBImageView;
//    auto fbUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
//    createImage2D(*context, Constants::EnvmapCube::format, fbUsageFlags, Constants::EnvmapCube::extent, pFBImage, pFBImageView);
//    pFBImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//    pFBImageView->subresourceRange.baseMipLevel = 0;
//    pFBImageView->subresourceRange.baseArrayLayer = 0;
//
//    auto offscreenFB = vsg::Framebuffer::create(renderPass, vsg::ImageViews{pFBImageView}, Constants::EnvmapCube::dim, Constants::EnvmapCube::dim, 1);
//
//    // Create render graph
//    // DescriptorSetLayout
//    vsg::DescriptorSetLayoutBindings descriptorSetLayoutBindings = {
//        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
//    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorSetLayoutBindings);
//    // And actual Descriptor for cubemap texture
//    auto envmapRectDescriptor = vsg::DescriptorImage::create(gEnvmapRect.imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{envmapRectDescriptor});
//
//    vsg::PushConstantRanges pushConstantRanges{
//        {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128 + 4}};
//
//    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
//
//    const vsg::ShaderStages shaders{vertexShader, fragmentShader};
//
//    auto vertexInputState = vsg::VertexInputState::create();
//    auto inputAssemblyState = vsg::InputAssemblyState::create();
//    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
//    inputAssemblyState->primitiveRestartEnable = VK_FALSE;
//    auto rasterState = vsg::RasterizationState::create();
//    rasterState->cullMode = VK_CULL_MODE_NONE; // none to render cube inner face
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
//    auto pipeline = vsg::GraphicsPipeline::create(pipelineLayout.get(), shaders, pipelineStates, /*subpass=*/0);
//
//    // CommandGraph to hold the different RenderGraphs used to render each view
//    auto commandGraph = vsg::CommandGraph::create(window);
//
//    VkImageSubresourceRange fbSubresRange = {};
//    fbSubresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//    fbSubresRange.baseArrayLayer = 0;
//    fbSubresRange.baseMipLevel = 0;
//    fbSubresRange.layerCount = 1;
//    fbSubresRange.levelCount = 0;
//    VkImageSubresourceLayers fbSubresLayers = {};
//    fbSubresLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//    fbSubresLayers.baseArrayLayer = 0;
//    fbSubresLayers.layerCount = 1;
//    fbSubresLayers.mipLevel = 0;
//    VkOffset3D fbSubresourceBound = {Constants::EnvmapCube::dim, Constants::EnvmapCube::dim, 1};
//
//    // set envmapCube (all mips) to TRANSFER_DST before every render pass
//    VkImageSubresourceRange cubeAllMipSubresRange = {};
//    cubeAllMipSubresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//    cubeAllMipSubresRange.baseArrayLayer = 0;
//    cubeAllMipSubresRange.baseMipLevel = 0;
//    cubeAllMipSubresRange.layerCount = 6;
//    cubeAllMipSubresRange.levelCount = Constants::EnvmapCube::numMips;
//
//    auto setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.envmapCube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cubeAllMipSubresRange);
//    commandGraph->addChild(setCubeLayoutTransferDst);
//
//    for (uint32_t f = 0; f < 6; f++)
//    {
//        auto viewportState = vsg::ViewportState::create(0, 0, Constants::EnvmapCube::dim, Constants::EnvmapCube::dim);
//        //auto camera = vsg::Camera::create(dummyProjMatrix, dummyViewMatrix, viewportState);
//        //auto dummyViewMatrix = vsg::RelativeViewMatrix(matrices[f], )
//        auto camera = vsg::Camera::create();
//
//        // bind & draw by vsgScene traversal
//        auto bindStates = vsg::StateGroup::create();
//        bindStates->add(vsg::BindGraphicsPipeline::create(pipeline));
//        bindStates->add(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, descriptorSet));
//        auto faceIdx = vsg::uintValue::create(f);
//        // sit behind two matrix4x4 pushed by vsg::Camera here
//        //bindStates->add(PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, projMatValue));
//        //bindStates->add(PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 64, viewMatValues[f]));
//        bindStates->add(PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 128, faceIdx));
//        // fullscreen quad
//        auto drawFullscreenQuad = vsg::Draw::create(3, 1, 0, 0);
//        bindStates->addChild(drawFullscreenQuad);
//        //bindStates->addChild(BindVertexBuffers::create(0, vsg::DataList{gSkyboxCube.vertices}));
//        //bindStates->addChild(BindIndexBuffer::create(gSkyboxCube.indices));
//        //bindStates->addChild(DrawIndexed::create(gSkyboxCube.indices->size(), 1, 0, 0, 0));
//
//        // dummy camera for MVP push constants, dummyScene for bind & draw
//        auto view = vsg::View::create(camera, bindStates);
//        // warp begin/end renderpass to every bind & draw
//        auto rendergraph = vsg::RenderGraph::create();
//        rendergraph->renderArea.offset = VkOffset2D{0, 0};
//        rendergraph->renderArea.extent = Constants::EnvmapCube::extent;
//        rendergraph->clearValues = {{0.0f, 0.0f, 0.0f, 1.0f}};
//        rendergraph->framebuffer = offscreenFB;
//        rendergraph->addChild(view);
//
//        // renderpass to offscreen framebuffer
//        commandGraph->addChild(rendergraph);
//
//        // setup barrier and copy framebuffer to cubemap face.
//        // renderpass之后，fb的layout为VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL，需要再转换到VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
//        auto setFBLayoutTransferSrc = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
//        commandGraph->addChild(setFBLayoutTransferSrc);
//
//        auto copyFBToCubeFace = vsg::CopyImage::create();
//        VkImageCopy copyRegion = {};
//        copyRegion.srcSubresource = fbSubresLayers;
//        copyRegion.srcOffset = {0, 0, 0};
//        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//        copyRegion.dstSubresource.baseArrayLayer = f;
//        copyRegion.dstSubresource.layerCount = 1;
//        copyRegion.dstSubresource.mipLevel = 0;
//        copyRegion.dstOffset = {0, 0, 0};
//        copyRegion.extent.width = static_cast<uint32_t>(Constants::EnvmapCube::dim);
//        copyRegion.extent.height = static_cast<uint32_t>(Constants::EnvmapCube::dim);
//        copyRegion.extent.depth = 1;
//        copyFBToCubeFace->regions = {copyRegion};
//        copyFBToCubeFace->srcImage = pFBImage;
//        copyFBToCubeFace->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//        copyFBToCubeFace->dstImage = textures.envmapCube;
//        copyFBToCubeFace->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//        commandGraph->addChild(copyFBToCubeFace);
//
//        // blit to higher mips
//        for (uint32_t targetMipLevel = 1; targetMipLevel < Constants::EnvmapCube::numMips; targetMipLevel++)
//        {
//            int32_t mipDim = (int32_t)Constants::EnvmapCube::dim >> targetMipLevel;
//            VkImageBlit blitRegion = {};
//            blitRegion.srcSubresource = fbSubresLayers;
//            blitRegion.srcOffsets[1] = fbSubresourceBound;
//            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//            blitRegion.dstSubresource.baseArrayLayer = f;
//            blitRegion.dstSubresource.layerCount = 1;
//            blitRegion.dstSubresource.mipLevel = targetMipLevel;
//            blitRegion.dstOffsets[1] = {mipDim, mipDim, 1};
//
//            auto blitFBToCubeFaceMip = vsg::BlitImage::create();
//            blitFBToCubeFaceMip->regions = {blitRegion};
//            blitFBToCubeFaceMip->srcImage = pFBImage;
//            blitFBToCubeFaceMip->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//            blitFBToCubeFaceMip->dstImage = textures.envmapCube;
//            blitFBToCubeFaceMip->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//            commandGraph->addChild(blitFBToCubeFaceMip);
//        }
//    }
//
//    // set for shader use layout after mipmap generaton. & signal event for further lut generation
//    auto setEvent = vsg::SetEvent::create(gVkEvents.envmapCubeGeneratedEvent, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
//    commandGraph->addChild(setEvent);
//    auto setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.envmapCube, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cubeAllMipSubresRange);
//    commandGraph->addChild(setCubeLayoutShaderRead);
//    gVkEvents.envmapCubeGeneratedBarrier = setCubeLayoutShaderRead->imageMemoryBarriers.at(0);
//    viewer->addRecordAndSubmitTaskAndPresentation({commandGraph});
//}
//
//void generateIrradianceCube(VsgContext& vsgContext)
//{
//    auto searchPaths = appData.options->paths;
//    searchPaths.push_back("D:\\Project\\VSGCAD\\CADVSG_Cloud\\data");
//    //auto vertexShaderFilepath = vsg::findFile("shaders/IBL/fullscreenquad.vert", searchPaths);
//    //auto fragShaderFilepath = vsg::findFile("shaders/IBL/irradianceCube.frag", searchPaths);
//    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/skyboxCubegen.vert", searchPaths);
//    auto fragShaderFilepath = vsg::findFile("shaders/IBL/irradianceCubeMesh.frag", searchPaths);
//    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
//    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
//    if (!vertexShader || !fragmentShader)
//    {
//        std::cout << "Could not create shaders." << std::endl;
//        return;
//    }
//
//    auto& window = vsgContext.window;
//    auto& context = vsgContext.context;
//    auto& viewer = vsgContext.viewer;
//
//    // CommandGraph to hold the different RenderGraphs used to render each view
//    auto commandGraph = vsg::CommandGraph::create(window);
//
//    auto waitEnvmapCubeGenerated = vsg::WaitEvents::create(
//        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//        gVkEvents.envmapCubeGeneratedEvent,
//        gVkEvents.envmapCubeGeneratedBarrier);
//    commandGraph->addChild(waitEnvmapCubeGenerated);
//
//    ptr<vsg::RenderPass> renderPass;
//    createRTTRenderPass(context, Constants::IrradianceCube::format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, renderPass);
//
//    // offscreen frame buffer (single 2D framebuffer reused for 6 faces)
//    ptr<vsg::Image> pFBImage;
//    ptr<vsg::ImageView> pFBImageView;
//    auto fbUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
//    createImage2D(*context, Constants::IrradianceCube::format, fbUsageFlags, Constants::IrradianceCube::extent, pFBImage, pFBImageView);
//    pFBImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//    pFBImageView->subresourceRange.baseMipLevel = 0;
//    pFBImageView->subresourceRange.baseArrayLayer = 0;
//
//    auto offscreenFB = vsg::Framebuffer::create(renderPass, vsg::ImageViews{pFBImageView}, Constants::IrradianceCube::dim, Constants::IrradianceCube::dim, 1);
//
//    // Create render graph
//    // DescriptorSetLayout
//    vsg::DescriptorSetLayoutBindings descriptorSetLayoutBindings = {
//        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
//    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorSetLayoutBindings);
//    // And actual Descriptor for cubemap texture
//    auto envmapRectDescriptor = vsg::DescriptorImage::create(textures.envmapCubeInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{envmapRectDescriptor});
//
//    vsg::PushConstantRanges pushConstantRanges = {
//        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128}};
//
//    // pipeline layout
//    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
//
//    // create pipeline
//    const vsg::ShaderStages shaders{vertexShader, fragmentShader};
//
//    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
//        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}};
//
//    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
//        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}};
//    auto vertexInputState = vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions);
//    auto inputAssemblyState = vsg::InputAssemblyState::create();
//    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
//    inputAssemblyState->primitiveRestartEnable = VK_FALSE;
//    auto rasterState = vsg::RasterizationState::create();
//    rasterState->cullMode = VK_CULL_MODE_NONE; // none to render cube inner face
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
//    auto pipeline = vsg::GraphicsPipeline::create(pipelineLayout.get(), shaders, pipelineStates, /*subpass=*/0);
//
//    auto setFBLayoutAttachment = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//    commandGraph->addChild(setFBLayoutAttachment);
//
//    // set irradianceCube to TRANSFER_DST before every render pass
//    VkImageSubresourceRange subresourceRange = {};
//    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//    subresourceRange.baseMipLevel = 0;
//    subresourceRange.levelCount = Constants::IrradianceCube::numMips;
//    subresourceRange.layerCount = 6;
//    auto setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.irradianceCube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
//    commandGraph->addChild(setCubeLayoutTransferDst);
//
//    std::vector<vsg::mat4> viewMats = {
//        // POSITIVE_X
//        rotate(radians(90.0f), vec3(0.0f, 1.0f, 0.0f)) * rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
//        // NEGATIVE_X
//        rotate(radians(-90.0f), vec3(0.0f, 1.0f, 0.0f)) * rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
//        // POSITIVE_Y
//        rotate(radians(-90.0f), vec3(1.0f, 0.0f, 0.0f)),
//        // NEGATIVE_Y
//        rotate(radians(90.0f), vec3(1.0f, 0.0f, 0.0f)),
//        // POSITIVE_Z
//        rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
//        // NEGATIVE_Z
//        rotate(radians(180.0f), vec3(0.0f, 0.0f, 1.0f)),
//    };
//
//    auto dummyProjMatrix = vsg::Perspective::create(vsg::degrees(M_PI / 2), 1.0, 0.1, 512.0);
//    std::vector<ptr<ViewMatrix>> myViews(6);
//    for (int i = 0; i < 6; i++)
//    {
//        myViews[i] = MyViewMatrix::create(viewMats[i]);
//    }
//
//    for (uint32_t m = 0; m < Constants::IrradianceCube::numMips; m++)
//    {
//        uint32_t mipDim = Constants::IrradianceCube::dim >> m;
//        auto viewportState = vsg::ViewportState::create(0, 0, mipDim, mipDim);
//        for (uint32_t f = 0; f < 6; f++)
//        {
//            auto camera = vsg::Camera::create(dummyProjMatrix, myViews[f], viewportState);
//            // bind & draw by vsgScene traversal
//            auto bindStates = vsg::StateGroup::create();
//            bindStates->add(vsg::BindGraphicsPipeline::create(pipeline));
//            bindStates->add(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, descriptorSet));
//
//            // fullscreen quad
//            //auto drawFullscreenQuad = vsg::Draw::create(3, 1, 0, 0);
//            //bindStates->addChild(drawFullscreenQuad);
//            auto drawSkybox = vsg::Group::create();
//            drawSkybox->addChild(BindVertexBuffers::create(0, DataList{gSkyboxCube.vertices}));
//            drawSkybox->addChild(BindIndexBuffer::create(gSkyboxCube.indices));
//            drawSkybox->addChild(DrawIndexed::create(gSkyboxCube.indices->size(), 1, 0, 0, 0));
//            bindStates->addChild(drawSkybox);
//            auto view = vsg::View::create(camera, bindStates);
//
//            // warp begin/end renderpass to every bind & draw
//            auto rendergraph = vsg::RenderGraph::create();
//            rendergraph->renderArea.offset = VkOffset2D{0, 0};
//            rendergraph->renderArea.extent = VkExtent2D{mipDim, mipDim};
//            rendergraph->clearValues = {{0.0f, 0.0f, 0.0f, 1.0f}};
//            rendergraph->framebuffer = offscreenFB;
//            rendergraph->addChild(view);
//
//            // renderpass to offscreen framebuffer
//            commandGraph->addChild(rendergraph);
//            // renderpass之后，fb的layout为VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL，需要再转换到VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
//            auto setFBLayoutTransfeSrc = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
//            commandGraph->addChild(setFBLayoutTransfeSrc);
//
//            auto copyFBToCubeMap = vsg::CopyImage::create();
//            VkImageCopy copyRegion = {};
//            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//            copyRegion.srcSubresource.baseArrayLayer = 0;
//            copyRegion.srcSubresource.mipLevel = 0;
//            copyRegion.srcSubresource.layerCount = 1;
//            copyRegion.srcOffset = {0, 0, 0};
//            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//            copyRegion.dstSubresource.baseArrayLayer = f;
//            copyRegion.dstSubresource.mipLevel = m;
//            copyRegion.dstSubresource.layerCount = 1;
//            copyRegion.dstOffset = {0, 0, 0};
//            copyRegion.extent.width = mipDim;
//            copyRegion.extent.height = mipDim;
//            copyRegion.extent.depth = 1;
//            copyFBToCubeMap->regions = {copyRegion};
//            copyFBToCubeMap->srcImage = pFBImage;
//            copyFBToCubeMap->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//            copyFBToCubeMap->dstImage = textures.irradianceCube;
//            copyFBToCubeMap->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//
//            commandGraph->addChild(copyFBToCubeMap);
//            //ptr<PipelineBarrier> setFBLayoutAttachment = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT,
//            //                                                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
//            commandGraph->addChild(setFBLayoutAttachment);
//        }
//    }
//    auto setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.irradianceCube,
//                                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//                                                                    subresourceRange);
//    commandGraph->addChild(setCubeLayoutShaderRead);
//
//    viewer->addRecordAndSubmitTaskAndPresentation({commandGraph});
//}
//
//void generatePrefilteredEnvmapCube(VsgContext& vsgContext)
//{
//    auto searchPaths = appData.options->paths;
//    searchPaths.push_back("D:\\Project\\VSGCAD\\CADVSG_Cloud\\data");
//    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/skyboxCubegen.vert", searchPaths);
//    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
//    auto fragShaderFilepath = vsg::findFile("shaders/IBL/prefilterenvmapMesh.frag", searchPaths);
//    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);
//    if (!vertexShader || !fragmentShader)
//    {
//        std::cout << "Could not create shaders." << std::endl;
//        return;
//    }
//
//    auto& window = vsgContext.window;
//    auto& context = vsgContext.context;
//    auto& viewer = vsgContext.viewer;
//
//    auto commandGraph = vsg::CommandGraph::create(window);
//
//    auto waitEnvmapCubeGenerated = vsg::WaitEvents::create(
//        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//        gVkEvents.envmapCubeGeneratedEvent,
//        gVkEvents.envmapCubeGeneratedBarrier);
//    commandGraph->addChild(waitEnvmapCubeGenerated);
//
//    ptr<vsg::RenderPass> renderPass;
//    createRTTRenderPass(context, Constants::PrefilteredEnvmapCube::format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, renderPass);
//
//    // offscreen frame buffer (single 2D framebuffer reused for 6 faces)
//    ptr<vsg::Image> pFBImage;
//    ptr<vsg::ImageView> pFBImageView;
//    auto fbUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
//    createImage2D(*context, Constants::PrefilteredEnvmapCube::format, fbUsageFlags, Constants::PrefilteredEnvmapCube::extent, pFBImage, pFBImageView);
//    pFBImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//    pFBImageView->subresourceRange.baseMipLevel = 0;
//    pFBImageView->subresourceRange.baseArrayLayer = 0;
//
//    auto offscreenFB = vsg::Framebuffer::create(renderPass, vsg::ImageViews{pFBImageView}, Constants::PrefilteredEnvmapCube::dim, Constants::PrefilteredEnvmapCube::dim, 1);
//
//    // Create render graph
//    // DescriptorSetLayout
//    vsg::DescriptorSetLayoutBindings descriptorSetLayoutBindings = {
//        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
//    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorSetLayoutBindings);
//    // And actual Descriptor for cubemap texture
//    auto envmapRectDescriptor = vsg::DescriptorImage::create(textures.envmapCubeInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{envmapRectDescriptor});
//
//    vsg::PushConstantRanges pushConstantRanges = {
//        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128},
//        {VK_SHADER_STAGE_FRAGMENT_BIT, 128, 12},
//    }; // faceIdx (u32=4) + roughness(float=4)
//
//    // pipeline layout
//    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
//
//    // create pipeline
//    const vsg::ShaderStages shaders{vertexShader, fragmentShader};
//
//    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
//        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}};
//
//    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
//        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}};
//    auto vertexInputState = vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions);
//    auto inputAssemblyState = vsg::InputAssemblyState::create();
//    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
//    inputAssemblyState->primitiveRestartEnable = VK_FALSE;
//    auto rasterState = vsg::RasterizationState::create();
//    rasterState->cullMode = VK_CULL_MODE_NONE; // none to render cube inner face
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
//    auto pipeline = vsg::GraphicsPipeline::create(pipelineLayout.get(), shaders, pipelineStates, /*subpass=*/0);
//
//    auto setFBLayoutAttachment = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//    commandGraph->addChild(setFBLayoutAttachment);
//
//    // set irradianceCube to TRANSFER_DST before every render pass
//    VkImageSubresourceRange subresourceRange = {};
//    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//    subresourceRange.baseMipLevel = 0;
//    subresourceRange.levelCount = Constants::PrefilteredEnvmapCube::numMips;
//    subresourceRange.layerCount = 6;
//    auto setCubeLayoutTransferDst = createImageLayoutPipelineBarrier(textures.prefilterCube, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
//    commandGraph->addChild(setCubeLayoutTransferDst);
//
//    auto dummyProjMatrix = vsg::Perspective::create(degrees(M_PI / 2.0), 1.0, 0.1, 512.0);
//    std::vector<vsg::mat4> viewMats = {
//        // POSITIVE_X
//        rotate(radians(90.0f), vec3(0.0f, 1.0f, 0.0f)) * rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
//        // NEGATIVE_X
//        rotate(radians(-90.0f), vec3(0.0f, 1.0f, 0.0f)) * rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
//        // POSITIVE_Y
//        rotate(radians(-90.0f), vec3(1.0f, 0.0f, 0.0f)),
//        // NEGATIVE_Y
//        rotate(radians(90.0f), vec3(1.0f, 0.0f, 0.0f)),
//        // POSITIVE_Z
//        rotate(radians(180.0f), vec3(1.0f, 0.0f, 0.0f)),
//        // NEGATIVE_Z
//        rotate(radians(180.0f), vec3(0.0f, 0.0f, 1.0f)),
//    };
//    std::vector<ptr<ViewMatrix>> myViews(6);
//    for (int i = 0; i < 6; i++)
//    {
//        myViews[i] = MyViewMatrix::create(viewMats[i]);
//    }
//
//    for (uint32_t m = 0; m < Constants::PrefilteredEnvmapCube::numMips; m++)
//    {
//        uint32_t mipDim = Constants::PrefilteredEnvmapCube::dim >> m;
//        auto viewportState = vsg::ViewportState::create(0, 0, mipDim, mipDim);
//        for (uint32_t f = 0; f < 6; f++)
//        {
//            auto camera = vsg::Camera::create(dummyProjMatrix, myViews[f], viewportState);
//
//            // bind & draw by vsgScene traversal
//            auto bindStates = vsg::StateGroup::create();
//            bindStates->add(vsg::BindGraphicsPipeline::create(pipeline));
//            bindStates->add(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, descriptorSet));
//
//            auto uintBlock = vsg::uintArray::create(3);
//            uintBlock->at(0) = Constants::PrefilteredEnvmapCube::numMips;
//            uintBlock->at(1) = m;
//            uintBlock->at(2) = f;
//            bindStates->add(PushConstants::create(VK_SHADER_STAGE_FRAGMENT_BIT, 128, uintBlock));
//
//            // fullscreen quad
//            //auto drawFullscreenQuad = vsg::Draw::create(3, 1, 0, 0);
//            //bindStates->addChild(drawFullscreenQuad);
//            auto drawSkybox = vsg::Group::create();
//            drawSkybox->addChild(BindVertexBuffers::create(0, DataList{gSkyboxCube.vertices}));
//            drawSkybox->addChild(BindIndexBuffer::create(gSkyboxCube.indices));
//            drawSkybox->addChild(DrawIndexed::create(gSkyboxCube.indices->size(), 1, 0, 0, 0));
//            bindStates->addChild(drawSkybox);
//
//            // dummy camera for MVP push constants, dummyScene for bind & draw
//            auto view = vsg::View::create(camera, bindStates);
//            // warp begin/end renderpass to every bind & draw
//            auto rendergraph = vsg::RenderGraph::create();
//            rendergraph->renderArea.offset = VkOffset2D{0, 0};
//            rendergraph->renderArea.extent = VkExtent2D{mipDim, mipDim};
//            rendergraph->clearValues = {{0.0f, 0.0f, 0.0f, 1.0f}};
//            rendergraph->framebuffer = offscreenFB;
//            rendergraph->addChild(view);
//
//            // renderpass to offscreen framebuffer
//            commandGraph->addChild(rendergraph);
//            // renderpass之后，fb的layout为VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL，需要再转换到VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
//            auto setFBLayoutTransfeSrc = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
//            commandGraph->addChild(setFBLayoutTransfeSrc);
//
//            auto copyFBToCubeMap = vsg::CopyImage::create();
//            VkImageCopy copyRegion = {};
//            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//            copyRegion.srcSubresource.baseArrayLayer = 0;
//            copyRegion.srcSubresource.mipLevel = 0;
//            copyRegion.srcSubresource.layerCount = 1;
//            copyRegion.srcOffset = {0, 0, 0};
//            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//            copyRegion.dstSubresource.baseArrayLayer = f;
//            copyRegion.dstSubresource.mipLevel = m;
//            copyRegion.dstSubresource.layerCount = 1;
//            copyRegion.dstOffset = {0, 0, 0};
//            copyRegion.extent.width = mipDim;
//            copyRegion.extent.height = mipDim;
//            copyRegion.extent.depth = 1;
//            copyFBToCubeMap->regions = {copyRegion};
//            copyFBToCubeMap->srcImage = pFBImage;
//            copyFBToCubeMap->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//            copyFBToCubeMap->dstImage = textures.prefilterCube;
//            copyFBToCubeMap->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//
//            commandGraph->addChild(copyFBToCubeMap);
//            ptr<PipelineBarrier> setFBLayoutAttachment = createImageLayoutPipelineBarrier(pFBImage, VK_IMAGE_ASPECT_COLOR_BIT,
//                                                                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
//            commandGraph->addChild(setFBLayoutAttachment);
//        }
//    }
//    auto setCubeLayoutShaderRead = createImageLayoutPipelineBarrier(textures.prefilterCube,
//                                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//                                                                    subresourceRange);
//    commandGraph->addChild(setCubeLayoutShaderRead);
//    viewer->addRecordAndSubmitTaskAndPresentation({commandGraph});
//}