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
#include "CADMeshIBL.h"

#include "PlaneLoader.h"

#include "fixDepth.h"

using namespace std;

class vsgRendererServer
{
    public:
    vsg::ref_ptr<vsg::Device> device;
    vsg::ref_ptr<vsg::Image> storageImagesIBL[4];
    vsg::ref_ptr<vsg::CopyImage> copyImagesIBL[4];
    vsg::ref_ptr<vsg::ImageInfo> imageInfosIBL[4];
    vsg::ref_ptr<vsg::Viewer> viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> viewer_IBL = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> final_viewer = vsg::Viewer::create();

    std::unordered_map<std::string, CADMesh*> transfered_meshes; //path, mesh*
    std::unordered_map<std::string, ModelInstance*> instance_phongs; //path, mesh*

    vsg::ref_ptr<vsg::ShaderSet> shadow_shader;
    vsg::ref_ptr<vsg::ShaderSet> model_shader;

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
    ConvertImage *convert_image;
    vsg::ref_ptr<vsg::Data> vsg_color_image;
    vsg::ref_ptr<vsg::Data> vsg_depth_image;
    vsg::ImageInfoList camera_info;
    vsg::ImageInfoList depth_info;
    vsg::ref_ptr<vsg::mat4Array> camera_matrix = vsg::mat4Array::create(1);
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

    void initRenderer(std::string engine_path, std::vector<vsg::dmat4>& model_transforms, std::vector<std::string>& model_paths, std::vector<std::string>& instance_names, vsg::dmat4 plane_transform)
    {
        // project_path = engine_path.append("Rendering/");
        project_path = engine_path;
        auto options = vsg::Options::create();
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
        
        convert_image = new ConvertImage(width, height);
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

        auto envWindowTraits = createWindowTraits("Background", 1, options);
        envWindowTraits->device = device;

        auto shadowWindowTraits = createWindowTraits("shadowShader", 2, options);
        shadowWindowTraits->device = device;

        auto intgWindowTraits = createWindowTraits("Integration", 3, options);
        intgWindowTraits->device = device;
        window = vsg::Window::create(cadWindowTraits);

        double nearFarRatio = 0.0001;       //近平面和远平面之间的比例

        //---------------------------------------------------场景创建--------------------------------------//
        auto modelGroup = vsg::Group::create();
        auto modelShadowGroup = vsg::Group::create();
        auto shadowGroup = vsg::Group::create();
        auto envSceneGroup = vsg::Group::create();
        auto wireframeGroup = vsg::Group::create();

        auto rootSwitch = vsg::Switch::create();
        rootSwitch->addChild(MASK_CAMERA_IMAGE, drawCameraImageNode);
        rootSwitch->addChild(MASK_SKYBOX, drawSkyboxNode);
        rootSwitch->addChild(MASK_PBR_FULL, modelGroup);
        rootSwitch->addChild(MASK_SHADOW_RECEIVER, shadowGroup);
        // rootSwitch->addChild(MASK_SKYBOX, envSceneGroup);
        // rootSwitch->addChild(MASK_WIREFRAME, wireframeGroup);
        
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
            float far;
            int shader_type;
        };
        
        auto params = vsg::ubyteArray::create(sizeof(MyParams));
        auto* params_ptr = reinterpret_cast<MyParams*>(params->dataPointer());
        params_ptr->semi_transparent = 1;
        params_ptr->width = render_width;
        params_ptr->height = render_height;
        params_ptr->far = 65.535;
        params_ptr->shader_type = shader_type;

        PlaneData planeData = createTestPlanes();
        float subdivisions = 0.1;
        PlaneData subdividedPlaneData = subdividePlanes(planeData, subdivisions);

        MeshData mesh = convertPlaneDataToMesh(subdividedPlaneData);

        auto pbr_shaderset = vsg::createPhysicsBasedRenderingShaderSet(options);
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
            // string texture = engine_path + "asset/data/obj/Medieval_building";
            string texture = engine_path + "asset/data/obj/helicopter-engine";
            shadow_recevier_mesh->preprocessProtoData(shadow_recevier_path.c_str(), texture.c_str(), shadow_recevier_transform, shadow_shader, shadowGroup, "shadow_receiver");
            // ModelInstance* shadow_recevier_instance = new ModelInstance();
            // shadow_recevier_instance->buildObjInstanceShadow(shadow_recevier_mesh, shadowGroup, shadow_shader, shadow_recevier_transform, camera_info, depth_info, params);
            // scenegraph->addChild(shadowStateGroup);
            // ModelInstance* ibl_shadow_recevier_instance = new ModelInstance();
            // auto iblStateGroup = vsg::StateGroup::create();
            // ibl_shadow_recevier_instance->buildObjInstanceShadow(shadow_recevier_mesh, iblStateGroup, gpc_ibl, gpc_shadow, shadow_recevier_transform, false);
            // rootSwitch->addChild(MASK_FAKE_BACKGROUND, iblStateGroup);
        }
        bool fullNormal = true;
        CADMesh::CreateDefaultMaterials();
        //---------------------------------------读取CAD模型------------------------------------------//
        for(int i = 0; i < model_paths.size(); i ++){
            std::string &path_i = model_paths[i];
            size_t pos = path_i.find_last_of('.');
            std::string format = path_i.substr(pos + 1);
            std::string texture_path_i = engine_path + texture_path;
            CADMesh* transfer_model;
            if (transfered_meshes.find(path_i) != transfered_meshes.end()){
                transfer_model = transfered_meshes[path_i];
            }else{
                transfer_model = new CADMesh();
                transfered_meshes[path_i] = transfer_model;
            }
            if(format == "obj")
            {
                transfer_model->preprocessProtoData(path_i.c_str(), texture_path_i.c_str(), model_transforms[i], pbriblShaderSet, modelGroup, instance_names[i]); //读取obj文件
            }
            else if(format == "fb")
            {
                //transfer_model->transferModel(model_paths[i], fullNormal, model_transforms[i]);
                transfer_model->preprocessFBProtoData(path_i, texture_path_i.c_str(), model_transforms[i], pbriblShaderSet, modelGroup, instance_names[i]);
            }



            // if(format == "obj"){
            //     ModelInstance* instance_phong = new ModelInstance();
            //     instance_phong->context = vsg::Context::create(window->getOrCreateDevice());
            //     // instance_phong->buildObjInstanceIBL(transfer_model, modelGroup, pbriblShaderSet, model_transforms[i], camera_info, depth_info, params1);
            //     //instance_phong->buildObjInstance(transfer_model, scenegraph, phongShader, model_transforms[i]);
            //     instance_phongs[instance_names[i]] = instance_phong;
            //     std::cout << "**************** reading obj ******************" << std::endl;
            // }
            // else if(format == "fb"){
            //     ModelInstance* instance_phong = new ModelInstance();
            //     instance_phong->context = vsg::Context::create(window->getOrCreateDevice());
            //     //instance_phong->buildInstance(transfer_model, scenegraph, phongShader, model_transforms[i]);
            //     instance_phong->buildFbInstance(transfer_model, scenegraph, pbriblShaderSet, gpc_ibl, model_transforms[i], options, engine_path);
            //     //instance_phong->buildInstanceIBL(transfer_model, scenegraph, gpc_ibl, gpc_shadow, model_transforms[i]);
            //     instance_phongs[instance_names[i]] = instance_phong;
            //     std::cout << "**************** reading fb ******************" << std::endl;
            // }
            // else if(format == "texture")
            // {
            //     ModelInstance* instance_phong = new ModelInstance();
            //     instance_phong->buildTextureSphere(scenegraph, gpc_ibl, gpc_ibl, model_transforms[i]);
            // }
        }
        CADMesh::buildDrawData(pbriblShaderSet, modelGroup); //读取obj文件

        std::cout << "model processing done" << std::endl;

        // HDR环境光采样

        update_directional_lights();
        scenegraph_safe->addChild(lightGroup);
        
        //----------------------------------------------------------------窗口1----------------------------------------------------------//
        viewer->addWindow(window);
        auto view = vsg::View::create(camera, scenegraph_safe);
        // view->features = vsg::RECORD_LIGHTS;
        view->mask = MASK_CAMERA_IMAGE | MASK_PBR_FULL;
        view->viewDependentState = CustomViewDependentState::create(view.get());
        auto renderGraph = vsg::RenderGraph::create(window, view);
        renderGraph->clearValues[0].color = {{-1.f, -1.f, -1.f, 1.f}};
        // auto renderImGui = vsgImGui::RenderImGui::create(window, gui::MyGui::create(options));
        // renderGraph->addChild(renderImGui);
        auto commandGraph = vsg::CommandGraph::create(window);
        auto commandGraph1 = vsg::CommandGraph::create(window);

        auto view1 = vsg::View::create(camera, scenegraph_safe);
        // view->features = vsg::RECORD_LIGHTS;
        view1->mask = MASK_PBR_FULL | MASK_SHADOW_RECEIVER;
        view1->viewDependentState = CustomViewDependentState::create(view1.get());
        auto renderGraph1 = vsg::RenderGraph::create(window, view1);
        // renderGraph1->getRenderPass()->attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        renderGraph1->clearValues[0].color = {{0.f, 0.f, 0.f, 0.f}};
        auto renderImGui = vsgImGui::RenderImGui::create(window, gui::MyGui::create(options));
        renderGraph1->addChild(renderImGui);

        // create the compute graph to compute the positions of the vertices
        // to use a different queue family, we need to use VK_SHARING_MODE_CONCURRENT with queueFamilyIndices for VkBuffer, or implement a queue family ownership transfer with a BufferMemoryBarrier
        //auto computeQueueFamily = physicalDevice->getQueueFamily(VK_QUEUE_COMPUTE_BIT);
        auto computeQueueFamily = commandGraph->queueFamily;
        auto computeCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily);
        auto computeCommandGraph1 = vsg::CommandGraph::create(device, computeQueueFamily);
        {
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
            camera_matrix_buffer_info = vsg::BufferInfo::create(camera_matrix);
            camera_matrix->properties.dataVariance = vsg::DYNAMIC_DATA;
            vsg::DescriptorSetLayoutBindings descriptorBindings{
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
                {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
                {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
                {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
                {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
                {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
                {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            };
            auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
            auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
            {
                auto computeShader = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex.comp", options);
                auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
                auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
                computeCommandGraph->addChild(bindPipeline);

                for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
                    ProtoData* proto_data = proto_data_itr.second;
                    auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                        proto_data->input_instance_buffer_info, proto_data->output_instance_buffer_info,
                                                                                        camera_plane_info_buffer_info, proto_data->bounds_buffer_info,
                                                                                        camera_matrix_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer});
                    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
                    computeCommandGraph->addChild(bindDescriptorSet);
                    computeCommandGraph->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 2 / 32 + 1, 1, 1));
                }
            }
            {
                auto computeShader = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex1.comp", options);
                auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
                auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
                computeCommandGraph1->addChild(bindPipeline);

                for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
                    ProtoData* proto_data = proto_data_itr.second;
                    auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                        proto_data->input_instance_buffer_info, proto_data->output_instance_buffer_info,
                                                                                        camera_plane_info_buffer_info, proto_data->bounds_buffer_info,
                                                                                        camera_matrix_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer});
                    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
                    computeCommandGraph1->addChild(bindDescriptorSet);
                    computeCommandGraph1->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 2 / 32 + 1, 1, 1));
                }
            }
        }


        viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
        viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});
        viewer->addEventHandler(vsg::Trackball::create(camera));
        //----------------------------------------------------------------窗口2----------------------------------------------------------//
        //   // create the viewer and assign window(s) to it
        // envWindowTraits->device = window->getOrCreateDevice(); 
        // env_window = vsg::Window::create(envWindowTraits);
        // viewer->addWindow(env_window);
        // auto env_view = vsg::View::create(camera, scenegraph);              //共用一个camera，改变一个window的视角，另一个window视角也会改变
        // env_view->mask = MASK_FAKE_BACKGROUND | MASK_PBR_FULL;
        // env_view->features = vsg::RECORD_LIGHTS;
        // auto env_renderGraph = vsg::RenderGraph::create(env_window, env_view); //如果用Env_window会报错
        // env_renderGraph->clearValues[0].color = {{0.f, 0.f, 0.f, 1.f}};

        // auto env_commandGraph = vsg::CommandGraph::create(env_window); //如果用Env_window会报错
        // env_commandGraph->addChild(env_renderGraph);

        // std::cout << "Env Window" << std::endl;

        // //----------------------------------------------------------------窗口3----------------------------------------------------------//
        // //shadowWindowTraits->device = window->getOrCreateDevice(); //共享设备 不加这句Env_window->getOrCreateDevice()就报错 一个bug de几天
        // shadow_window = vsg::Window::create(shadowWindowTraits);
        // viewer->addWindow(shadow_window);
        // auto Shadow_view = vsg::View::create(camera, scenegraph);                 //共用一个camera，改变一个window的视角，另一个window视角也会改变
        
        // Shadow_view->mask = MASK_SHADOW_CASTER | MASK_SHADOW_RECEIVER;
        // Shadow_view->features = vsg::RECORD_SHADOW_MAPS;
        // Shadow_view->viewDependentState = CustomViewDependentState::create(Shadow_view.get());
        // env_view->viewDependentState = Shadow_view->viewDependentState;

        // auto Shadow_renderGraph = vsg::RenderGraph::create(shadow_window, Shadow_view); //如果用Env_window会报错
        // Shadow_renderGraph->clearValues[0].color = {{1.f, 1.f, 1.f, 1.f}};
        // auto Shadow_commandGraph = vsg::CommandGraph::create(shadow_window); //如果用Env_window会报错
        // Shadow_commandGraph->addChild(Shadow_renderGraph);
        auto pipelineBarrier_compute_to_render = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            0
        );

        // 遍历所有实例化数据 Buffer
        for(auto& proto_data_itr : CADMesh::proto_id_to_data_map)
        {
            ProtoData* proto_data = proto_data_itr.second;
            auto barrier = vsg::BufferMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,      // Compute Shader 写入
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT, // 顶点输入读取
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                proto_data->draw_indirect->bufferInfo->buffer,                  // 当前 Buffer
                0,                               // 偏移量
                VK_WHOLE_SIZE                    // 整个 Buffer
            );
            pipelineBarrier_compute_to_render->add(barrier);
        }


        auto pipelineBarrier_render0_to_compute1 = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // 源阶段
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,          // 目标阶段
            0                                              // 依赖标志
        );

        colorImageBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // Render0 写入
            VK_ACCESS_SHADER_READ_BIT, // Compute1 读取
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // 旧布局
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // 新布局
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED
        );

        commandGraph->addChild(computeCommandGraph);
        commandGraph->addChild(pipelineBarrier_compute_to_render);
        commandGraph->addChild(renderGraph);
        commandGraph1->addChild(pipelineBarrier_render0_to_compute1);
        commandGraph1->addChild(computeCommandGraph1);
        commandGraph1->addChild(pipelineBarrier_compute_to_render);
        commandGraph1->addChild(renderGraph1);

        std::cout << "Shadow Window" << std::endl;
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph, commandGraph1});
        // viewer->setupThreading();
        viewer->compile(); //编译命令图。接受一个可选的`ResourceHints`对象作为参数，用于提供编译时的一些提示和配置。通过调用这个函数，可以将命令图编译为可执行的命令。
        // std::cout << "4" << std::endl;

        
        

        VkExtent2D extent = {};
        extent.width = render_width;
        extent.height = render_height;
        final_screenshotHandler = ScreenshotHandler::create(window, extent, ENCODER);
        screenshotHandler = ScreenshotHandler::create();        
        allocate_fix_depth_memory(render_width, render_height);
    }
    vsg::ref_ptr<vsg::ImageMemoryBarrier> colorImageBarrier;

    void setRealColorAndImage(const std::string& real_color, const std::string& real_depth){
        if (color_pixels) {
            stbi_image_free(color_pixels);
            color_pixels = nullptr;
        }
        if (depth_pixels) {
            stbi_image_free(depth_pixels);
            depth_pixels = nullptr;
        }
        color_pixels = convert_image->convertColor(real_color);
        depth_pixels = convert_image->convertDepth(real_depth);
    }

    void setRealColorAndImage(unsigned char * real_color, unsigned short * real_depth){
        if (color_pixels) {
            stbi_image_free(color_pixels);
            color_pixels = nullptr;
        }
        if (depth_pixels) {
            stbi_image_free(depth_pixels);
            depth_pixels = nullptr;
        }
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

    bool render() {
        camera_matrix->set(0, (vsg::mat4)camera->viewMatrix->transform());
        camera_matrix->dirty();
        auto t0 = std::chrono::high_resolution_clock::now();
        while (viewer->advanceToNextFrame()) {
            colorImageBarrier->image = window->imageView(window->imageIndex())->image;
            
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

            auto t4 = std::chrono::high_resolution_clock::now();
            viewer->handleEvents();

            auto t5 = std::chrono::high_resolution_clock::now();
            viewer->update();

            auto t6 = std::chrono::high_resolution_clock::now();
            viewer->recordAndSubmit();

            auto t7 = std::chrono::high_resolution_clock::now();
            viewer->present();

            auto t8 = std::chrono::high_resolution_clock::now();

            gui::global_params->render_func_times[0] = std::chrono::duration<double, std::milli>(t1 - t0).count();
            gui::global_params->render_func_times[1] = std::chrono::duration<double, std::milli>(t2 - t1).count();
            gui::global_params->render_func_times[2] = std::chrono::duration<double, std::milli>(t3 - t2).count();
            gui::global_params->render_func_times[3] = std::chrono::duration<double, std::milli>(t4 - t3).count();
            gui::global_params->render_func_times[4] = std::chrono::duration<double, std::milli>(t5 - t4).count();
            gui::global_params->render_func_times[5] = std::chrono::duration<double, std::milli>(t6 - t5).count();
            gui::global_params->render_func_times[6] = std::chrono::duration<double, std::milli>(t7 - t6).count();
            gui::global_params->render_func_times[7] = std::chrono::duration<double, std::milli>(t8 - t7).count();

            return true;
        }
        return false;
    }

    void repaint(std::string model_name, std::string part_name, int state){
        instance_phongs[model_name]->repaint(part_name, state);
    }

    void getWindowImage(uint8_t* color){
        final_screenshotHandler->screenshot_cpuimage(window, color);
    }

    void getEncodeImage(std::vector<std::vector<uint8_t>>& vPacket){
        final_screenshotHandler->encodeImage(window, vPacket);
    }
};

#endif //VSGR_RENDERER_SERVER_H