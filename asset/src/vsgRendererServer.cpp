#include "vsgRendererServer.h"
#include <filesystem>
#include <algorithm>
#include <array>

#include "LightInfoStateSerializer.h"

namespace
{
class WindowTransferReadyForGui : public vsg::Inherit<vsg::Command, WindowTransferReadyForGui>
{
public:
    explicit WindowTransferReadyForGui(vsg::ref_ptr<vsg::Window> in_window) :
        window(std::move(in_window))
    {
    }

    vsg::ref_ptr<vsg::Window> window;

    void record(vsg::CommandBuffer& commandBuffer) const override
    {
        if (!window) return;

        size_t imageIndex = window->imageIndex();
        if (imageIndex >= window->numFrames()) return;

        auto imageView = window->imageView(imageIndex);
        auto transferToColorLoad = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            imageView->image,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

        auto pipelineBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            transferToColorLoad);
        pipelineBarrier->record(commandBuffer);
    }
};
}

std::string getDirectoryPath(const std::string& path) {
    if (path.empty()) return path;

    // Find the last '/' or '\' so both separator styles are supported.
    size_t lastSeparator = path.find_last_of("/\\");

    // If no separator exists, the file lives in the current directory, so return ".".
    if (lastSeparator == std::string::npos) {
        return ".";
    }

    // Slice from the beginning up to the last separator, excluding the separator itself.
    // Example: "a/b/c.txt" becomes "a/b".
    std::string dirPath = path.substr(0, lastSeparator);

    // Special case: if the path is rooted, do not return an empty string.
    if (dirPath.empty()) {
        // For "/" or "C:\", return the root path itself.
        return path.substr(0, lastSeparator + 1);
    }

    return dirPath;
}

void vsgRendererServer::loadHDRConfig()
{
    const std::string lightinfo_json_path = vsg::findFile("json/LightInfo.json", options->paths).string();
    auto json_manager = std::make_shared<JsonConfigManager>("", "", lightinfo_json_path);
    auto lightinfo_serializer = std::make_shared<LightInfoStateSerializer>(json_manager);

    SceneRuntimeState bootstrap_state;
    bootstrap_state.hdr_image_num = hdr_image_num;
    bootstrap_state.hdr_image_max_num = hdr_image_max_num;
    bootstrap_state.baseBrightness = pc_data ? pc_data->value().baseBrightness : bootstrap_state.baseBrightness;

    std::string error_message;
    if (!lightinfo_serializer->load(bootstrap_state, &error_message))
    {
        std::cerr << "Failed to load HDR config from LightInfo.json: " << error_message << std::endl;
        return;
    }

    hdr_image_max_num = std::max(bootstrap_state.hdr_image_max_num, 1);
    hdr_image_num = std::clamp(bootstrap_state.hdr_image_num, 1, hdr_image_max_num);
    hdr_base_brightness = bootstrap_state.hdr_base_brightness;
    if (pc_data)
    {
        pc_data->value().baseBrightness = bootstrap_state.baseBrightness;
        pc_data->dirty();
    }
}

void vsgRendererServer::initRenderer(std::string engine_path, std::vector<vsg::dmat4>& model_transforms, std::vector<std::string>& model_paths, std::vector<std::string>& instance_names, vsg::dmat4 plane_transform)
{
    env_lighting_update_ready = false;
    env_lighting_update_pending = false;

    options->fileCache = vsg::getEnv("VSG_FILE_CACHE"); //2
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->paths.push_back(engine_path + "asset/data/");
    options->sharedObjects = vsg::SharedObjects::create();

    // Set up shaders after options->paths is initialized
    setUpShader();
    global_buffer_data->properties.dataVariance = vsg::DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    camera_image_params->properties.dataVariance = vsg::DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    syncGlobalBufferData();
    global_buffer_info_list = {vsg::BufferInfo::create(global_buffer_data)};

    vsg::info("SERVER: Init Vulkan Device");

    // Initialize the Vulkan device manually.
    vsg::Names instanceExtensions;
    vsg::Names requestedLayers;
    bool debugLayer = false;
    bool apiDumpLayer = false;
    uint32_t vulkanVersion = VK_API_VERSION_1_1;
    instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    if (debugLayer || apiDumpLayer)
    {
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
        if (apiDumpLayer) requestedLayers.push_back("VK_LAYER_LUNARG_api_dump");
    }
    instanceExtensions.push_back("VK_KHR_surface");
    
    #ifdef _WIN32
        instanceExtensions.push_back("VK_KHR_win32_surface"); // Windows surface extension
        instanceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
        instanceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    #else
        instanceExtensions.push_back("VK_KHR_xcb_surface"); // Linux XCB surface extension
    #endif

    vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

    vsg::info("mainV2: Create Instance");

    frame_image_resources = std::make_unique<FrameImageResources>();
    frame_image_resources->initialize(width, height);


    vsg::ref_ptr<vsg::Instance> instance;
    try {
        instance = vsg::Instance::create(instanceExtensions, validatedNames, vulkanVersion); // Create the Vulkan instance.
    } catch (const vsg::Exception& ex) {
        vsg::error("Error creating Vulkan Instance: ", ex.message, ", result code: ", ex.result);
        return;
    }
    catch (const std::exception& e) {
        vsg::error("Error creating Vulkan Instance: ", e.what());
        return;
    }

    auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (!physicalDevice || queueFamily < 0)
    {
        vsg::error("Could not create PhysicalDevice");
        return;
    }

    vsg::Names deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    deviceExtensions.insert(deviceExtensions.end(), {VK_KHR_MULTIVIEW_EXTENSION_NAME,
                                                    VK_KHR_MAINTENANCE2_EXTENSION_NAME,
                                                    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
                                                    VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
                                                    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                                                    VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
#ifdef _WIN32
                                                    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
                                                    VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
                                                    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
#else
                                                    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
                                                    VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
                                                    VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
#endif
                                                    });

    vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}};

    auto deviceFeatures = vsg::DeviceFeatures::create();
    deviceFeatures->get().samplerAnisotropy = VK_TRUE;
    deviceFeatures->get().geometryShader = VK_TRUE;
    try {
        device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, deviceFeatures);
    }
    catch (const vsg::Exception& ex) {
        vsg::error("Error creating Vulkan Device: ", ex.message, ", result code: ", ex.result);
        return;
    }
    auto context = vsg::Context::create(device);

    vsgContext.viewer = viewer_IBL;
    vsgContext.context = context;
    vsgContext.device = device;
    vsgContext.queueFamily = queueFamily;
    IBL::appData.options = options;
    loadHDRConfig();
    IBL::createResources(vsgContext, hdr_image_max_num);
    IBL::generateBRDFLUT(vsgContext);
    preprocessEnvMap();
    vsg::info("IBL: Environment lighting data created, creating window");
    auto cadWindowTraits = createWindowTraits("Model", 0, options);
    cadWindowTraits->device = device;
    window = vsg::Window::create(cadWindowTraits);
    window->getOrCreateSwapchain();
    VkExtent2D renderExtent = {
        static_cast<uint32_t>(render_width),
        static_cast<uint32_t>(render_height)};

    offscreenTarget = OffscreenRenderTarget::create();
    offscreenTarget->init(device, renderExtent, msaaSamples, window->depthFormat(), cadWindowTraits->depthImageUsage);

    bool requiresDepthRead = (cadWindowTraits->depthImageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    offscreenTarget->buildRenderPass(device, window->surfaceFormat().format, window->depthFormat(), requiresDepthRead);
    offscreenTarget->buildFramebuffer(renderExtent);
    VkExtent2D ssaoExtent = {
        std::max(1u, renderExtent.width / 2),
        std::max(1u, renderExtent.height / 2)};
    ssaoTarget = ColorRenderTarget::create();
    ssaoTarget->init(device, ssaoExtent, VK_FORMAT_R32G32B32A32_SFLOAT);
    realSceneTarget = ColorRenderTarget::create();
    realSceneTarget->init(device, renderExtent, window->surfaceFormat().format);
    finalColorTarget = ColorRenderTarget::create();
    finalColorTarget->init(device, renderExtent, window->surfaceFormat().format, 0, VK_IMAGE_LAYOUT_GENERAL, msaaSamples);
    if (offscreenTarget->isMultisampled())
    {
        resolvedEffectDepthTarget = ColorRenderTarget::create();
        resolvedEffectDepthTarget->init(device, renderExtent, VK_FORMAT_R32G32B32A32_SFLOAT);
        resolvedEffectNormalTarget = ColorRenderTarget::create();
        resolvedEffectNormalTarget->init(device, renderExtent, VK_FORMAT_R32G32B32A32_SFLOAT);
        resolvedEffectWorldPosTarget = ColorRenderTarget::create();
        resolvedEffectWorldPosTarget->init(device, renderExtent, VK_FORMAT_R32G32B32A32_SFLOAT);
    }

    //---------------------------------------------------Create scene----------------------------------//
    auto modelGroup = vsg::Group::create();
    auto transparentGroup = vsg::Group::create();
    auto shadowGroup = vsg::Group::create();
    auto wireframeGroup = vsg::Group::create();
    auto textGroup = vsg::Group::create();
    auto ssaoScene = vsg::Group::create();
    auto resolvedEffectDepthScene = vsg::Group::create();
    auto resolvedEffectNormalScene = vsg::Group::create();
    auto resolvedEffectWorldPosScene = vsg::Group::create();
    auto realSceneScene = vsg::Group::create();
    auto compositeScene = vsg::Group::create();

    auto rootSwitch = vsg::Switch::create();
    rootSwitch->addChild(MASK_CAMERA_IMAGE, drawCameraDepthPrepassNode);
    rootSwitch->addChild(MASK_SKYBOX, drawSkyboxNode);
    rootSwitch->addChild(MASK_SHADOW_RECEIVER, shadowGroup);
    rootSwitch->addChild(MASK_PBR_FULL, modelGroup);
    rootSwitch->addChild(MASK_TRANSPARENT, transparentGroup);
    rootSwitch->addChild(MASK_TEXT, textGroup);
    rootSwitch->addChild(MASK_WIREFRAME, wireframeGroup);

    vsg::ref_ptr<vsg::Group> scenegraph_safe = vsg::Group::create();
    scenegraph_safe->addChild(rootSwitch);
    

    // -----------------------Configure camera parameters------------------------//
    auto viewport = vsg::ViewportState::create(0, 0, renderExtent.width, renderExtent.height);
    auto perspective = vsg::Perspective::create(fx, fy, cx, cy, width, height, near_plane, far_plane);

    vsg::dvec3 centre = {0.0, 0.0, 1.0};                    // Fixed look-at target.
    vsg::dvec3 eye = vsg::dvec3(0.0, 0.0, 0.0); // Fixed camera position.
    vsg::dvec3 up = {0.0, -1.0, 0.0};                        // Fixed up direction.
    auto lookAt = vsg::LookAt::create(eye, centre, up);
    camera = vsg::Camera::create(perspective, lookAt, viewport);
    pending_camera_matrix = lookAt->transform();
    camera_dirty = false;
    VkExtent2D extent = renderExtent;

    syncGlobalBufferData();

    CADMesh::camera_info = frame_image_resources->cameraInfo();
    CADMesh::depth_info = frame_image_resources->depthInfo();
    if(shadow_receiver_path != "")
    {
        CADMesh* shadow_receiver_mesh = new CADMesh();
        shadow_receiver_mesh->preprocessProtoData(shadow_receiver_path.c_str(), getDirectoryPath(shadow_receiver_path).c_str(), shadow_receiver_transform, shadow_shader, shadowGroup, "shadow_receiver");
    }
    //---------------------------------------Load CAD models--------------------------------------//
    for(int i = 0; i < model_paths.size(); i ++){
        std::string &path_i = model_paths[i];
        size_t pos = path_i.find_last_of('.');
        std::string format = path_i.substr(pos + 1);
        std::string texture_path_i = getDirectoryPath(path_i);
        CADMesh* transfer_model;
        if (transfered_meshes.find(path_i) != transfered_meshes.end()){
            transfer_model = transfered_meshes[path_i];
        }else{
            transfer_model = new CADMesh();
            transfered_meshes[path_i] = transfer_model;
            if(cull_mode_none_model_paths.find(path_i) != cull_mode_none_model_paths.end())
                transfer_model->back_cull = false;
        }
        if(format == "obj")
        {
            transfer_model->preprocessProtoData(path_i.c_str(), texture_path_i.c_str(), model_transforms[i], IBL::customPbrShaderSet(options), modelGroup, instance_names[i], transparentGroup); // Load the OBJ model.
        }
        else if(format == "fb")
        {
            transfer_model->preprocessFBProtoData(path_i, texture_path_i.c_str(), model_transforms[i], IBL::customPbrShaderSet(options), modelGroup, instance_names[i], transparentGroup);
        }
    }
    // Populate CADMesh static scene-instance data.
    CADMesh::scene_instance_names = instance_names;
    CADMesh::scene_original_transforms = model_transforms;
    // Record the instance-name to relative-path mapping.
    for (int i = 0; i < model_paths.size(); i++) {
        std::string rel_path = model_paths[i];
        if (rel_path.find(engine_path) == 0)
            rel_path = rel_path.substr(engine_path.length());
        CADMesh::instance_name_to_rel_path[instance_names[i]] = rel_path;
    }
    if(shadow_receiver_path != "") {
        CADMesh::scene_instance_names.push_back("shadow_receiver");
        CADMesh::scene_original_transforms.push_back(shadow_receiver_transform);
    }

    CADMesh::buildDrawData(modelGroup, global_buffer_info_list, offscreenTarget->shadowSampleImageView); // Build draw data.
    CADMesh::buildDynamicLinesData(line_shader, wireframeGroup, global_buffer_info_list); // Build dynamic line data.
    CADMesh::buildDynamicPointsData(point_shader, wireframeGroup, global_buffer_info_list); // Build dynamic point data.
    CADMesh::buildDynamicTextsData(textGroup, options, vsg::findFile("fonts/times.vsgt", options->paths).string());
    CADMesh::processPMI(transfered_meshes, line_shader, wireframeGroup, textGroup, options, global_buffer_info_list, vsg::findFile("fonts/times.vsgt", options->paths).string());
    vsg::info("Model processing done");

    if (offscreenTarget->isMultisampled())
    {
        SSAOPass::buildStandaloneResolvedDepthData(
            options,
            resolvedEffectDepthScene,
            offscreenTarget->multisampleDepthImageView);
        SSAOPass::buildStandaloneResolvedNormalData(
            options,
            resolvedEffectNormalScene,
            offscreenTarget->multisampleDepthImageView,
            offscreenTarget->gbufferImageView1);
        SSAOPass::buildStandaloneResolvedWorldPosData(
            options,
            resolvedEffectWorldPosScene,
            offscreenTarget->multisampleDepthImageView,
            offscreenTarget->gbufferImageView2);
    }
    bool useResolvedSsaoInputs = resolvedEffectNormalTarget && resolvedEffectWorldPosTarget;
    SSAOPass::buildStandaloneSSAOData(
        options,
        ssaoScene,
        useResolvedSsaoInputs ? resolvedEffectNormalTarget->colorImageView : offscreenTarget->gbufferImageView1,
        useResolvedSsaoInputs ? resolvedEffectWorldPosTarget->colorImageView : offscreenTarget->gbufferImageView2,
        ssaoExtent,
        global_buffer_info_list,
        useResolvedSsaoInputs);
    SSAOPass::buildStandaloneSSAOCompositeData(
        options,
        compositeScene,
        offscreenTarget->gbufferImageView0,
        ssaoTarget->colorImageView,
        realSceneTarget->colorImageView,
        global_buffer_info_list,
        offscreenTarget->isMultisampled(),
        msaaSamples);
    SSAOPass::buildStandaloneRealSceneData(
        options,
        realSceneScene,
        frame_image_resources->cameraInfo(),
        frame_image_resources->depthInfo(),
        offscreenTarget->depthImageView,
        global_buffer_info_list,
        vsg::ref_ptr<vsg::Data>(pc_data));

    // Sample HDR environment lighting.
    init_directional_lights();
    update_directional_lights();
    scenegraph_safe->addChild(curLightGroup);
    
    auto commandGraph = vsg::CommandGraph::create(window);
    auto commandGraph1 = vsg::CommandGraph::create(window);
    auto computeQueueFamily = commandGraph->queueFamily;
    auto computeQueueFamily1 = commandGraph1->queueFamily;

    //---------------------------------------------------------------Window 1-----------------------------------------------//
    viewer->addWindow(window);
    view = vsg::View::create(camera, scenegraph_safe);
    CADMesh::active_view = view.get();
    view->mask = MASK_CAMERA_IMAGE | MASK_PBR_FULL | MASK_SHADOW_RECEIVER;
    auto shadow_view_dependent_state = CustomViewDependentState::create(view.get(), device, computeQueueFamily, options);
    view->viewDependentState = shadow_view_dependent_state;
    auto renderGraph = vsg::RenderGraph::create(window, view);

    renderGraph->clearValues[0].color = {{-1.f, -1.f, -1.f, 1.f}};
    auto view1 = vsg::View::create(camera, scenegraph_safe);
    view1->mask = MASK_PBR_FULL | MASK_TRANSPARENT | MASK_WIREFRAME | MASK_TEXT | MASK_SHADOW_RECEIVER;
    view1->viewDependentState = CustomViewDependentState1::create(view1.get());
    view1->viewDependentState->pre_depth_pass = view->viewDependentState;
    auto renderGraph1 = vsg::RenderGraph::create(window, view1);

    renderGraph->framebuffer = offscreenTarget->framebuffer;
    renderGraph->renderArea.offset = {0, 0};
    renderGraph->renderArea.extent = renderExtent;
    renderGraph1->framebuffer = offscreenTarget->framebuffer;
    renderGraph1->renderArea.offset = {0, 0};
    renderGraph1->renderArea.extent = renderExtent;
    vsgserver::renderer = this;
    auto renderImGui = vsgImGui::RenderImGui::create(window, gui::MyGui::create(pc_data, vsg::findFile("json/Scenes.json", options->paths).string(), vsg::findFile("json/Materials.json", options->paths).string(), vsg::findFile("json/LightInfo.json", options->paths).string()));
    auto guiRenderGraph = vsg::RenderGraph::create(window);
    guiRenderGraph->renderArea.offset = {0, 0};
    guiRenderGraph->renderArea.extent = window->extent2D();
    guiRenderGraph->addChild(renderImGui);

    auto createPassCamera = [&](VkExtent2D targetExtent) {
        auto passViewport = vsg::ViewportState::create(0, 0, targetExtent.width, targetExtent.height);
        return vsg::Camera::create(camera->projectionMatrix, camera->viewMatrix, passViewport);
    };
    auto createStandaloneRenderGraph = [&](vsg::ref_ptr<ColorRenderTarget> target,
                                           vsg::ref_ptr<vsg::Group> scene,
                                           const std::array<float, 4>& clearColor,
                                           vsg::ref_ptr<vsg::ViewDependentState> viewDependentState = {}) {
        auto passView = vsg::View::create(createPassCamera(target->getExtent()), scene);
        if (viewDependentState)
        {
            passView->viewDependentState = viewDependentState;
        }
        auto passRenderGraph = vsg::RenderGraph::create();
        passRenderGraph->renderArea.offset = {0, 0};
        passRenderGraph->renderArea.extent = target->getExtent();
        passRenderGraph->clearValues.resize(target->clearValueCount());
        passRenderGraph->clearValues[0].color = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
        passRenderGraph->framebuffer = target->framebuffer;
        passRenderGraph->addChild(passView);
        return passRenderGraph;
    };
    auto ssaoRenderGraph = createStandaloneRenderGraph(ssaoTarget, ssaoScene, {1.0f, 1.0f, 1.0f, 1.0f});
    vsg::ref_ptr<vsg::RenderGraph> resolvedEffectDepthRenderGraph;
    vsg::ref_ptr<vsg::RenderGraph> resolvedEffectNormalRenderGraph;
    vsg::ref_ptr<vsg::RenderGraph> resolvedEffectWorldPosRenderGraph;
    if (resolvedEffectDepthTarget && resolvedEffectNormalTarget && resolvedEffectWorldPosTarget)
    {
        resolvedEffectDepthRenderGraph = createStandaloneRenderGraph(resolvedEffectDepthTarget, resolvedEffectDepthScene, {0.0f, 0.0f, 0.0f, 1.0f});
        resolvedEffectNormalRenderGraph = createStandaloneRenderGraph(resolvedEffectNormalTarget, resolvedEffectNormalScene, {0.0f, 0.0f, 0.0f, 1.0f});
        resolvedEffectWorldPosRenderGraph = createStandaloneRenderGraph(resolvedEffectWorldPosTarget, resolvedEffectWorldPosScene, {0.0f, 0.0f, 0.0f, 1.0f});
    }
    auto realSceneRenderGraph = createStandaloneRenderGraph(realSceneTarget, realSceneScene, {0.0f, 0.0f, 0.0f, 0.0f}, view->viewDependentState);
    auto compositeRenderGraph = createStandaloneRenderGraph(finalColorTarget, compositeScene, {0.0f, 0.0f, 0.0f, 1.0f});
    
    OcclusionCullingPasses::initOcclusionCullingPassesImageInfo(extent, offscreenTarget);
    OcclusionCullingPasses::generateCameraData(fx, fy, cx, cy, width, height, near_plane, far_plane, camera);

    auto clear_image_commandgraph = vsg::CommandGraph::create(device, computeQueueFamily);
    Utils::BuildClearCommandGraph(clear_image_commandgraph, extent, offscreenTarget, msaaSamples);
    commandGraph->addChild(clear_image_commandgraph);
    auto depth_preprocess_command_graph = vsg::CommandGraph::create(device, computeQueueFamily);
    depth_preprocess_stage.build(depth_preprocess_command_graph, options, *frame_image_resources);
    commandGraph->addChild(depth_preprocess_command_graph);
    auto depth_cull_command_graph1 = vsg::CommandGraph::create(device, computeQueueFamily);
    commandGraph->addChild(depth_cull_command_graph1);
    commandGraph->addChild(renderGraph);

    auto depth_pyramid_CommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
    commandGraph1->addChild(depth_pyramid_CommandGraph);
    commandGraph1->addChild(renderGraph1);

    {
        VkImageSubresourceRange colorRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        auto mainOutputsCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
        auto mainOutputsBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0);

        if (offscreenTarget->isMultisampled())
        {
            mainOutputsBarrier->add(vsg::ImageMemoryBarrier::create(
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->colorImage,
                colorRange));
        }
        else
        {
            mainOutputsBarrier->add(vsg::ImageMemoryBarrier::create(
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->gbufferImage0,
                colorRange));
        }

        mainOutputsBarrier->add(vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            offscreenTarget->gbufferImage1,
            colorRange));
        mainOutputsBarrier->add(vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            offscreenTarget->gbufferImage2,
            colorRange));
        mainOutputsBarrier->add(vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            offscreenTarget->materialImage,
            colorRange));
        mainOutputsBarrier->add(vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            offscreenTarget->shadowWriteImage,
            colorRange));
        mainOutputsBarrier->add(vsg::ImageMemoryBarrier::create(
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            offscreenTarget->shadowSampleImage,
            colorRange));
        mainOutputsCommandGraph->addChild(mainOutputsBarrier);

        if (offscreenTarget->isMultisampled())
        {
            auto resolveShadowHistory = vsg::ResolveImage::create();
            VkImageResolve resolveRegion{};
            resolveRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            resolveRegion.srcSubresource.mipLevel = 0;
            resolveRegion.srcSubresource.baseArrayLayer = 0;
            resolveRegion.srcSubresource.layerCount = 1;
            resolveRegion.dstSubresource = resolveRegion.srcSubresource;
            resolveRegion.extent = {extent.width, extent.height, 1};
            resolveShadowHistory->srcImage = offscreenTarget->shadowWriteImage;
            resolveShadowHistory->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            resolveShadowHistory->dstImage = offscreenTarget->shadowSampleImage;
            resolveShadowHistory->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            resolveShadowHistory->regions = {resolveRegion};
            mainOutputsCommandGraph->addChild(resolveShadowHistory);
        }
        else
        {
            auto copyShadowHistory = vsg::CopyImage::create();
            VkImageCopy copyRegion{};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.dstSubresource = copyRegion.srcSubresource;
            copyRegion.extent = {extent.width, extent.height, 1};
            copyShadowHistory->srcImage = offscreenTarget->shadowWriteImage;
            copyShadowHistory->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            copyShadowHistory->dstImage = offscreenTarget->shadowSampleImage;
            copyShadowHistory->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copyShadowHistory->regions = {copyRegion};
            mainOutputsCommandGraph->addChild(copyShadowHistory);
        }

        auto shadowHistoryReady = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            vsg::ImageMemoryBarrier::create(
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->shadowSampleImage,
                colorRange));
        mainOutputsCommandGraph->addChild(shadowHistoryReady);
        commandGraph1->addChild(mainOutputsCommandGraph);
    }

    if (offscreenTarget->isMultisampled() &&
        offscreenTarget->multisampleDepthImage &&
        offscreenTarget->multisampleDepthImage != offscreenTarget->depthImage &&
        resolvedEffectDepthRenderGraph &&
        resolvedEffectNormalRenderGraph &&
        resolvedEffectWorldPosRenderGraph)
    {
        auto resolvedEffectInputsReady = vsg::CommandGraph::create(device, computeQueueFamily1);
        auto depthReadBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            offscreenTarget->multisampleDepthImage,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1});
        resolvedEffectInputsReady->addChild(vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            depthReadBarrier));
        commandGraph1->addChild(resolvedEffectInputsReady);
        commandGraph1->addChild(resolvedEffectDepthRenderGraph);
        commandGraph1->addChild(resolvedEffectNormalRenderGraph);
        commandGraph1->addChild(resolvedEffectWorldPosRenderGraph);
    }

    if (useResolvedSsaoInputs)
    {
        auto resolvedEffectReadyCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
        auto resolvedEffectReadyBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0);
        resolvedEffectReadyBarrier->add(vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            resolvedEffectDepthTarget->colorImage,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}));
        resolvedEffectReadyBarrier->add(vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            resolvedEffectNormalTarget->colorImage,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}));
        resolvedEffectReadyBarrier->add(vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            resolvedEffectWorldPosTarget->colorImage,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}));
        resolvedEffectReadyCommandGraph->addChild(resolvedEffectReadyBarrier);
        commandGraph1->addChild(resolvedEffectReadyCommandGraph);
    }

    commandGraph1->addChild(ssaoRenderGraph);

    {
        auto ssaoReadyCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
        auto ssaoReady = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            ssaoTarget->colorImage,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
        ssaoReadyCommandGraph->addChild(vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            ssaoReady));
        commandGraph1->addChild(ssaoReadyCommandGraph);
    }

    {
        auto realSceneDepthReadCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
        auto depthToReadOnly = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            offscreenTarget->depthImage,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1});
        realSceneDepthReadCommandGraph->addChild(vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            depthToReadOnly));
        commandGraph1->addChild(realSceneDepthReadCommandGraph);
    }

    commandGraph1->addChild(realSceneRenderGraph);

    {
        auto realSceneReadyCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
        auto realSceneReadyBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0);
        realSceneReadyBarrier->add(vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            realSceneTarget->colorImage,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}));
        realSceneReadyBarrier->add(vsg::ImageMemoryBarrier::create(
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            offscreenTarget->depthImage,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}));
        realSceneReadyCommandGraph->addChild(realSceneReadyBarrier);
        commandGraph1->addChild(realSceneReadyCommandGraph);
    }

    commandGraph1->addChild(compositeRenderGraph);

    {
        auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(
            finalColorTarget->colorImageView, window);
        auto copyCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
        copyCommandGraph->addChild(copyImageViewToWindow);
        commandGraph1->addChild(copyCommandGraph);
    }

    {
        auto transferReadyForGui = vsg::CommandGraph::create(device, computeQueueFamily1);
        transferReadyForGui->addChild(WindowTransferReadyForGui::create(window));
        commandGraph1->addChild(transferReadyForGui);
    }

    commandGraph1->addChild(guiRenderGraph);

    viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});
    viewer->addEventHandler(vsg::Trackball::create(camera));

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph, commandGraph1});
    viewer->compile(); // Compile the command graphs into executable work.

    OcclusionCullingPasses::buildFirstComputePass(depth_cull_command_graph1, options);
    OcclusionCullingPasses::buildDepthPyramid(depth_pyramid_CommandGraph, options, extent, offscreenTarget);
    OcclusionCullingPasses::buildSecondComputePass(depth_pyramid_CommandGraph, options, extent);

    viewer->compile(); // Recompile after adding the compute passes.

    VkExtent2D encode_extent = {};
    encode_extent.width = encode_width;
    encode_extent.height = encode_height;
    final_screenshotHandler = ScreenshotHandler::create(window, extent, encode_extent, ENCODER);

    flushPendingEnvLightingUpdate();
}

bool vsgRendererServer::render() {
    if (camera->viewMatrix->is_compatible(typeid(vsg::LookAt))){
        vsg::LookAt* lookAt = dynamic_cast<vsg::LookAt*>(camera->viewMatrix.get());
        pc_data->value().camera_pos = lookAt->eye;
    }
    pc_data->value().frame_num = ++frame_num;
    pc_data->value().last_view = vsg::mat4(camera->viewMatrix->transform());
    syncGlobalBufferData();
    pc_data->dirty();

    // At the start of each frame, copy the current matrices into the previous-frame buffers.
    CADMesh::copyCurrentToLastMatrices();

    // Apply the pending camera matrix if one has been queued.
    if (camera_dirty) {
        auto lookat = camera->viewMatrix.cast<vsg::LookAt>();
        if (lookat) {
            lookat->set(pending_camera_matrix);
        }
        camera_dirty = false;
    }

    OcclusionCullingPasses::camera_matrix->set(0, (vsg::mat4)camera->viewMatrix->transform());
    OcclusionCullingPasses::camera_matrix->set(1, vsg::mat4(camera->projectionMatrix->transform() * camera->viewMatrix->transform()));
    OcclusionCullingPasses::camera_matrix->dirty();
    auto t0 = std::chrono::high_resolution_clock::now();
    while (viewer->advanceToNextFrame()) {

        auto t1 = std::chrono::high_resolution_clock::now();
        frame_image_resources->uploadDepthPixels(depth_pixels);

        auto t2 = std::chrono::high_resolution_clock::now();
        auto color_image = frame_image_resources->colorImage();
        uint8_t* vsg_color_image_beginPointer = static_cast<uint8_t*>(color_image->dataPointer(0));
        std::copy(color_pixels, color_pixels + width * height * 3, vsg_color_image_beginPointer);

        auto t3 = std::chrono::high_resolution_clock::now();
        color_image->dirty();

        gui::global_params->render_func_times[0] = std::chrono::duration<double, std::milli>(t1 - t0).count();
        gui::global_params->render_func_times[1] = std::chrono::duration<double, std::milli>(t2 - t1).count();
        gui::global_params->render_func_times[2] = std::chrono::duration<double, std::milli>(t3 - t2).count();

        auto t4 = std::chrono::high_resolution_clock::now();
        viewer->handleEvents();

        auto t5 = std::chrono::high_resolution_clock::now();
        viewer->update();

        auto t6 = std::chrono::high_resolution_clock::now();
        viewer->recordAndSubmit();

        auto t7 = std::chrono::high_resolution_clock::now();
        viewer->present();
        if (vsgserver::runtime_controller)
        {
            vsgserver::runtime_controller->clearServerDirtyFlags();
        }

        auto t8 = std::chrono::high_resolution_clock::now();

        gui::global_params->render_func_times[3] = std::chrono::duration<double, std::milli>(t4 - t3).count();
        gui::global_params->render_func_times[4] = std::chrono::duration<double, std::milli>(t5 - t4).count();
        gui::global_params->render_func_times[5] = std::chrono::duration<double, std::milli>(t6 - t5).count();
        gui::global_params->render_func_times[6] = std::chrono::duration<double, std::milli>(t7 - t6).count();
        gui::global_params->render_func_times[7] = std::chrono::duration<double, std::milli>(t8 - t7).count();

        return true;
    }
    if (vsgserver::runtime_controller)
    {
        vsgserver::runtime_controller->clearServerDirtyFlags();
    }
    return false;
}
