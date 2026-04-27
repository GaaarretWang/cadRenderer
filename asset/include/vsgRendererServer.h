/**
 * vsgRendererServer.h - 渲染引擎核心渲染器类
 *
 * 职责：管理整个渲染管线（pipeline），包括：
 * - 双 CommandGraph 架构（主渲染 + IBL预计算）
 * - IBL (Image-Based Lighting) 环境光照资源
 * - CUDA-Vulkan 互操作（interop）用于深度图处理
 * - 相机管理、模型加载、渲染状态编排
 *
 * VSG（VulkanSceneGraph）核心概念速查：
 * - ref_ptr<T>：VSG 的智能指针（类似 std::shared_ptr），管理 Vulkan 资源生命周期
 * - Viewer：VSG 的主循环控制器，负责事件处理、帧更新、命令录制与提交
 * - View：代表一个渲染视图（绑定相机 + 场景图根节点）
 * - CommandGraph：命令图，描述一帧中需要执行的 Vulkan 命令序列
 * - StateGroup：状态组，管理渲染状态（shader、纹理、uniform等）的场景图节点
 * - Device：Vulkan 逻辑设备，GPU 资源的抽象接口
 */
#ifndef VSGRENDERERSERVER_H
#define VSGRENDERERSERVER_H
#pragma  once
#include <iostream>
#include <unordered_set>
#include <screenshot.h>
#include <vsg/all.h>
#include "ConfigShader.h"
#include "ImGui.h"
#include "MyMask.h"
#include "CustomViewDependentState.h"
#include "CustomViewDependentState1.h"
#include "IBL.h"
#include "PlaneLoader.h"
#include "SSAOPass.h"
#include "OcclusionCullingPasses.h"

#include "fixDepth.h"
#include "json.hpp"
#include "OffscreenRenderTarget.h"

class vsgRendererServer
{
    public:
    // ---- VSG 核心对象 ----
    vsg::ref_ptr<vsg::Device> device;           // Vulkan 逻辑设备，所有 GPU 资源的创建入口
    vsg::ref_ptr<vsg::Viewer> viewer = vsg::Viewer::create();     // 主渲染循环：驱动帧更新、事件处理、命令录制提交
    vsg::ref_ptr<vsg::Viewer> viewer_IBL = vsg::Viewer::create(); // 专用 IBL 预计算循环（独立于主渲染）
    vsg::ref_ptr<vsg::View> view;               // 渲染视图：绑定相机 + 场景图根节点

    // 已加载的 CAD 网格数据，key 为模型文件路径
    std::unordered_map<std::string, CADMesh*> transfered_meshes;

    // ShaderSet：VSG 中一组 shader 程序的集合（vert+frag），封装了顶点输入布局和 uniform 描述
    vsg::ref_ptr<vsg::ShaderSet> shadow_shader;   // 阴影渲染 shader
    vsg::ref_ptr<vsg::ShaderSet> line_shader;      // 线框渲染 shader
    vsg::ref_ptr<vsg::ShaderSet> point_shader;     // 点云渲染 shader

    vsg::ref_ptr<ScreenshotHandler> final_screenshotHandler; // 截图/编码处理器

    vsg::ref_ptr<vsg::Window> window;   // Vulkan 窗口/swapchain 的 VSG 封装
    vsg::ref_ptr<vsg::Camera> camera;   // VSG 相机（包含 projection + view 矩阵）

    // 离屏渲染目标，用于 MRT（Multiple Render Target）多附件输出
    vsg::ref_ptr<OffscreenRenderTarget> offscreenTarget;

    // ---- IBL (Image-Based Lighting) 环境光照资源 ----
    // vsgContext：IBL 模块的 VSG 上下文，持有 IBL 预计算所需的 Vulkan 资源
    IBL::VsgContext vsgContext = {};
    // StateGroup：VSG 场景图中管理渲染状态的节点。挂载在它下面的子节点会继承其 shader/纹理/uniform 等状态
    vsg::ref_ptr<vsg::StateGroup> drawSkyboxNode = vsg::StateGroup::create();          // 天空盒渲染节点
    vsg::ref_ptr<vsg::StateGroup> drawCameraImageNode = vsg::StateGroup::create();     // 相机图像叠加节点（用于 AR 融合）
    vsg::ref_ptr<vsg::StateGroup> drawIBLSceneNode = vsg::StateGroup::create();        // IBL 场景主渲染节点
    vsg::ref_ptr<vsg::StateGroup> drawIBLBackgroundNode = vsg::StateGroup::create();   // IBL 背景渲染节点
    vsg::ref_ptr<vsg::StateGroup> drawShadowBackgroundNode = vsg::StateGroup::create(); // 阴影背景渲染节点
    // Group：VSG 场景图中的普通分组节点，只管理子节点列表，不携带渲染状态
    std::unordered_map<int, vsg::ref_ptr<vsg::Group>> lightGroups;        // HDR索引 → 该 HDR 对应的光源组
    vsg::ref_ptr<vsg::Group> curLightGroup = vsg::Group::create();         // 当前激活的光源组
    std::unordered_map<int, vsg::ref_ptr<vsg::Group>> hdr_to_light_group_map;
    std::unordered_map<int, float> hdr_base_brightness;    // 每个 HDR 的基础亮度系数
    int hdr_image_num = 4;       // 当前使用的 HDR 环境贴图索引
    int hdr_image_max_num = 7;   // HDR 环境贴图总数

    // 阴影接收体信息
    std::string shadow_receiver_path;           // 阴影接收面的模型路径
    vsg::dmat4 shadow_receiver_transform;       // 阴影接收面的世界变换矩阵
    std::unordered_set<std::string> cull_mode_none_model_paths; // 不做背面剔除的模型路径集合

    // Push Constant：Vulkan 中每帧/每次 draw call 传入 shader 的轻量级 uniform 数据
    // vsg::Value<T>：VSG 的 typed 数据容器，支持 GPU buffer 绑定和 dirty() 标记
    vsg::ref_ptr<vsg::Value<GlobalPCData>> pc_data = vsg::Value<GlobalPCData>::create();       // 每帧全局 push constant
    vsg::ref_ptr<vsg::Value<GlobalConstantData>> constant_data = vsg::Value<GlobalConstantData>::create(); // 全局常量 buffer
    vsg::BufferInfoList constant_data_buffer_info_list; // constant_data 的 GPU buffer 绑定信息

    // ---- 相机内参（Camera Intrinsics）----
    // 来自真实相机标定参数，用于虚实融合（virtual-reality fusion）的投影对齐
    float fx = 386.52199190267083;   // 焦距 x（像素单位）
    float fy = 387.32300428823663;   // 焦距 y（像素单位）
    float cx = 326.5103569741365;    // 主点 x（图像中心偏移）
    float cy = 237.40293732598795;   // 主点 y（图像中心偏移）

    // 投影矩阵远近平面
    float near_plane = 0.1f;
    float far_plane = 65.535f;

    // 分辨率体系：width/height 为逻辑分辨率，render_* 为实际渲染分辨率，encode_* 为编码输出分辨率
    int width;
    int height;
    int render_width;
    int render_height;
    int encode_width;
    int encode_height;

    // VSG Data 对象：封装像素数据的容器，可直接绑定到 vsg::ImageInfo 用于 shader 采样
    vsg::ref_ptr<vsg::Data> vsg_color_image;  // 颜色附件数据
    vsg::ref_ptr<vsg::Data> vsg_depth_image;  // 深度附件数据
    vsg::ImageInfoList camera_info;  // 相机图像的 ImageInfo（sampler + image），供 shader 采样
    vsg::ImageInfoList depth_info;   // 深度图的 ImageInfo

    // ---- CUDA-Vulkan 互操作（Interop）----
    // 用于在 CUDA 和 Vulkan 之间共享深度图，避免 CPU 端拷贝
    // depth_interop_image：Vulkan Image，通过 external memory 扩展导出给 CUDA 使用
    vsg::ref_ptr<vsg::Image> depth_interop_image;
    Cudaimage* depth_cuimage = nullptr;  // CUDA 侧的映射句柄，CUDA kernel 直接读写该内存

    // GPU 端拷贝管线：将 interop image 的内容拷贝到 depth_info 对应的 image
    vsg::ref_ptr<vsg::CommandPool> depth_copy_commandPool; // 命令池，用于分配 copy 命令
    vsg::ref_ptr<vsg::Fence> depth_copy_fence;             // 围栏，用于 CPU 同步等待拷贝完成
    vsg::ref_ptr<vsg::Queue> depth_copy_queue;             // 执行拷贝的 GPU 队列

    // 每帧从 GPU 读回的像素数据（CPU 端缓冲区）
    unsigned char * color_pixels = nullptr;   // 颜色像素（RGBA）
    unsigned short * depth_pixels = nullptr;  // 深度像素（16-bit）
    mergeShaderType shader_type;              // 合成 shader 类型（决定渲染管线路径）

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_4_BIT; // 多重采样倍数（4x MSAA）

    uint32_t frame_num = 0;                   // 帧计数器
    vsg::dmat4 pending_camera_matrix;         // 待应用的相机矩阵（dirty 标记模式，延迟到 render() 统一更新）
    bool camera_dirty = false;                // 相机是否需要更新

    /**
     * 创建 WindowTraits（VSG 窗口配置对象）
     * WindowTraits 封装了 Vulkan 窗口的创建参数，包括：
     * - swapchain 配置（图像用途、格式）
     * - 深度缓冲格式和用途
     * - 多重采样（MSAA）设置
     * - 所需的 Vulkan 设备扩展（external memory/semaphore 用于 CUDA 互操作）
     */
    vsg::ref_ptr<vsg::WindowTraits> createWindowTraits(std::string windowTitle, int num,  vsg::ref_ptr<vsg::Options> options)
    {
        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->samples = msaaSamples;  // 设置多重采样
        windowTraits->windowTitle = windowTitle;
        windowTraits->width = render_width;
        windowTraits->height = render_height;
        windowTraits->x = render_width * (num % 2);
        windowTraits->y = render_height * (num / 2);
        // enable transfer from the colour and depth buffer images
        windowTraits->swapchainPreferences.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        windowTraits->depthImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                         | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        windowTraits->depthFormat = VK_FORMAT_D32_SFLOAT;

        // if we are multisampling then to enable copying of the depth buffer we have to enable a depth buffer resolve extension for vsg::RenderPass or require a minimum vulkan version of 1.2
        if (windowTraits->samples != VK_SAMPLE_COUNT_1_BIT)
        {
            windowTraits->vulkanVersion = VK_API_VERSION_1_2;
            windowTraits->depthImageUsage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT; // 优化内存
        }
        windowTraits->deviceExtensionNames = {
            VK_KHR_MULTIVIEW_EXTENSION_NAME,
            VK_KHR_MAINTENANCE2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
            VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, 
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
            #ifdef _WIN32
            VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
            #else
            VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME
            #endif
            };
        return windowTraits;
    }

public:
    // 设置渲染分辨率体系，render_scale 控制渲染倍率，encode_scale 控制编码输出倍率
    void setWidthAndHeight(int width, int height, double render_scale, double encode_scale){
        this->render_width = width * render_scale;
        this->render_height = height * render_scale;
        this->encode_width = width * encode_scale;
        this->encode_height = height * encode_scale;
        this->width = width;
        this->height = height;

    }

    // 设置相机内参（来自真实相机标定），用于虚实投影对齐
    void setKParameters(float fx, float fy, float cx, float cy){
        this->fx = fx;
        this->fy = fy;
        this->cx = cx;
        this->cy = cy;
    }

    // 初始化三套 shader 程序（shadow / line / point），从 shaders/ 目录加载 .vert/.frag 文件
    void setUpShader(){
        //-----------------------------------------设置shader------------------------------------//
        ConfigShader config_shader;
        shadow_shader = config_shader.buildShadowShader(vsg::findFile("shaders/shadow.vert", options->paths), vsg::findFile("shaders/shadow.frag", options->paths));
        line_shader = config_shader.buildLineShader(vsg::findFile("shaders/line.vert", options->paths), vsg::findFile("shaders/line.frag", options->paths));
        point_shader = config_shader.buildLineShader(vsg::findFile("shaders/point.vert", options->paths), vsg::findFile("shaders/point.frag", options->paths));
    }

    /**
     * 预处理所有 HDR 环境贴图，生成 IBL 所需的 cubemap 资源
     * 每个 HDR 贴图需要生成三种 cubemap，通过渲染到纹理实现：
     * - Envmap：原始环境贴图 cubemap
     * - IrradianceCube：漫反射辐照度 cubemap（低频，用于间接光照）
     * - PrefilteredEnvmapCube：预滤波镜面反射 cubemap（多级 mip 对应不同粗糙度）
     *
     * 使用 viewer_IBL（专用 Viewer）驱动预计算渲染循环
     */
    void preprocessEnvMap(){
        // ===================== 第 1 遍：处理当前激活的 HDR =====================
        // hdr_image_num = 4（当前使用的 HDR 编号）
        // -1 表示写入主纹理（envmapCube / irradianceCube / prefilterCube）
        // 这些主纹理将直接被着色器使用

        // 查找当前 HDR 文件路径（如 textures/4.hdr）
        std::string envmapFilepath = vsg::findFile("textures/" + std::to_string(hdr_image_num) + ".hdr", options->paths);
        // 生成环境贴图 cubemap（equirectangular → cubemap）
        IBL::generateEnvmap(vsgContext, envmapFilepath, -1);
        // 生成漫反射辐照度 cubemap（球面调和卷积）
        IBL::generateIrradianceCube(vsgContext, -1);
        // 生成预滤波镜面反射 cubemap（GGX 重要性采样）
        IBL::generatePrefilteredEnvmapCube(vsgContext, -1);

        // 编译 CommandGraph：收集资源需求，构建渲染管线
        viewer_IBL->compile();
        // 驱动一帧渲染循环，执行离线预计算
        bool process_done = false;
        while (viewer_IBL->advanceToNextFrame())  // 获取下一帧，返回 false 表示退出
        {
            if(process_done)
                break;
            viewer_IBL->handleEvents();       // 处理窗口输入事件（键盘、鼠标等）
            viewer_IBL->update();             // 更新场景图中标记 dirty 的数据
            viewer_IBL->recordAndSubmit();    // 录制 Vulkan 命令并提交到 GPU 队列
            viewer_IBL->present();            // 将渲染结果呈现到窗口/交换链
            process_done = true;              // 标记已完成，只执行一帧
        }

        // ===================== 第 2 遍：预生成其他 HDR 的缓存纹理 =====================
        // 从 hdr_image_max_num (7) 倒序遍历到 1
        // 将结果存入 testMap[i] / irraMap[i] / prefMap[i]
        // 运行时切换 HDR 时，通过 updateHDRTextures 快速拷贝到主纹理
        for(int i = hdr_image_max_num; i > 0; i--){
            // 跳过当前 HDR（已在第 1 遍处理）
            if(i == hdr_image_num) continue;

            // 查找第 i 个 HDR 文件路径
            std::string envmapFilepath = vsg::findFile("textures/" + std::to_string(i) + ".hdr", options->paths);
            IBL::generateEnvmap(vsgContext, envmapFilepath, i);
            IBL::generateIrradianceCube(vsgContext, i);
            IBL::generatePrefilteredEnvmapCube(vsgContext, i);

            // 编译 CommandGraph
            viewer_IBL->compile();
            // 执行一帧渲染，完成离线预计算
            bool process_done = false;
            while (viewer_IBL->advanceToNextFrame())
            {
                if(process_done)
                    break;
                viewer_IBL->handleEvents();
                viewer_IBL->update();
                viewer_IBL->recordAndSubmit();
                viewer_IBL->present();
                process_done = true;
            }
        }

        // ===================== 第 3 遍：构建天空盒渲染节点 =====================
        // 创建天空盒 StateGroup（绑定 IBL 纹理、shader、几何数据）
        IBL::drawSkyboxVSGNode(vsgContext, drawSkyboxNode, render_width, render_height);
        // 创建相机图像叠加节点（AR 融合用）
        IBL::drawSkyboxVSGNode(vsgContext, drawCameraImageNode, render_width, render_height, camera_info,
                               shader_type == CAMERA_DEPTH ? depth_info : vsg::ImageInfoList{},
                               shader_type == CAMERA_DEPTH ? vsg::ref_ptr<vsg::Data>(pc_data) : vsg::ref_ptr<vsg::Data>{});
    }

    // 运行时切换 HDR 环境贴图：更新 HDR 纹理并重建天空盒节点
    void updateEnvMap(){
        auto command = vsg::Commands::create();
        IBL::updateHDRTextures(command, hdr_image_num);

        auto physicalDevice = window->getPhysicalDevice();
        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) {
            command->record(commandBuffer);
        });

        IBL::drawSkyboxVSGNode(vsgContext, drawSkyboxNode, render_width, render_height);
        IBL::drawSkyboxVSGNode(vsgContext, drawCameraImageNode, render_width, render_height, camera_info,
                               shader_type == CAMERA_DEPTH ? depth_info : vsg::ImageInfoList{},
                               shader_type == CAMERA_DEPTH ? vsg::ref_ptr<vsg::Data>(pc_data) : vsg::ref_ptr<vsg::Data>{});
    }

    // 切换当前激活的光源组为当前 HDR 对应的光源配置
    void update_directional_lights(){
        curLightGroup->children.clear();
        curLightGroup->addChild(lightGroups[hdr_image_num]);
    }

    // 从 LightInfo.json 读取 HDR 最大数量配置
    void loadHDRConfig(){
        std::string json_path = vsg::findFile("json/LightInfo.json", options->paths);
        std::ifstream json_file(json_path);

        if (!json_file.is_open()) {
            std::cerr << "错误：无法打开光源配置文件 " << json_path << std::endl;
            return;
        }

        json json_data = json::parse(json_file);
        json_file.close();

        if (json_data.contains("hdr_image_max_num")) {
            hdr_image_max_num = json_data["hdr_image_max_num"].get<int>();
        }
    }

    // 从 LightInfo.json 解析所有 HDR 的方向光源配置（方向、强度、面积），初始化 lightGroups
    void init_directional_lights(){
        std::string json_path = vsg::findFile("json/LightInfo.json", options->paths);
        std::ifstream json_file(json_path);

        if (!json_file.is_open()) {
            std::cerr << "错误：无法打开光源配置文件 " << json_path << std::endl;
            return;
        }

        json json_data = json::parse(json_file);
        json_file.close();

        for (auto& [hdr_idx_str, hdr_data] : json_data.items()) {
            // Skip non-HDR entries (like hdr_image_max_num)
            if (!hdr_data.is_object() || !hdr_data.contains("lights")) {
                continue;
            }
            std::cout << hdr_idx_str << std::endl;
            int hdr_idx = std::stoi(hdr_idx_str);
            vsg::ref_ptr<vsg::Group> light_i = vsg::Group::create();
            lightGroups[hdr_idx] = light_i;

            // 读取 baseBrightness（如果存在）
            if (hdr_data.contains("baseBrightness")) {
                hdr_base_brightness[hdr_idx] = hdr_data["baseBrightness"].get<float>();
            }

            for (auto& light_data : hdr_data["lights"]) {
                auto directional_light = vsg::DirectionalLight::create();
                directional_light->area = light_data["area"].get<float>();
                directional_light->intensity = light_data["brightness"].get<float>();
                auto direction = light_data["direction"].get<std::vector<float>>();
                directional_light->direction = -vsg::normalize(vsg::vec3(direction[0], direction[1], direction[2]));
                directional_light->shadowMaps = 1;
                light_i->addChild(directional_light);
            }
        }
    }

    // 工具方法：将 vsg::Data 包装为 ImageInfoList，供 shader 采样使用
    vsg::ImageInfoList createImageInfo(vsg::ref_ptr<vsg::Data> in_data){
        auto sampler = vsg::Sampler::create();
        sampler->magFilter = VK_FILTER_NEAREST;
        sampler->minFilter = VK_FILTER_NEAREST;

        vsg::ref_ptr<vsg::ImageInfo> imageInfosIBL = vsg::ImageInfo::create(sampler, in_data);
        vsg::ImageInfoList imageInfosListIBL = {imageInfosIBL};
        return imageInfosListIBL;
    }
    // VSG 资源查找选项，控制模型/纹理/shader 的搜索路径
    vsg::ref_ptr<vsg::Options> options = vsg::Options::create();

    // 初始化渲染器：加载模型、构建场景图 CommandGraph、编译 Vulkan 管线（最重的初始化步骤）
    void initRenderer(std::string engine_path, std::vector<vsg::dmat4>& model_transforms, std::vector<std::string>& model_paths, std::vector<std::string>& instance_names, vsg::dmat4 plane_transform);
    
    // 设置真实世界图像数据指针（来自 AR 相机），用于虚实融合渲染
    void setRealColorAndImage(unsigned char * real_color, unsigned short * real_depth){
        color_pixels = real_color;
        depth_pixels = real_depth;
    }

    // 通过 eye/centre/up 三元组更新相机（lookAt 方式），标记 dirty 延迟到 render() 统一更新
    void updateCamera(vsg::dvec3 centre, vsg::dvec3 eye, vsg::dvec3 up){
        auto lookat = vsg::LookAt::create(eye, centre, up);
        pending_camera_matrix = lookat->transform();
        camera_dirty = true;
    }

    // 通过 4x4 view 矩阵直接更新相机
    void updateCamera(vsg::dmat4 view_matrix){
        pending_camera_matrix = view_matrix;
        camera_dirty = true;
    }
    
    // 更新指定实例的世界变换矩阵（支持 model 级别和 instance 级别），并触发阴影重绘
    void updateObjectPose(std::string instance_name, vsg::dmat4 model_matrix){
        auto& matrix_index = CADMesh::id_to_matrix_index_map[instance_name];

        bool is_model_level = (CADMesh::model_name_to_global_index.count(instance_name) > 0);

        if (is_model_level) {
            uint32_t model_idx = CADMesh::model_name_to_global_index[instance_name];
            CADMesh::global_model_matrix_buffer->set(model_idx, vsg::mat4(model_matrix));
            CADMesh::global_model_matrix_buffer->dirty();
            CADMesh::updatePMITransforms(model_idx);
        } else {
            for(int i = 0; i < matrix_index.size(); i++){
                auto proto = matrix_index[i].proto_data;
                auto index = matrix_index[i].index;
                proto->instance_buffer->set(index, vsg::mat4(model_matrix));
                proto->instance_buffer->dirty();
            }
        }
        auto* cvds_pose = static_cast<CustomViewDependentState*>(view->viewDependentState.get());
        cvds_pose->draw_shadow_pose = true;
    }

    /**
     * 运行时更新环境光照：重新生成天空盒、切换光源、标记 IBL 纹理 dirty
     * viewer->compile() 会重新编译受影响的 CommandGraph，确保新纹理绑定生效
     */
    void updateEnvLighting(){
        updateEnvMap();
        update_directional_lights();
        IBL::textures.params->dirty();
        viewer->compile(); //编译命令图。接受一个可选的`ResourceHints`对象作为参数，用于提供编译时的一些提示和配置。通过调用这个函数，可以将命令图编译为可执行的命令。
        auto* cvds_light = static_cast<CustomViewDependentState*>(view->viewDependentState.get());
        cvds_light->draw_shadow_light = true;
    }

    // 执行一帧渲染：处理相机更新、录制 CommandGraph、提交到 GPU、读回像素
    bool render();

    // 动态更新线框数据（顶点+索引），用 -10000.f 填充未使用区域使其不可见
    void addLineData(float* vertices_pointer, size_t vertices_size, uint32_t* indices_pointer, size_t indices_size){
        auto output_vertices = static_cast<float*>(CADMesh::dynamic_lines.vertices->dataPointer(0));
        std::fill_n(output_vertices, CADMesh::dynamic_lines.vertices->size() * 3, -10000.f);
        std::copy(vertices_pointer, vertices_pointer + std::min(vertices_size, CADMesh::dynamic_lines.vertices->size() * 3), output_vertices);
        CADMesh::dynamic_lines.vertices->dirty();

        auto output_indices = static_cast<uint32_t*>(CADMesh::dynamic_lines.indices->dataPointer(0));
        std::fill_n(output_indices, CADMesh::dynamic_lines.indices->size(), 0);
        std::copy(indices_pointer, indices_pointer + std::min(indices_size, CADMesh::dynamic_lines.indices->size()), output_indices);
        CADMesh::dynamic_lines.indices->dirty();
    }

    // 动态更新点云数据（顶点+索引），与 addLineData 逻辑相同
    void addPointData(float* vertices_pointer, size_t vertices_size, uint32_t* indices_pointer, size_t indices_size){
        auto output_vertices = static_cast<float*>(CADMesh::dynamic_points.vertices->dataPointer(0));
        std::fill_n(output_vertices, CADMesh::dynamic_points.vertices->size() * 3, -10000.f);
        std::copy(vertices_pointer, vertices_pointer + std::min(vertices_size, CADMesh::dynamic_points.vertices->size() * 3), output_vertices);
        CADMesh::dynamic_points.vertices->dirty();

        auto output_indices = static_cast<uint32_t*>(CADMesh::dynamic_points.indices->dataPointer(0));
        std::fill_n(output_indices, CADMesh::dynamic_points.indices->size(), 0);
        std::copy(indices_pointer, indices_pointer + std::min(indices_size, CADMesh::dynamic_points.indices->size()), output_indices);
        CADMesh::dynamic_points.indices->dirty();
    }

    // 动态更新文本标注数据（文字内容 + 布局属性如位置、颜色、billboard 等）
    void addTextData(std::vector<std::string>& texts, std::vector<vsg::ref_ptr<vsg::StandardLayout>>& dynamic_text_layouts){
        for(int i = 0; i < std::min(CADMesh::dynamic_texts.text.size(), texts.size()); i ++){
            CADMesh::dynamic_texts.dynamic_text_labels[i]->value() = texts[i];
            CADMesh::dynamic_texts.standardLayout[i]->billboard = dynamic_text_layouts[i]->billboard;
            CADMesh::dynamic_texts.standardLayout[i]->position = dynamic_text_layouts[i]->position;
            CADMesh::dynamic_texts.standardLayout[i]->horizontal = dynamic_text_layouts[i]->horizontal;
            CADMesh::dynamic_texts.standardLayout[i]->vertical = dynamic_text_layouts[i]->vertical;
            CADMesh::dynamic_texts.standardLayout[i]->color = dynamic_text_layouts[i]->color;
            CADMesh::dynamic_texts.standardLayout[i]->outlineWidth = dynamic_text_layouts[i]->outlineWidth;
            CADMesh::dynamic_texts.text[i]->setup(0, options);
        }
    }
    // 设置指定实例的高亮/选中状态（通过 highlight_buffer 传入 shader）
    void repaint(std::string instance_name, uint32_t state){
        auto& matrix_index = CADMesh::id_to_matrix_index_map[instance_name];

        for(int i = 0; i < matrix_index.size(); i ++){
            auto proto = matrix_index[i].proto_data;
            auto index = matrix_index[i].index;
            proto->highlight_buffer->set(index * 4, state);
            proto->highlight_buffer->dirty();
        }
    }

    // 从 GPU 读回当前帧的颜色图像到 CPU 缓冲区
    void getWindowImage(uint8_t* color){
        final_screenshotHandler->screenshot_cpuimage(window, color);
    }

    // 将当前帧编码为压缩格式（如 JPEG/H.264），用于网络传输
    void getEncodeImage(std::vector<std::vector<uint8_t>>& vPacket){
        final_screenshotHandler->encodeImage(window, vPacket);
    }

    // GPU 端拷贝：将 CUDA interop 写入的深度图拷贝到 depth_info 对应的 Vulkan image（vkCmdCopyImage）
    void copyInteropToDepthImage();
};

#endif //VSGR_RENDERER_SERVER_H