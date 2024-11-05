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

using namespace std;

class vsgRendererServer
{
    public:
    vsg::ref_ptr<vsg::Device> device;
    vsg::ref_ptr<vsg::Image> storage_images[6];
    vsg::ref_ptr<vsg::CopyImage> copy_images[6];
    vsg::ref_ptr<vsg::ImageInfo> imageInfos[6];
    vsg::ref_ptr<vsg::Viewer> viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> env_viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> shadow_viewer = vsg::Viewer::create();
    vsg::ref_ptr<vsg::Viewer> final_viewer = vsg::Viewer::create();

    std::unordered_map<std::string, CADMesh*> transfered_meshes; //path, mesh*
    std::unordered_map<std::string, ModelInstance*> instance_phongs; //path, mesh*
    std::unordered_map<std::string, ModelInstance*> instance_shadows; //path, mesh*

    vsg::ref_ptr<vsg::ShaderSet> phong_shader;
    vsg::ref_ptr<vsg::ShaderSet> plane_shader;
    vsg::ref_ptr<vsg::ShaderSet> shadow_shader;
    vsg::ref_ptr<vsg::ShaderSet> merge_shader;


    vsg::ref_ptr<ScreenshotHandler> final_screenshotHandler;
    vsg::ref_ptr<ScreenshotHandler> shadow_screenshotHandler;
    vsg::ref_ptr<ScreenshotHandler> env_screenshotHandler;
    vsg::ref_ptr<ScreenshotHandler> screenshotHandler;

    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::Window> env_window;
    vsg::ref_ptr<vsg::Window> shadow_window;
    vsg::ref_ptr<vsg::Window> final_window;
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
        phong_shader = config_shader.buildShader(project_path + "asset/data/shaders/standard.vert", project_path + "asset/data/shaders/standard_phong.frag");
        plane_shader = config_shader.buildShader(project_path + "asset/data/shaders/plane.vert", project_path + "asset/data/shaders/plane.frag");
        shadow_shader = config_shader.buildShader(project_path + "asset/data/shaders/shadow.vert", project_path + "asset/data/shaders/shadow.frag");
        if(objtracking_shader)
            merge_shader = config_shader.buildIntgShader(project_path + "asset/data/shaders/merge.vert", project_path + "asset/data/shaders/merge_objtracking.frag");
        else
            merge_shader = config_shader.buildIntgShader(project_path + "asset/data/shaders/merge.vert", project_path + "asset/data/shaders/merge.frag");
    }

    void initRenderer(std::string engine_path, std::vector<vsg::dmat4>& model_transforms, std::vector<std::string>& model_paths, std::vector<std::string>& instance_names, vsg::dmat4 plane_transform, vsg::vec3 light_direction)
    {
        // project_path = engine_path.append("Rendering/");
        project_path = engine_path;
        auto options = vsg::Options::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE"); //2
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
        options->sharedObjects = vsg::SharedObjects::create();

        //创建窗口数据
        auto cadWindowTraits = createWindowTraits("CADModel", 0, options);
        auto envWindowTraits = createWindowTraits("Background", 1, options);
        auto shadowWindowTraits = createWindowTraits("shadow_shader", 2, options);
        auto intgWindowTraits = createWindowTraits("Integration", 3, options);

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
                    transfer_model->buildObjNode(path_i.c_str(), "", cadScenegraph, phong_shader, model_transforms[i]); //读取obj文件
                else if(format == "fb")
                    transfer_model->transferModel(model_paths[i], fullNormal, cadScenegraph, phong_shader, model_transforms[i]);
                transfered_meshes[path_i] = transfer_model;
            }            
            if(format == "obj"){
                ModelInstance* instance_phong = new ModelInstance();
                instance_phong->buildObjInstance(transfer_model, cadScenegraph, phong_shader, model_transforms[i]);
                instance_phongs[instance_names[i]] = instance_phong;
                ModelInstance* instance_shadow = new ModelInstance();
                instance_shadow->buildObjInstance(transfer_model, shadowScenegraph, shadow_shader, model_transforms[i]);
                instance_shadows[instance_names[i]] = instance_shadow;
            }
            else if(format == "fb"){
                ModelInstance* instance_phong = new ModelInstance();
                instance_phong->buildInstance(transfer_model, cadScenegraph, phong_shader, model_transforms[i]);
                instance_phongs[instance_names[i]] = instance_phong;
                ModelInstance* instance_shadow = new ModelInstance();
                instance_shadow->buildInstance(transfer_model, shadowScenegraph, shadow_shader, model_transforms[i]);
                instance_shadows[instance_names[i]] = instance_shadow;
            }
        }
        cad.buildPlaneNode(shadowScenegraph, plane_shader, plane_transform);
        cad.buildPlaneNode(envScenegraph, phong_shader, plane_transform);
        // auto bounds = vsg::visit<vsg::ComputeBounds>(cadScenegraph).bounds;
        // vsg::dvec3 model_centre = (bounds.min + bounds.max) * 0.5;
        // vsg::dvec3 model_size = (bounds.max - bounds.min);
        // double model_scale = 0.1 / (vsg::length(bounds.max - bounds.min) * 0.5);
        // auto transform_cadscene = vsg::MatrixTransform::create();
        // transform_cadscene->matrix = vsg::translate(vsg::dvec3(0.24006, 1.01482, -0.591005)) * vsg::scale(model_scale, model_scale, model_scale) * vsg::translate(-model_centre);
        // transform_cadscene->addChild(cadScenegraph);
        // cadScenegraph = transform_cadscene;
        // auto transform_shadow = vsg::MatrixTransform::create();
        // transform_shadow->matrix = vsg::translate(vsg::dvec3(0.24006, 1.01482, -0.591005)) * vsg::scale(model_scale, model_scale, model_scale) * vsg::translate(-model_centre);
        // transform_shadow->addChild(shadowScenegraph);
        // shadowScenegraph = transform_shadow;
        // auto transform_env = vsg::MatrixTransform::create();
        // transform_env->matrix = vsg::translate(vsg::dvec3(0.24006, 1.01482, -0.591005)) * vsg::scale(model_scale, model_scale, model_scale) * vsg::translate(-model_centre);
        // for(int i = 0; i < 4; i ++){
        //     for(int j = 0; j < 4; j ++){
        //         std::cout << transform_env->matrix[i][j] << ", ";
        //     }
        // }
        // transform_env->addChild(envScenegraph);
        // envScenegraph = transform_env;
        // std::cout << model_centre;
        // std::cout << model_size;
        // auto new_bounds = vsg::visit<vsg::ComputeBounds>(cadScenegraph).bounds;
        // vsg::dvec3 new_model_centre = (new_bounds.min + new_bounds.max) * 0.5;
        // vsg::dvec3 new_model_size = (new_bounds.max - new_bounds.min);
        // std::cout << new_model_centre;
        // std::cout << new_model_size;

        //-----------------------------------设置光源----------------------------------//
        auto group = vsg::Group::create();
        auto Env_group = vsg::Group::create();
        auto Shadow_group = vsg::Group::create();
        group->addChild(cadScenegraph);
        Env_group->addChild(envScenegraph); //加在这里则scenegraph中也有背景内容
        Shadow_group->addChild(shadowScenegraph);
        vsg::ref_ptr<vsg::DirectionalLight> directionalLight; //定向光源 ref_ptr智能指针
        if (numLights >= 1)
        {
            std::cout << "-----------------DirectionalLight--------" << std::endl;
            directionalLight = vsg::DirectionalLight::create();
            directionalLight->name = "directional";
            directionalLight->color.set(1.0, 1.0, 1.0);
            directionalLight->intensity = 1.3;
            directionalLight->direction = light_direction;
            directionalLight->shadowMaps = numShadowMapsPerLight;
            group->addChild(directionalLight);
            Env_group->addChild(directionalLight);
            Shadow_group->addChild(directionalLight);
        }

        vsg::ref_ptr<vsg::AmbientLight> ambientLight; //环境光
        if (numLights >= 2)
        {
            std::cout << "-----------------AmbientLight--------" << std::endl;
            ambientLight = vsg::AmbientLight::create();
            ambientLight->name = "ambient";
            ambientLight->color.set(1.0, 1.0, 1.0);
            ambientLight->intensity = 0.3;
            group->addChild(ambientLight);
            Env_group->addChild(ambientLight);
            Shadow_group->addChild(ambientLight);
        }

        if (numLights >= 3)
        {
            std::cout << "-----------------DirectionalLight2--------" << std::endl;
            directionalLight->intensity = 0.7;
            ambientLight->intensity = 0.1;

            auto directionalLight2 = vsg::DirectionalLight::create();
            directionalLight2->name = "2nd directional";
            directionalLight2->color.set(1.0, 1.0, 0.0);
            directionalLight2->intensity = 0.7;
            directionalLight2->direction = vsg::normalize(vsg::vec3(0.9, 1.0, -1.0));
            directionalLight2->shadowMaps = numShadowMapsPerLight;
            group->addChild(directionalLight2);
            Env_group->addChild(directionalLight2);
            Shadow_group->addChild(directionalLight2);
        }
        cadScenegraph = group;
        envScenegraph = Env_group;
        shadowScenegraph = Shadow_group;

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
            auto view = vsg::View::create(camera, cadScenegraph);
            auto renderGraph = vsg::RenderGraph::create(window, view);
            renderGraph->clearValues[0].color = {{0.f, 0.f, 0.f, 1.f}};
            auto commandGraph = vsg::CommandGraph::create(window);
            commandGraph->addChild(renderGraph);
            viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
            viewer->compile(); //编译命令图。接受一个可选的`ResourceHints`对象作为参数，用于提供编译时的一些提示和配置。通过调用这个函数，可以将命令图编译为可执行的命令。
            viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});
            viewer->addEventHandler(vsg::Trackball::create(camera));
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
        envWindowTraits->device = window->getOrCreateDevice(); //共享设备 不加这句Env_window->getOrCreateDevice()就报错 一个bug de几天
        env_window = vsg::Window::create(envWindowTraits);
        env_viewer->addWindow(env_window);
        auto Env_view = vsg::View::create(camera, envScenegraph);              //共用一个camera，改变一个window的视角，另一个window视角也会改变
        auto Env_renderGraph = vsg::RenderGraph::create(env_window, Env_view); //如果用Env_window会报错
        Env_renderGraph->clearValues[0].color = {{0.f, 0.f, 0.f, 1.f}};
        auto Env_commandGraph = vsg::CommandGraph::create(env_window); //如果用Env_window会报错
        Env_commandGraph->addChild(Env_renderGraph);
        env_viewer->assignRecordAndSubmitTaskAndPresentation({Env_commandGraph});
        env_viewer->compile();
        env_viewer->addEventHandlers({vsg::CloseHandler::create(env_viewer)});
        env_viewer->addEventHandler(vsg::Trackball::create(camera));

        //----------------------------------------------------------------窗口3----------------------------------------------------------//
        shadowWindowTraits->device = window->getOrCreateDevice(); //共享设备 不加这句Env_window->getOrCreateDevice()就报错 一个bug de几天
        shadow_window = vsg::Window::create(shadowWindowTraits);
        shadow_viewer->addWindow(shadow_window);
        auto Shadow_view = vsg::View::create(camera, shadowScenegraph);                 //共用一个camera，改变一个window的视角，另一个window视角也会改变
        auto Shadow_renderGraph = vsg::RenderGraph::create(shadow_window, Shadow_view); //如果用Env_window会报错
        Shadow_renderGraph->clearValues[0].color = {{1.f, 1.f, 1.f, 1.f}};
        auto Shadow_commandGraph = vsg::CommandGraph::create(shadow_window); //如果用Env_window会报错
        Shadow_commandGraph->addChild(Shadow_renderGraph);
        shadow_viewer->assignRecordAndSubmitTaskAndPresentation({Shadow_commandGraph});
        shadow_viewer->compile();
        shadow_viewer->addEventHandlers({vsg::CloseHandler::create(shadow_viewer)});
        shadow_viewer->addEventHandler(vsg::Trackball::create(camera));

        //----------------------------------------------------------------窗口4----------------------------------------------------------//
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

        // convertDepthImage = createStorageImage(i % 2);
        // auto context = vsg::Context::create(window->getOrCreateDevice());
        // convertDepthImageInfo = vsg::ImageInfo::create(vsg::Sampler::create(), vsg::createImageView(*context, storage_images[i], VK_IMAGE_ASPECT_COLOR_BIT), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        
        // vsg::ref_ptr<vsg::ShaderSet> merge_shader = cad.buildIntgShader(project_path + "asset/data/shaders/merge.vert", project_path + "asset/data/shaders/merge.frag");
        cad.buildIntgNode(intgScenegraph, merge_shader, imageInfos, vsg_color_image, vsg_depth_image); //读取几何信息1

        double mergeradius = 2000.0; // 固定观察距离
        auto mergeviewport = vsg::ViewportState::create(0, 0, intgWindowTraits->width, intgWindowTraits->height);
        auto mergeperspective = vsg::Perspective::create(60.0, static_cast<double>(intgWindowTraits->width) / static_cast<double>(intgWindowTraits->height), nearFarRatio * mergeradius, mergeradius * 10.0);
        vsg::dvec3 mergecentre = {0.0, 0.0, 0.0};                              // 固定观察点
        vsg::dvec3 mergeeye = mergecentre + vsg::dvec3(0.0, 0.0, mergeradius); // 固定相机位置
        vsg::dvec3 mergeup = {0.0, 1.0, 0.0};                                  // 固定观察方向
        auto mergelookAt = vsg::LookAt::create(mergeeye, mergecentre, mergeup);
        auto mergecamera = vsg::Camera::create(mergeperspective, mergelookAt, mergeviewport);

        intgWindowTraits->device = window->getOrCreateDevice(); //共享设备 不加这句Env_window->getOrCreateDevice()就报错 一个bug de几天
        final_window = vsg::Window::create(intgWindowTraits);
        final_viewer->addWindow(final_window);

        auto Final_view = vsg::View::create(mergecamera, intgScenegraph);            //共用一个camera，改变一个window的视角，另一个window视角也会改变
        auto Final_renderGraph = vsg::RenderGraph::create(final_window, Final_view); //如果用Env_window会报错
        Final_renderGraph->clearValues[0].color = {{0.8f, 0.8f, 0.8f, 1.f}};

        auto Final_commandGraph = vsg::CommandGraph::create(final_window); //如果用Env_window会报错
        for (int i = 0; i < 5; i++)
            Final_commandGraph->addChild(copy_images[i]);
        Final_commandGraph->addChild(Final_renderGraph);
        final_viewer->assignRecordAndSubmitTaskAndPresentation({Final_commandGraph});
        //auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(imageInfos[4]->imageView, final_window);
        //Final_commandGraph->addChild(copyImageViewToWindow);
        final_viewer->compile();
        final_viewer->addEventHandlers({vsg::CloseHandler::create(final_viewer)});
        final_viewer->addEventHandler(vsg::Trackball::create(camera));
        
        VkExtent2D extent = {};
        extent.width = render_width;
        extent.height = render_height;
        final_screenshotHandler = ScreenshotHandler::create(final_window, extent, ENCODER);
        // final_screenshotHandler = ScreenshotHandler::create();
        shadow_screenshotHandler = ScreenshotHandler::create();
        env_screenshotHandler = ScreenshotHandler::create();
        screenshotHandler = ScreenshotHandler::create();

        device = window->getOrCreateDevice();
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
        instance_shadows[instance_name]->nodePtr[""].transform->matrix = model_matrix;
    }


    bool render(std::vector<std::vector<uint8_t>>& vPacket){
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

                color_pixel->x = color_pixels[i * 3];
                color_pixel->y = color_pixels[i * 3 + 1];
                color_pixel->z = color_pixels[i * 3 + 2];
                *depth_pixel = depth_pixels[i];
            }

            // depth fit 
            // for(int iterate_num = 0; iterate_num < 30; iterate_num++){
            //     for(int i = 0; i < height; i++){
            //         for(int j = 0; j < width; j++){
            //             uint16_t* depth_pixel = static_cast<uint16_t*>(vsg_depth_image->dataPointer(width * i + j));
            //             if(*depth_pixel < 100){
            //                 for(int m = -1; m <= 1; m ++){ //height
            //                     for(int n = -1; n <= 1; n ++){ //width
            //                         if((i + m) >= 0 && (i + m) < height && (j + n) >= 0 && (j + n) < width){
            //                             uint16_t* neighbor_pixel = static_cast<uint16_t*>(vsg_depth_image->dataPointer(width * (i + m) + (j + n)));
                                        
            //                             if(*neighbor_pixel > 100)
            //                                 *depth_pixel = *neighbor_pixel;
            //                         }
            //                     }
            //                 }
            //             }
            //         }
            //     }
            // }

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
            copy_images[0]->srcImage = screenshotHandler->screenshot_image(window);
            copy_images[0]->srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            copy_images[1]->srcImage = screenshotHandler->screenshot_depth(window);
            copy_images[1]->srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            viewer->present();

            //------------------------------------------------------窗口2-----------------------------------------------------//
            env_viewer->handleEvents(); //将保存在`UIEvents`对象中的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。
            env_viewer->update();
            env_viewer->recordAndSubmit(); //于记录和提交命令图。窗口2提交会冲突报错
            copy_images[2]->srcImage = env_screenshotHandler->screenshot_image(env_window);
            copy_images[2]->srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            copy_images[3]->srcImage = env_screenshotHandler->screenshot_depth(env_window);
            copy_images[3]->srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            env_viewer->present();
            // env_screenshotHandler->screenshot_cpudepth(env_window);
            // env_screenshotHandler->screenshot_encodeimage(final_window, color);

            //------------------------------------------------------窗口3-----------------------------------------------------//
            shadow_viewer->handleEvents(); //将保存在`UIEvents`对象中的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。
            shadow_viewer->update();
            shadow_viewer->recordAndSubmit(); //于记录和提交命令图。窗口2提交会冲突报错
            copy_images[4]->srcImage = shadow_screenshotHandler->screenshot_image(shadow_window);
            copy_images[4]->srcImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            copy_images[5]->srcImage = shadow_screenshotHandler->screenshot_depth(shadow_window);
            copy_images[5]->srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            shadow_viewer->present(); //计算完之后一起呈现在窗口

            //------------------------------------------------------窗口4-----------------------------------------------------//
            final_viewer->handleEvents(); //将保存在`UIEvents`对象中的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。
            final_viewer->update();
            final_viewer->recordAndSubmit(); //于记录和提交命令图。窗口2提交会冲突报错
            // shadow_screenshotHandler->screenshot_encodeimage(shadow_window, color);
            final_screenshotHandler->encodeImage(final_window, vPacket);
            // final_screenshotHandler->screenshot_cpudepth(final_window);
            // final_screenshotHandler->screenshot_cpuimage(final_window, color);
            final_viewer->present();
            return true;
        }
        else{
            return false;
        }
    }
};


