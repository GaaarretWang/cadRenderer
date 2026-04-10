#include "vsgRendererServer.h"
#include <filesystem>

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

void vsgRendererServer::initRenderer(std::string engine_path, std::vector<vsg::dmat4>& model_transforms, std::vector<std::string>& model_paths, std::vector<std::string>& instance_names, vsg::dmat4 plane_transform)
{
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE"); //2
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->paths.push_back(engine_path + "asset/data/");
    options->sharedObjects = vsg::SharedObjects::create();

    // Set up shaders after options->paths is initialized
    setUpShader();
    constant_data->properties.dataVariance = vsg::DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    camera_image_params->properties.dataVariance = vsg::DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    syncConstantData();
    constant_data_buffer_info_list = {vsg::BufferInfo::create(constant_data)};

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


    // Create the window resources.
    // This pass contains virtual objects only.
    auto cadWindowTraits = createWindowTraits("Model", 0, options);
    cadWindowTraits->device = device;
    window = vsg::Window::create(cadWindowTraits);
    window->getOrCreateSwapchain();

    // Create offscreen render target (Stage 2: create but not yet used)
    offscreenTarget = OffscreenRenderTarget::create();
    offscreenTarget->init(device, window->extent2D(), msaaSamples, window->depthFormat(), cadWindowTraits->depthImageUsage);

    // Stage 3: Create render pass and framebuffer (not yet used by render graphs)
    bool requiresDepthRead = (cadWindowTraits->depthImageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    offscreenTarget->buildRenderPass(device, window->surfaceFormat().format, window->depthFormat(), requiresDepthRead);
    offscreenTarget->buildFramebuffer(window->extent2D());

    double nearFarRatio = 0.0001;       // Ratio between the near and far planes.

    //---------------------------------------------------Create scene----------------------------------//
    auto modelGroup = vsg::Group::create();
    auto modelShadowGroup = vsg::Group::create();
    auto shadowGroup = vsg::Group::create();
    auto envSceneGroup = vsg::Group::create();
    auto wireframeGroup = vsg::Group::create();
    auto textGroup = vsg::Group::create();
    auto SSAOGroup = vsg::Group::create();
    auto SSAODenoiseGroup = vsg::Group::create();

    auto rootSwitch = vsg::Switch::create();
    rootSwitch->addChild(MASK_CAMERA_IMAGE, drawCameraImageNode);
    rootSwitch->addChild(MASK_SKYBOX, drawSkyboxNode);
    rootSwitch->addChild(MASK_SHADOW_RECEIVER, shadowGroup);
    rootSwitch->addChild(MASK_PBR_FULL, modelGroup);
    rootSwitch->addChild(MASK_TEXT, textGroup);
    rootSwitch->addChild(MASK_WIREFRAME, wireframeGroup);
    auto rootSwitch1 = vsg::Switch::create();
    rootSwitch1->addChild(MASK_SSAO, vsg::NextSubPass::create());
    rootSwitch1->addChild(MASK_SSAO, SSAOGroup);
    rootSwitch1->addChild(MASK_SSAO, vsg::NextSubPass::create());
    auto SSAOPipelineBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,                                                            // dstStageMask
        0
    );
    auto ssaoImageBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // Previous step: color attachment writes
        VK_ACCESS_SHADER_READ_BIT,           // Next step: fragment shader reads
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        offscreenTarget->ssaoResultImage,
        VkImageSubresourceRange{
            VK_IMAGE_ASPECT_COLOR_BIT,       // Important: this image uses a color format rather than a depth format.
            0, 1, 0, 1                       // Synchronize the whole image
        }
    );
    SSAOPipelineBarrier->add(ssaoImageBarrier);
    rootSwitch1->addChild(MASK_SSAO, SSAOPipelineBarrier);
    rootSwitch1->addChild(MASK_SSAO, SSAODenoiseGroup);

    vsg::ref_ptr<vsg::Group> scenegraph_safe = vsg::Group::create();
    scenegraph_safe->addChild(rootSwitch);
    scenegraph_safe->addChild(rootSwitch1);
    

    // -----------------------Configure camera parameters------------------------//
    double radius = 2000.0; // Fixed viewing distance.
    auto viewport = vsg::ViewportState::create(0, 0, cadWindowTraits->width, cadWindowTraits->height);
    // auto perspective = vsg::Perspective::create(60.0, static_cast<double>(640) / static_cast<double>(480), nearFarRatio * radius, radius * 10.0);
    auto perspective = vsg::Perspective::create(fx, fy, cx, cy, width, height, near_plane, far_plane);

    vsg::dvec3 centre = {0.0, 0.0, 1.0};                    // Fixed look-at target.
    vsg::dvec3 eye = vsg::dvec3(0.0, 0.0, 0.0); // Fixed camera position.
    vsg::dvec3 up = {0.0, -1.0, 0.0};                        // Fixed up direction.
    auto lookAt = vsg::LookAt::create(eye, centre, up);
    camera = vsg::Camera::create(perspective, lookAt, viewport);
    pending_camera_matrix = lookAt->transform();
    camera_dirty = false;
    VkExtent2D extent = {};
    extent.width = render_width;
    extent.height = render_height;

    syncConstantData();

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
            transfer_model->preprocessProtoData(path_i.c_str(), texture_path_i.c_str(), model_transforms[i], IBL::customPbrShaderSet(options), modelGroup, instance_names[i]); // Load the OBJ model.
        }
        else if(format == "fb")
        {
            transfer_model->preprocessFBProtoData(path_i, texture_path_i.c_str(), model_transforms[i], IBL::customPbrShaderSet(options), modelGroup, instance_names[i]);
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


    vsg::ref_ptr<vsg::PushConstants> pc = vsg::PushConstants::create(
                VK_SHADER_STAGE_ALL, 128, pc_data);

    CADMesh::buildDrawData(modelGroup, pc, constant_data_buffer_info_list, offscreenTarget->shadowSampleImageView); // Build draw data.
    CADMesh::buildDynamicLinesData(line_shader, wireframeGroup, constant_data_buffer_info_list); // Build dynamic line data.
    CADMesh::buildDynamicPointsData(point_shader, wireframeGroup, constant_data_buffer_info_list); // Build dynamic point data.
    CADMesh::buildDynamicTextsData(textGroup, options, vsg::findFile("fonts/times.vsgt", options->paths).string());
    CADMesh::processPMI(transfered_meshes, line_shader, wireframeGroup, textGroup, options, constant_data_buffer_info_list, vsg::findFile("fonts/times.vsgt", options->paths).string());
    vsg::info("Model processing done");

    SSAOPass::buildSSAOData(options, SSAOGroup, offscreenTarget->gbufferImageView0, offscreenTarget->gbufferImageView1, offscreenTarget->gbufferImageView2, extent);
    SSAOPass::buildSSAODenoiseData(options, SSAODenoiseGroup, offscreenTarget->gbufferImageView0, offscreenTarget->shadowWriteImageView, offscreenTarget->ssaoResultImageView);

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
    // view->features = vsg::RECORD_LIGHTS;
    view->mask = MASK_CAMERA_IMAGE | MASK_PBR_FULL | MASK_SHADOW_RECEIVER;
    // view->mask = MASK_SKYBOX | MASK_PBR_FULL | MASK_SHADOW_RECEIVER;
    auto shadow_view_dependent_state = CustomViewDependentState::create(view.get(), device, computeQueueFamily, options);
    view->viewDependentState = shadow_view_dependent_state;
    auto renderGraph = vsg::RenderGraph::create(window, view);

    renderGraph->clearValues[0].color = {{-1.f, -1.f, -1.f, 1.f}};
    auto view1 = vsg::View::create(camera, scenegraph_safe);
    // view->features = vsg::RECORD_LIGHTS;
    view1->mask = MASK_PBR_FULL | MASK_WIREFRAME | MASK_TEXT | MASK_SHADOW_RECEIVER | MASK_SSAO;
    view1->viewDependentState = CustomViewDependentState1::create(view1.get());
    view1->viewDependentState->pre_depth_pass = view->viewDependentState;
    auto renderGraph1 = vsg::RenderGraph::create(window, view1);

    // Override both render graphs to use offscreen framebuffer
    renderGraph->framebuffer = offscreenTarget->framebuffer;
    renderGraph1->framebuffer = offscreenTarget->framebuffer;
    vsgserver::renderer = this;
    auto renderImGui = vsgImGui::RenderImGui::create(window, gui::MyGui::create(pc_data, vsg::findFile("json/Scenes.json", options->paths).string(), vsg::findFile("json/Materials.json", options->paths).string(), vsg::findFile("json/LightInfo.json", options->paths).string()));
    renderGraph1->addChild(renderImGui);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    OcclusionCullingPasses::initOcclusionCullingPassesImageInfo(extent, offscreenTarget);
    auto depthPyramidImage = OcclusionCullingPasses::depthPyramidImage;
    auto depth_pyramid_sampler = OcclusionCullingPasses::depth_pyramid_sampler;
    auto depthPyramidImageView = OcclusionCullingPasses::depthPyramidImageView;
    auto depthPyramidImageInfo = OcclusionCullingPasses::depthPyramidImageInfo;
    auto framebuffer_depthImageInfo = OcclusionCullingPasses::framebuffer_depthImageInfo;

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

    // Barrier: transition offscreen color from COLOR_ATTACHMENT_OPTIMAL to GENERAL
    // (CopyImageViewToWindow expects GENERAL layout)
    {
        auto barrierCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
        auto offscreenToGeneral = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            offscreenTarget->colorImage,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        );
        barrierCommandGraph->addChild(vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, offscreenToGeneral
        ));
        commandGraph1->addChild(barrierCommandGraph);
    }

    // Copy offscreen color to swapchain for display
    {
        auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(
            offscreenTarget->colorImageView, window);
        auto copyCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
        copyCommandGraph->addChild(copyImageViewToWindow);
        commandGraph1->addChild(copyCommandGraph);
    }

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
}

bool vsgRendererServer::render() {
    if (camera->viewMatrix->is_compatible(typeid(vsg::LookAt))){
        vsg::LookAt* lookAt = dynamic_cast<vsg::LookAt*>(camera->viewMatrix.get());
        pc_data->value().camera_pos = lookAt->eye;
    }
    syncConstantData();
    pc_data->value().frame_num = ++frame_num;
    pc_data->value().last_view = vsg::mat4(camera->viewMatrix->transform());
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


