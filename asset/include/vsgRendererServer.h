#ifndef VSGRENDERERSERVER_H
#define VSGRENDERERSERVER_H
#pragma  once
#include <iostream>
#include <screenshot.h>
#include <vsg/all.h>
#include "convertPng.h"
#include "ConfigShader.h"
#include "ModelInstance.h"
#include "ImGui.h"
#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include "assimp.h"
#include "HDRLightSampler.h"

#include "PlaneLoader.h"

#include "fixDepth.h"

using namespace std;

class vsgRendererServer
{
    public:
    vsg::ref_ptr<vsg::Device> device;
    vsg::ref_ptr<vsg::ImageInfo> imageInfosIBL[4];
    vsg::ref_ptr<vsg::Viewer> viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> viewer_IBL = vsg::Viewer::create();

    std::unordered_map<std::string, CADMesh*> transfered_meshes; //path, mesh*
    std::unordered_map<std::string, ModelInstance*> instance_phongs; //path, mesh*

    vsg::ref_ptr<vsg::ShaderSet> shadow_shader;
    vsg::ref_ptr<vsg::ShaderSet> model_shader;
    vsg::ref_ptr<vsg::ShaderSet> line_shader;
    vsg::ref_ptr<vsg::ShaderSet> point_shader;

    vsg::ref_ptr<ScreenshotHandler> final_screenshotHandler;
    vsg::ref_ptr<ScreenshotHandler> screenshotHandler;

    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::Window> env_window;
    vsg::ref_ptr<vsg::Window> shadow_window;
    vsg::ref_ptr<vsg::Window> final_window;
    vsg::ref_ptr<vsg::Camera> camera;

    //IBL
    IBL::VsgContext vsgContext = {};
    vsg::ref_ptr<vsg::StateGroup> drawSkyboxNode = vsg::StateGroup::create();
    vsg::ref_ptr<vsg::StateGroup> drawCameraImageNode = vsg::StateGroup::create();
    vsg::ref_ptr<vsg::StateGroup> drawIBLSceneNode = vsg::StateGroup::create();
    vsg::ref_ptr<vsg::StateGroup> drawIBLBackgroundNode = vsg::StateGroup::create();
    vsg::ref_ptr<vsg::StateGroup> drawShadowBackgroundNode = vsg::StateGroup::create();
    vsg::ref_ptr<vsg::Group> lightGroup = vsg::Group::create();
    int hdr_image_num = 1;

    vsg::ref_ptr<vsg::DirectionalLight> directionalLight[4];
    vsg::ref_ptr<vsg::Switch> directionalLightSwitch = vsg::Switch::create();

    std::string project_path;
    std::string shadow_recevier_path;
    vsg::dmat4 shadow_recevier_transform;
    std::string texture_path = "asset/data/obj/helicopter-engine";
    // std::string texture_path = "asset/data/obj/Medieval_building";

    struct CameraPlaneInfo{
        vsg::vec4 n[6];
    };
    CameraPlaneInfo camera_plane_info;


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
    vsg::ref_ptr<vsg::Data> vsg_color_image;
    vsg::ref_ptr<vsg::Data> vsg_depth_image;
    vsg::ImageInfoList camera_info;
    vsg::ImageInfoList depth_info;
    vsg::ref_ptr<vsg::mat4Array> camera_matrix = vsg::mat4Array::create(2);
    vsg::ref_ptr<vsg::BufferInfo> camera_matrix_buffer_info;

    //every frame's real color and depth
    unsigned char * color_pixels = nullptr;
    unsigned short * depth_pixels = nullptr;
    mergeShaderType shader_type;

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_4_BIT;//多重采样的倍数

    vsg::ref_ptr<vsg::ShaderSet> buildMergeShaderSet(vsg::ref_ptr<vsg::Options> options) {
        auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/IBL/fullscreenquad.vert", options);
        vsg::ref_ptr<vsg::ShaderStage> fragShader;
        if(shader_type == FULL_MODEL){
            fragShader = vsg::read_cast<vsg::ShaderStage>("shaders/merge_full_model.frag", options);
        }else if(shader_type == CAMERA_DEPTH){
            fragShader = vsg::read_cast<vsg::ShaderStage>("shaders/new_merge.frag", options);
        }else if(shader_type == CAD_DAPTH){
            fragShader = vsg::read_cast<vsg::ShaderStage>("shaders/new_merge_background.frag", options);
        }
        auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragShader});

        const uint32_t TEXTURE_DESCRIPTOR_SET = 0;
        const uint32_t MATERIAL_DESCRIPTOR_SET = 1;
        shaderSet->addDescriptorBinding("cadColor", "", TEXTURE_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
        shaderSet->addDescriptorBinding("cadDepth", "", TEXTURE_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_D32_SFLOAT}));
        shaderSet->addDescriptorBinding("planeColor", "", TEXTURE_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8_UNORM}));
        shaderSet->addDescriptorBinding("planeDepth", "", TEXTURE_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R16_UNORM}));
        shaderSet->addDescriptorBinding("shadowColor", "", TEXTURE_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
        shaderSet->addDescriptorBinding("shadowDepth", "", TEXTURE_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_D32_SFLOAT}));
        //shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(TEXTURE_DESCRIPTOR_SET));
        return shaderSet;
    };

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
        windowTraits->depthImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
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
    void setWidthAndHeight(int width, int height, double scale){
        this->render_width = width * scale;
        this->render_height = height * scale;
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
        model_shader = config_shader.buildModelShader(project_path + "asset/data/shaders/model.vert", project_path + "asset/data/shaders/model.frag");
        line_shader = config_shader.buildLineShader(project_path + "asset/data/shaders/line.vert", project_path + "asset/data/shaders/line.frag");
        point_shader = config_shader.buildLineShader(project_path + "asset/data/shaders/point.vert", project_path + "asset/data/shaders/point.frag");
    }

    void preprocessEnvMap(){
        std::string envmapFilepath = project_path + "asset/data/textures/" + std::to_string(hdr_image_num) + ".hdr";
        IBL::generateEnvmap(vsgContext, envmapFilepath);
        IBL::generateIrradianceCube(vsgContext);
        IBL::generatePrefilteredEnvmapCube(vsgContext);

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

        IBL::drawSkyboxVSGNode(vsgContext, drawSkyboxNode, render_width, render_height);
        IBL::drawSkyboxVSGNode(vsgContext, drawCameraImageNode, render_width, render_height, camera_info);
    }

    void update_directional_lights(){
        lightGroup->children.clear();
        // HDR环境光采样
        HDRLightSampler lightSampler;
        lightSampler.loadHDRImage(project_path + "asset/data/textures/" + std::to_string(hdr_image_num) + ".hdr");
        lightSampler.computeLuminanceMap();
        lightSampler.computeCDF();
        auto sampledLights = lightSampler.sampleLights(1);


        //-----------------------------------设置光源----------------------------------//
        //vsg::ref_ptr<vsg::DirectionalLight> directionalLight; //定向光源 ref_ptr智能指针
        for (const auto& light : sampledLights)
        {   
            vsg::vec3 lightColor = vsg::vec3(light.color[0], light.color[1], light.color[2]);
            vsg::vec3 lightPosition = vsg::vec3(light.position[0], light.position[1], light.position[2]);

            auto pointLight = vsg::DirectionalLight::create();
            pointLight->color = lightColor;
            pointLight->intensity = light.intensity;
            pointLight->direction = vsg::normalize(-lightPosition);
            pointLight->shadowMaps = 1;
            
            auto lightTransform = vsg::MatrixTransform::create();
            lightTransform->matrix = vsg::translate(lightPosition);
            lightTransform->addChild(pointLight);
            
            lightGroup->addChild(lightTransform);
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
    
    vsg::ref_ptr<vsg::ClearDepthStencilImage> clearDepth = vsg::ClearDepthStencilImage::create();
    vsg::ref_ptr<vsg::ClearDepthStencilImage> clearDepth1 = vsg::ClearDepthStencilImage::create();
    vsg::ref_ptr<vsg::PipelineBarrier> preClearBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,      // 源阶段（无前置操作）
        VK_PIPELINE_STAGE_TRANSFER_BIT,         // 目标阶段（传输操作）
        0                                       // 依赖标志
    );

    vsg::ref_ptr<vsg::ImageMemoryBarrier> layoutTransition = vsg::ImageMemoryBarrier::create(
        0,                                      // 源访问掩码（无依赖）
        VK_ACCESS_TRANSFER_WRITE_BIT,           // 目标访问掩码（传输写入）
        VK_IMAGE_LAYOUT_UNDEFINED,              // 旧布局（假设初始状态为 UNDEFINED）
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // 新布局
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED
    );
    vsg::ref_ptr<vsg::Image> depthPyramidImage = vsg::Image::create();

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
    }

    void updateEnvLighting(){
        preprocessEnvMap();
        update_directional_lights();
        IBL::textures.params->dirty();
        viewer->compile(); //编译命令图。接受一个可选的`ResourceHints`对象作为参数，用于提供编译时的一些提示和配置。通过调用这个函数，可以将命令图编译为可执行的命令。
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
            proto->highlight_buffer->set(index / 2, state);
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