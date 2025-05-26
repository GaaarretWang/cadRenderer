#include "RenderingServer.h"

RenderingServer::RenderingServer() = default;

RenderingServer::~RenderingServer() = default;

int RenderingServer::Init(int argc, char** argv){
    renderer.setWidthAndHeight(width, height, upsample_scale);
    renderer.setKParameters(fx, fy, cx, cy);

    init_model_transforms.push_back(vsg::dmat4(0.000132165, 0, 0, 0, 0, 0.000132165, 0, 0, 0, 0, 0.000132165, 0, -0.00434349, 8.06674e-09, 0.0100961, 1));
    init_model_transforms.push_back(vsg::dmat4(0.0001, 0, 0, 0, 0, 0.0001, 0, 0, 0, 0, 0.0001, 0, -0.00434349, 8.06674e-09, 0.0100961, 1));
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.24006, 1.01482, -0.591005, 1) * init_model_transforms[0]);
    model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -0.20006, 1.01482, -0.6, 1) * init_model_transforms[0]
                               * vsg::dmat4(
                                    10, 0, 0, 0, 
                                    0, 10, 0, 0, 
                                    0, 0, 10, 0, 
                                    0, 0, 0, 1));
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -0.20006, 1.01482, -0.0, 1) * init_model_transforms[0]
    //                            * vsg::dmat4(
    //                                 10, 0, 0, 0, 
    //                                 0, 10, 0, 0, 
    //                                 0, 0, 10, 0, 
    //                                 0, 0, 0, 1));
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.30006, 1.01482, -0.8, 1) * init_model_transforms[0]
    //                            * vsg::dmat4(
    //                                 0.2, 0, 0, 0, 
    //                                 0, 0.2, 0, 0, 
    //                                 0, 0, 0.2, 0, 
    //                                 0, 0, 0, 1));
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.30006, 1.01482, -0.8, 1) * init_model_transforms[0]
    //                            * vsg::dmat4(
    //                                 0.2, 0, 0, 0, 
    //                                 0, 0.2, 0, 0, 
    //                                 0, 0, 0.2, 0, 
    //                                 0, 0, 0, 1));
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.30006, 1.01482, -0.8, 1) * init_model_transforms[0]
    //                            * vsg::dmat4(
    //                                 0.2, 0, 0, 0, 
    //                                 0, 0.2, 0, 0, 
    //                                 0, 0, 0.2, 0, 
    //                                 0, 0, 0, 1));
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.30006, 1.01482, -0.8, 1) * init_model_transforms[0]
    //                            * vsg::dmat4(
    //                                 0.2, 0, 0, 0, 
    //                                 0, 0.2, 0, 0, 
    //                                 0, 0, 0.2, 0, 
    //                                 0, 0, 0, 1));
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.24006, 0.81482, -0.991005, 1) * init_model_transforms[1]);
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.24006, 0.81482, -0.491005, 1) * init_model_transforms[1] * mat);
    // model_paths.push_back(rendering_dir + "asset/data/obj/Standtube.obj");
    // model_paths.push_back(rendering_dir + "asset/data/geos/大舱壁-ASM(PMI).fb");
    // model_paths.push_back(rendering_dir + "asset/data/obj/helicopter-engine/helicopter-engine.quads.obj");
    // model_paths.push_back(rendering_dir + "asset/data/obj/helicopter-engine/helicopter-engine.quads.obj");
    // model_paths.push_back(rendering_dir + "asset/data/obj/Medieval_building/output.obj");
    // model_paths.push_back(rendering_dir + "asset/data/geos/handNode_0.fb");
    model_paths.push_back(rendering_dir + "asset/data/geos/YIBIAOPAN.fb");
    // model_paths.push_back(rendering_dir + "asset/data/geos/YIBIAOPAN.fb");
    // model_paths.push_back(rendering_dir + "asset/data/geos/zhijiaC.fb");
    // model_paths.push_back(rendering_dir + "asset/data/geos/SeatPart.fb");
    // model_paths.push_back(rendering_dir + "asset/data/geos/LandingGear.fb");
    // model_paths.push_back(rendering_dir + "asset/data/geos/airplaneBody.fb");
    // model_paths.push_back(rendering_dir + "asset/data/geos/Engine.fb");
    // instance_names.push_back("大舱壁-ASM(PMI)");
    // instance_names.push_back("小舱壁-ASM-修改焊接后0");
    // instance_names.push_back("大舱壁-ASM(PMI)");
    // instance_names.push_back("Medieval_building");
    // instance_names.push_back("Medieval_building1");
    instance_names.push_back("YIBIAOPAN1");
    instance_names.push_back("YIBIAOPAN2");
    // instance_names.push_back("YIBIAOPAN3");
    // instance_names.push_back("YIBIAOPAN4");
    // instance_names.push_back("texture");

    vsg::CommandLine arguments(&argc, argv);

    renderer.setUpShader(rendering_dir);
    // vsg::dmat4 plane_transform = vsg::translate(0.0, 1.5, 0.0) * vsg::rotate(90.0, 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, 1.0);
    vsg::dmat4 plane_transform = vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -0.78, 1);
    // vsg::vec3 light_direction = vsg::normalize(vsg::vec3(0, 1, 0));
    //vsg::vec3 light_direction = vsg::normalize(vsg::vec3(-1.0, 0.2, -1.0));
    renderer.shadow_recevier_path = rendering_dir + "asset/data/obj/shadow_receiver.obj";
    renderer.shadow_recevier_transform = vsg::dmat4();
    renderer.shader_type = shader_type;
    renderer.initRenderer(rendering_dir, model_transforms, model_paths, instance_names, plane_transform);
    
    device = renderer.device;
    return 0;
}

int RenderingServer::Update(){
    // if(frame_count == 1000){
    //     renderer.hdr_image_num = 0;
    //     renderer.updateEnvLighting();
    // }

    model_transforms[0][3][2] += 0.005;
    if(model_transforms[0][3][2] > -0.45)
        model_transforms[0][3][2] = -0.679909;
    // renderer.updateObjectPose("YIBIAOPAN1", model_transforms[0]);

    // static PlaneData planeData = createTestPlanes();
    // static float subdivisions = 0.1;
    // static PlaneData subdividedPlaneData = subdividePlanes(planeData, subdivisions);
    // static MeshData mesh = convertPlaneDataToMesh(subdividedPlaneData);
    // float* vertices_pointer = static_cast<float*>(mesh.vertices->dataPointer(0));
    // size_t vertices_size = mesh.vertices->size() * 3;
    // uint32_t* indices_pointer = static_cast<uint32_t*>(mesh.indices->dataPointer(0));
    // size_t indices_size = mesh.indices->size();
    // renderer.addLineData(vertices_pointer, vertices_size, indices_pointer, indices_size);
    // renderer.addPointData(vertices_pointer, vertices_size, indices_pointer, indices_size);

    // std::vector<std::string> texts{"aaaaaaa"};
    // std::vector<vsg::ref_ptr<vsg::StandardLayout>> dynamic_text_layouts(1, vsg::StandardLayout::create());
    // dynamic_text_layouts[0]->billboard = true;
    // dynamic_text_layouts[0]->position = vsg::vec3(0.0, 0.0, 0.0);
    // dynamic_text_layouts[0]->horizontal = vsg::vec3(1.0, 0.0, 0.0);
    // dynamic_text_layouts[0]->vertical = vsg::vec3(0.0, 1.0, 0.0);
    // dynamic_text_layouts[0]->color = vsg::vec4(1.0, 0.9, 1.0, 1.0);
    // dynamic_text_layouts[0]->outlineWidth = 0.1;
    // renderer.addTextData(texts, dynamic_text_layouts);

    // a proto instance movement
    vsg::dmat4 matrix = vsg::dmat4(0.173648, 0, 0, 0,
                                   0.984808, 0, -0.173648, 0,
                                   0, 1, 0, 0,
                                   -238.327, -215.6, 22.5233, 1);
    static double z_offset = 0.0;
    z_offset += 0.5;
    if(z_offset > 20)
        z_offset = 0;
    matrix[3][2] += z_offset;
    // renderer.updateObjectPose("YIBIAOPAN128A9D3E8-D181-40BA-A99F-DDDF0B3F3384352", matrix);
    // renderer.repaint("YIBIAOPAN128A9D3E8-D181-40BA-A99F-DDDF0B3F3384352", 1);

    vsg::dvec3 centre = {lookat_vector[0], lookat_vector[1], lookat_vector[2]};                    // 固定观察点
    vsg::dvec3 eye = {lookat_vector[3], lookat_vector[4], lookat_vector[5]};// 固定相机位置
    vsg::dvec3 up = {lookat_vector[6], lookat_vector[7], lookat_vector[8]};                       // 固定观察方向

    if(cameara_pos_bool){//停止位姿变化
        renderer.updateCamera(centre, eye, up);

        auto color_pixels = all_images[frame_count % all_images.size()].color.get();
        auto depth_pixels = all_images[frame_count % all_images.size()].depth.get();
        renderer.setRealColorAndImage(color_pixels, depth_pixels);

        std::this_thread::sleep_for(std::chrono::milliseconds(25));

        if(stop_cameara_pos)
            cameara_pos_bool = false;
    }


    auto startRender = std::chrono::high_resolution_clock::now();
    renderer.render();
    auto endRender = std::chrono::high_resolution_clock::now();
    gui::global_params->render_server_times[0] = std::chrono::duration<double, std::milli>(endRender - startRender).count();

    auto startEncode = std::chrono::high_resolution_clock::now();
    renderer.getEncodeImage(vPacket);
    auto endEncode = std::chrono::high_resolution_clock::now();
    gui::global_params->render_server_times[1] = std::chrono::duration<double, std::milli>(endEncode - startEncode).count();
    
    frame_count ++;
    // std::cout << "frame_count " << frame_count << std::endl;
    return 0;
}