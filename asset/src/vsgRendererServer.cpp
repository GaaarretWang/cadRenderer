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
    IBL::createResources(vsgContext);
    IBL::generateBRDFLUT(vsgContext);
    preprocessEnvMap();
    std::cout << "IBL:创建环境光数据完成----创建窗口" << std::endl;


    //创建窗口数据
    // 只包含虚拟物体
    auto cadWindowTraits = createWindowTraits("Model", 0, options);
    cadWindowTraits->device = device;
    cadWindowTraits->useMRT = true;
    window = vsg::Window::create(cadWindowTraits);

    double nearFarRatio = 0.0001;       //近平面和远平面之间的比例

    //---------------------------------------------------场景创建--------------------------------------//
    auto modelGroup = vsg::Group::create();
    auto modelShadowGroup = vsg::Group::create();
    auto shadowGroup = vsg::Group::create();
    auto envSceneGroup = vsg::Group::create();
    auto wireframeGroup = vsg::Group::create();
    auto textGroup = vsg::Group::create();

    auto rootSwitch = vsg::Switch::create();
    rootSwitch->addChild(MASK_CAMERA_IMAGE, drawCameraImageNode);
    rootSwitch->addChild(MASK_SKYBOX, drawSkyboxNode);
    rootSwitch->addChild(MASK_PBR_FULL, modelGroup);
    rootSwitch->addChild(MASK_SHADOW_RECEIVER, shadowGroup);
    rootSwitch->addChild(MASK_TEXT, textGroup);
    rootSwitch->addChild(MASK_WIREFRAME, wireframeGroup);
    
    vsg::ref_ptr<vsg::Group> scenegraph_safe = vsg::Group::create();
    scenegraph_safe->addChild(rootSwitch);
    std::cout << "1" << std::endl;
    vsg::ref_ptr<vsg::PbrMaterialValue> objectMaterial;
    
    struct SetPipelineStates : public vsg::Visitor
    {
        uint32_t base = 0;
        const vsg::AttributeBinding& binding;
        VkVertexInputRate vir;
        uint32_t stride;
        VkFormat format;

        SetPipelineStates(uint32_t in_base, const vsg::AttributeBinding& in_binding, VkVertexInputRate in_vir, uint32_t in_stride, VkFormat in_format) :
            base(in_base),
            binding(in_binding),
            vir(in_vir),
            stride(in_stride),
            format(in_format) {}

        void apply(Object& object) override { object.traverse(*this); }
        void apply(vsg::VertexInputState& vis) override
        {
            uint32_t bindingIndex = base + static_cast<uint32_t>(vis.vertexAttributeDescriptions.size());
            vis.vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{binding.location, bindingIndex, (format != VK_FORMAT_UNDEFINED) ? format : binding.format, 0});
            vis.vertexBindingDescriptions.push_back(VkVertexInputBindingDescription{bindingIndex, stride, vir});
        }
    };

    auto addVertexAttribute = [](vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc, std::string name, VkVertexInputRate vertexInputRate, vsg::Data::Properties props) -> bool {
        const auto& attributeBinding = gpc->shaderSet->getAttributeBinding(name);
        if (attributeBinding)
        {
            SetPipelineStates setVertexAttributeState(gpc->baseAttributeBinding, attributeBinding, vertexInputRate, props.stride, props.format);
            gpc->accept(setVertexAttributeState);

            return true;
        }
        return false;
    };

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


    vsg::Data::Properties vec2ArrayProps = {};
    vec2ArrayProps.stride = sizeof(vsg::vec2);
    vsg::Data::Properties vec3ArrayProps = {};
    vec3ArrayProps.stride = sizeof(vsg::vec3);
    vsg::Data::Properties vec4ValueProps = {};

    auto pbriblShaderSet = IBL::customPbrShaderSet(options);//
    auto gpc_ibl = vsg::GraphicsPipelineConfigurator::create(pbriblShaderSet);
    addVertexAttribute(gpc_ibl, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
    addVertexAttribute(gpc_ibl, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
    addVertexAttribute(gpc_ibl, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, vec2ArrayProps);
    addVertexAttribute(gpc_ibl, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, vec4ValueProps);
    gpc_ibl->assignTexture("cameraImage", camera_info);
    gpc_ibl->assignTexture("depthImage", depth_info);
    // auto params = vsg::floatArray::create(3);
    // params->set(0, 1.f);
    // params->set(1, 640.f * 2);
    // params->set(2, 480.f * 2);
    // gpc_ibl->assignUniform("customParams", params);//是否半透明判断
    //gpc_ibl->assignDescriptor("material", plane_mat);
    gpc_ibl->init();

    auto wireframeShaderSet = IBL::customPbrShaderSet(options);//
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->polygonMode = VK_POLYGON_MODE_LINE;
    wireframeShaderSet->defaultGraphicsPipelineStates.push_back(rasterizationState);
    auto gpc_ibl_wireframe = vsg::GraphicsPipelineConfigurator::create(wireframeShaderSet);
    addVertexAttribute(gpc_ibl_wireframe, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
    addVertexAttribute(gpc_ibl_wireframe, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
    addVertexAttribute(gpc_ibl_wireframe, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, vec2ArrayProps);
    addVertexAttribute(gpc_ibl_wireframe, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, vec4ValueProps);
    // gpc_ibl_wireframe->assignTexture("cameraImage", camera_info);
    // gpc_ibl_wireframe->assignTexture("depthImage", depth_info);
    // gpc_ibl_wireframe->assignUniform("customParams", params);//是否半透明判断
    //gpc_ibl->assignDescriptor("material", plane_mat);
    gpc_ibl_wireframe->init();

    // auto gpc_shadow = vsg::GraphicsPipelineConfigurator::create(shadow_shader);
    // addVertexAttribute(gpc_shadow, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
    // addVertexAttribute(gpc_shadow, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
    // addVertexAttribute(gpc_shadow, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, vec2ArrayProps);
    // addVertexAttribute(gpc_shadow, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, vec4ValueProps);
    // auto extent_array = vsg::floatArray::create(2);
    // extent_array->set(0, render_width * 1.f);
    // extent_array->set(1, render_height * 1.f);
    // gpc_shadow->assignDescriptor("extent", extent_array);
    // gpc_shadow->assignTexture("cameraImage", camera_info);
    // gpc_shadow->assignTexture("depthImage", depth_info);
    // gpc_shadow->init();

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

    PlaneData planeData = createTestPlanes();
    float subdivisions = 0.1;
    PlaneData subdividedPlaneData = subdividePlanes(planeData, subdivisions);

    MeshData mesh = convertPlaneDataToMesh(subdividedPlaneData);
    {
        vsg::ref_ptr<vsg::Geometry> reconstructDrawCmd = vsg::Geometry::create();
        reconstructDrawCmd->assignArrays({mesh.vertices,
                                    mesh.normals,
                                    vsg::vec2Array::create(1),
                                    vsg::vec4Value::create(1, 1, 1, 1)});
        reconstructDrawCmd->assignIndices(mesh.indices);
        reconstructDrawCmd->commands.push_back(vsg::DrawIndexed::create(mesh.indices->size(), 1, 0, 0, 0));
                            
        // auto pbrStateGroup = vsg::StateGroup::create();
        // gpc_ibl->copyTo(pbrStateGroup);
        // pbrStateGroup->addChild(reconstructDrawCmd);

        auto wireframeStateGroup = vsg::StateGroup::create();
        gpc_ibl_wireframe->copyTo(wireframeStateGroup);
        wireframeStateGroup->addChild(reconstructDrawCmd);


        // auto shadowStateGroup = vsg::StateGroup::create();
        // gpc_shadow->copyTo(shadowStateGroup);
        // shadowStateGroup->addChild(reconstructDrawCmd);

        // rootSwitch->addChild(MASK_WIREFRAME, wireframeStateGroup);
        // rootSwitch->addChild(MASK_PBR_FULL, pbrStateGroup);
        // rootSwitch->addChild(MASK_SHADOW_RECEIVER, shadowStateGroup);
    }
    // auto params1 = vsg::floatArray::create(4);
    // params1->set(0, 1);
    // params1->set(1, render_width * 1.f);
    // params1->set(2, render_height * 1.f);
    // params1->set(3, 65.535);
    CADMesh::camera_info = camera_info;
    CADMesh::depth_info = depth_info;
    CADMesh::params = params;
    if(shadow_recevier_path != "")
    {
        CADMesh* shadow_recevier_mesh = new CADMesh();
        shadow_recevier_mesh->preprocessProtoData(shadow_recevier_path.c_str(), getDirectoryPath(shadow_recevier_path).c_str(), shadow_recevier_transform, shadow_shader, shadowGroup, "shadow_receiver");
    }
    bool fullNormal = true;
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
    newmatrix = vsg::mat4Array::create(2);
    vsg::ref_ptr<vsg::PushConstants> pc = vsg::PushConstants::create(
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 128, newmatrix);
    CADMesh::buildDrawData(pbriblShaderSet, modelGroup, pc); //读取obj文件
    CADMesh::buildDynamicLinesData(line_shader, wireframeGroup); //读取obj文件
    CADMesh::buildDynamicPointsData(point_shader, wireframeGroup); //读取obj文件
    CADMesh::buildDynamicTextsData(textGroup, options, project_path + "asset/data/fonts/times.vsgt"); //读取obj文件

    std::cout << "model processing done" << std::endl;

    // HDR环境光采样
    init_directional_lights();
    update_directional_lights();
    scenegraph_safe->addChild(curLightGroup);
    

    //----------------------------------------------------------------窗口1----------------------------------------------------------//
    viewer->addWindow(window);
    view = vsg::View::create(camera, scenegraph_safe);
    // view->features = vsg::RECORD_LIGHTS;
    view->mask = MASK_CAMERA_IMAGE | MASK_PBR_FULL | MASK_SHADOW_RECEIVER;
    auto shadow_view_dependent_state = CustomViewDependentState::create(view.get());
    view->viewDependentState = shadow_view_dependent_state;
    auto renderGraph = vsg::RenderGraph::create(window, view);
    renderGraph->clearValues[0].color = {{-1.f, -1.f, -1.f, 1.f}};
    // auto renderImGui = vsgImGui::RenderImGui::create(window, gui::MyGui::create(options));
    // renderGraph->addChild(renderImGui);
    auto commandGraph = vsg::CommandGraph::create(window);
    auto commandGraph1 = vsg::CommandGraph::create(window);

    auto view1 = vsg::View::create(camera, scenegraph_safe);
    // view->features = vsg::RECORD_LIGHTS;
    view1->mask = MASK_PBR_FULL | MASK_WIREFRAME | MASK_TEXT | MASK_SHADOW_RECEIVER;
    view1->viewDependentState = CustomViewDependentState1::create(view1.get());
    view1->viewDependentState->pre_depth_pass = view->viewDependentState;
    auto renderGraph1 = vsg::RenderGraph::create(window, view1);
    // renderGraph1->getRenderPass()->attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderGraph1->clearValues[0].color = {{0.f, 0.f, 0.f, 0.f}};
    auto renderImGui = vsgImGui::RenderImGui::create(window, gui::MyGui::create(options));
    renderGraph1->addChild(renderImGui);


    VkExtent2D extent = {};
    extent.width = render_width;
    extent.height = render_height;
    depthPyramidImage->imageType = VK_IMAGE_TYPE_2D;
    depthPyramidImage->format = VK_FORMAT_R32_SFLOAT; // 假设与深度附件兼容
    depthPyramidImage->mipLevels = 7; // 共 7 层
    depthPyramidImage->usage = VK_IMAGE_USAGE_STORAGE_BIT |          // 计算着色器读写
                                VK_IMAGE_USAGE_SAMPLED_BIT | 
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT |     // 可能需要mipmap生成
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT;      // 可能需要初始化
    depthPyramidImage->initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    depthPyramidImage->extent.width = extent.width;
    depthPyramidImage->extent.height = extent.height;
    depthPyramidImage->extent.depth = 1;


    auto depth_pyramid_sampler = vsg::Sampler::create();
    depth_pyramid_sampler->minLod = 0;
    depth_pyramid_sampler->maxLod = 6;
    depth_pyramid_sampler->magFilter = VK_FILTER_NEAREST;  // 放大时使用 Nearest
    depth_pyramid_sampler->minFilter = VK_FILTER_NEAREST;  // 缩小时使用 Nearest
    depth_pyramid_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; // Mipmap 使用 Nearest
    vsg::ref_ptr<vsg::ImageView> depthPyramidImageView = vsg::ImageView::create(depthPyramidImage);
    depthPyramidImageView->subresourceRange.baseMipLevel = 0;
    depthPyramidImageView->subresourceRange.levelCount = 7;
    vsg::ref_ptr<vsg::ImageInfo> depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, depthPyramidImageView);

    vsg::ref_ptr<vsg::ImageInfo> framebuffer_depthImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, window->getOrCreateDepthImageView());

    camera_plane_info.n[0] = vsg::normalize(vsg::vec4(0, 0, -1, -near_plane));
    camera_plane_info.n[1] = vsg::normalize(vsg::vec4(0, 0, 1, far_plane));
    camera_plane_info.n[2] = vsg::normalize(vsg::vec4(2*fx/width, 0, -2*cx/width, 0));
    camera_plane_info.n[3] = vsg::normalize(vsg::vec4(-2*fx/width, 0, -2+2*cx/width, 0));
    camera_plane_info.n[4] = vsg::normalize(vsg::vec4(0, 2*fy/height, -2+2*cy/height, 0));
    camera_plane_info.n[5] = vsg::normalize(vsg::vec4(0, -2*fy/height, -2*cy/height, 0));
    vsg::ref_ptr<vsg::Array<CameraPlaneInfo>> camera_plane_info_buffer = vsg::Array<CameraPlaneInfo>::create(1);
    camera_plane_info_buffer->set(0, camera_plane_info);
    auto camera_plane_info_buffer_info = vsg::BufferInfo::create(camera_plane_info_buffer);
    camera_matrix->set(0, vsg::mat4(camera->viewMatrix->transform()));
    camera_matrix->set(1, vsg::mat4(camera->projectionMatrix->transform() * camera->viewMatrix->transform()));
    camera_matrix_buffer_info = vsg::BufferInfo::create(camera_matrix);
    camera_matrix->properties.dataVariance = vsg::DYNAMIC_DATA;


    preClearBarrier->add(layoutTransition);

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




    auto computeQueueFamily = commandGraph->queueFamily;
    auto computeQueueFamily1 = commandGraph1->queueFamily;
    auto computeCommandGraphShadow = vsg::CommandGraph::create(device, computeQueueFamily);
    computeCommandGraphShadow->submitOrder = -1;

    auto depth_cull_command_graph1 = vsg::CommandGraph::create(device, computeQueueFamily);
    commandGraph->addChild(depth_cull_command_graph1);
    commandGraph->addChild(renderGraph);

    auto depth_pyramid_CommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
    commandGraph1->addChild(depth_pyramid_CommandGraph);
    commandGraph1->addChild(renderGraph1);
    viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});
    viewer->addEventHandler(vsg::Trackball::create(camera));
    viewer->assignRecordAndSubmitTaskAndPresentation({computeCommandGraphShadow, commandGraph, commandGraph1});
    viewer->compile(); //编译命令图。接受一个可选的`ResourceHints`对象作为参数，用于提供编译时的一些提示和配置。通过调用这个函数，可以将命令图编译为可执行的命令。


    {
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
        {
            auto computeShader = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex_shadow.comp", options);
            auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
            auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
            computeCommandGraphShadow->addChild(bindPipeline);

            auto ShadowPipelineBarrier = vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,                                                            // dstStageMask
                0
            );
            for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
                ProtoData* proto_data = proto_data_itr.second;
                auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                    proto_data->input_instance_buffer_info, proto_data->input_highlight_buffer_info, 
                                                                                    proto_data->output_instance_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer});
                auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
                computeCommandGraphShadow->addChild(bindDescriptorSet);
                computeCommandGraphShadow->addChild(vsg::Dispatch::create(1, 1, 1));
                auto indirect_draw_barrier = vsg::BufferMemoryBarrier::create(
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    proto_data->draw_indirect->bufferInfo->buffer,
                    0,
                    VK_WHOLE_SIZE
                );
                auto instance_data_barrier = vsg::BufferMemoryBarrier::create(
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_UNIFORM_READ_BIT, 
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    proto_data->output_instance_buffer_info->buffer,
                    0,
                    VK_WHOLE_SIZE
                );
                ShadowPipelineBarrier->add(indirect_draw_barrier);
                ShadowPipelineBarrier->add(instance_data_barrier);
            }
            computeCommandGraphShadow->addChild(ShadowPipelineBarrier);
        }
    }
    
    {
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});

        auto ShadowToPass1CullBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0
        );
        for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
            ProtoData* proto_data = proto_data_itr.second;
            auto indirectBarrier = vsg::BufferMemoryBarrier::create(
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                proto_data->draw_indirect->bufferInfo->buffer,
                0,
                VK_WHOLE_SIZE
            );
            ShadowToPass1CullBarrier->add(indirectBarrier);
        }
        depth_cull_command_graph1->addChild(ShadowToPass1CullBarrier);

        auto Pass1CullToPass1Barrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            0
        );
        auto computeShader = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex.comp", options);
        auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
        auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
        depth_cull_command_graph1->addChild(bindPipeline);

        for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
            ProtoData* proto_data = proto_data_itr.second;
            auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                proto_data->input_instance_buffer_info, proto_data->input_highlight_buffer_info, 
                                                                                proto_data->output_instance_buffer_info, camera_plane_info_buffer_info, 
                                                                                proto_data->bounds_buffer_info, camera_matrix_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_cull_command_graph1->addChild(bindDescriptorSet);
            depth_cull_command_graph1->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 2 / 700 + 1, 1, 1));

            auto indirectBarrier = vsg::BufferMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                proto_data->draw_indirect->bufferInfo->buffer,
                0,
                VK_WHOLE_SIZE
            );
            Pass1CullToPass1Barrier->add(indirectBarrier);
        }
        depth_cull_command_graph1->addChild(Pass1CullToPass1Barrier);
    }

    struct ComputePushConstants {
        uint32_t width;
        uint32_t height;
        char padding[8];
    };
    {
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, 
                vsg::PushConstantRanges{
                    {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants)} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
                });
        {
            auto barrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                depthPyramidImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 7, 0, 1}
            );

            auto depthToComputeBarrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,  
                VK_ACCESS_SHADER_READ_BIT,   
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,  
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                window->getOrCreateDepthImage(),
                VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}
            );

            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, barrier, depthToComputeBarrier
            ));

            auto computeShader = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex_depthimage.comp", options);
            auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
            auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
            depth_pyramid_CommandGraph->addChild(bindPipeline);
            auto pcData = vsg::Value<ComputePushConstants>::create(ComputePushConstants{extent.width, extent.height});
            depth_pyramid_CommandGraph->addChild(vsg::PushConstants::create(
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                pcData
            ));

            auto storageImage = vsg::DescriptorImage::create(vsg::ImageInfoList{depthPyramidImageInfo, framebuffer_depthImageInfo}, 0);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageImage});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_pyramid_CommandGraph->addChild(bindDescriptorSet);
            depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create((extent.width + 31) / 32, (extent.height + 31) / 32, 1));

            auto barrier1 = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                depthPyramidImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // 前一层级
            );

            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, barrier1
            ));


        }

        for(uint32_t i = 1; i < 7; i ++)
        {
            auto computeShader = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex_depthpyramid.comp", options);
            auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
            auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
            depth_pyramid_CommandGraph->addChild(bindPipeline);
            auto pcData = vsg::Value<ComputePushConstants>::create(ComputePushConstants{extent.width >> i, extent.height >> i});
            depth_pyramid_CommandGraph->addChild(vsg::PushConstants::create(
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                pcData
            ));

            auto i_image_view = vsg::ImageView::create(depthPyramidImage);
            i_image_view->subresourceRange.baseMipLevel = i;
            i_image_view->subresourceRange.levelCount = 1;
            auto i_depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, i_image_view);
            auto i1_image_view = vsg::ImageView::create(depthPyramidImage);
            i1_image_view->subresourceRange.baseMipLevel = i - 1;
            i1_image_view->subresourceRange.levelCount = 1;
            auto i1_depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, i1_image_view);
            auto storageImage0 = vsg::DescriptorImage::create(vsg::ImageInfoList{i_depthPyramidImageInfo}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            auto storageImage1 = vsg::DescriptorImage::create(vsg::ImageInfoList{i1_depthPyramidImageInfo}, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageImage0, storageImage1});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_pyramid_CommandGraph->addChild(bindDescriptorSet);
            uint32_t mipWidth = std::max(1u, extent.width >> i);
            uint32_t mipHeight = std::max(1u, extent.height >> i);
            depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create(
                (mipWidth + 31) / 32,
                (mipHeight + 31) / 32,
                1
            ));

            auto barrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                depthPyramidImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1} // 前一层级
            );

            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, barrier
            ));
        }
        auto pyramidFinalBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_SHADER_WRITE_BIT,          // 前序：金字塔生成的写入
            VK_ACCESS_SHADER_READ_BIT,           // 后续：剔除阶段的读取
            VK_IMAGE_LAYOUT_GENERAL,             // 金字塔生成时的布局
            VK_IMAGE_LAYOUT_GENERAL,             // 剔除读取时的布局（保持一致）
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            depthPyramidImage,
            VkImageSubresourceRange{
                VK_IMAGE_ASPECT_COLOR_BIT,       // 关键！depthPyramidImage是R32_SFLOAT（普通颜色格式），不是深度格式，不能用DEPTH_BIT
                0, 7, 0, 1                       // 同步所有7个mip层
            }
        );

        depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 前序阶段：金字塔生成的计算阶段
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 后续阶段：剔除的计算阶段
            0,
            pyramidFinalBarrier
        ));
    }

    {
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, 
                vsg::PushConstantRanges{
                    {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants)} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
                });

        auto Pass2CullToPass2Barrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            0
        );
        auto computeShader = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex1.comp", options);
        auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
        auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
        auto computeShader_seat = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex1_seat.comp", options);
        auto pipeline_seat = vsg::ComputePipeline::create(pipelineLayout, computeShader_seat);
        auto bindPipeline_seat = vsg::BindComputePipeline::create(pipeline_seat);
        depth_pyramid_CommandGraph->addChild(bindPipeline);
        auto pcData1 = vsg::Value<ComputePushConstants>::create(ComputePushConstants{extent.width, extent.height});
        depth_pyramid_CommandGraph->addChild(vsg::PushConstants::create(
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            pcData1
        ));

        auto pre_pipeline = bindPipeline;
        for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
            ProtoData* proto_data = proto_data_itr.second;
            if(proto_data->instance_matrix.size() / 2 > 32 && pre_pipeline == bindPipeline){
                depth_pyramid_CommandGraph->addChild(bindPipeline_seat);
                pre_pipeline = bindPipeline_seat;
            }
            else if(proto_data->instance_matrix.size() / 2 <= 32 && pre_pipeline == bindPipeline_seat){
                depth_pyramid_CommandGraph->addChild(bindPipeline);
                pre_pipeline = bindPipeline;
            }
            auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                proto_data->input_instance_buffer_info, proto_data->input_highlight_buffer_info, 
                                                                                proto_data->output_instance_buffer_info, camera_plane_info_buffer_info, 
                                                                                proto_data->bounds_buffer_info, camera_matrix_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto storageImage = vsg::DescriptorImage::create(vsg::ImageInfoList{depthPyramidImageInfo}, 8);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer, storageImage});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_pyramid_CommandGraph->addChild(bindDescriptorSet);
            if(proto_data->instance_matrix.size() / 2 > 32)
                depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 2 / 700 + 1, 1, 1));
            else
                depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 2 / 32 + 1, 1, 1));
            auto indirectBarrier = vsg::BufferMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                proto_data->draw_indirect->bufferInfo->buffer,
                0,
                VK_WHOLE_SIZE
            );
            Pass2CullToPass2Barrier->add(indirectBarrier);
        }
        depth_pyramid_CommandGraph->addChild(Pass2CullToPass2Barrier);
    }
    viewer->compile(); //编译命令图。接受一个可选的`ResourceHints`对象作为参数，用于提供编译时的一些提示和配置。通过调用这个函数，可以将命令图编译为可执行的命令。
    // std::cout << "4" << std::endl;
    final_screenshotHandler = ScreenshotHandler::create(window, extent, ENCODER);
    screenshotHandler = ScreenshotHandler::create();        
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
    camera_matrix->set(0, (vsg::mat4)camera->viewMatrix->transform());
    camera_matrix->set(1, vsg::mat4(camera->projectionMatrix->transform() * camera->viewMatrix->transform()));
    camera_matrix->dirty();
    auto t0 = std::chrono::high_resolution_clock::now();
    while (viewer->advanceToNextFrame()) {
        static int tmp = 0;
        auto t3 = std::chrono::high_resolution_clock::now();
        if(!tmp){
            layoutTransition->image = window->_depthImage;
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

            t3 = std::chrono::high_resolution_clock::now();
            vsg_color_image->dirty();
            vsg_depth_image->dirty();

            gui::global_params->render_func_times[0] = std::chrono::duration<double, std::milli>(t1 - t0).count();
            gui::global_params->render_func_times[1] = std::chrono::duration<double, std::milli>(t2 - t1).count();
            gui::global_params->render_func_times[2] = std::chrono::duration<double, std::milli>(t3 - t2).count();
        }

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

