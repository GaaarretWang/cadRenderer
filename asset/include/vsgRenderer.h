#include <vsgImGui/RenderImGui.h>
#include <vsgImGui/SendEventsToImGui.h>
#include <vsgImGui/imgui.h>

// #include <CADMesh.h>
#include <iostream>
#include <screenshot.h>
#include <vsg/all.h>
#include "convertPng.h"
#include "ConfigShader.h"
#include "ModelInstance.h"
simplelogger::Logger* logger = simplelogger::LoggerFactory::CreateConsoleLogger();

#include <CADMesh.h>
#include <iostream>
#include <screenshot.h>
#include <vsg/all.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/core/Data.h>

#include "importer/assimp.h"

#include "IBL.h"
#include "ImGui.hpp"
#include "refactor/CADMeshIBL.h"

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

using namespace std;

class vsgRenderer
{
    vsg::ref_ptr<vsg::Image> storageImages[6];
    vsg::ref_ptr<vsg::CopyImage> copyImages[6];
    vsg::ref_ptr<vsg::ImageInfo> imageInfos[6];
    vsg::ref_ptr<vsg::Image> storageImagesIBL[6];
    vsg::ref_ptr<vsg::CopyImage> copyImagesIBL[6];
    vsg::ref_ptr<vsg::ImageInfo> imageInfosIBL[7];
    vsg::ref_ptr<vsg::Image> convertDepthImage;
    vsg::ref_ptr<vsg::ImageInfo> convertDepthImageInfo;
    vsg::ref_ptr<vsg::Viewer> viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> Env_viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> Shadow_viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> Projection_viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> Final_viewer = vsg::Viewer::create();

    std::unordered_map<std::string, CADMesh*> transfered_meshes; //path, mesh*
    std::unordered_map<std::string, ModelInstance*> instance_phongs; //path, mesh*
    std::unordered_map<std::string, ModelInstance*> instance_shadows; //path, mesh*

    vsg::ref_ptr<vsg::DirectionalLight> directionalLight[4];
    vsg::ref_ptr<vsg::Switch> directionalLightSwitch = vsg::Switch::create();

    gui::Params* guiParams = gui::Params::create();


    vsg::ref_ptr<ScreenshotHandler> Final_screenshotHandler;
    vsg::ref_ptr<ScreenshotHandler> Shadow_screenshotHandler;
    vsg::ref_ptr<ScreenshotHandler> Env_screenshotHandler;
    vsg::ref_ptr<ScreenshotHandler> Projection_screenshotHandler;
    vsg::ref_ptr<ScreenshotHandler> screenshotHandler;

    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::Window> Env_window;
    vsg::ref_ptr<vsg::Window> Shadow_window;
    vsg::ref_ptr<vsg::Window> Projection_window;
    vsg::ref_ptr<vsg::Window> Final_window;
    vsg::ref_ptr<vsg::Camera> camera;

    std::string project_path;

    float fx = 386.52199190267083;//焦距(x轴上)
    float fy = 387.32300428823663;//焦距(y轴上)
    float cx = 326.5103569741365;//图像中心点(x轴)
    float cy = 237.40293732598795;//图像中心点(y轴)
    float near = 0.1f;
    float far = 65.535f;
    int width;
    int height;
    ConvertImage *convertimage;
    vsg::ref_ptr<vsg::Data> vsgColorImage;
    vsg::ref_ptr<vsg::Data> vsgDepthImage;

    //every frame's real color and depth
    unsigned char * color_pixels;
    unsigned short * depth_pixels;

    

    vsg::ref_ptr<vsg::WindowTraits> createWindowTraits(string windowTitle, int num,  vsg::ref_ptr<vsg::Options> options)
    {
        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = windowTitle;
        windowTraits->width = width;
        windowTraits->height = height;
        windowTraits->x = width * (num % 2);
        windowTraits->y = height * (num / 2);
        // enable transfer from the colour and depth buffer images
        windowTraits->swapchainPreferences.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        windowTraits->depthImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        windowTraits->depthFormat = VK_FORMAT_D16_UNORM;

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
            storageImage->format = VK_FORMAT_D16_UNORM;//VK_FORMAT_D32_SFLOAT                                                                                   //VK_FORMAT_R8G8B8A8_UNORM;
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
    void setWidthAndHeight(int width, int height){
        this->width = width;
        this->height = height;
    }

    void setKParameters(float fx, float fy, float cx, float cy){
        this->fx = fx;
        this->fy = fy;
        this->cx = cx;
        this->cy = cy;
    }

    void initRenderer(std::string engine_path, std::vector<vsg::dmat4>& model_transforms, std::vector<std::string>& model_paths, std::vector<std::string>& instance_names)
    {
        // project_path = engine_path.append("Rendering/");
        project_path = engine_path;
        auto options = vsg::Options::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE"); //2
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
        options->paths.push_back("./data");
        options->sharedObjects = vsg::SharedObjects::create();

        // zsz：手动初始化vulkan设备
        vsg::Names instanceExtensions;
        vsg::Names requestedLayers;
        bool debugLayer = true;
        bool apiDumpLayer = false;
        uint32_t vulkanVersion = VK_API_VERSION_1_3;


        std::cout << "mainV2:初始化Vulkan设备" << std::endl;

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
        }

        auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        if (!physicalDevice || queueFamily < 0)
        {
            std::cout << "Could not create PhysicalDevice" << std::endl;
        }

        vsg::Names deviceExtensions;
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        deviceExtensions.insert(deviceExtensions.end(), {VK_KHR_MULTIVIEW_EXTENSION_NAME,
                                                        VK_KHR_MAINTENANCE2_EXTENSION_NAME,
                                                        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
                                                        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME});

        vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}};

        auto deviceFeatures = vsg::DeviceFeatures::create();
        deviceFeatures->get().samplerAnisotropy = VK_TRUE;
        deviceFeatures->get().geometryShader = VK_TRUE;

        auto device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, deviceFeatures);

        auto viewer = vsg::Viewer::create();
        auto context = vsg::Context::create(device);

        IBL::VsgContext vsgContext = {};
        vsgContext.viewer = viewer;
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

        viewer->compile();
        while (viewer->advanceToNextFrame())
        {
            viewer->handleEvents();
            viewer->update();
            viewer->recordAndSubmit();
            viewer->present();
            break;
        }
        viewer->recordAndSubmitTasks.clear();

        auto drawSkyboxNode = IBL::drawSkyboxVSGNode(vsgContext);

        std::cout << "IBL:创建环境光数据完成----创建窗口" << std::endl;

        // 创建窗口数据
        // 只包含虚拟物体
        auto cadWindowTraits = createWindowTraits("CADModel", 0, options);
        cadWindowTraits->device = device;
        // 背景图像（目前包含所有Shadow Receiver用于Debug）
        auto envWindowTraits = createWindowTraits("Background", 1, options);
        envWindowTraits->device = device;
        // 输出的Shadow Value
        auto shadowWindowTraits = createWindowTraits("shadowShader", 2, options);
        shadowWindowTraits->device = device;
        //投影窗口
        auto projectionWindowTraits = createWindowTraits("Projection", 3, options);
        // 合成的窗口
        auto intgWindowTraits = createWindowTraits("Integration", 4, options);
        intgWindowTraits->device = device;

        double nearFarRatio = 0.0001;       //近平面和远平面之间的比例
        auto numShadowMapsPerLight = 10; //每个光源的阴影贴图数量
        auto numLights = 2;                //光源数量

        //-----------------------------------------设置shader------------------------------------//
        ConfigShader config_shader;
        CADMesh cad;
        v2::CADMesh cadV2;
        //建立shader的函数有所不同,检查点
        vsg::ref_ptr<vsg::ShaderSet> phongShader = config_shader.buildShader(project_path + "asset/data/shaders/standard.vert", project_path + "asset/data/shaders/standard_phong.frag");
        vsg::ref_ptr<vsg::ShaderSet> planeShader = config_shader.buildShader(project_path + "asset/data/shaders/plane.vert", project_path + "asset/data/shaders/plane.frag");
        vsg::ref_ptr<vsg::ShaderSet> projectionShader = cad.buildShader(project_path + "asset/data/shaders/shadow.vert", project_path + "asset/data/shaders/shadow.frag");
        vsg::ref_ptr<vsg::ShaderSet> shadowShader = cad.buildShader(project_path + "asset/data/shaders/shadow.vert", project_path + "asset/data/shaders/shadow.frag");
        vsg::ref_ptr<vsg::ShaderSet> mergeShader = config_shader.buildIntgShader(project_path + "asset/data/shaders/merge.vert", project_path + "asset/data/shaders/merge.frag");

        //是否使用线框表示
        bool wireFrame = 0;
        if (wireFrame)
        {
            auto rasterizationState = vsg::RasterizationState::create();
            rasterizationState->polygonMode = VK_POLYGON_MODE_LINE;
            phongShader->defaultGraphicsPipelineStates.push_back(rasterizationState);
        }

        guiParams->metallic = 0.5f;
        guiParams->roughness = 0.3f;
        
        guiParams->lightParams.directionalLightCount = 1;
        guiParams->lightParams.lightDirectionsThetaPhi[0][0] = 0.5f;
        guiParams->lightParams.lightDirectionsThetaPhi[0][1] = 0.0f;


        //---------------------------------------------------场景创建--------------------------------------//
        std::cout << "场景创建" << std::endl;

        auto geometryGroup = vsg::Group::create();
        //auto geometryGroupSwitch = vsg::Switch::create();
        //geometryGroupSwitch->addChild(MASK_GEOMETRY, geometryGroup);
        auto modelGroup = vsg::Group::create();
        //auto modelGroupSwitch = vsg::Switch::create();
        //modelGroupSwitch->addChild(MASK_MODEL, modelGroup);
        auto envSceneGroup = vsg::Group::create();
        //auto otherGroupSwitch = vsg::Switch::create();
        //otherGroupSwitch->addChild(MASK_FAKE_BACKGROUND, otherGroup;
        auto projectionGroup = vsg::Group::create();
        auto shadowGroup = vsg::Group::create();
        //shadowGroupSwitch->addChild(MASK_DRAW_SHADOW, shadowGroup);
        //scenegraph->addChild(otherGroupSwitch);
        //scenegraph->addChild(geometryGroupSwitch);
        //scenegraph->addChild(modelGroupSwitch);
        //auto shadowGroup = vsg::Group::create();
        auto rootSwitch = vsg::Switch::create();
        rootSwitch->addChild(MASK_GEOMETRY, geometryGroup);
        rootSwitch->addChild(MASK_MODEL, modelGroup);
        rootSwitch->addChild(MASK_DRAW_SHADOW, shadowGroup);
        rootSwitch->addChild(MASK_FAKE_BACKGROUND, envSceneGroup);
        vsg::ref_ptr<vsg::Group> scenegraph = vsg::Group::create();
        scenegraph->addChild(rootSwitch);

        auto projectionScenegraph = vsg::Group::create();

        // skybox
        envSceneGroup->addChild(drawSkyboxNode);
        // Shader资源
        vsg::ref_ptr<vsg::PbrMaterialValue> objectMaterial;
        auto pbriblShaderSet = IBL::customPbrShaderSet(options);
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
        auto gpc_shadow = vsg::GraphicsPipelineConfigurator::create(shadowShader);
        addVertexAttribute(gpc_shadow, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
        addVertexAttribute(gpc_shadow, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, vec3ArrayProps);
        addVertexAttribute(gpc_shadow, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, vec2ArrayProps);
        addVertexAttribute(gpc_shadow, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, vec4ValueProps);
        gpc_shadow->assignDescriptor("material", plane_mat);
        gpc_shadow->init();

        // ---------------------------------读取重建模型------------------------------------//
        vsg::ref_ptr<vsg::Node> reconstructRoot;
        vsg::ref_ptr<vsg::Geometry> reconstructDrawCmd;
        {
            auto roomFilepath = vsg::findFile("geos/room.ply", options->paths);
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
                //auto gpc_mesh = gpc_ibl;
                //gpc_mesh->assignDescriptor("material", plane_mat);
                //gpc_mesh->init();
                //gpc_mesh->copyTo(pbrStateGroup);
                pbrStateGroup->addChild(reconstructDrawCmd);
                auto shadowStateGroup = vsg::StateGroup::create();
                gpc_shadow->copyTo(shadowStateGroup);
                shadowStateGroup->addChild(reconstructDrawCmd);

                //stateSwtich->addChild(MASK_PBR_FULL | MASK_FAKE_BACKGROUND, pbrStateGroup);
                //stateSwtich->addChild(MASK_DRAW_SHADOW, shadowStateGroup);

                rootSwitch->addChild(MASK_FAKE_BACKGROUND | MASK_SHADOW_CASTER, pbrStateGroup);
                rootSwitch->addChild(MASK_DRAW_SHADOW, shadowStateGroup);
                reconstructRoot = stateSwtich;

                cad.buildEnvPlaneNode(projectionScenegraph, importMesh, planeShader, model_transforms[0]);
            }
            else
            {
                std::cerr << "Error loading ply mesh" << std::endl;
            }
        }

        //---------------------------------------读取CAD模型------------------------------------------//
        CADMesh cad;

        //读取.fb格式的模型参数信息
        bool fullNormal = 0;
        // const std::string& path1 = project_path + "asset/data/FBDataOut/运输车.fb";
        // const std::string& path1 = project_path + "asset/data/geos/3ED_827.fb";
        

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
                    transfer_model->buildObjNode(path_i.c_str(), "", phongShader, model_transforms[i]); //读取obj文件
                else if(format == "fb")
                    transfer_model->transferModel(model_paths[i], fullNormal, phongShader, model_transforms[i]);
                transfered_meshes[path_i] = transfer_model;
            }
   
            if(format == "obj"){
                /*
                ModelInstance* instance_phong = new ModelInstance();
                instance_phong->buildObjInstance(transfer_model, scenegraph, phongShader, model_transforms[i]);
                instance_phongs[instance_names[i]] = instance_phong;
                */
                ModelInstance* instance_shadow = new ModelInstance();
                instance_shadow->buildObjInstance(transfer_model, projectionScenegraph, projectionShader, model_transforms[i]);
                instance_shadows[instance_names[i]] = instance_shadow;
            }
            else if(format == "fb"){
                /*
                ModelInstance* instance_phong = new ModelInstance();
                instance_phong->buildInstance(transfer_model, scenegraph, phongShader, model_transforms[i]);
                instance_phongs[instance_names[i]] = instance_phong;
                */
                ModelInstance* instance_shadow = new ModelInstance();
                instance_shadow->buildInstance(transfer_model, projectionScenegraph, projectionShader, model_transforms[i]);
                instance_shadows[instance_names[i]] = instance_shadow;
            }
            
        }

        vsg::ref_ptr<vsg::Node> cadMeshRoot;

        auto cadMeshDrawCmd = cadV2.createDrawCmd(gpc_ibl);
        auto cadMeshPbrStateGroup = vsg::StateGroup::create();
        cadMeshPbrStateGroup->addChild(cadMeshDrawCmd);

        auto gpc_object = vsg::GraphicsPipelineConfigurator::create(*gpc_ibl);
        gpc_object->assignDescriptor("material", object_mat);
        gpc_object->init();
        gpc_object->copyTo(cadMeshPbrStateGroup);

        auto cadMeshShadowStateGroup = vsg::StateGroup::create();
        cadMeshShadowStateGroup->addChild(cadMeshDrawCmd);
        gpc_shadow->copyTo(cadMeshShadowStateGroup);

        auto cadMeshSwitch = vsg::Switch::create();
        cadMeshSwitch->addChild(MASK_MODEL, cadMeshPbrStateGroup);
        cadMeshSwitch->addChild(MASK_DRAW_SHADOW, cadMeshShadowStateGroup);

        vsg::ref_ptr<vsg::MatrixTransform> meshTransform = vsg::MatrixTransform::create(vsg::scale(1.0e-3));
        meshTransform->addChild(cadMeshSwitch);
        cadMeshRoot = meshTransform;
        scenegraph->addChild(meshTransform);

        vsg::ref_ptr<vsg::Node> drawCube;
        vsg::ref_ptr<vsg::MatrixTransform> cubeTransform;
        {
            drawCube = cadV2.testCube(gpc_ibl);
            auto cubeTrans = vsg::MatrixTransform::create(vsg::translate(1.0, 0.0, 0.0) * vsg::scale(1.0));
            auto cubeSwitch = vsg::Switch::create();
            cubeSwitch->addChild(MASK_MODEL, drawCube);
            auto drawCubeShadow = cadV2.testCube(gpc_shadow);
            cubeSwitch->addChild(MASK_DRAW_SHADOW, drawCubeShadow);
            cubeTrans->addChild(cubeSwitch);
            drawCube = cubeTrans;
            cubeTransform = cubeTrans;
            scenegraph->addChild(cubeTrans);
        }

        //-----------------------------------设置光源----------------------------------//
        //vsg::ref_ptr<vsg::DirectionalLight> directionalLight; //定向光源 ref_ptr智能指针
        projectionGroup->addChild(projectionScenegraph);
        for (int i = 0; i < 4; i++)
        {
            directionalLight[i] = vsg::DirectionalLight::create();
            directionalLight[i]->name = "directional_" + std::to_string(i);
            directionalLight[i]->color.set(1.0, 1.0, 1.0);
            directionalLight[i]->intensity = 1.3;
            directionalLight[i]->shadowMaps = 1;
            switch (i)
            {
            case 0:
                directionalLight[i]->direction = vsg::normalize(vsg::vec3(1.0, 0.0, -1.0));
                break;
            case 1:
                directionalLight[i]->direction = vsg::normalize(vsg::vec3(0.9, 1.0, -1.0));
                break;
            }
            directionalLightSwitch->addChild(false, directionalLight[i]);
            projectionGroup->addChild(directionalLight[i]);
        }
        directionalLightSwitch->setSingleChildOn(0);
        projectionScenegraph = projectionGroup;
        scenegraph->addChild(directionalLightSwitch);

        // -----------------------设置相机参数------------------------------//
        double radius = 2000.0; // 固定观察距离
        auto viewport = vsg::ViewportState::create(0, 0, cadWindowTraits->width, cadWindowTraits->height);
        // auto perspective = vsg::Perspective::create(60.0, static_cast<double>(640) / static_cast<double>(480), nearFarRatio * radius, radius * 10.0);

        auto perspective = vsg::Perspective::create(fx, fy, cx, cy, width, height, near, far);

        vsg::dvec3 centre = {0.0, 0.0, 0.0};                    // 固定观察点
        vsg::dvec3 eye = centre + vsg::dvec3(0.0, radius, 0.0); // 固定相机位置
        vsg::dvec3 up = {0.0, 0.0, 1.0};                        // 固定观察方向
        auto lookAt = vsg::LookAt::create(eye, centre, up);
        camera = vsg::Camera::create(perspective, lookAt, viewport);
        
        auto renderImGui = vsgImGui::RenderImGui::create(window, gui::MyGui::create(guiParams, options));
        //----------------------------------------------------------------几何窗口----------------------------------------------------------//
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
            auto event = vsg::Event::create_if(true, window->getOrCreateDevice()); // Vulkan creates VkEvent in an unsignalled state
            auto screenshotHandler = ScreenshotHandler::create(event);
            viewer->addEventHandler(screenshotHandler);
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
        //----------------------------------------------------------------环境窗口----------------------------------------------------------//
        //   // create the viewer and assign window(s) to it
        envWindowTraits->device = window->getOrCreateDevice(); //共享设备 不加这句Env_window->getOrCreateDevice()就报错 一个bug de几天
        Env_window = vsg::Window::create(envWindowTraits);
        Env_viewer->addWindow(Env_window);
        auto Env_view = vsg::View::create(camera, scenegraph);              //共用一个camera，改变一个window的视角，另一个window视角也会改变
        Env_view->mask = MASK_PBR_FULL | MASK_SHADOW_CASTER | MASK_FAKE_BACKGROUND;
        Env_view->features = vsg::RECORD_LIGHTS;
        auto Env_renderGraph = vsg::RenderGraph::create(Env_window, Env_view); //如果用Env_window会报错
        Env_renderGraph->clearValues[0].color = {{0.f, 0.f, 0.f, 1.f}};
        Env_renderGraph->addChild(renderImGui);

        auto Env_commandGraph = vsg::CommandGraph::create(Env_window); //如果用Env_window会报错
        Env_commandGraph->addChild(Env_renderGraph);
        Env_viewer->assignRecordAndSubmitTaskAndPresentation({Env_commandGraph});
        Env_viewer->compile();
        Env_viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
        Env_viewer->addEventHandlers({vsg::CloseHandler::create(Env_viewer)});
        auto Env_event = vsg::Event::create_if(true, Env_window->getOrCreateDevice()); // Vulkan creates VkEvent in an unsignalled 
        Env_viewer->addEventHandler(vsg::Trackball::create(camera));
        auto Env_screenshotHandler = ScreenshotHandler::create(Env_event);
        Env_viewer->addEventHandler(Env_screenshotHandler);

        //----------------------------------------------------------------阴影窗口----------------------------------------------------------//
        shadowWindowTraits->device = window->getOrCreateDevice(); //共享设备 不加这句Env_window->getOrCreateDevice()就报错 一个bug de几天
        Shadow_window = vsg::Window::create(shadowWindowTraits);
        auto Shadow_device = Shadow_window->getOrCreateDevice();
        Shadow_viewer->addWindow(Shadow_window);
        auto Shadow_view = vsg::View::create(camera,scenegraph);                 //共用一个camera，改变一个window的视角，另一个window视角也会改变
        
        Shadow_view->mask = MASK_DRAW_SHADOW;
        Shadow_view->features = vsg::RECORD_SHADOW_MAPS;
        Shadow_view->viewDependentState = CustomViewDependentState::create(Shadow_view.get());
        Env_view->viewDependentState = Shadow_view->viewDependentState;
        
        auto Shadow_renderGraph = vsg::RenderGraph::create(Shadow_window, Shadow_view); //如果用Env_window会报错
        Shadow_renderGraph->clearValues[0].color = {{1.f, 1.f, 1.f, 1.f}};
        auto Shadow_commandGraph = vsg::CommandGraph::create(Shadow_window); //如果用Env_window会报错
        Shadow_commandGraph->addChild(Shadow_renderGraph);
        Shadow_viewer->assignRecordAndSubmitTaskAndPresentation({Shadow_commandGraph});
        Shadow_viewer->compile();
        Shadow_viewer->addEventHandlers({vsg::CloseHandler::create(Shadow_viewer)});
        auto Shadow_event = vsg::Event::create_if(true, Shadow_window->getOrCreateDevice()); // Vulkan creates VkEvent in an unsignalled state
        auto Shadow_screenshotHandler = ScreenshotHandler::create(Shadow_event);
        Shadow_viewer->addEventHandler(Shadow_screenshotHandler);

        //----------------------------------------------------------------投影窗口----------------------------------------------------------//
        projectionWindowTraits->device = window->getOrCreateDevice(); //共享设备 
        Projection_window = vsg::Window::create(projectionWindowTraits);
        auto Projection_device = Projection_window->getOrCreateDevice();
        Projection_viewer->addWindow(Projection_window);
        auto Projection_view = vsg::View::create(camera, projectionScenegraph);                 //共用一个camera，改变一个window的视角，另一个window视角也会改变
        auto Projection_renderGraph = vsg::RenderGraph::create(Projection_window, Projection_view); //如果用Env_window会报错
        Projection_renderGraph->clearValues[0].color = {{1.f, 1.f, 1.f, 1.f}};
        auto Projection_commandGraph = vsg::CommandGraph::create(Projection_window); //如果用Env_window会报错
        Projection_commandGraph->addChild(Projection_renderGraph);
        Projection_viewer->assignRecordAndSubmitTaskAndPresentation({Projection_commandGraph});
        Projection_viewer->compile();
        Projection_viewer->addEventHandlers({vsg::CloseHandler::create(Projection_viewer)});
        auto Projection_event = vsg::Event::create_if(true, Shadow_window->getOrCreateDevice()); // Vulkan creates VkEvent in an unsignalled state
        auto Projection_screenshotHandler = ScreenshotHandler::create(Projection_event);
        Projection_viewer->addEventHandler(Projection_screenshotHandler);

        //----------------------------------------------------------------融合窗口----------------------------------------------------------//
        for (int i = 0; i < 6; i++)
        {
            vsg::ref_ptr<vsg::Image> storageImage = vsg::Image::create();
            storageImage->imageType = VK_IMAGE_TYPE_2D;
            if (i % 2 == 0)
                storageImage->format = VK_FORMAT_B8G8R8A8_UNORM; //VK_FORMAT_R8G8B8A8_UNORM;
            else
                storageImage->format = VK_FORMAT_D32_SFLOAT; //VK_FORMAT_R8G8B8A8_UNORM;
            storageImage->extent.width = window->extent2D().width;
            storageImage->extent.height = window->extent2D().height;
            storageImage->extent.depth = 1;
            storageImage->mipLevels = 1;
            storageImage->arrayLayers = 1;
            storageImage->samples = VK_SAMPLE_COUNT_1_BIT;
            storageImage->tiling = VK_IMAGE_TILING_OPTIMAL;
            storageImage->usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // sample和storage一定要标
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
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.extent.width = window->extent2D().width;
            copyRegion.extent.height = window->extent2D().height;
            copyRegion.extent.depth = 1;
            copyImagesIBL[i]->regions.push_back(copyRegion);
        }

        vsg::ref_ptr<vsg::Node> mergeScenegraph;

        convertimage = new ConvertImage(width, height);
        vsgColorImage = vsg::ubvec3Array2D::create(width, height);
        vsgDepthImage = vsg::ushortArray2D::create(width, height);
        vsgColorImage->properties.format = VK_FORMAT_R8G8B8_UNORM;
        vsgColorImage->properties.dataVariance = vsg::DYNAMIC_DATA;
        vsgDepthImage->properties.format = VK_FORMAT_R16_UNORM;
        vsgDepthImage->properties.dataVariance = vsg::DYNAMIC_DATA;
        // for (int i = 0; i < width * height; i++)
        // {
        //     auto color_pixel = static_cast<vsg::ubvec3*>(vsgColorImage->dataPointer(i));
        //     uint16_t* depth_pixel = static_cast<uint16_t*>(vsgDepthImage->dataPointer(i));

        //     color_pixel->x = 0;
        //     color_pixel->y = 0;
        //     color_pixel->z = 0;
        //     depth_pixel = 0;
        // }
        // vsgColorImage->dirty();
        // vsgDepthImage->dirty();

        for (int i = 0; i < 6; i++)
        {
            storageImages[i] = createStorageImage(i % 2);
            auto context = vsg::Context::create(window->getOrCreateDevice());
            imageInfos[i] = vsg::ImageInfo::create(vsg::Sampler::create(), vsg::createImageView(*context, storageImages[i], VK_IMAGE_ASPECT_COLOR_BIT), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            copyImages[i] = vsg::CopyImage::create();
            copyImages[i]->dstImage = storageImages[i];
            copyImages[i]->dstImageLayout = imageInfos[i]->imageLayout;
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
            copyImages[i]->regions.push_back(copyRegion);
        }

        // convertDepthImage = createStorageImage(i % 2);
        // auto context = vsg::Context::create(window->getOrCreateDevice());
        // convertDepthImageInfo = vsg::ImageInfo::create(vsg::Sampler::create(), vsg::createImageView(*context, storageImages[i], VK_IMAGE_ASPECT_COLOR_BIT), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        
        // vsg::ref_ptr<vsg::ShaderSet> mergeShader = cad.buildIntgShader(project_path + "asset/data/shaders/merge.vert", project_path + "asset/data/shaders/merge.frag");
        cad.buildIntgNode(mergeScenegraph, mergeShader, imageInfosIBL, imageInfos, vsgColorImage, vsgDepthImage); //读取几何信息1

        double mergeradius = 2000.0; // 固定观察距离
        auto mergeviewport = vsg::ViewportState::create(0, 0, intgWindowTraits->width, intgWindowTraits->height);
        auto mergeperspective = vsg::Perspective::create(60.0, static_cast<double>(intgWindowTraits->width) / static_cast<double>(intgWindowTraits->height), nearFarRatio * mergeradius, mergeradius * 10.0);
        vsg::dvec3 mergecentre = {0.0, 0.0, 0.0};                              // 固定观察点
        vsg::dvec3 mergeeye = mergecentre + vsg::dvec3(0.0, 0.0, mergeradius); // 固定相机位置
        vsg::dvec3 mergeup = {0.0, 1.0, 0.0};                                  // 固定观察方向
        auto mergelookAt = vsg::LookAt::create(mergeeye, mergecentre, mergeup);
        auto mergecamera = vsg::Camera::create(mergeperspective, mergelookAt, mergeviewport);

        intgWindowTraits->device = window->getOrCreateDevice(); //共享设备 不加这句Env_window->getOrCreateDevice()就报错 一个bug de几天
        Final_window = vsg::Window::create(intgWindowTraits);
        Final_viewer->addWindow(Final_window);
        auto Final_view = vsg::View::create(mergecamera, mergeScenegraph);            //共用一个camera，改变一个window的视角，另一个window视角也会改变
        auto Final_renderGraph = vsg::RenderGraph::create(Final_window, Final_view); //如果用Env_window会报错
        Final_renderGraph->clearValues[0].color = {{0.8f, 0.8f, 0.8f, 1.f}};
        auto Final_commandGraph = vsg::CommandGraph::create(Final_window); //如果用Env_window会报错
        for (int i = 0; i < 5; i++)
            Final_commandGraph->addChild(copyImages[i]);
        Final_commandGraph->addChild(Final_renderGraph);
        Final_viewer->assignRecordAndSubmitTaskAndPresentation({Final_commandGraph});
        //auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(imageInfos[4]->imageView, Final_window);
        //Final_commandGraph->addChild(copyImageViewToWindow);
        Final_viewer->compile();
        Final_viewer->addEventHandlers({vsg::CloseHandler::create(Final_viewer)});
        Final_viewer->addEventHandler(vsg::Trackball::create(camera));
        
        VkExtent2D extent = {};
        extent.width = width;
        extent.height = height;
        // Final_screenshotHandler = ScreenshotHandler::create(window, extent);
        Final_screenshotHandler = ScreenshotHandler::create();
        Shadow_screenshotHandler = ScreenshotHandler::create();
        Env_screenshotHandler = ScreenshotHandler::create();
        Projection_screenshotHandler = ScreenshotHandler::create();
        screenshotHandler = ScreenshotHandler::create();

    }

    void setRealColorAndImage(const std::string& real_color, const std::string& real_depth){
        color_pixels = convertimage->convertColor(real_color);
        depth_pixels = convertimage->convertDepth(real_depth);
    }

    void setRealColorAndImage(unsigned char * real_color, unsigned short * real_depth){
        color_pixels = real_color;
        depth_pixels = real_depth;
    }

    void updateCamera(vsg::dvec3 centre, vsg::dvec3 eye, vsg::dvec3 up){
        camera->viewMatrix = vsg::LookAt::create(eye, centre, up);
        auto out_transform = camera->viewMatrix->transform();
    }

    void updateCamera(vsg::dmat4 view_matrix){
        camera->viewMatrix = vsg::LookAt::create(view_matrix);
    }
    
    void updateObjectPose(std::string instance_name, vsg::dmat4 model_matrix){
        instance_phongs[instance_name]->nodePtr[""].transform->matrix = model_matrix;
        instance_shadows[instance_name]->nodePtr[""].transform->matrix = model_matrix;
    }


    bool render(std::vector<std::vector<uint8_t>> &color){
        // std::cout << "new frame: " << std::endl;
        // for (int i = 0; i < 9; ++i) {
        //     std::cout << lookAtVector[i] << " ";
        // }
        // std::cout << std::endl;
        // std::cout << real_color << std::endl;
        // // std::cout << real_depth << std::endl;
        // float a;
        // std::cin >> a;
        //--------------------------------------------------------------渲染循环----------------------------------------------------------//
        if (viewer->advanceToNextFrame() && Env_viewer->advanceToNextFrame() && Shadow_viewer->advanceToNextFrame() && Projection_viewer->advanceToNextFrame() && Final_viewer->advanceToNextFrame())
        {
            // vsg::dvec3 centre = {lookAtVector[0], lookAtVector[1], lookAtVector[2]};                    // 固定观察点
            // vsg::dvec3 eye = {lookAtVector[3], lookAtVector[4], lookAtVector[5]};// 固定相机位置
            // vsg::dvec3 up = {lookAtVector[6], lookAtVector[7], lookAtVector[8]};                       // 固定观察方向
            // auto lookAt = vsg::LookAt::create(eye, centre, up);
            // camera->viewMatrix = lookAt;

            for (int i = 0; i < width * height; i++)
            {
                auto color_pixel = static_cast<vsg::ubvec3*>(vsgColorImage->dataPointer(i));
                uint16_t* depth_pixel = static_cast<uint16_t*>(vsgDepthImage->dataPointer(i));

                color_pixel->x = color_pixels[i * 3];
                color_pixel->y = color_pixels[i * 3 + 1];
                color_pixel->z = color_pixels[i * 3 + 2];
                *depth_pixel = depth_pixels[i];
            }
            for(int iterate_num = 0; iterate_num < 30; iterate_num++){
                for(int i = 0; i < height; i++){
                    for(int j = 0; j < width; j++){
                        uint16_t* depth_pixel = static_cast<uint16_t*>(vsgDepthImage->dataPointer(width * i + j));
                        if(*depth_pixel < 100){
                            for(int m = -1; m <= 1; m ++){ //height
                                for(int n = -1; n <= 1; n ++){ //width
                                    if((i + m) >= 0 && (i + m) < height && (j + n) >= 0 && (j + n) < width){
                                        uint16_t* neighbor_pixel = static_cast<uint16_t*>(vsgDepthImage->dataPointer(width * (i + m) + (j + n)));
                                        
                                        if(*neighbor_pixel > 100)
                                            *depth_pixel = *neighbor_pixel;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // uint16_t* depth_pixel = static_cast<uint16_t*>(vsgDepthImage->Data());
            // depth_pixel = depth_pixels;

            vsgColorImage->dirty();
            vsgDepthImage->dirty();


            // for(int i = 0; i < 9; i ++)
            //     std::cout << lookAtVector[i] << " ";
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
            Env_viewer->handleEvents(); //将保存在`UIEvents`对象中的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。
            auto& lightParams = guiParams->lightParams;
            for (int i = 0; i < lightParams.directionalLightCount; i++)
            {
                directionalLightSwitch->children[i].mask = vsg::boolToMask(true);
                directionalLight[i]->color = vsg::vec3(lightParams.lightColor[i][0], lightParams.lightColor[i][1], lightParams.lightColor[i][2]);
                directionalLight[i]->intensity = lightParams.lightColor[i][3];
                float theta = lightParams.lightDirectionsThetaPhi[i][0] * 3.14159265f;
                float phi = lightParams.lightDirectionsThetaPhi[i][1] * 3.14159265f * 2.0f;
                float sinTheta = sinf(theta);
                directionalLight[i]->direction = vsg::vec3(sinTheta * cosf(phi), sinTheta * sinf(phi), cosf(theta));
            }
            for(int i=lightParams.directionalLightCount; i<4; i++)
                directionalLightSwitch->children[i].mask = vsg::boolToMask(false);

            if (lightParams.envmapEnabled)
            {
                IBL::textures.params->at(0).set(0, 0, 0, lightParams.envmapStrength);
            }else{
                IBL::textures.params->at(0).set(0, 0, 0, 0.0f);
            }
            IBL::textures.params->dirty();

            Env_viewer->update();
            Env_viewer->recordAndSubmit(); //于记录和提交命令图。窗口2提交会冲突报错
            copyImagesIBL[2]->srcImage = Env_screenshotHandler->screenshot_image(Env_window);
            copyImagesIBL[2]->srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            copyImagesIBL[3]->srcImage = Env_screenshotHandler->screenshot_depth(Env_window);
            copyImagesIBL[3]->srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            Env_viewer->present();
            // Env_screenshotHandler->screenshot_cpudepth(Env_window);
            // Env_screenshotHandler->screenshot_encodeimage(Final_window, color);

            //------------------------------------------------------窗口3-----------------------------------------------------//
            Shadow_viewer->handleEvents(); //将保存在`UIEvents`对象中的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。
            Shadow_viewer->update();
            Shadow_viewer->recordAndSubmit(); //于记录和提交命令图。窗口2提交会冲突报错
            copyImagesIBL[4]->srcImage = Shadow_screenshotHandler->screenshot_image(Shadow_window);
            copyImagesIBL[4]->srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            copyImagesIBL[5]->srcImage = Shadow_screenshotHandler->screenshot_depth(Shadow_window);
            copyImagesIBL[5]->srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            Shadow_viewer->present(); //计算完之后一起呈现在窗口

            //------------------------------------------------------窗口4-----------------------------------------------------//
            Projection_viewer->handleEvents(); //将保存在`UIEvents`对象中的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。
            Projection_viewer->update();
            Projection_viewer->recordAndSubmit(); //于记录和提交命令图。窗口2提交会冲突报错
            copyImages[4]->srcImage = Projection_screenshotHandler->screenshot_image(Projection_window);
            copyImages[4]->srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            copyImages[5]->srcImage = Projection_screenshotHandler->screenshot_depth(Shadow_window);
            copyImages[5]->srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            Shadow_viewer->present(); //计算完之后一起呈现在窗口

            //------------------------------------------------------窗口5-----------------------------------------------------//
            Final_viewer->handleEvents(); //将保存在`UIEvents`对象中的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。
            Final_viewer->update();
            Final_viewer->recordAndSubmit(); //于记录和提交命令图。窗口2提交会冲突报错
            // Shadow_screenshotHandler->screenshot_encodeimage(Shadow_window, color);
            // screenshotHandler->screenshot_encodeimage(window, color);
            // Final_screenshotHandler->screenshot_encodeimage(Final_window, color);
            // Final_screenshotHandler->screenshot_cpudepth(Final_window);
            // for(int i = 0; i < size; i ++){
            //     std::cout << (int) color[i] << " ";
            // }
            // std::cout << std::endl;

            Final_viewer->present();
            return true;
        }
        else{
            return false;
        }
    }
};


