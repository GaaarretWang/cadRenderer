#pragma  once
#include <iostream>
#include <screenshot.h>
#include <vsg/all.h>
#include "convertPng.h"
#include "ConfigShader.h"
#include "ModelInstance.h"

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include "assimp.h"
#include "IBL.h"
#include "CADMeshIBL.h"
#include "HDRLightSampler.h"

#include "screenshotIBL.h"

using namespace std;

class vsgRendererServer
{
    public:
    vsg::ref_ptr<vsg::Device> device;
    vsg::ref_ptr<vsg::Image> storage_images[6];
    vsg::ref_ptr<vsg::CopyImage> copy_images[6];
    vsg::ref_ptr<vsg::ImageInfo> imageInfos[6];
    vsg::ref_ptr<vsg::Image> storageImagesIBL[6];
    vsg::ref_ptr<vsg::CopyImage> copyImagesIBL[6];
    vsg::ref_ptr<vsg::ImageInfo> imageInfosIBL[6];
    vsg::ref_ptr<vsg::Viewer> viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> viewer_IBL = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> env_viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> shadow_viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> final_viewer = vsg::Viewer::create();

    std::unordered_map<std::string, CADMesh*> transfered_meshes; //path, mesh*
    std::unordered_map<std::string, ModelInstance*> instance_phongs; //path, mesh*
    std::unordered_map<std::string, ModelInstance*> instance_shadows; //path, mesh*

    vsg::ref_ptr<vsg::ShaderSet> pbr_shader;
    vsg::ref_ptr<vsg::ShaderSet> phong_shader;
    vsg::ref_ptr<vsg::ShaderSet> plane_shader;
    vsg::ref_ptr<vsg::ShaderSet> shadow_shader;
    vsg::ref_ptr<vsg::ShaderSet> merge_shader;


    vsg::ref_ptr<ScreenshotHandler> final_screenshotHandler;
    vsg::ref_ptr<IBLScreenshot::ScreenshotHandler> shadow_screenshotHandler;
    vsg::ref_ptr<IBLScreenshot::ScreenshotHandler> env_screenshotHandler;
    vsg::ref_ptr<IBLScreenshot::ScreenshotHandler> screenshotHandler;

    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::Window> env_window;
    vsg::ref_ptr<vsg::Window> shadow_window;
    vsg::ref_ptr<vsg::Window> final_window;
    vsg::ref_ptr<vsg::Camera> camera;

    vsg::ref_ptr<vsg::DirectionalLight> directionalLight[4];
    vsg::ref_ptr<vsg::Switch> directionalLightSwitch = vsg::Switch::create();

    std::string project_path;

    float fx = 386.52199190267083;//焦距(x轴上)
    float fy = 387.32300428823663;//焦距(y轴上)
    float cx = 326.5103569741365;//图像中心点(x轴)
    float cy = 237.40293732598795;//图像中心点(y轴)
    float near = 0.1f;
    float far = 65.535f;
    int width;
    int height;
    int render_width;
    int render_height;
    ConvertImage *convert_image;
    vsg::ref_ptr<vsg::Data> vsg_color_image;
    vsg::ref_ptr<vsg::Data> vsg_depth_image;

    //every frame's real color and depth
    unsigned char * color_pixels;
    unsigned short * depth_pixels;

    vsg::ref_ptr<vsg::WindowTraits> createWindowTraits(string windowTitle, int num,  vsg::ref_ptr<vsg::Options> options)
    {
        auto windowTraits = vsg::WindowTraits::create();
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
        if (windowTraits->samples != VK_SAMPLE_COUNT_1_BIT) windowTraits->vulkanVersion = VK_API_VERSION_1_2;
        windowTraits->deviceExtensionNames = {
            VK_KHR_MULTIVIEW_EXTENSION_NAME,
            VK_KHR_MAINTENANCE2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
            VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, 
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME};
        return windowTraits;
    }

    //format = 0是color， 1是depth
    vsg::ref_ptr<vsg::Image> createStorageImage(int format)
    {
        vsg::ref_ptr<vsg::Image> storageImage = vsg::Image::create();
        storageImage->imageType = VK_IMAGE_TYPE_2D;
        if (format == 0) {
            storageImage->format = VK_FORMAT_B8G8R8A8_UNORM;                                                                                                       //VK_FORMAT_R8G8B8A8_UNORM;
            storageImage->usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // sample和storage一定要标
        }
        else if(format == 1)
        {
            storageImage->format = VK_FORMAT_D16_UNORM;                                                                                       //VK_FORMAT_R8G8B8A8_UNORM;
            storageImage->usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; // sample和storage一定要标
        }
        storageImage->extent.width = window->extent2D().width;
        storageImage->extent.height = window->extent2D().height;
        storageImage->extent.depth = 1;
        storageImage->mipLevels = 1;
        storageImage->arrayLayers = 1;
        storageImage->samples = VK_SAMPLE_COUNT_1_BIT;
        storageImage->tiling = VK_IMAGE_TILING_OPTIMAL;
        storageImage->initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        storageImage->flags = 0;
        storageImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        return storageImage;
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

    void setUpShader(std::string project_path, bool objtracking_shader){
        //-----------------------------------------设置shader------------------------------------//
        ConfigShader config_shader;
        pbr_shader = config_shader.buildShader(project_path + "asset/data/shaders/standard.vert", project_path + "asset/data/shaders/standard_pbr.frag");
        phong_shader = config_shader.buildShader(project_path + "asset/data/shaders/standard.vert", project_path + "asset/data/shaders/standard_phong.frag");
        plane_shader = config_shader.buildShader(project_path + "asset/data/shaders/plane.vert", project_path + "asset/data/shaders/plane.frag");
        shadow_shader = config_shader.buildShader(project_path + "asset/data/shaders/IBL/shadow.vert", project_path + "asset/data/shaders/IBL/shadow.frag");
        if(objtracking_shader)
            merge_shader = config_shader.buildIntgShader(project_path + "asset/data/shaders/merge.vert", project_path + "asset/data/shaders/merge_objtracking.frag");
        else
            merge_shader = config_shader.buildIntgShader(project_path + "asset/data/shaders/merge.vert", project_path + "asset/data/shaders/merge.frag");
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

        std::cout << "SERVER:初始化Vulkan设备" << std::endl;
        
        //手动初始化vulkan设备
        vsg::Names instanceExtensions;
        vsg::Names requestedLayers;
        bool debugLayer = true;
        bool apiDumpLayer = false;
        uint32_t vulkanVersion = VK_API_VERSION_1_3;

        if (debugLayer || apiDumpLayer)
        {
            instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
            if (apiDumpLayer) requestedLayers.push_back("VK_LAYER_LUNARG_api_dump");
        }
        instanceExtensions.push_back("VK_KHR_surface");
        
        #ifdef _WIN32
            instanceExtensions.push_back("VK_KHR_win32_surface");//如果你使用windows
        #else
            instanceExtensions.push_back("VK_KHR_xcb_surface"); //如果你使用linux
        #endif

        vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

        std::cout << "mainV2:创建实例" << std::endl;
        

        vsg::ref_ptr<vsg::Instance> instance;
        try {
            instance = vsg::Instance::create(instanceExtensions, validatedNames, vulkanVersion);//问题语
        } catch (const vsg::Exception& ex) {
            std::cout << "-----Error creating Vulkan Instance: " << ex.message << "----result code: "<< ex.result <<std::endl;
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
                                                        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
                                                        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
                                                        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME
                                                        });

        vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}};

        auto deviceFeatures = vsg::DeviceFeatures::create();
        deviceFeatures->get().samplerAnisotropy = VK_TRUE;
        deviceFeatures->get().geometryShader = VK_TRUE;

        device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, deviceFeatures);

        auto context = vsg::Context::create(device);

        IBL::VsgContext vsgContext = {};
        vsgContext.viewer = viewer_IBL;
        vsgContext.context = context;
        vsgContext.device = device;
        vsgContext.queueFamily = queueFamily;

        std::cout << "IBL:创建环境光数据" << std::endl;

        IBL::appData.options = options;
        IBL::createResources(vsgContext);
        IBL::generateBRDFLUT(vsgContext);
        IBL::generateEnvmap(vsgContext);
        IBL::generateIrradianceCube(vsgContext);
        IBL::generatePrefilteredEnvmapCube(vsgContext);

        uint32_t frame = 0;

        viewer_IBL->compile();
        while (viewer_IBL->advanceToNextFrame())
        {
            viewer_IBL->handleEvents();
            viewer_IBL->update();
            viewer_IBL->recordAndSubmit();
            viewer_IBL->present();
            break;
        }
        viewer_IBL->recordAndSubmitTasks.clear();

        auto drawSkyboxNode = IBL::drawSkyboxVSGNode(vsgContext);

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

        double nearFarRatio = 0.0001;       //近平面和远平面之间的比例
        auto numShadowMapsPerLight = 10; //每个光源的阴影贴图数量
        auto numLights = 2;                //光源数量

        //是否使用线框表示
        bool wireFrame = 0;
        if (wireFrame)
        {
            auto rasterizationState = vsg::RasterizationState::create();
            rasterizationState->polygonMode = VK_POLYGON_MODE_LINE;
            phong_shader->defaultGraphicsPipelineStates.push_back(rasterizationState);
        }
        //---------------------------------------------------场景创建--------------------------------------//
        auto cadScenegraph = vsg::Group::create();
        auto envScenegraph = vsg::Group::create();
        auto shadowScenegraph = vsg::Group::create();
        auto intgScenegraph = vsg::Group::create();

        auto geometryGroup = vsg::Group::create();
        auto modelGroup = vsg::Group::create();
        auto envSceneGroup = vsg::Group::create();
        auto projectionGroup = vsg::Group::create();
        auto shadowGroup = vsg::Group::create();
        auto rootSwitch = vsg::Switch::create();
        rootSwitch->addChild(MASK_GEOMETRY, geometryGroup);
        rootSwitch->addChild(MASK_MODEL, modelGroup);
        rootSwitch->addChild(MASK_DRAW_SHADOW, shadowGroup);
        rootSwitch->addChild(MASK_FAKE_BACKGROUND, envSceneGroup);
        vsg::ref_ptr<vsg::Group> scenegraph = vsg::Group::create();
        scenegraph->addChild(rootSwitch);

        envSceneGroup->addChild(drawSkyboxNode);

        vsg::ref_ptr<vsg::PbrMaterialValue> objectMaterial;
        auto pbriblShaderSet = IBL::customPbrShaderSet(options);//
        //材质
        auto plane_mat = vsg::PbrMaterialValue::create();
        plane_mat->value().roughnessFactor = 0.5f;
        plane_mat->value().metallicFactor = 0.0f;
        plane_mat->value().baseColorFactor = vsg::vec4(1, 1, 1, 1);

        auto object_mat = vsg::PbrMaterialValue::create();
        object_mat->value().roughnessFactor = 0.1f;
        object_mat->value().metallicFactor = 0.9f;
        object_mat->value().baseColorFactor = vsg::vec4(0.701960784313725f, 0.0549019607843137f, 0.0549019607843137f, 1.f);

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

        auto dummyArrayVec2 = vsg::vec2Array::create(1);
        auto dummyArrayVec3 = vsg::vec3Array::create(1);
        auto dummyArrayVec4 = vsg::vec3Array::create(1);
        vsg::Data::Properties vec2ArrayProps = {};
        vec2ArrayProps.stride = sizeof(vsg::vec2);
        vsg::Data::Properties vec3ArrayProps = {};
        vec3ArrayProps.stride = sizeof(vsg::vec3);
        vsg::Data::Properties vec4ValueProps = {};

        auto gpc_ibl = vsg::GraphicsPipelineConfigurator::create(pbriblShaderSet);

        vsg::DataList dummyArrays;
        addVertexAttribute(gpc_ibl, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
        addVertexAttribute(gpc_ibl, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
        addVertexAttribute(gpc_ibl, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, vec2ArrayProps);
        addVertexAttribute(gpc_ibl, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, vec4ValueProps);
        //gpc_ibl->assignDescriptor("material", plane_mat);
        gpc_ibl->init();
        auto gpc_shadow = vsg::GraphicsPipelineConfigurator::create(shadow_shader);
        addVertexAttribute(gpc_shadow, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
        addVertexAttribute(gpc_shadow, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
        addVertexAttribute(gpc_shadow, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, vec2ArrayProps);
        addVertexAttribute(gpc_shadow, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, vec4ValueProps);
        gpc_shadow->assignDescriptor("material", plane_mat);
        gpc_shadow->init();

        //---------------------------------------读取CAD模型------------------------------------------//
        CADMesh cad;

        //读取.fb格式的模型参数信息
        bool fullNormal = 0;

            
        vsg::ref_ptr<vsg::Node> reconstructRoot;
        vsg::ref_ptr<vsg::Geometry> reconstructDrawCmd;
        {
            auto roomFilepath = vsg::findFile("geos/plane.ply", options->paths);
            SimpleMesh importMesh;
            if (importMeshPly(roomFilepath.string(), importMesh))
            {
                reconstructDrawCmd = vsg::Geometry::create();
                reconstructDrawCmd->assignArrays({importMesh.vertices,
                                                importMesh.normals,
                                                vsg::vec2Array::create(1),
                                                vsg::vec4Value::create(1, 1, 1, 1)});
                reconstructDrawCmd->assignIndices(importMesh.indices);
                reconstructDrawCmd->commands.push_back(vsg::DrawIndexed::create(importMesh.numIndices, 1, 0, 0, 0));

                auto stateSwtich = vsg::Switch::create();
                auto pbrStateGroup = vsg::StateGroup::create();
                auto gpc_mesh = vsg::GraphicsPipelineConfigurator::create(*gpc_ibl);
                gpc_mesh->assignDescriptor("material", plane_mat);
                gpc_mesh->init();
                gpc_mesh->copyTo(pbrStateGroup);
                pbrStateGroup->addChild(reconstructDrawCmd);
                auto shadowStateGroup = vsg::StateGroup::create();
                gpc_shadow->copyTo(shadowStateGroup);
                shadowStateGroup->addChild(reconstructDrawCmd);

                //stateSwtich->addChild(MASK_PBR_FULL | MASK_FAKE_BACKGROUND, pbrStateGroup);
                //stateSwtich->addChild(MASK_DRAW_SHADOW, shadowStateGroup);

                rootSwitch->addChild(MASK_FAKE_BACKGROUND | MASK_SHADOW_CASTER, pbrStateGroup);
                rootSwitch->addChild(MASK_DRAW_SHADOW, shadowStateGroup);
                reconstructRoot = stateSwtich;
            }
            else
            {
                std::cerr << "Error loading ply mesh" << std::endl;
            }
        }

    
        for(int i = 0; i < model_paths.size(); i ++){
            std::string &path_i = model_paths[i];
            size_t pos = path_i.find_last_of('.');
            std::string format = path_i.substr(pos + 1);
            CADMesh* transfer_model;
            if (transfered_meshes.find(path_i) != transfered_meshes.end()){
                transfer_model = transfered_meshes[path_i];
            }else{
                transfer_model = new CADMesh();
                if(format == "obj")
                {
                    transfer_model->buildObjNode(path_i.c_str(), "", model_transforms[i]); //读取obj文件
                }
                else if(format == "fb")
                {
                    //transfer_model->transferModel(model_paths[i], fullNormal, model_transforms[i]);
                    transfer_model->buildNewNode(model_paths[i], fullNormal, scenegraph);
                }
                transfered_meshes[path_i] = transfer_model;
            }
   
            if(format == "obj"){
                
                ModelInstance* instance_phong = new ModelInstance();
                instance_phong->buildObjInstanceIBL(transfer_model, scenegraph, gpc_ibl, gpc_shadow, model_transforms[i]);
                //instance_phong->buildObjInstance(transfer_model, scenegraph, phongShader, model_transforms[i]);
                instance_phongs[instance_names[i]] = instance_phong;
            }
            else if(format == "fb"){
                
                ModelInstance* instance_phong = new ModelInstance();
                //instance_phong->buildInstance(transfer_model, scenegraph, phongShader, model_transforms[i]);
                instance_phong->buildFbInstance(transfer_model, scenegraph, gpc_ibl, gpc_shadow, model_transforms[i]);
                //instance_phong->buildInstanceIBL(transfer_model, scenegraph, gpc_ibl, gpc_shadow, model_transforms[i]);
                instance_phongs[instance_names[i]] = instance_phong;
                
            }
            
        }

        // HDR环境光采样
        HDRLightSampler lightSampler;
        lightSampler.loadHDRImage(engine_path + "asset/data/textures/FINAL.hdr");
        lightSampler.computeLuminanceMap();
        lightSampler.computeCDF();
        auto sampledLights = lightSampler.sampleLights(2);


        //-----------------------------------设置光源----------------------------------//
        //vsg::ref_ptr<vsg::DirectionalLight> directionalLight; //定向光源 ref_ptr智能指针
        auto lightGroup = vsg::Group::create();
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
        scenegraph->addChild(lightGroup);

        // -----------------------设置相机参数------------------------------//
        double radius = 2000.0; // 固定观察距离
        auto viewport = vsg::ViewportState::create(0, 0, cadWindowTraits->width, cadWindowTraits->height);
        // auto perspective = vsg::Perspective::create(60.0, static_cast<double>(640) / static_cast<double>(480), nearFarRatio * radius, radius * 10.0);
        auto perspective = vsg::Perspective::create(fx, fy, cx, cy, width, height, near, far);

        vsg::dvec3 centre = {0.0, 0.0, 1.0};                    // 固定观察点
        vsg::dvec3 eye = vsg::dvec3(0.0, 0.0, 0.0); // 固定相机位置
        vsg::dvec3 up = {0.0, -1.0, 0.0};                        // 固定观察方向
        auto lookAt = vsg::LookAt::create(eye, centre, up);
        camera = vsg::Camera::create(perspective, lookAt, viewport);

        //----------------------------------------------------------------窗口1----------------------------------------------------------//
        try
        {
        // create the viewer and assign window(s) to it
            window = vsg::Window::create(cadWindowTraits);
            viewer->addWindow(window);
            auto view = vsg::View::create(camera, scenegraph);
            view->features = vsg::RECORD_LIGHTS;
            view->mask = MASK_PBR_FULL;
            view->viewDependentState = CustomViewDependentState::create(view.get());
            auto renderGraph = vsg::RenderGraph::create(window, view);
            renderGraph->clearValues[0].color = {{0.f, 0.f, 0.f, 1.f}};
            auto commandGraph = vsg::CommandGraph::create(window);
            commandGraph->addChild(renderGraph);
            viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
            viewer->compile(); //编译命令图。接受一个可选的`ResourceHints`对象作为参数，用于提供编译时的一些提示和配置。通过调用这个函数，可以将命令图编译为可执行的命令。
            viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});
            viewer->addEventHandler(vsg::Trackball::create(camera));
            auto event = vsg::Event::create_if(true, window->getOrCreateDevice());
            screenshotHandler = IBLScreenshot::ScreenshotHandler::create(event);
            viewer_IBL->addEventHandler(screenshotHandler);
        }
        catch (const vsg::Exception& ve)
        {
            vsg::debug("CompileManager::compile() exception caught : ", ve.message);
            std::cout << ve.message;
            std::cout << "done!1" << std::endl;
        }
        catch (...)
        {
            vsg::debug("CompileManager::compile() exception caught");
            std::cout << "done!2" << std::endl;
        }
        //----------------------------------------------------------------窗口2----------------------------------------------------------//
        //   // create the viewer and assign window(s) to it
        envWindowTraits->device = window->getOrCreateDevice(); 
        env_window = vsg::Window::create(envWindowTraits);
        env_viewer->addWindow(env_window);
        auto env_view = vsg::View::create(camera, scenegraph);              //共用一个camera，改变一个window的视角，另一个window视角也会改变
        env_view->mask = MASK_PBR_FULL | MASK_SHADOW_CASTER | MASK_FAKE_BACKGROUND;
        env_view->features = vsg::RECORD_LIGHTS;
        auto env_renderGraph = vsg::RenderGraph::create(env_window, env_view); //如果用Env_window会报错
        env_renderGraph->clearValues[0].color = {{0.f, 0.f, 0.f, 1.f}};
        
        auto env_commandGraph = vsg::CommandGraph::create(env_window); //如果用Env_window会报错
        env_commandGraph->addChild(env_renderGraph);
        env_viewer->assignRecordAndSubmitTaskAndPresentation({env_commandGraph});
        env_viewer->compile();
        env_viewer->addEventHandlers({vsg::CloseHandler::create(env_viewer)});
        auto env_event = vsg::Event::create_if(true, env_window->getOrCreateDevice()); // Vulkan creates VkEvent in an unsignalled 
        //env_viewer->addEventHandler(vsg::Trackball::create(camera));
        env_screenshotHandler = IBLScreenshot::ScreenshotHandler::create(env_event);
        env_viewer->addEventHandler(env_screenshotHandler);

        std::cout << "环境窗口" << std::endl;

        //----------------------------------------------------------------窗口3----------------------------------------------------------//
        //shadowWindowTraits->device = window->getOrCreateDevice(); //共享设备 不加这句Env_window->getOrCreateDevice()就报错 一个bug de几天
        shadow_window = vsg::Window::create(shadowWindowTraits);
        shadow_viewer->addWindow(shadow_window);
        auto Shadow_view = vsg::View::create(camera, scenegraph);                 //共用一个camera，改变一个window的视角，另一个window视角也会改变
        
        Shadow_view->mask = MASK_DRAW_SHADOW;
        Shadow_view->features = vsg::RECORD_SHADOW_MAPS;
        Shadow_view->viewDependentState = CustomViewDependentState::create(Shadow_view.get());
        env_view->viewDependentState = Shadow_view->viewDependentState;

        auto Shadow_renderGraph = vsg::RenderGraph::create(shadow_window, Shadow_view); //如果用Env_window会报错
        Shadow_renderGraph->clearValues[0].color = {{1.f, 1.f, 1.f, 1.f}};
        auto Shadow_commandGraph = vsg::CommandGraph::create(shadow_window); //如果用Env_window会报错
        Shadow_commandGraph->addChild(Shadow_renderGraph);
        shadow_viewer->assignRecordAndSubmitTaskAndPresentation({Shadow_commandGraph});
        shadow_viewer->compile();
        shadow_viewer->addEventHandlers({vsg::CloseHandler::create(shadow_viewer)});
        //shadow_viewer->addEventHandler(vsg::Trackball::create(camera));
        auto Shadow_event = vsg::Event::create_if(true, shadow_window->getOrCreateDevice()); // Vulkan creates VkEvent in an unsignalled state
        shadow_screenshotHandler = IBLScreenshot::ScreenshotHandler::create(Shadow_event);
        shadow_viewer->addEventHandler(shadow_screenshotHandler);

        std::cout << "阴影窗口" << std::endl;

        /*
        auto Shadow_event = vsg::Event::create_if(true, Shadow_window->getOrCreateDevice()); // Vulkan creates VkEvent in an unsignalled state
        Shadow_screenshotHandler = IBLScreenshot::ScreenshotHandler::create(Shadow_event);
        Shadow_viewer->addEventHandler(Shadow_screenshotHandler);
        */

        //----------------------------------------------------------------窗口4----------------------------------------------------------//
        for (int i = 0; i < 6; i++)
        {
            vsg::ref_ptr<vsg::Image> storageImage = vsg::Image::create();
            storageImage->imageType = VK_IMAGE_TYPE_2D;
            if (i % 2 == 0){
                storageImage->format = VK_FORMAT_R8G8B8A8_UNORM; //VK_FORMAT_R8G8B8A8_UNORM;
                storageImage->usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            }else{
                storageImage->format = VK_FORMAT_D32_SFLOAT; //VK_FORMAT_R8G8B8A8_UNORM;
                storageImage->usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
            storageImage->extent.width = window->extent2D().width;
            storageImage->extent.height = window->extent2D().height;
            storageImage->extent.depth = 1;
            storageImage->mipLevels = 1;
            storageImage->arrayLayers = 1;
            storageImage->samples = VK_SAMPLE_COUNT_1_BIT;
            storageImage->tiling = VK_IMAGE_TILING_OPTIMAL;
            storageImage->initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            storageImage->flags = 0;
            storageImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            storageImagesIBL[i] = storageImage;
            auto context = vsg::Context::create(window->getOrCreateDevice());
            imageInfosIBL[i] = vsg::ImageInfo::create(vsg::Sampler::create(), vsg::createImageView(*context, storageImage, VK_IMAGE_ASPECT_COLOR_BIT), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            copyImagesIBL[i] = vsg::CopyImage::create();
            copyImagesIBL[i]->dstImage = storageImage;
            copyImagesIBL[i]->dstImageLayout = imageInfosIBL[i]->imageLayout;
            VkImageCopy copyRegion = {};
            if(i % 2 == 0)
            {
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.dstSubresource.layerCount = 1;
            }
            else
            {
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                copyRegion.dstSubresource.layerCount = 1;
            }
            copyRegion.extent.width = window->extent2D().width;
            copyRegion.extent.height = window->extent2D().height;
            copyRegion.extent.depth = 1;
            copyImagesIBL[i]->regions.push_back(copyRegion);
        }
        
        convert_image = new ConvertImage(width, height);
        vsg_color_image = vsg::ubvec3Array2D::create(width, height);
        vsg_depth_image = vsg::ushortArray2D::create(width, height);
        vsg_color_image->properties.format = VK_FORMAT_R8G8B8_UNORM;
        vsg_color_image->properties.dataVariance = vsg::DYNAMIC_DATA;
        vsg_depth_image->properties.format = VK_FORMAT_R16_UNORM;
        vsg_depth_image->properties.dataVariance = vsg::DYNAMIC_DATA;
        // for (int i = 0; i < width * height; i++)
        // {
        //     auto color_pixel = static_cast<vsg::ubvec3*>(vsg_color_image->dataPointer(i));
        //     uint16_t* depth_pixel = static_cast<uint16_t*>(vsg_depth_image->dataPointer(i));

        //     color_pixel->x = 0;
        //     color_pixel->y = 0;
        //     color_pixel->z = 0;
        //     depth_pixel = 0;
        // }
        // vsg_color_image->dirty();
        // vsg_depth_image->dirty();

        for (int i = 0; i < 6; i++)
        {
            storage_images[i] = createStorageImage(i % 2);
            auto context = vsg::Context::create(window->getOrCreateDevice());
            imageInfos[i] = vsg::ImageInfo::create(vsg::Sampler::create(), vsg::createImageView(*context, storage_images[i], VK_IMAGE_ASPECT_COLOR_BIT), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            copy_images[i] = vsg::CopyImage::create();
            copy_images[i]->dstImage = storage_images[i];
            copy_images[i]->dstImageLayout = imageInfos[i]->imageLayout;
            VkImageCopy copyRegion = {};
            if (i % 2 == 0)
            {
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.dstSubresource.layerCount = 1;
            }
            else
            {
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                copyRegion.dstSubresource.layerCount = 1;
            }

            copyRegion.extent.width = window->extent2D().width;
            copyRegion.extent.height = window->extent2D().height;
            copyRegion.extent.depth = 1;
            copy_images[i]->regions.push_back(copyRegion);
        }

        vsg::ref_ptr<vsg::Group> mergeScenegraph = vsg::Group::create();
        {
            auto buildMergeShaderSet = [](vsg::ref_ptr<vsg::Options> options) -> vsg::ref_ptr<vsg::ShaderSet> {
                auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/IBL/fullscreenquad.vert", options);
                auto fragShader = vsg::read_cast<vsg::ShaderStage>("shaders/new_merge.frag", options);
                auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragShader});

                const uint32_t TEXTURE_DESCRIPTOR_SET = 0;
                const uint32_t MATERIAL_DESCRIPTOR_SET = 1;
                shaderSet->addDescriptorBinding("cadColor", "", TEXTURE_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
                shaderSet->addDescriptorBinding("cadDepth", "", TEXTURE_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_D32_SFLOAT}));
                shaderSet->addDescriptorBinding("planeColor", "", TEXTURE_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
                shaderSet->addDescriptorBinding("planeDepth", "", TEXTURE_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_D32_SFLOAT}));
                shaderSet->addDescriptorBinding("shadowColor", "", TEXTURE_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
                shaderSet->addDescriptorBinding("shadowDepth", "", TEXTURE_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_D32_SFLOAT}));
                //shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(TEXTURE_DESCRIPTOR_SET));
                return shaderSet;
            };
            auto mergeShaderSet = buildMergeShaderSet(options);

            vsg::DataList Env_vertexArrays;
            auto Env_graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(mergeShaderSet); //渲染管线创建
            vsg::ImageInfoList cadColor = {imageInfosIBL[0]};
            vsg::ImageInfoList cadDepth = {imageInfosIBL[1]};
            vsg::ImageInfoList planeColor = {imageInfosIBL[2]};
            vsg::ImageInfoList planeDepth = {imageInfosIBL[3]};
            vsg::ImageInfoList shadowColor = {imageInfosIBL[4]};
            vsg::ImageInfoList shadowDepth = {imageInfosIBL[5]};


            Env_graphicsPipelineConfig->assignTexture("cadColor", cadColor);
            Env_graphicsPipelineConfig->assignTexture("cadDepth", cadDepth);
            Env_graphicsPipelineConfig->assignTexture("shadowColor", shadowColor);
            Env_graphicsPipelineConfig->assignTexture("shadowDepth", shadowDepth);
            Env_graphicsPipelineConfig->assignTexture("planeColor", vsg_color_image);
            Env_graphicsPipelineConfig->assignTexture("planeDepth", vsg_depth_image);
            //Env_graphicsPipelineConfig->assignTexture("projectionColor", projectionColor);
            //Env_graphicsPipelineConfig->assignTexture("projectionDepth", projectionDepth);

            // 绑定索引
            auto Env_drawCommands = vsg::Commands::create();
            //cout << mesh.indexes->size() << endl;
            Env_drawCommands->addChild(vsg::Draw::create(3, 1, 0, 0));

            //双面显示
            auto ds_state = vsg::DepthStencilState::create();
            ds_state->depthTestEnable = VK_FALSE;
            auto Env_rs = vsg::RasterizationState::create();
            Env_rs->cullMode = VK_CULL_MODE_NONE;
            Env_graphicsPipelineConfig->pipelineStates.push_back(ds_state);
            Env_graphicsPipelineConfig->pipelineStates.push_back(Env_rs);

            Env_graphicsPipelineConfig->init();

            // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
            auto Env_stateGroup = vsg::StateGroup::create();
            Env_graphicsPipelineConfig->copyTo(Env_stateGroup);
            Env_stateGroup->addChild(Env_drawCommands);
            mergeScenegraph = Env_stateGroup;
        }
        
        
       

        // convertDepthImage = createStorageImage(i % 2);
        // auto context = vsg::Context::create(window->getOrCreateDevice());
        // convertDepthImageInfo = vsg::ImageInfo::create(vsg::Sampler::create(), vsg::createImageView(*context, storageImages[i], VK_IMAGE_ASPECT_COLOR_BIT), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        
        // vsg::ref_ptr<vsg::ShaderSet> mergeShader = cad.buildIntgShader(project_path + "asset/data/shaders/merge.vert", project_path + "asset/data/shaders/merge.frag");
        std::cout << "融合窗口" << std::endl;
        //cad.buildIntgNode(mergeScenegraph, mergeShader, imageInfosIBL, imageInfos, vsgColorImage, vsgDepthImage); //读取几何信息1
        
        double mergeradius = 2000.0; // 固定观察距离
        auto mergeviewport = vsg::ViewportState::create(0, 0, intgWindowTraits->width, intgWindowTraits->height);
        auto mergeperspective = vsg::Perspective::create(60.0, static_cast<double>(intgWindowTraits->width) / static_cast<double>(intgWindowTraits->height), nearFarRatio * mergeradius, mergeradius * 10.0);
        vsg::dvec3 mergecentre = {0.0, 0.0, 0.0};                              // 固定观察点
        vsg::dvec3 mergeeye = mergecentre + vsg::dvec3(0.0, 0.0, mergeradius); // 固定相机位置
        vsg::dvec3 mergeup = {0.0, 1.0, 0.0};                                  // 固定观察方向
        auto mergelookAt = vsg::LookAt::create(mergeeye, mergecentre, mergeup);
        auto mergecamera = vsg::Camera::create(mergeperspective, mergelookAt, mergeviewport);

        //intgWindowTraits->device = window->getOrCreateDevice(); //共享设备 不加这句Env_window->getOrCreateDevice()就报错 一个bug de几天
        final_window = vsg::Window::create(intgWindowTraits);
        final_viewer->addWindow(final_window);
        auto final_view = vsg::View::create(mergecamera, mergeScenegraph);            //共用一个camera，改变一个window的视角，另一个window视角也会改变
        auto final_renderGraph = vsg::RenderGraph::create(final_window, final_view); //如果用Env_window会报错
        final_renderGraph->clearValues[0].color = {{0.8f, 0.8f, 0.8f, 1.f}};
        auto final_commandGraph = vsg::CommandGraph::create(final_window); //如果用Env_window会报错
        for (int i = 0; i <= 5; i++){
            final_commandGraph->addChild(copyImagesIBL[i]);
        }
        final_commandGraph->addChild(final_renderGraph);
        final_viewer->assignRecordAndSubmitTaskAndPresentation({final_commandGraph});
        //auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(imageInfos[4]->imageView, Final_window);
        //Final_commandGraph->addChild(copyImageViewToWindow);
        final_viewer->compile();
        final_viewer->addEventHandlers({vsg::CloseHandler::create(final_viewer)});
        final_viewer->addEventHandler(vsg::Trackball::create(camera));

        VkExtent2D extent = {};
        extent.width = render_width;
        extent.height = render_height;
        final_screenshotHandler = ScreenshotHandler::create(final_window, extent, ENCODER);
        
    }

    void setRealColorAndImage(const std::string& real_color, const std::string& real_depth){
        color_pixels = convert_image->convertColor(real_color);
        depth_pixels = convert_image->convertDepth(real_depth);
    }

    void setRealColorAndImage(unsigned char * real_color, unsigned short * real_depth){
        color_pixels = real_color;
        depth_pixels = real_depth;
    }

    void updateCamera(vsg::dvec3 centre, vsg::dvec3 eye, vsg::dvec3 up){
        auto lookat = vsg::LookAt::create(eye, centre, up);
        auto cameralookat = camera->viewMatrix.cast<vsg::LookAt>();
        cameralookat->set(lookat->transform());

        // auto out_transform = camera->viewMatrix->transform();
    }

    void updateCamera(vsg::dmat4 view_matrix){
        // camera->viewMatrix = vsg::LookAt::create(view_matrix);
        auto lookat = camera->viewMatrix.cast<vsg::LookAt>();
        lookat->set(view_matrix);
    }
    
    void updateObjectPose(std::string instance_name, vsg::dmat4 model_matrix){
        instance_phongs[instance_name]->nodePtr[""].transform->matrix = model_matrix;
        //instance_shadows[instance_name]->nodePtr[""].transform->matrix = model_matrix;
    }


    bool render(){
        // std::cout << "new frame: " << std::endl;
        // for (int i = 0; i < 9; ++i) {
        //     std::cout << lookat_vector[i] << " ";
        // }
        // std::cout << std::endl;
        // std::cout << real_color << std::endl;
        // // std::cout << real_depth << std::endl;
        // float a;
        // std::cin >> a;
        //--------------------------------------------------------------渲染循环----------------------------------------------------------//
        if (viewer->advanceToNextFrame() && env_viewer->advanceToNextFrame() && shadow_viewer->advanceToNextFrame() && final_viewer->advanceToNextFrame())
        {
            // vsg::dvec3 centre = {lookat_vector[0], lookat_vector[1], lookat_vector[2]};                    // 固定观察点
            // vsg::dvec3 eye = {lookat_vector[3], lookat_vector[4], lookat_vector[5]};// 固定相机位置
            // vsg::dvec3 up = {lookat_vector[6], lookat_vector[7], lookat_vector[8]};                       // 固定观察方向
            // auto lookAt = vsg::LookAt::create(eye, centre, up);
            // camera->viewMatrix = lookAt;

            for (int i = 0; i < width * height; i++)
            {
                auto color_pixel = static_cast<vsg::ubvec3*>(vsg_color_image->dataPointer(i));
                uint16_t* depth_pixel = static_cast<uint16_t*>(vsg_depth_image->dataPointer(i));

                color_pixel->x = color_pixels[i * 3];//segmentation fault
                color_pixel->y = color_pixels[i * 3 + 1];
                color_pixel->z = color_pixels[i * 3 + 2];
                *depth_pixel = depth_pixels[i];
            }

            // depth fit 
            for(int iterate_num = 0; iterate_num < 30; iterate_num++){
                for(int i = 0; i < height; i++){
                    for(int j = 0; j < width; j++){
                        uint16_t* depth_pixel = static_cast<uint16_t*>(vsg_depth_image->dataPointer(width * i + j));
                        if(*depth_pixel < 100){
                            for(int m = -1; m <= 1; m ++){ //height
                                for(int n = -1; n <= 1; n ++){ //width
                                    if((i + m) >= 0 && (i + m) < height && (j + n) >= 0 && (j + n) < width){
                                        uint16_t* neighbor_pixel = static_cast<uint16_t*>(vsg_depth_image->dataPointer(width * (i + m) + (j + n)));
                                        
                                        if(*neighbor_pixel > 100)
                                            *depth_pixel = *neighbor_pixel;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // uint16_t* depth_pixel = static_cast<uint16_t*>(vsg_depth_image->Data());
            // depth_pixel = depth_pixels;

            vsg_color_image->dirty();
            vsg_depth_image->dirty();


            // for(int i = 0; i < 9; i ++)
            //     std::cout << lookat_vector[i] << " ";
            // std::cout << std::endl;

            //------------------------------------------------------窗口1-----------------------------------------------------//
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents(); //将保存在`UIEvents`对象中的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。
            viewer->update();
            viewer->recordAndSubmit(); //于记录和提交命令图。它会遍历`RecordAndSubmitTasks`列表中的任务，并对每个任务执行记录和提交操作。
            //viewer->present(); //呈现渲染结果。遍历`Presentations`列表中的呈现器，并对每个呈现器执行呈现操作，将渲染结果显示在窗口中。
            copyImagesIBL[0]->srcImage = screenshotHandler->screenshot_image(window);
            copyImagesIBL[0]->srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            copyImagesIBL[1]->srcImage = screenshotHandler->screenshot_depth(window);
            copyImagesIBL[1]->srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            viewer->present();

            //------------------------------------------------------窗口2-----------------------------------------------------//
            env_viewer->handleEvents(); //将保存在`UIEvents`对象中的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。

            IBL::textures.params->dirty();

            env_viewer->update();
            env_viewer->recordAndSubmit(); //于记录和提交命令图。窗口2提交会冲突报错
            copyImagesIBL[2]->srcImage = env_screenshotHandler->screenshot_image(env_window);
            copyImagesIBL[2]->srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            copyImagesIBL[3]->srcImage = env_screenshotHandler->screenshot_depth(env_window);
            copyImagesIBL[3]->srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            env_viewer->present();
            // env_screenshotHandler->screenshot_cpudepth(env_window);
            // env_screenshotHandler->screenshot_encodeimage(final_window, color);

            //------------------------------------------------------窗口3-----------------------------------------------------//
            shadow_viewer->handleEvents(); //将保存在`UIEvents`对象中screenshotHandler的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。
            shadow_viewer->update();
            shadow_viewer->recordAndSubmit(); //于记录和提交命令图。窗口2提交会冲突报错
            copyImagesIBL[4]->srcImage = shadow_screenshotHandler->screenshot_image(shadow_window);
            copyImagesIBL[4]->srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            copyImagesIBL[5]->srcImage = shadow_screenshotHandler->screenshot_depth(shadow_window);
            copyImagesIBL[5]->srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            shadow_viewer->present(); //计算完之后一起呈现在窗口

            //------------------------------------------------------窗口4-----------------------------------------------------//
            final_viewer->handleEvents(); //将保存在`UIEvents`对象中的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。
            final_viewer->update();
            final_viewer->recordAndSubmit(); //于记录和提交命令图。窗口2提交会冲突报错
            // shadow_screenshotHandler->screenshot_encodeimage(shadow_window, color);
            // final_screenshotHandler->encodeImage(final_window, vPacket);
            // final_screenshotHandler->screenshot_cpudepth(final_window);
            // final_screenshotHandler->screenshot_cpuimage(final_window, color);
            final_viewer->present();
            return true;
        }
        else{
            return false;
        }
    }

    void getWindowImage(uint8_t* color){
        final_screenshotHandler->screenshot_cpuimage(final_window, color);
    }

    void getEncodeImage(std::vector<std::vector<uint8_t>>& vPacket){
        final_screenshotHandler->encodeImage(final_window, vPacket);
    }
};