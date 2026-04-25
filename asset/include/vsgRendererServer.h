#ifndef VSGRENDERERSERVER_H
#define VSGRENDERERSERVER_H
#pragma  once
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <screenshot.h>
#include <vsg/all.h>
#include "ConfigShader.h"
#include "DepthPreprocessStage.h"
#include "FrameImageResources.h"
#include "ImGui.h"
#include "MyMask.h"
#include "CustomViewDependentState.h"
#include "CustomViewDependentState1.h"
#include "IBL.h"
#include "PlaneLoader.h"
#include "SSAOPass.h"
#include "OcclusionCullingPasses.h"
#include "RenderState.h"
#include "ShaderPreprocessor.h"

#include "json.hpp"
#include "OffscreenRenderTarget.h"

class vsgRendererServer
{
    public:
    vsg::ref_ptr<vsg::Device> device;
    vsg::ref_ptr<vsg::Viewer> viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> viewer_IBL = vsg::Viewer::create();
    vsg::ref_ptr<vsg::View> view;

    std::unordered_map<std::string, CADMesh*> transfered_meshes; //path, mesh*

    vsg::ref_ptr<vsg::ShaderSet> shadow_shader;
    vsg::ref_ptr<vsg::ShaderSet> line_shader;
    vsg::ref_ptr<vsg::ShaderSet> point_shader;

    vsg::ref_ptr<ScreenshotHandler> final_screenshotHandler;

    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::Camera> camera;

    // Offscreen render target for MRT attachments
    vsg::ref_ptr<OffscreenRenderTarget> offscreenTarget;
    vsg::ref_ptr<ColorRenderTarget> ssaoTarget;
    vsg::ref_ptr<ColorRenderTarget> realSceneTarget;
    vsg::ref_ptr<ColorRenderTarget> finalColorTarget;
    vsg::ref_ptr<ColorRenderTarget> resolvedEffectDepthTarget;
    vsg::ref_ptr<ColorRenderTarget> resolvedEffectMaskTarget;
    vsg::ref_ptr<ColorRenderTarget> resolvedEffectNormalTarget;
    vsg::ref_ptr<ColorRenderTarget> resolvedEffectWorldPosTarget;
    vsg::ref_ptr<ColorRenderTarget> resolvedEffectMaterialTarget;
    vsg::ref_ptr<ColorRenderTarget> deferredOpaqueTarget;
    vsg::ref_ptr<ColorRenderTarget> deferredFinalTarget;

    //IBL
    IBL::VsgContext vsgContext = {};
    vsg::ref_ptr<vsg::StateGroup> drawSkyboxNode = vsg::StateGroup::create();
    vsg::ref_ptr<vsg::StateGroup> drawCameraBaseNode = vsg::StateGroup::create();
    std::unordered_map<int, vsg::ref_ptr<vsg::Group>> lightGroups;
    vsg::ref_ptr<vsg::Group> curLightGroup = vsg::Group::create();
    std::unordered_map<int, float> hdr_base_brightness; // baseBrightness value for each HDR environment.
    int hdr_image_num = 4;
    int hdr_image_max_num = 7;
    float camera_tracking_fps = 0.0f;

    std::string shadow_receiver_path;
    vsg::dmat4 shadow_receiver_transform;
    std::unordered_set<std::string> cull_mode_none_model_paths;
    vsg::ref_ptr<vsg::Value<GlobalPCData>> pc_data = vsg::Value<GlobalPCData>::create();
    vsg::ref_ptr<vsg::Value<GlobalConstantData>> global_buffer_data = vsg::Value<GlobalConstantData>::create();
    vsg::BufferInfoList global_buffer_info_list;
    float fx = 386.52199190267083; // Focal length on the x axis.
    float fy = 387.32300428823663; // Focal length on the y axis.
    float cx = 326.5103569741365; // Principal point on the x axis.
    float cy = 237.40293732598795; // Principal point on the y axis.

    float near_plane = 0.1f;
    float far_plane = 65.535f;

    int width;
    int height;
    int render_width;
    int render_height;
    int encode_width;
    int encode_height;

    std::unique_ptr<FrameImageResources> frame_image_resources;
    vsg::ref_ptr<vsg::Value<IBL::DynamicSkyboxParams>> camera_image_params = vsg::Value<IBL::DynamicSkyboxParams>::create();

    DepthPreprocessStage depth_preprocess_stage;

    //every frame's real color and depth
    unsigned char * color_pixels = nullptr;
    unsigned short * depth_pixels = nullptr;
    int enable_real_depth_occlusion = 0;
    int shadow_mode = SHADOW_RECEIVER_PLANE;

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_4_BIT; // MSAA sample count.

    uint32_t frame_num = 0;
    vsg::dmat4 pending_camera_matrix;
    bool camera_dirty = false;
    bool env_lighting_update_ready = false;
    bool env_lighting_update_pending = false;
    vsg::ref_ptr<vsg::WindowTraits> createWindowTraits(std::string windowTitle, int num,  vsg::ref_ptr<vsg::Options> options)
    {
        auto windowTraits = vsg::WindowTraits::create();
        // Final present+GUI runs on the window render pass; keep it single-sampled
        // so the copied composite lands on the same image the GUI pass loads.
        windowTraits->samples = VK_SAMPLE_COUNT_1_BIT;
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
            windowTraits->depthImageUsage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT; // Allow transient depth-memory optimization.
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
    void setRealDepthOcclusion(int enabled)
    {
        enable_real_depth_occlusion = (enabled != 0) ? 1 : 0;
    }

    void setShadowMode(int mode)
    {
        shadow_mode = (mode == SHADOW_REAL_DEPTH) ? SHADOW_REAL_DEPTH : SHADOW_RECEIVER_PLANE;
    }

    void setDepthCompletionParams(const SceneRuntimeState::DepthCompletionParams& params)
    {
        setRealDepthOcclusion(params.enable_real_depth_occlusion);
        setShadowMode(params.shadow_mode);
        depth_preprocess_stage.setParams(params);
    }

    void syncGlobalBufferData()
    {
        auto& global = global_buffer_data->value();
        global.last_view = pc_data ? pc_data->value().last_view : vsg::mat4();
        global.camera_pos = pc_data ? pc_data->value().camera_pos : vsg::vec3();
        global.softness = pc_data ? pc_data->value().softness : 1.0f;
        global.baseBrightness = pc_data ? pc_data->value().baseBrightness : 2.0f;
        global.ssao_radius = pc_data ? pc_data->value().ssao_radius : 0.1f;
        global.exposure = pc_data ? pc_data->value().exposure : 8.0f;
        global.softness_falloff = pc_data ? pc_data->value().softness_falloff : 1.0f;
        global.shadow_bias = pc_data ? pc_data->value().shadow_bias : 0.0001f;
        global.z_far = far_plane;
        global.width = render_width;
        global.height = render_height;
        global.ssao_kernel_size = pc_data ? pc_data->value().ssao_kernel_size : 64;
        global.denoise_size = pc_data ? pc_data->value().denoise_size : 5;
        global.blocker_sample_num = pc_data ? pc_data->value().blocker_sample_num : 16;
        global.pcf_sample_num = pc_data ? pc_data->value().pcf_sample_num : 16;
        global.shadow_type = pc_data ? pc_data->value().shadow_type : 1;
        global.frame_num = pc_data ? pc_data->value().frame_num : 0;
        global.enable_real_depth_occlusion = enable_real_depth_occlusion;
        global.shadow_mode = shadow_mode;
        global_buffer_data->dirty();

        if (camera_image_params)
        {
            camera_image_params->value().exposure = 3.0f;
            camera_image_params->value().gamma = 2.2f;
            camera_image_params->value().width = render_width * 1.0f;
            camera_image_params->value().height = render_height * 1.0f;
            camera_image_params->value().enableRealDepthOcclusion = static_cast<float>(enable_real_depth_occlusion);
            camera_image_params->value().shadowMode = static_cast<float>(shadow_mode);
            camera_image_params->dirty();
        }
    }

    void setWidthAndHeight(int width, int height, double render_scale, double encode_scale){
        this->render_width = width * render_scale;
        this->render_height = height * render_scale;
        this->encode_width = width * encode_scale;
        this->encode_height = height * encode_scale;
        this->width = width;
        this->height = height;

    }

    void setKParameters(float fx, float fy, float cx, float cy){
        this->fx = fx;
        this->fy = fy;
        this->cx = cx;
        this->cy = cy;
    }

    void setUpShader(){
        //-----------------------------------------Configure shaders--------------------------------//
        auto shadersDir = vsg::findFile("shaders", options->paths);
        if (shadersDir.empty())
        {
            throw std::runtime_error("Shader directory not found.");
        }

        ShaderPreprocessor preprocessor(shadersDir.string());
        if (!preprocessor.processAll())
        {
            throw std::runtime_error("Shader preprocessing failed.");
        }

        ConfigShader config_shader;
        shadow_shader = config_shader.buildShadowShader(vsg::findFile("shaders/output/shadow.vert", options->paths).string(), vsg::findFile("shaders/output/shadow.frag", options->paths).string());
        line_shader = config_shader.buildLineShader(vsg::findFile("shaders/output/line.vert", options->paths).string(), vsg::findFile("shaders/output/line.frag", options->paths).string());
        point_shader = config_shader.buildLineShader(vsg::findFile("shaders/output/point.vert", options->paths).string(), vsg::findFile("shaders/output/point.frag", options->paths).string());
    }

    bool runIblViewerFrame(const std::string& stage)
    {
        viewer_IBL->compile();
        if (!viewer_IBL->advanceToNextFrame())
        {
            vsg::error("IBL: viewer did not advance for ", stage);
            return false;
        }

        viewer_IBL->handleEvents();
        viewer_IBL->update();
        viewer_IBL->recordAndSubmit();
        viewer_IBL->present();
        return true;
    }

    void rebuildSkyboxNode()
    {
        auto skyboxNode = IBL::drawSkyboxVSGNode(drawSkyboxNode, render_width, render_height);
        if (!skyboxNode)
        {
            throw std::runtime_error("Failed to rebuild skybox node.");
        }
    }

    void rebuildCameraBaseNode()
    {
        if (!frame_image_resources)
        {
            throw std::runtime_error("Frame image resources are not initialized.");
        }

        auto depthInfo = frame_image_resources->depthInfo();
        if (depthInfo.empty())
        {
            throw std::runtime_error("Frame image depth info is empty.");
        }

        auto cameraBaseNode = IBL::drawCameraBaseVSGNode(
            drawCameraBaseNode,
            render_width,
            render_height,
            frame_image_resources->cameraInfo(),
            depthInfo,
            camera_image_params);
        if (!cameraBaseNode)
        {
            throw std::runtime_error("Failed to rebuild camera base node.");
        }
    }

    void rebuildBackgroundNodes()
    {
        rebuildSkyboxNode();
        rebuildCameraBaseNode();
    }

    void preprocessEnvMap(){
        std::string envmapFilepath = vsg::findFile("textures/" + std::to_string(hdr_image_num) + ".hdr", options->paths).string();
        IBL::generateEnvmap(vsgContext, envmapFilepath, -1);
        IBL::generateIrradianceCube(vsgContext, -1);
        IBL::generatePrefilteredEnvmapCube(vsgContext, -1);
        if (!runIblViewerFrame("selected hdr " + std::to_string(hdr_image_num)))
        {
            throw std::runtime_error("IBL preprocessing failed for selected HDR.");
        }
        for(int i = hdr_image_max_num; i > 0; i--){
            std::string envmapFilepath = vsg::findFile("textures/" + std::to_string(i) + ".hdr", options->paths).string();
            IBL::generateEnvmap(vsgContext, envmapFilepath, i);
            IBL::generateIrradianceCube(vsgContext, i);
            IBL::generatePrefilteredEnvmapCube(vsgContext, i);
            if (!runIblViewerFrame("hdr " + std::to_string(i)))
            {
                throw std::runtime_error("IBL preprocessing failed for HDR " + std::to_string(i) + ".");
            }
        }

        rebuildBackgroundNodes();
    }

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

        rebuildBackgroundNodes();
    }

    void update_directional_lights(){
        curLightGroup->children.clear();
        curLightGroup->addChild(lightGroups[hdr_image_num]);
    }

    void loadHDRConfig();

    void init_directional_lights(){
        std::string json_path = vsg::findFile("json/LightInfo.json", options->paths).string();
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

    vsg::ref_ptr<vsg::Options> options = vsg::Options::create();

    void initRenderer(std::string engine_path, std::vector<vsg::dmat4>& model_transforms, std::vector<std::string>& model_paths, std::vector<std::string>& instance_names, vsg::dmat4 plane_transform);

    void setRealColorAndImage(unsigned char * real_color, unsigned short * real_depth){
        color_pixels = real_color;
        depth_pixels = real_depth;
    }

    void applyFrameInput(const CameraFrameInput& frame_input){
        vsg::dvec3 centre = {frame_input.lookat[0], frame_input.lookat[1], frame_input.lookat[2]};
        vsg::dvec3 eye = {frame_input.lookat[3], frame_input.lookat[4], frame_input.lookat[5]};
        vsg::dvec3 up = {frame_input.lookat[6], frame_input.lookat[7], frame_input.lookat[8]};
        updateCamera(centre, eye, up);
        setRealColorAndImage(frame_input.color, frame_input.depth);
    }

    void updateCamera(vsg::dvec3 centre, vsg::dvec3 eye, vsg::dvec3 up){
        auto lookat = vsg::LookAt::create(eye, centre, up);
        pending_camera_matrix = lookat->transform();
        camera_dirty = true;
    }

    void updateCamera(vsg::dmat4 view_matrix){
        pending_camera_matrix = view_matrix;
        camera_dirty = true;
    }

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

    void updateEnvLighting(){
        updateEnvMap();
        update_directional_lights();
        IBL::textures.params->dirty();
        viewer->compile();
        if (view && view->viewDependentState)
        {
            auto* light_state = static_cast<CustomViewDependentState*>(view->viewDependentState.get());
            if (light_state)
            {
                light_state->draw_shadow_light = true;
            }
        }
    }

    void requestEnvLightingUpdate()
    {
        if (!env_lighting_update_ready || !view || !window || !device || !view->viewDependentState)
        {
            env_lighting_update_pending = true;
            return;
        }

        env_lighting_update_pending = false;
        updateEnvLighting();
    }

    void flushPendingEnvLightingUpdate()
    {
        env_lighting_update_ready = true;
        if (env_lighting_update_pending)
        {
            env_lighting_update_pending = false;
            updateEnvLighting();
        }
    }

    bool render();

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

    void clearPointAndLineData(){
        auto output_line_vertices = static_cast<float*>(CADMesh::dynamic_lines.vertices->dataPointer(0));
        std::fill_n(output_line_vertices, CADMesh::dynamic_lines.vertices->size() * 3, -10000.f);
        CADMesh::dynamic_lines.vertices->dirty();

        auto output_line_indices = static_cast<uint32_t*>(CADMesh::dynamic_lines.indices->dataPointer(0));
        std::fill_n(output_line_indices, CADMesh::dynamic_lines.indices->size(), 0);
        CADMesh::dynamic_lines.indices->dirty();

        auto output_point_vertices = static_cast<float*>(CADMesh::dynamic_points.vertices->dataPointer(0));
        std::fill_n(output_point_vertices, CADMesh::dynamic_points.vertices->size() * 3, -10000.f);
        CADMesh::dynamic_points.vertices->dirty();

        auto output_point_indices = static_cast<uint32_t*>(CADMesh::dynamic_points.indices->dataPointer(0));
        std::fill_n(output_point_indices, CADMesh::dynamic_points.indices->size(), 0);
        CADMesh::dynamic_points.indices->dirty();
    }

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
    void repaint(std::string instance_name, uint32_t state){
        auto& matrix_index = CADMesh::id_to_matrix_index_map[instance_name];

        for(int i = 0; i < matrix_index.size(); i ++){
            auto proto = matrix_index[i].proto_data;
            auto index = matrix_index[i].index;
            proto->highlight_buffer->set(index * 4, state);
            proto->highlight_buffer->dirty();
        }
    }

    void getWindowImage(uint8_t* color){
        final_screenshotHandler->screenshot_cpuimage(window, color);
    }

    void getEncodeImage(std::vector<std::vector<uint8_t>>& vPacket){
        final_screenshotHandler->encodeImage(window, vPacket);
    }
};

#endif //VSGR_RENDERER_SERVER_H
