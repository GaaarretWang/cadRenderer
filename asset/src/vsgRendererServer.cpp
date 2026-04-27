// ==========================================================================
// vsgRendererServer.cpp
// 核心渲染器实现：负责 Vulkan 设备初始化、渲染管线构建、每帧渲染循环
//
// 渲染管线结构（双 CommandGraph 架构）:
//   CommandGraph (frame N):
//     ├── Clear Pass（清除 GBuffer + 深度）
//     ├── Pass1: Frustum Culling（视锥剔除 compute shader）
//     └── Render Pass（3 个 subpass: Main Render → SSAO → SSAO Denoise）
//   CommandGraph1 (frame N+1):
//     ├── Depth Pyramid（10 级深度金字塔生成）
//     ├── Pass2: Depth Culling（深度遮挡剔除 compute shader）
//     ├── Synthesis Render（view1: 天空盒 + CAD + 线框 + 文字）
//     ├── Barrier（离屏颜色布局转换）
//     └── Copy to Window（拷贝到 Swapchain）
// ==========================================================================

#include "vsgRendererServer.h"
#include <filesystem>

// 从完整路径中提取目录路径（支持 '/' 和 '\' 两种分隔符）
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

// --------------------------------------------------------------------------
// initRenderer: 渲染器初始化入口
// 功能: 初始化 Vulkan 设备、创建 IBL 资源、构建完整渲染管线
// 参数:
//   - engine_path: 引擎根目录路径
//   - model_transforms: 各模型的变换矩阵列表
//   - model_paths: 模型文件路径列表
//   - instance_names: 实例名称列表
//   - plane_transform: 平面变换矩阵（用于阴影接收面）
// --------------------------------------------------------------------------
void vsgRendererServer::initRenderer(std::string engine_path, std::vector<vsg::dmat4>& model_transforms, std::vector<std::string>& model_paths, std::vector<std::string>& instance_names, vsg::dmat4 plane_transform)
{
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->paths.push_back(engine_path + "asset/data/");
    options->sharedObjects = vsg::SharedObjects::create();

    setUpShader(); // 加载阴影、线框、点的 ShaderSet

    vsg::info("SERVER: Init Vulkan Device");

    // ===================== 手动初始化 Vulkan Instance =====================
    // Vulkan Instance 是应用程序与 Vulkan 驱动之间的连接对象
    // 类似于 OpenGL 的 GLContext，但更轻量且需要显式配置扩展
    
    // instanceExtensions: 扩展列表 - Vulkan 功能扩展
    // 每个扩展对应 GPU 的特定硬件能力（如 surface、swapchain、external memory 等）
    vsg::Names instanceExtensions;
    
    // requestedLayers: 验证层列表 - 用于调试的中间层
    // 可捕获 Vulkan API 调用错误、检测资源泄漏等（仅开发环境启用）
    vsg::Names requestedLayers;

    // debugLayer: 调试层开关 - 启用 VK_LAYER_KHRONOS_validation
    // 提供详细的验证信息和性能警告
    bool debugLayer = false;
    // apiDumpLayer: API 日志层开关 - 记录所有 Vulkan API 调用
    // 用于追踪渲染调用序列和调试
    bool apiDumpLayer = false;
    uint32_t vulkanVersion = VK_API_VERSION_1_1;
    // VK_KHR_get_physical_device_properties_2: 物理设备查询扩展
    instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    // 条件编译：根据 debugLayer/apiDumpLayer 决定是否启用验证层
    if (debugLayer || apiDumpLayer)
    {
        // VK_EXT_debug_report: 调试报告扩展
        // 允许应用程序注册回调接收验证层消息
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        
        // VK_LAYER_KHRONOS_validation: Khronos 官方验证层
        // 检测 API 使用错误、资源泄漏、内存越界等
        requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
        
        // VK_LAYER_LUNARG_api_dump: API 调用日志层
        // 输出每个 Vulkan 调用的完整参数（用于调试）
        if (apiDumpLayer) requestedLayers.push_back("VK_LAYER_LUNARG_api_dump");
    }
    
    // VK_KHR_surface: 表面扩展 - 创建渲染表面（窗口）
    // 是 Vulkan 渲染到屏幕的必需扩展
    instanceExtensions.push_back("VK_KHR_surface");

    // 平台特定扩展：根据操作系统选择
    #ifdef _WIN32
        // VK_KHR_win32_surface: Windows 平台表面扩展
        // 创建与 Win32 窗口句柄关联的 Vulkan 表面
        instanceExtensions.push_back("VK_KHR_win32_surface");
        // VK_KHR_external_semaphore_capabilities: 外部信号量能力
        // 支持与其他 API（如 CUDA）同步
        instanceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
        // VK_KHR_external_memory_capabilities: 外部内存能力
        // 支持与其他 API（如 CUDA）共享显存
        instanceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    #else
        // VK_KHR_xcb_surface: Linux X11 平台表面扩展
        // 创建与 X11 窗口关联的 Vulkan 表面
        instanceExtensions.push_back("VK_KHR_xcb_surface");
    #endif

    // validateInstancelayerNames: 验证层名称验证
    // 确保请求的验证层存在且版本兼容，过滤不存在的层
    vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

    vsg::info("mainV2: Create Instance");

    // ===================== 创建相机图像纹理（CPU→GPU 传输用） =====================
    // vsg_color_image: 相机颜色图像，CPU 端每帧更新颜色像素数据
    // vsg_depth_image: 相机深度图像，CPU 端每帧更新深度像素数据
    
    // ubvec3Array2D: 无符号字节 vec3 数组（RGB），用于存储颜色数据
    vsg_color_image = vsg::ubvec3Array2D::create(width, height);
    // ushortArray2D: 无符号短整型数组，用于存储深度数据（16位精度）
    vsg_depth_image = vsg::ushortArray2D::create(width, height);
    
    // properties.format: 像素格式定义
    // VK_FORMAT_R8G8B8_UNORM: 8位无符号 normalized RGB（0-255）
    vsg_color_image->properties.format = VK_FORMAT_R8G8B8_UNORM;
    // DYNAMIC_DATA: 标记数据每帧都会更新，提示 GPU 优化上传策略
    vsg_color_image->properties.dataVariance = vsg::DYNAMIC_DATA; // 每帧动态更新
    
    // VK_FORMAT_R16_UNORM: 16位无符号 normalized 深度（0-65535）
    vsg_depth_image->properties.format = VK_FORMAT_R16_UNORM;
    vsg_depth_image->properties.dataVariance = vsg::DYNAMIC_DATA;
    
    // createImageInfo: 将 VSG 数据数组转换为 Vulkan ImageInfo
    // ImageInfo 包含 image、imageView、sampler，用于 描述符集 绑定
    camera_info = createImageInfo(vsg_color_image);
    depth_info = createImageInfo(vsg_depth_image);


    // ===================== 创建 Vulkan Instance =====================
    // vsg::Instance: Vulkan 实例对象，管理应用程序与 GPU 驱动的连接
    // 参数: instanceExtensions（扩展列表）, validatedNames（验证层）, vulkanVersion（API版本）
    vsg::ref_ptr<vsg::Instance> instance;
    try {
        // vsg::Instance::create: 创建 Vulkan 实例，可能抛出 vsg::Exception
        instance = vsg::Instance::create(instanceExtensions, validatedNames, vulkanVersion);
    } catch (const vsg::Exception& ex) {
        // 捕获 VSG 特定的异常，包含 Vulkan 错误码和消息
        vsg::error("Error creating Vulkan Instance: ", ex.message, ", result code: ", ex.result);
        return;
    }
    catch (const std::exception& e) {
        // 捕获标准 C++ 异常
        vsg::error("Error creating Vulkan Instance: ", e.what());
        return;
    }

    // 获取物理设备和图形队列族索引
    // getPhysicalDeviceAndQueueFamily: 查询支持图形操作的物理设备
    // 返回: physicalDevice（GPU硬件）, queueFamily（图形队列索引）
    auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (!physicalDevice || queueFamily < 0)
    {
        vsg::error("Could not create PhysicalDevice");
        return;
    }

    // ===================== 设备扩展（CUDA-Vulkan 互操作必需） =====================
    // deviceExtensions: 设备级别扩展，控制 GPU 特定功能
    vsg::Names deviceExtensions;
    
    // VK_KHR_swapchain: 交换链扩展，渲染输出到窗口的必需扩展
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    
    // 批量添加多个设备扩展
    deviceExtensions.insert(deviceExtensions.end(), {
        // VK_KHR_multiview: 多视图扩展，支持单次渲染到多个视图（VR/AR）
        VK_KHR_MULTIVIEW_EXTENSION_NAME,
        // VK_KHR_maintenance2: 维护2扩展，修复 Vulkan 1.0 的一些问题
        VK_KHR_MAINTENANCE2_EXTENSION_NAME,
        // VK_KHR_create_renderpass2: RenderPass 2 扩展，新版渲染通道创建API
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
        // VK_KHR_depth_stencil_resolve: 深度模板解析扩展，多重采样抗锯齿支持
        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
        // VK_KHR_buffer_device_address: 缓冲区设备地址扩展，GPU 直接寻址
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        // VK_KHR_external_memory: 外部内存扩展，跨 API 共享显存
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
#ifdef _WIN32
        // Windows 平台特定的外部内存扩展
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
        // 外部信号量扩展，支持跨 API 同步
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        // Windows 平台特定的外部信号量扩展
        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
#else
        // Linux 平台特定的外部内存扩展（使用 fd 文件描述符）
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        // 外部信号量扩展
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        // Linux 平台特定的外部信号量扩展
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
#endif
    });

    // QueueSettings: 队列配置，设置图形队列的优先级
    // queueFamily: 之前获取的图形队列族索引
    // {1.0}: 队列优先级（0.0-1.0，1.0 最高）
    vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}};

    // 启用各向异性过滤和几何着色器
    // DeviceFeatures: GPU 特性开关，需要硬件支持才能启用
    auto deviceFeatures = vsg::DeviceFeatures::create();
    // samplerAnisotropy: 各向异性过滤，远处纹理更清晰
    deviceFeatures->get().samplerAnisotropy = VK_TRUE;
    // geometryShader: 几何着色器，可在顶点着色器后处理图元
    deviceFeatures->get().geometryShader = VK_TRUE;
    
    try {
        // vsg::Device::create: 创建 Vulkan 逻辑设备
        device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, deviceFeatures);
    }
    catch (const vsg::Exception& ex) {
        vsg::error("Error creating Vulkan Device: ", ex.message, ", result code: ", ex.result);
        return;
    }
    
    // vsg::Context: VSG 的 Vulkan 上下文对象，管理 资源分配 和 命令缓冲
    auto context = vsg::Context::create(device);

    // ===================== IBL 资源初始化 =====================
    // 将 Vulkan 上下文传递给 IBL 模块，生成 BRDF LUT、环境贴图等 PBR 资源
    vsgContext.viewer = viewer_IBL;
    vsgContext.context = context;
    vsgContext.device = device;
    vsgContext.queueFamily = queueFamily;
    IBL::appData.options = options;

    loadHDRConfig(); // 读取 HDR 最大数量配置

    IBL::createResources(vsgContext, hdr_image_max_num);  // 创建立方体贴图、LUT 等 GPU 资源
    IBL::generateBRDFLUT(vsgContext);                     // 生成 BRDF 积分查找表
    
    preprocessEnvMap();                                    // 预处理所有 HDR 环境贴图
    
    vsg::info("IBL: Environment lighting data created, creating window");


    // ===================== 创建窗口和离屏渲染目标 =====================
    auto cadWindowTraits = createWindowTraits("Model", 0, options);
    cadWindowTraits->device = device;
    window = vsg::Window::create(cadWindowTraits);
    window->getOrCreateSwapchain();

    // 创建离屏渲染目标（GBuffer + 深度 + SSAO + Shadow 附件）
    offscreenTarget = OffscreenRenderTarget::create();
    offscreenTarget->init(device, window->extent2D(), msaaSamples, window->depthFormat(), cadWindowTraits->depthImageUsage);

    // 构建 3-subpass 的 RenderPass 和 Framebuffer
    bool requiresDepthRead = (cadWindowTraits->depthImageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    offscreenTarget->buildRenderPass(device, window->surfaceFormat().format, window->depthFormat(), requiresDepthRead);
    offscreenTarget->buildFramebuffer(window->extent2D());

    double nearFarRatio = 0.0001;

    // ===================== 场景图构建 =====================
    // 创建各个渲染组，通过 View Mask 控制可见性
    auto modelGroup = vsg::Group::create();          // PBR 虚拟 CAD 模型
    auto modelShadowGroup = vsg::Group::create();
    auto shadowGroup = vsg::Group::create();          // 阴影接收面
    auto envSceneGroup = vsg::Group::create();
    auto wireframeGroup = vsg::Group::create();       // 线框叠加
    auto textGroup = vsg::Group::create();            // 文字叠加
    auto SSAOGroup = vsg::Group::create();            // SSAO 全屏四边形
    auto SSAODenoiseGroup = vsg::Group::create();     // SSAO 降噪全屏四边形

    // rootSwitch: Subpass 0 内容（主渲染 pass）
    // VSG 的 Switch 节点按 mask 位选择性遍历子节点
    auto rootSwitch = vsg::Switch::create();
    rootSwitch->addChild(MASK_CAMERA_IMAGE, drawCameraImageNode);   // 相机图像叠加
    rootSwitch->addChild(MASK_SKYBOX, drawSkyboxNode);              // 天空盒
    rootSwitch->addChild(MASK_SHADOW_RECEIVER, shadowGroup);        // 阴影接收面
    rootSwitch->addChild(MASK_PBR_FULL, modelGroup);                // CAD 模型
    rootSwitch->addChild(MASK_TEXT, textGroup);                     // 文字
    rootSwitch->addChild(MASK_WIREFRAME, wireframeGroup);           // 线框

    // rootSwitch1: Subpass 1→2 过渡 + SSAO 降噪
    // NextSubPass: VSG 节点，推进到下一个 subpass
    auto rootSwitch1 = vsg::Switch::create();
    rootSwitch1->addChild(MASK_SSAO, vsg::NextSubPass::create());  // Subpass 0 → Subpass 1
    rootSwitch1->addChild(MASK_SSAO, SSAOGroup);                   // Subpass 1: SSAO 生成
    rootSwitch1->addChild(MASK_SSAO, vsg::NextSubPass::create());  // Subpass 1 → Subpass 2

    // SSAO 结果的布局转换屏障：COLOR_ATTACHMENT → SHADER_READ_ONLY
    // 在 Subpass 1（SSAO 生成）完成后，将 ssaoResult 转换为着色器可读布局
    auto SSAOPipelineBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,                                                            // dstStageMask
        0
    );
    auto ssaoImageBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,           // 前序：金字塔生成的写入
        VK_ACCESS_SHADER_READ_BIT,                      // 后续：剔除阶段的读取
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        offscreenTarget->ssaoResultImage,
        VkImageSubresourceRange{
            VK_IMAGE_ASPECT_COLOR_BIT,       // 关键！depthPyramidImage是R32_SFLOAT（普通颜色格式），不是深度格式，不能用DEPTH_BIT
            0, 1, 0, 1                       // 同步所有7个mip层
        }
    );
    SSAOPipelineBarrier->add(ssaoImageBarrier);
    rootSwitch1->addChild(MASK_SSAO, SSAOPipelineBarrier);
    rootSwitch1->addChild(MASK_SSAO, SSAODenoiseGroup);             // Subpass 2: SSAO 降噪 + 阴影合并

    // 完整场景图：Subpass 0 内容 + Subpass 1-2 内容
    vsg::ref_ptr<vsg::Group> scenegraph_safe = vsg::Group::create();
    scenegraph_safe->addChild(rootSwitch);
    scenegraph_safe->addChild(rootSwitch1);
    

    // ===================== 相机设置 =====================
    // 使用内参（fx, fy, cx, cy）创建透视投影，适配 AR/MR 场景的相机标定参数
    double radius = 2000.0;
    auto viewport = vsg::ViewportState::create(0, 0, cadWindowTraits->width, cadWindowTraits->height);
    auto perspective = vsg::Perspective::create(fx, fy, cx, cy, width, height, near_plane, far_plane);

    vsg::dvec3 centre = {0.0, 0.0, 1.0};           // 观察目标点
    vsg::dvec3 eye = vsg::dvec3(0.0, 0.0, 0.0);   // 相机位置（原点）
    vsg::dvec3 up = {0.0, -1.0, 0.0};              // Y 轴朝上方向（屏幕坐标系 Y 向下）
    auto lookAt = vsg::LookAt::create(eye, centre, up);
    camera = vsg::Camera::create(perspective, lookAt, viewport);
    pending_camera_matrix = lookAt->transform();
    camera_dirty = false;
    VkExtent2D extent = {};
    extent.width = render_width;
    extent.height = render_height;

    // ===================== 全局常量数据 =====================
    constant_data->value().width = render_width;
    constant_data->value().height = render_height;
    constant_data->value().z_far = 65.535;           // 远平面距离
    constant_data->value().shader_type = shader_type; // 渲染模式（普通/相机深度等）
    constant_data->dirty();
    constant_data_buffer_info_list = {vsg::BufferInfo::create(constant_data)};

    // 将相机和深度图像信息传递给 CADMesh，供着色器采样
    CADMesh::camera_info = camera_info;
    CADMesh::depth_info = depth_info;
    // ===================== 阴影接收面加载 =====================
    if(shadow_receiver_path != "" && shader_type != CAMERA_DEPTH)
    {
        CADMesh* shadow_receiver_mesh = new CADMesh();
        shadow_receiver_mesh->preprocessProtoData(shadow_receiver_path.c_str(), getDirectoryPath(shadow_receiver_path).c_str(), shadow_receiver_transform, shadow_shader, shadowGroup, "shadow_receiver");
    }
    // ===================== 加载 CAD 模型 =====================
    // 支持 OBJ 和 FB (FlatBuffers) 两种格式，同一模型只加载一次（缓存在 transfered_meshes 中）
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
            transfer_model->preprocessFBProtoData(path_i, texture_path_i.c_str(), model_transforms[i], IBL::customPbrShaderSet(options), modelGroup, instance_names[i]);
        }
    }
    // 填充场景实例元数据（名称映射、变换矩阵备份）
    // 填充CADMesh场景实例静态数据
    CADMesh::scene_instance_names = instance_names;
    CADMesh::scene_original_transforms = model_transforms;
    // 记录实例名→相对路径映射（engine_path + 相对路径 = 绝对路径）
    for (int i = 0; i < model_paths.size(); i++) {
        std::string rel_path = model_paths[i];
        if (rel_path.find(engine_path) == 0)
            rel_path = rel_path.substr(engine_path.length());
        CADMesh::instance_name_to_rel_path[instance_names[i]] = rel_path;
    }
    if(shader_type != CAMERA_DEPTH) {
        CADMesh::scene_instance_names.push_back("shadow_receiver");
        CADMesh::scene_original_transforms.push_back(shadow_receiver_transform);
    }


    // ===================== 构建渲染数据 =====================
    // Push Constants: 每帧传递相机矩阵、帧号等全局数据（128 bytes, VK_SHADER_STAGE_ALL）
    vsg::ref_ptr<vsg::PushConstants> pc = vsg::PushConstants::create(
                VK_SHADER_STAGE_ALL, 128, pc_data);

    // 构建各类型的绘制数据（DrawIndexedIndirect 命令 + 描述符集）
    CADMesh::buildDrawData(modelGroup, pc, constant_data_buffer_info_list, offscreenTarget->shadowSampleImageView);
    CADMesh::buildDynamicLinesData(line_shader, wireframeGroup, constant_data_buffer_info_list);
    CADMesh::buildDynamicPointsData(point_shader, wireframeGroup, constant_data_buffer_info_list);
    CADMesh::buildDynamicTextsData(textGroup, options, vsg::findFile("fonts/times.vsgt", options->paths));
    CADMesh::processPMI(transfered_meshes, line_shader, wireframeGroup, textGroup, options, constant_data_buffer_info_list, vsg::findFile("fonts/times.vsgt", options->paths));
    vsg::info("Model processing done");

    // 构建 SSAO 和 SSAO Denoise 的全屏渲染数据
    SSAOPass::buildSSAOData(options, SSAOGroup, offscreenTarget->gbufferImageView0, offscreenTarget->gbufferImageView1, offscreenTarget->gbufferImageView2, extent);
    SSAOPass::buildSSAODenoiseData(options, SSAODenoiseGroup, offscreenTarget->gbufferImageView0, offscreenTarget->shadowWriteImageView, offscreenTarget->ssaoResultImageView);

    // HDR 环境光采样：加载光源配置并添加到场景图
    init_directional_lights();
    update_directional_lights();
    scenegraph_safe->addChild(curLightGroup);

    // ===================== 双 CommandGraph 架构 =====================
    // commandGraph:  frame N 的 Clear → Pass1 → Render Pass
    // commandGraph1: frame N+1 的 Depth Pyramid → Pass2 → Synthesis → Barrier → Copy
    auto commandGraph = vsg::CommandGraph::create(window);
    auto commandGraph1 = vsg::CommandGraph::create(window);
    auto computeQueueFamily = commandGraph->queueFamily;
    auto computeQueueFamily1 = commandGraph1->queueFamily;

    // ===================== View 和 RenderGraph 配置 =====================
    // view (Subpass 0): 主渲染视图 — 相机图像 + CAD 模型 + 阴影接收面
    // 使用 CustomViewDependentState 管理阴影贴图和光源数据
    viewer->addWindow(window);
    view = vsg::View::create(camera, scenegraph_safe);
    CADMesh::active_view = view.get();
    view->mask = MASK_CAMERA_IMAGE | MASK_PBR_FULL | MASK_SHADOW_RECEIVER;
    auto shadow_view_dependent_state = CustomViewDependentState::create(view.get(), device, computeQueueFamily, options);
    view->viewDependentState = shadow_view_dependent_state;
    auto renderGraph = vsg::RenderGraph::create(window, view);

    renderGraph->clearValues[0].color = {{-1.f, -1.f, -1.f, 1.f}};

    // view1 (Subpass 1-2): 合成渲染视图 — CAD 模型 + 线框 + 文字 + SSAO
    // 使用 CustomViewDependentState1（共享主 view 的 descriptor set）
    auto view1 = vsg::View::create(camera, scenegraph_safe);
    view1->mask = MASK_PBR_FULL | MASK_WIREFRAME | MASK_TEXT | MASK_SHADOW_RECEIVER | MASK_SSAO;
    view1->viewDependentState = CustomViewDependentState1::create(view1.get());
    view1->viewDependentState->pre_depth_pass = view->viewDependentState; // 共享主 view 的光源/阴影 descriptor
    auto renderGraph1 = vsg::RenderGraph::create(window, view1);

    // 两个 RenderGraph 都使用同一个离屏 Framebuffer（GBuffer 共享）
    renderGraph->framebuffer = offscreenTarget->framebuffer;
    renderGraph1->framebuffer = offscreenTarget->framebuffer;
    vsgserver::renderer = this;
    auto renderImGui = vsgImGui::RenderImGui::create(window, gui::MyGui::create(pc_data, vsg::findFile("json/Scenes.json", options->paths), vsg::findFile("json/Materials.json", options->paths), vsg::findFile("json/LightInfo.json", options->paths)));
    renderGraph1->addChild(renderImGui);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ===================== 遮挡剔除管线初始化 =====================
    OcclusionCullingPasses::initOcclusionCullingPassesImageInfo(extent, offscreenTarget);
    auto depthPyramidImage = OcclusionCullingPasses::depthPyramidImage;
    auto depth_pyramid_sampler = OcclusionCullingPasses::depth_pyramid_sampler;
    auto depthPyramidImageView = OcclusionCullingPasses::depthPyramidImageView;
    auto depthPyramidImageInfo = OcclusionCullingPasses::depthPyramidImageInfo;
    auto framebuffer_depthImageInfo = OcclusionCullingPasses::framebuffer_depthImageInfo;

    OcclusionCullingPasses::generateCameraData(fx, fy, cx, cy, width, height, near_plane, far_plane, camera);

    // ===================== CommandGraph 构建 =====================
    // CommandGraph: Clear → Pass1 (frustum cull) → Render Pass (3 subpasses)
    auto clear_image_commandgraph = vsg::CommandGraph::create(device, computeQueueFamily);
    Utils::BuildClearCommandGraph(clear_image_commandgraph, extent, offscreenTarget, msaaSamples);
    commandGraph->addChild(clear_image_commandgraph);     // Pass: 清除 GBuffer + 深度
    auto depth_cull_command_graph1 = vsg::CommandGraph::create(device, computeQueueFamily);
    commandGraph->addChild(depth_cull_command_graph1);     // Pass1: 视锥剔除 compute shader
    commandGraph->addChild(renderGraph);                   // Render Pass: Main → SSAO → Denoise

    // CommandGraph1: Depth Pyramid → Pass2 (depth cull) → Synthesis → Barrier → Copy
    auto depth_pyramid_CommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
    commandGraph1->addChild(depth_pyramid_CommandGraph);   // 深度金字塔 + Pass2 剔除
    commandGraph1->addChild(renderGraph1);                 // Synthesis Render (view1)

    // ===================== Barrier: 离屏颜色布局转换 =====================
    // 将离屏颜色附件从 COLOR_ATTACHMENT_OPTIMAL 转换为 GENERAL
    // vkCmdCopyImage 要求源图像在 TRANSFER_SRC 或 GENERAL 布局
    {
        // 创建在计算队列上执行的命令图，专门用于执行图像布局转换
        auto barrierCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);

        // 图像内存屏障：设置颜色图像的布局转换与访问权限变更
        auto offscreenToGeneral = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,    // 旧访问：颜色附件写入权限
            VK_ACCESS_TRANSFER_READ_BIT,             // 新访问：传输读取权限
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,// 旧布局：渲染输出最优布局
            VK_IMAGE_LAYOUT_GENERAL,                 // 新布局：通用布局（支持拷贝）
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            offscreenTarget->colorImage,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        );

        // 管线屏障：指定GPU执行阶段同步（颜色输出阶段 → 传输阶段）
        barrierCommandGraph->addChild(vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 
            offscreenToGeneral
        ));

        // 把屏障命令加入主命令流
        commandGraph1->addChild(barrierCommandGraph);
    }

    // ===================== Copy: 离屏颜色 → Swapchain =====================
    // 将离屏渲染结果拷贝到 Swapchain 图像，用于最终显示
    {
        auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(
            offscreenTarget->colorImageView, window);
        auto copyCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily1);
        copyCommandGraph->addChild(copyImageViewToWindow);
        commandGraph1->addChild(copyCommandGraph);
    }

    // 注册事件处理器（ImGui 输入、关闭、相机控制）
    viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});
    viewer->addEventHandler(vsg::Trackball::create(camera));

    // 将两个 CommandGraph 分配给 RecordAndSubmit 和 Presentation
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph, commandGraph1});
    viewer->compile(); // 编译命令图，将 VSG 节点树转换为可执行的 Vulkan 命令

    // 构建遮挡剔除的 compute pass（需在 compile 之后）
    OcclusionCullingPasses::buildFirstComputePass(depth_cull_command_graph1, options);
    OcclusionCullingPasses::buildDepthPyramid(depth_pyramid_CommandGraph, options, extent, offscreenTarget);
    OcclusionCullingPasses::buildSecondComputePass(depth_pyramid_CommandGraph, options, extent);

    viewer->compile(); // 第二次编译（补充 compute pass 的资源）

    // 截屏/编码处理器
    VkExtent2D encode_extent = {};
    encode_extent.width = encode_width;
    encode_extent.height = encode_height;
    final_screenshotHandler = ScreenshotHandler::create(window, extent, encode_extent, ENCODER);

    // ===================== CUDA-Vulkan 深度互操作 =====================
    // 创建可导出到 CUDA 的 Vulkan 图像，使用 R16_UNORM 格式（与深度像素格式匹配）
    // LINEAR tiling 便于 CUDA 端直接内存访问
    depth_interop_image = vsg::Image::create();
    depth_interop_image->imageType = VK_IMAGE_TYPE_2D;
    depth_interop_image->format = VK_FORMAT_R16_UNORM;
    depth_interop_image->extent.width = width;
    depth_interop_image->extent.height = height;
    depth_interop_image->extent.depth = 1;
    depth_interop_image->arrayLayers = 1;
    depth_interop_image->mipLevels = 1;
    depth_interop_image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_interop_image->samples = VK_SAMPLE_COUNT_1_BIT;
    depth_interop_image->tiling = VK_IMAGE_TILING_LINEAR;
    depth_interop_image->usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    // 启用外部内存导出（CUDA-Vulkan 互操作的 Vulkan 侧配置）
    VkExternalMemoryImageCreateInfo depthExtMemInfo = {};
    depthExtMemInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
    depthExtMemInfo.pNext = nullptr;
    #ifdef _WIN32
    depthExtMemInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
    #else
    depthExtMemInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
    #endif
    depth_interop_image->pNext = &depthExtMemInfo;

    // 内存分配信息：标记为可导出（Win32 HANDLE 或 Linux fd）
    VkExportMemoryAllocateInfo depthExportAllocInfo = {};
    depthExportAllocInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    depthExportAllocInfo.pNext = nullptr;
    #ifdef _WIN32
    depthExportAllocInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
    #else
    depthExportAllocInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
    #endif
    depth_interop_image->pNextAllocInfo = &depthExportAllocInfo;
    depth_interop_image->compile(device); // 编译 Image 并获取内存需求

    // 分配可导出的设备内存并绑定到图像
    auto depthDeviceMemory = vsg::DeviceMemory::create(device,
        depth_interop_image->getMemoryRequirements(device->deviceID),
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &depthExportAllocInfo);
    depth_interop_image->bind(depthDeviceMemory, 0);

    auto depthBufferSize = depth_interop_image->getMemoryRequirements(device->deviceID).size;
    VkExtent2D depthExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    // 创建 CUDA 侧的互操作图像对象：导入 Vulkan 内存句柄到 CUDA
    depth_cuimage = new Cudaimage(depth_interop_image, device, depthBufferSize, depthExtent);

    vsg::info("CUDA-Vulkan depth interop image created, buffer size = ", depthBufferSize);

    // 初始化 GPU 拷贝基础设施（vkCmdCopyImage: interop → depth_info image）
    auto interopPhysicalDevice = window->getPhysicalDevice();
    auto interopQueueFamilyIndex = interopPhysicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    depth_copy_commandPool = vsg::CommandPool::create(device, interopQueueFamilyIndex);
    depth_copy_fence = vsg::Fence::create(device);
    depth_copy_queue = device->getQueue(interopQueueFamilyIndex);
}

// --------------------------------------------------------------------------
// render: 每帧渲染入口
// 功能: 更新相机参数 → CUDA 深度处理 → 上传颜色纹理 → VSG 渲染循环
// 返回值: true=成功渲染一帧, false=viewer 退出
// --------------------------------------------------------------------------
bool vsgRendererServer::render() {
    // 更新 Push Constant 中的相机位置
    if (camera->viewMatrix->is_compatible(typeid(vsg::LookAt))){
        vsg::LookAt* lookAt = dynamic_cast<vsg::LookAt*>(camera->viewMatrix.get());
        pc_data->value().camera_pos = lookAt->eye;
    }
    pc_data->value().frame_num = ++frame_num;
    pc_data->value().last_view = vsg::mat4(camera->viewMatrix->transform());
    pc_data->dirty();

    // 每帧开始：将当前帧的模型矩阵备份到上一帧缓冲区（用于运动模糊等）
    CADMesh::copyCurrentToLastMatrices();

    // 检查并应用待更新的相机矩阵（由外部 updateCamera() 触发）
    if (camera_dirty) {
        auto lookat = camera->viewMatrix.cast<vsg::LookAt>();
        if (lookat) {
            lookat->set(pending_camera_matrix);
        }
        camera_dirty = false;
    }

    // 更新遮挡剔除用的相机矩阵（view + projection）
    OcclusionCullingPasses::camera_matrix->set(0, (vsg::mat4)camera->viewMatrix->transform());
    OcclusionCullingPasses::camera_matrix->set(1, vsg::mat4(camera->projectionMatrix->transform() * camera->viewMatrix->transform()));
    OcclusionCullingPasses::camera_matrix->dirty();
    auto t0 = std::chrono::high_resolution_clock::now();

    // ===================== 渲染循环 =====================
    while (viewer->advanceToNextFrame()) {

        auto t1 = std::chrono::high_resolution_clock::now();

        // CUDA 深度互操作路径：
        // CPU 深度数据 → CUDA 内核处理（直接写入 interop 内存） → vkCmdCopyImage → 着色器采样
        fix_depth_interop(width, height, depth_pixels, reinterpret_cast<void*>(depth_cuimage->get()));
        // GPU 端拷贝: interop image → depth_info image（着色器可采样的格式）
        copyInteropToDepthImage();

        auto t2 = std::chrono::high_resolution_clock::now();
        // 将 CPU 端的相机颜色像素数据拷贝到 VSG 纹理
        uint8_t* vsg_color_image_beginPointer = static_cast<uint8_t*>(vsg_color_image->dataPointer(0));
        std::copy(color_pixels, color_pixels + width * height * 3, vsg_color_image_beginPointer);

        auto t3 = std::chrono::high_resolution_clock::now();
        vsg_color_image->dirty(); // 标记纹理数据已更新，触发 GPU 重新上传

        gui::global_params->render_func_times[0] = std::chrono::duration<double, std::milli>(t1 - t0).count();
        gui::global_params->render_func_times[1] = std::chrono::duration<double, std::milli>(t2 - t1).count();
        gui::global_params->render_func_times[2] = std::chrono::duration<double, std::milli>(t3 - t2).count();

        auto t4 = std::chrono::high_resolution_clock::now();
        viewer->handleEvents();   // 处理输入事件（鼠标、键盘）

        auto t5 = std::chrono::high_resolution_clock::now();
        viewer->update();         // 更新场景图状态

        auto t6 = std::chrono::high_resolution_clock::now();
        viewer->recordAndSubmit(); // 录制并提交所有 CommandGraph 到 GPU

        auto t7 = std::chrono::high_resolution_clock::now();
        viewer->present();        // 呈现 Swapchain 图像到屏幕

        auto t8 = std::chrono::high_resolution_clock::now();

        gui::global_params->render_func_times[3] = std::chrono::duration<double, std::milli>(t4 - t3).count();
        gui::global_params->render_func_times[4] = std::chrono::duration<double, std::milli>(t5 - t4).count();
        gui::global_params->render_func_times[5] = std::chrono::duration<double, std::milli>(t6 - t5).count();
        gui::global_params->render_func_times[6] = std::chrono::duration<double, std::milli>(t7 - t6).count();
        gui::global_params->render_func_times[7] = std::chrono::duration<double, std::milli>(t8 - t7).count();

        return true;
    }
    return false;
}

// --------------------------------------------------------------------------
// copyInteropToDepthImage: GPU 端深度图像拷贝
// 功能: 通过 vkCmdCopyImage 将 CUDA 互操作深度图像拷贝到 depth_info 图像
//       供后续渲染 pass 的着色器采样使用
// 执行流程:
//   1. 布局转换（pre-copy barrier）
//   2. vkCmdCopyImage
//   3. 布局转换（post-copy barrier）
// --------------------------------------------------------------------------
void vsgRendererServer::copyInteropToDepthImage() {
    auto depth_target_image = depth_info[0]->imageView->image;
    auto command = vsg::Commands::create();

    // 1. Pre-copy barrier: 布局转换
    //    interop image: UNDEFINED → TRANSFER_SRC（准备作为拷贝源）
    //    depth_info image: SHADER_READ_ONLY → TRANSFER_DST（准备作为拷贝目标）
    auto preCopyBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0);

    preCopyBarrier->add(vsg::ImageMemoryBarrier::create(
        0, VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        depth_interop_image,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}));

    preCopyBarrier->add(vsg::ImageMemoryBarrier::create(
        VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        depth_target_image,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}));

    command->addChild(preCopyBarrier);

    // 2. vkCmdCopyImage: interop image → depth_info image
    auto copyImage = vsg::CopyImage::create();
    copyImage->srcImage = depth_interop_image;
    copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyImage->dstImage = depth_target_image;
    copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkImageCopy region = {};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    copyImage->regions.push_back(region);
    command->addChild(copyImage);

    // 3. Post-copy barrier: depth_info image TRANSFER_DST → SHADER_READ_ONLY
    //    拷贝完成后，将 depth_info 转换为着色器可读布局
    auto postCopyBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);

    postCopyBarrier->add(vsg::ImageMemoryBarrier::create(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        depth_target_image,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}));

    command->addChild(postCopyBarrier);

    // 4. 提交 GPU 命令并等待完成（同步提交，确保拷贝在渲染前完成）
    vsg::submitCommandsToQueue(depth_copy_commandPool, depth_copy_fence, 100000000000,
        depth_copy_queue, [&](vsg::CommandBuffer& commandBuffer) {
        command->record(commandBuffer);
    });
}

