#ifndef VSGRENDERERSERVER_H
#define VSGRENDERERSERVER_H
#pragma  once
#include <iostream>
#include <unordered_set>
#include <screenshot.h>
#include <vsg/all.h>
#include "convertPng.h"
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
using namespace std;

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

    //IBL
    IBL::VsgContext vsgContext = {};
    vsg::ref_ptr<vsg::StateGroup> drawSkyboxNode = vsg::StateGroup::create();
    vsg::ref_ptr<vsg::StateGroup> drawCameraImageNode = vsg::StateGroup::create();
    vsg::ref_ptr<vsg::StateGroup> drawIBLSceneNode = vsg::StateGroup::create();
    vsg::ref_ptr<vsg::StateGroup> drawIBLBackgroundNode = vsg::StateGroup::create();
    vsg::ref_ptr<vsg::StateGroup> drawShadowBackgroundNode = vsg::StateGroup::create();
    std::unordered_map<int, vsg::ref_ptr<vsg::Group>> lightGroups;
    vsg::ref_ptr<vsg::Group> curLightGroup = vsg::Group::create();
    std::unordered_map<int, vsg::ref_ptr<vsg::Group>> hdr_to_light_group_map;
    int hdr_image_num = 2;
    int hdr_image_max_num = 5;

    std::string project_path;
    std::string shadow_recevier_path;
    vsg::dmat4 shadow_recevier_transform;
    std::unordered_set<std::string> cull_mode_none_model_paths;
    vsg::ref_ptr<vsg::Value<GlobalPCData>> pc_data = vsg::Value<GlobalPCData>::create();

    float fx = 386.52199190267083;//焦距(x轴上)
    float fy = 387.32300428823663;//焦距(y轴上)
    float cx = 326.5103569741365;//图像中心点(x轴)
    float cy = 237.40293732598795;//图像中心点(y轴)

    float near_plane = 0.1f;
    float far_plane = 65.535f;

    int width;
    int height;
    int render_width;
    int render_height;
    int encode_width;
    int encode_height;

    vsg::ref_ptr<vsg::Data> vsg_color_image;
    vsg::ref_ptr<vsg::Data> vsg_depth_image;
    vsg::ImageInfoList camera_info;
    vsg::ImageInfoList depth_info;

    //every frame's real color and depth
    unsigned char * color_pixels = nullptr;
    unsigned short * depth_pixels = nullptr;
    mergeShaderType shader_type;

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;//多重采样的倍数

    vsg::ref_ptr<vsg::WindowTraits> createWindowTraits(string windowTitle, int num,  vsg::ref_ptr<vsg::Options> options)
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

    void setUpShader(std::string project_path){
        //-----------------------------------------设置shader------------------------------------//
        ConfigShader config_shader;
        shadow_shader = config_shader.buildShadowShader(project_path + "asset/data/shaders/shadow.vert", project_path + "asset/data/shaders/shadow.frag");
        line_shader = config_shader.buildLineShader(project_path + "asset/data/shaders/line.vert", project_path + "asset/data/shaders/line.frag");
        point_shader = config_shader.buildLineShader(project_path + "asset/data/shaders/point.vert", project_path + "asset/data/shaders/point.frag");
    }

    void preprocessEnvMap(){
        std::string envmapFilepath = project_path + "asset/data/textures/" + std::to_string(hdr_image_num) + ".hdr";
        IBL::generateEnvmap(vsgContext, envmapFilepath, -1);
        IBL::generateIrradianceCube(vsgContext, -1);
        IBL::generatePrefilteredEnvmapCube(vsgContext, -1);

        viewer_IBL->compile();
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
        for(int i = hdr_image_max_num; i > 0; i--){
            std::string envmapFilepath = project_path + "asset/data/textures/" + std::to_string(i) + ".hdr";
            IBL::generateEnvmap(vsgContext, envmapFilepath, i);
            IBL::generateIrradianceCube(vsgContext, i);
            IBL::generatePrefilteredEnvmapCube(vsgContext, i);

            viewer_IBL->compile();
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

        IBL::drawSkyboxVSGNode(vsgContext, drawSkyboxNode, render_width, render_height);
        IBL::drawSkyboxVSGNode(vsgContext, drawCameraImageNode, render_width, render_height, camera_info);
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

        IBL::drawSkyboxVSGNode(vsgContext, drawSkyboxNode, render_width, render_height);
        IBL::drawSkyboxVSGNode(vsgContext, drawCameraImageNode, render_width, render_height, camera_info);
    }

    void update_directional_lights(){
        curLightGroup->children.clear();
        curLightGroup->addChild(lightGroups[hdr_image_num]);
    }

    void init_directional_lights(){
        std::string json_path = project_path + "asset/LightInfo.json";
        std::ifstream json_file(json_path);
        
        if (!json_file.is_open()) {
            std::cerr << "错误：无法打开光源配置文件 " << json_path << std::endl;
            return;
        }

        json json_data = json::parse(json_file);
        json_file.close();

        for (auto& [hdr_idx_str, hdr_data] : json_data.items()) {
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

    vsg::ImageInfoList createImageInfo(vsg::ref_ptr<vsg::Data> in_data){
        auto sampler = vsg::Sampler::create();
        sampler->magFilter = VK_FILTER_NEAREST;
        sampler->minFilter = VK_FILTER_NEAREST;

        vsg::ref_ptr<vsg::ImageInfo> imageInfosIBL = vsg::ImageInfo::create(sampler, in_data);
        vsg::ImageInfoList imageInfosListIBL = {imageInfosIBL};
        return imageInfosListIBL;
    }
    vsg::ref_ptr<vsg::Options> options = vsg::Options::create();

    void initRenderer(std::string engine_path, std::vector<vsg::dmat4>& model_transforms, std::vector<std::string>& model_paths, std::vector<std::string>& instance_names, vsg::dmat4 plane_transform);
    
    void setRealColorAndImage(unsigned char * real_color, unsigned short * real_depth){
        color_pixels = real_color;
        depth_pixels = real_depth;
    }

    void updateCamera(vsg::dvec3 centre, vsg::dvec3 eye, vsg::dvec3 up){
        auto lookat = vsg::LookAt::create(eye, centre, up);
        auto cameralookat = camera->viewMatrix.cast<vsg::LookAt>();
        cameralookat->set(lookat->transform());
    }

    void updateCamera(vsg::dmat4 view_matrix){
        auto lookat = camera->viewMatrix.cast<vsg::LookAt>();
        lookat->set(view_matrix);
    }
    
    void updateObjectPose(std::string instance_name, vsg::dmat4 model_matrix){
        auto& matrix_index = CADMesh::id_to_matrix_index_map[instance_name];

        for(int i = 0; i < matrix_index.size(); i ++){
            auto proto = matrix_index[i].proto_data;
            auto index = matrix_index[i].index;
            proto->instance_buffer->set(index, vsg::mat4(model_matrix));
            proto->instance_buffer->dirty();
        }
        view->viewDependentState->draw_shadow = true;
    }

    void updateEnvLighting(){
        updateEnvMap();
        update_directional_lights();
        IBL::textures.params->dirty();
        viewer->compile(); //编译命令图。接受一个可选的`ResourceHints`对象作为参数，用于提供编译时的一些提示和配置。通过调用这个函数，可以将命令图编译为可执行的命令。
        view->viewDependentState->draw_shadow = true;
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
            proto->highlight_buffer->set(index / 2 * 4, state);
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