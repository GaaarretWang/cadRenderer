#include "vsgRendererServer.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <filesystem>

std::string getDirectoryPath(const std::string& path) {
    if (path.empty()) return path;

    // 查找最后一个 '/' 或 '\'（同时支持两种分隔符）
    size_t lastSeparator = path.find_last_of("/\\");

    // 如果没有找到分隔符，说明是当前目录下的文件，返回当前目录 "."
    if (lastSeparator == std::string::npos) {
        return ".";
    }

    // 截取从开头到最后一个分隔符的前一个位置（不包含分隔符本身）
    // 例如 "a/b/c.txt" → 截取到 "a/b"
    std::string dirPath = path.substr(0, lastSeparator);

    // 特殊情况：如果路径是根目录（如 "/a" 或 "C:\b"），确保不返回空
    if (dirPath.empty()) {
        // 对于 "/" 或 "C:\" 这类根路径，返回自身（保留根符号）
        return path.substr(0, lastSeparator + 1);
    }

    return dirPath;
}

void vsgRendererServer::initRenderer(std::string engine_path, std::vector<vsg::dmat4>& model_transforms, std::vector<std::string>& model_paths, std::vector<std::string>& instance_names, vsg::dmat4 plane_transform)
{
    // project_path = engine_path.append("Rendering/");
    project_path = engine_path;
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE"); //2
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->paths.push_back(engine_path + "asset/data/");
    options->sharedObjects = vsg::SharedObjects::create();

    std::cout << "SERVER: Init Vulkan Device" << std::endl;
    
    //手动初始化vulkan设备
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
        instanceExtensions.push_back("VK_KHR_win32_surface");//如果你使用windows
        instanceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
        instanceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    #else
        instanceExtensions.push_back("VK_KHR_xcb_surface"); //如果你使用linux
    #endif

    vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

    std::cout << "mainV2: Create Instance" << std::endl;
    
    vsg_color_image = vsg::ubvec3Array2D::create(width, height);
    vsg_depth_image = vsg::ushortArray2D::create(width, height);
    vsg_color_image->properties.format = VK_FORMAT_R8G8B8_UNORM;
    vsg_color_image->properties.dataVariance = vsg::DYNAMIC_DATA;
    vsg_depth_image->properties.format = VK_FORMAT_R16_UNORM;
    vsg_depth_image->properties.dataVariance = vsg::DYNAMIC_DATA;
    camera_info = createImageInfo(vsg_color_image);
    depth_info = createImageInfo(vsg_depth_image);


    vsg::ref_ptr<vsg::Instance> instance;
    try {
        instance = vsg::Instance::create(instanceExtensions, validatedNames, vulkanVersion);//问题语
    } catch (const vsg::Exception& ex) {
        std::cout << "-----Error creating Vulkan Instance: " << ex.message << "----result code: "<< ex.result <<std::endl;
        return;
    }
    catch (...) {
        std::cout << "-----Error creating Vulkan Instance: unknown error" << std::endl;
        return;
    }

    auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (!physicalDevice || queueFamily < 0)
    {
        std::cout << "Could not create PhysicalDevice" << std::endl;
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
        std::cout << "-----Error creating Vulkan Device: " << ex.message << "----result code: " << ex.result << std::endl;
        return;
    }
    auto context = vsg::Context::create(device);

    vsgContext.viewer = viewer_IBL;
    vsgContext.context = context;
    vsgContext.device = device;
    vsgContext.queueFamily = queueFamily;
    //std::cout<<"IBL:创建环境光数据"<< std::endl;
    IBL::appData.options = options;
    IBL::createResources(vsgContext, hdr_image_max_num);
    IBL::generateBRDFLUT(vsgContext);
    preprocessEnvMap();
    std::cout << "IBL:创建环境光数据完成----创建窗口" << std::endl;


    //创建窗口数据
    // 只包含虚拟物体
    auto cadWindowTraits = createWindowTraits("Model", 0, options);
    cadWindowTraits->device = device;
    cadWindowTraits->useMRT = true;
    window = vsg::Window::create(cadWindowTraits);
    window->getOrCreateSwapchain();

    double nearFarRatio = 0.0001;       //近平面和远平面之间的比例

    //---------------------------------------------------场景创建--------------------------------------//
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
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // 前序：金字塔生成的写入
        VK_ACCESS_SHADER_READ_BIT,           // 后续：剔除阶段的读取
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        window->_SSAOResultImage,
        VkImageSubresourceRange{
            VK_IMAGE_ASPECT_COLOR_BIT,       // 关键！depthPyramidImage是R32_SFLOAT（普通颜色格式），不是深度格式，不能用DEPTH_BIT
            0, 1, 0, 1                       // 同步所有7个mip层
        }
    );
    SSAOPipelineBarrier->add(ssaoImageBarrier);
    rootSwitch1->addChild(MASK_SSAO, SSAOPipelineBarrier);
    rootSwitch1->addChild(MASK_SSAO, SSAODenoiseGroup);

    vsg::ref_ptr<vsg::Group> scenegraph_safe = vsg::Group::create();
    scenegraph_safe->addChild(rootSwitch);
    scenegraph_safe->addChild(rootSwitch1);
    

    // -----------------------设置相机参数------------------------------//
    double radius = 2000.0; // 固定观察距离
    auto viewport = vsg::ViewportState::create(0, 0, cadWindowTraits->width, cadWindowTraits->height);
    // auto perspective = vsg::Perspective::create(60.0, static_cast<double>(640) / static_cast<double>(480), nearFarRatio * radius, radius * 10.0);
    auto perspective = vsg::Perspective::create(fx, fy, cx, cy, width, height, near_plane, far_plane);

    vsg::dvec3 centre = {0.0, 0.0, 1.0};                    // 固定观察点
    vsg::dvec3 eye = vsg::dvec3(0.0, 0.0, 0.0); // 固定相机位置
    vsg::dvec3 up = {0.0, -1.0, 0.0};                        // 固定观察方向
    auto lookAt = vsg::LookAt::create(eye, centre, up);
    camera = vsg::Camera::create(perspective, lookAt, viewport);
    VkExtent2D extent = {};
    extent.width = render_width;
    extent.height = render_height;


    struct MyParams
    {
        float semi_transparent;
        int width;
        int height;
        float camera_far;
        int shader_type;
    };
    
    auto params = vsg::ubyteArray::create(sizeof(MyParams));
    auto* params_ptr = reinterpret_cast<MyParams*>(params->dataPointer());
    params_ptr->semi_transparent = 1;
    params_ptr->width = render_width;
    params_ptr->height = render_height;
    params_ptr->camera_far = 65.535;
    params_ptr->shader_type = shader_type;

    CADMesh::camera_info = camera_info;
    CADMesh::depth_info = depth_info;
    CADMesh::params = params;
    //---------------------------------------读取CAD模型------------------------------------------//
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
            transfer_model->preprocessProtoData(path_i.c_str(), texture_path_i.c_str(), model_transforms[i], IBL::customPbrShaderSet(options), modelGroup, instance_names[i]); //读取obj文件
        }
        else if(format == "fb")
        {
            //transfer_model->transferModel(model_paths[i], fullNormal, model_transforms[i]);
            transfer_model->preprocessFBProtoData(path_i, texture_path_i.c_str(), model_transforms[i], IBL::customPbrShaderSet(options), modelGroup, instance_names[i]);
        }
    }

    if(shadow_recevier_path != "")
    {
        CADMesh* shadow_recevier_mesh = new CADMesh();
        shadow_recevier_mesh->preprocessProtoData(shadow_recevier_path.c_str(), getDirectoryPath(shadow_recevier_path).c_str(), shadow_recevier_transform, shadow_shader, shadowGroup, "shadow_receiver");
    }

    newmatrix = vsg::mat4Array::create(2);
    vsg::ref_ptr<vsg::PushConstants> pc = vsg::PushConstants::create(
                VK_SHADER_STAGE_ALL, 128, newmatrix);

    CADMesh::buildDrawData(modelGroup, pc); //读取obj文件
    CADMesh::buildDynamicLinesData(line_shader, wireframeGroup); //读取obj文件
    CADMesh::buildDynamicPointsData(point_shader, wireframeGroup); //读取obj文件
    CADMesh::buildDynamicTextsData(textGroup, options, project_path + "asset/data/fonts/times.vsgt"); //读取obj文件
    std::cout << "model processing done" << std::endl;
    SSAOPass::buildSSAOData(options, SSAOGroup, window->_GBufferImageView0, window->_GBufferImageView1, window->_GBufferImageView2, extent);
    SSAOPass::buildSSAODenoiseData(options, SSAODenoiseGroup, window->_GBufferImageView0, window->_SSAOResultImageView);

    // HDR环境光采样
    init_directional_lights();
    update_directional_lights();
    scenegraph_safe->addChild(curLightGroup);
    
    auto commandGraph = vsg::CommandGraph::create(window);
    auto commandGraph1 = vsg::CommandGraph::create(window);
    auto computeQueueFamily = commandGraph->queueFamily;
    auto computeQueueFamily1 = commandGraph1->queueFamily;

    //----------------------------------------------------------------窗口1----------------------------------------------------------//
    viewer->addWindow(window);
    view = vsg::View::create(camera, scenegraph_safe);
    // view->features = vsg::RECORD_LIGHTS;
    view->mask = MASK_CAMERA_IMAGE | MASK_PBR_FULL | MASK_SHADOW_RECEIVER;
    // view->mask = MASK_SKYBOX | MASK_PBR_FULL | MASK_SHADOW_RECEIVER;
    auto shadow_view_dependent_state = CustomViewDependentState::create(view.get(), device, computeQueueFamily, project_path);
    view->viewDependentState = shadow_view_dependent_state;
    auto renderGraph = vsg::RenderGraph::create(window, view);
    // auto renderImGui = vsgImGui::RenderImGui::create(window, gui::MyGui::create(options));
    // renderGraph->addChild(renderImGui);

    renderGraph->clearValues[0].color = {{-1.f, -1.f, -1.f, 1.f}};
    auto view1 = vsg::View::create(camera, scenegraph_safe);
    // view->features = vsg::RECORD_LIGHTS;
    // view1->mask = MASK_PBR_FULL | MASK_WIREFRAME | MASK_TEXT | MASK_SHADOW_RECEIVER;
    view1->mask = MASK_PBR_FULL | MASK_WIREFRAME | MASK_TEXT | MASK_SHADOW_RECEIVER | MASK_SSAO;
    view1->viewDependentState = CustomViewDependentState1::create(view1.get());
    view1->viewDependentState->pre_depth_pass = view->viewDependentState;
    auto renderGraph1 = vsg::RenderGraph::create(window, view1);
    auto renderImGui = vsgImGui::RenderImGui::create(window, gui::MyGui::create(options));
    renderGraph1->addChild(renderImGui);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    OcclusionCullingPasses::initOcclusionCullingPassesImageInfo(extent, window);
    auto depthPyramidImage = OcclusionCullingPasses::depthPyramidImage;
    auto depth_pyramid_sampler = OcclusionCullingPasses::depth_pyramid_sampler;
    auto depthPyramidImageView = OcclusionCullingPasses::depthPyramidImageView;
    auto depthPyramidImageInfo = OcclusionCullingPasses::depthPyramidImageInfo;
    auto framebuffer_depthImageInfo = OcclusionCullingPasses::framebuffer_depthImageInfo;

    OcclusionCullingPasses::generateCameraData(fx, fy, cx, cy, width, height, near_plane, far_plane, camera);





    clearDepth->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // 必须为 TRANSFER_DST_OPTIMAL 或 GENERAL
    clearDepth->depthStencil = {0.0f, 0};
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    clearDepth->ranges = {range};

    clearDepth1->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // 必须为 TRANSFER_DST_OPTIMAL 或 GENERAL
    clearDepth1->depthStencil = {0.0f, 0};
    clearDepth1->ranges = {range};

    if(msaaSamples != VK_SAMPLE_COUNT_1_BIT)
        commandGraph->addChild(clearDepth);
    commandGraph->addChild(clearDepth1);


    VkImageSubresourceRange range0{};
    range0.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // 颜色附件
    range0.baseMipLevel = 0;
    range0.levelCount = 1;
    range0.baseArrayLayer = 0;
    range0.layerCount = 1;

    auto clearColor0 = vsg::ClearColorImage::create();
    clearColor0->image = window->_GBufferImage0;
    clearColor0->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // 符合要求的布局
    clearColor0->color = {0.0f, 0.0f, 0.0f, 1.0f}; // 清除颜色：黑色（RGBA）
    clearColor0->ranges = {range0};

    auto clearColor1 = vsg::ClearColorImage::create();
    clearColor1->image = window->_GBufferImage1;
    clearColor1->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // 符合要求的布局
    clearColor1->color = {0.0f, 0.0f, 0.0f, 1.0f}; // 清除颜色：默认法线（0,0,1）映射后的值
    clearColor1->ranges = {range0};

    auto clearColor2 = vsg::ClearColorImage::create();
    clearColor2->image = window->_GBufferImage2;
    clearColor2->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // 符合要求的布局
    clearColor2->color = {0.0f, 0.0f, 0.0f, 1.0f}; // 清除颜色：默认法线（0,0,1）映射后的值
    clearColor2->ranges = {range0};

    // 3. 添加到命令图（和原有深度清除逻辑一致）
    commandGraph->addChild(clearColor0);
    commandGraph->addChild(clearColor1);
    commandGraph->addChild(clearColor2);

    auto depth_cull_command_graph1 = vsg::CommandGraph::create(device, computeQueueFamily);
    commandGraph->addChild(depth_cull_command_graph1);
    commandGraph->addChild(renderGraph);

    auto depth_pyramid_CommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
    commandGraph1->addChild(depth_pyramid_CommandGraph);
    commandGraph1->addChild(renderGraph1);
    viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});
    viewer->addEventHandler(vsg::Trackball::create(camera));
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph, commandGraph1});
    viewer->compile(); //编译命令图。接受一个可选的`ResourceHints`对象作为参数，用于提供编译时的一些提示和配置。通过调用这个函数，可以将命令图编译为可执行的命令。

    
    OcclusionCullingPasses::buildFirstComputePass(depth_cull_command_graph1, project_path);
    OcclusionCullingPasses::buildDepthPyramid(depth_pyramid_CommandGraph, project_path, window, extent);
    OcclusionCullingPasses::buildSecondComputePass(depth_pyramid_CommandGraph, project_path, extent);


    viewer->compile(); //编译命令图。接受一个可选的`ResourceHints`对象作为参数，用于提供编译时的一些提示和配置。通过调用这个函数，可以将命令图编译为可执行的命令。
    // std::cout << "4" << std::endl;
    VkExtent2D encode_extent = {};
    encode_extent.width = encode_width;
    encode_extent.height = encode_height;
    final_screenshotHandler = ScreenshotHandler::create(window, extent, encode_extent, ENCODER);
    allocate_fix_depth_memory(render_width, render_height);
    std::cout << "4" << std::endl;
}

bool vsgRendererServer::render() {
    newmatrix->set(0, (vsg::mat4)camera->viewMatrix->inverse());
    if (camera->viewMatrix->is_compatible(typeid(vsg::LookAt))){
        vsg::LookAt* lookAt = dynamic_cast<vsg::LookAt*>(camera->viewMatrix.get());
        vsg::mat4 data = {};
        data[0] = vsg::vec4(lookAt->eye, 0.0f);
        newmatrix->set(1, data);
    }
    newmatrix->dirty();
    OcclusionCullingPasses::camera_matrix->set(0, (vsg::mat4)camera->viewMatrix->transform());
    OcclusionCullingPasses::camera_matrix->set(1, vsg::mat4(camera->projectionMatrix->transform() * camera->viewMatrix->transform()));
    OcclusionCullingPasses::camera_matrix->dirty();
    auto t0 = std::chrono::high_resolution_clock::now();
    while (viewer->advanceToNextFrame()) {
        if(msaaSamples != VK_SAMPLE_COUNT_1_BIT)
            clearDepth->image = window->_multisampleDepthImage;
        clearDepth1->image = window->_depthImage;

        auto t1 = std::chrono::high_resolution_clock::now();
        fix_depth(width, height, depth_pixels);

        auto t2 = std::chrono::high_resolution_clock::now();
        uint8_t* vsg_color_image_beginPointer = static_cast<uint8_t*>(vsg_color_image->dataPointer(0));
        std::copy(color_pixels, color_pixels + width * height * 3, vsg_color_image_beginPointer);
        uint16_t* vsg_depth_image_beginPointer = static_cast<uint16_t*>(vsg_depth_image->dataPointer(0));
        std::copy(depth_pixels, depth_pixels + width * height, vsg_depth_image_beginPointer);

        auto t3 = std::chrono::high_resolution_clock::now();
        vsg_color_image->dirty();
        vsg_depth_image->dirty();

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

        auto t8 = std::chrono::high_resolution_clock::now();

        gui::global_params->render_func_times[3] = std::chrono::duration<double, std::milli>(t4 - t3).count();
        gui::global_params->render_func_times[4] = std::chrono::duration<double, std::milli>(t5 - t4).count();
        gui::global_params->render_func_times[5] = std::chrono::duration<double, std::milli>(t6 - t5).count();
        gui::global_params->render_func_times[6] = std::chrono::duration<double, std::milli>(t7 - t6).count();
        gui::global_params->render_func_times[7] = std::chrono::duration<double, std::milli>(t8 - t7).count();

        view->viewDependentState->draw_shadow = false;

        return true;
    }
    return false;
}

