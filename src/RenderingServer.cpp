#include "RenderingServer.h"

RenderingServer::RenderingServer() = default;

RenderingServer::~RenderingServer() = default;

int RenderingServer::Init(int argc, char** argv){
    renderer.setWidthAndHeight(width, height, upsample_scale);
    renderer.setKParameters(fx, fy, cx, cy);

    init_model_transforms.push_back(vsg::dmat4(0.000132165, 0, 0, 0, 0, 0.000132165, 0, 0, 0, 0, 0.000132165, 0, -0.00434349, 8.06674e-09, 0.0100961, 1));
    init_model_transforms.push_back(vsg::dmat4(0.000132165, 0, 0, 0, 0, 0.000132165, 0, 0, 0, 0, 0.000132165, 0, -0.00434349, 8.06674e-09, 0.0100961, 1));
    model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.24006, 1.01482, -0.591005, 1) * init_model_transforms[0]);
    model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.30006, 1.01482, -0.591005, 1) * init_model_transforms[0]
                               * vsg::dmat4(
                                    20, 0, 0, 0, 
                                    0, 20, 0, 0, 
                                    0, 0, 20, 0, 
                                    0, 0, 0, 1));
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.24006, 0.81482, -0.991005, 1) * init_model_transforms[1]);
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.24006, 0.81482, -0.491005, 1) * init_model_transforms[1] * mat);
    // model_paths.push_back(rendering_dir + "asset/data/geos/大舱壁-ASM(PMI).fb");
    model_paths.push_back(rendering_dir + "asset/data/obj/Medieval_building/output.obj");
    model_paths.push_back(rendering_dir + "asset/data/geos/YIBIAOPAN.fb");
    // model_paths.push_back(rendering_dir + "asset/data/obj/小舱壁-ASM-修改焊接后.obj");
    // instance_names.push_back("大舱壁-ASM(PMI)");
    // instance_names.push_back("小舱壁-ASM-修改焊接后0");
    // instance_names.push_back("大舱壁-ASM(PMI)");
    instance_names.push_back("Medieval_building");
    instance_names.push_back("YIBIAOPAN");
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
    if(frame_count == 1000){
        renderer.hdr_image_num = 0;
        renderer.updateEnvLighting();
    }
    //model_transforms[2][3][2] += 0.005;
    if(model_transforms[2][3][2] > -0.45)
        model_transforms[2][3][2] = -0.679909;

    renderer.updateObjectPose("YIBIAOPAN", model_transforms[1]);

    if(frame_count % 64 == 31)
    {
        renderer.repaint("YIBIAOPAN", "006F1783-7FDA-4B8E-9219-69528CB35488", 0);
        renderer.repaint("YIBIAOPAN", "0103F622-8C3C-4872-833D-850E1D60190D", 0);
        renderer.repaint("YIBIAOPAN", "06FF30A9-50CA-4296-9636-2F7B7EFB6CC4", 0);
        renderer.repaint("YIBIAOPAN", "08333F85-D22B-4EF5-888F-F96CA81C56EF", 0);
        renderer.repaint("YIBIAOPAN", "0A7FC82F-8950-446A-8E10-871EAB94BD7A", 0);
        renderer.repaint("YIBIAOPAN", "0B6D2CBA-6C5E-46AD-B2A5-E6E9CCBEF127", 0);
    }
    else if(frame_count % 64 == 0)
    {
        renderer.repaint("YIBIAOPAN", "006F1783-7FDA-4B8E-9219-69528CB35488", 1);
        renderer.repaint("YIBIAOPAN", "0103F622-8C3C-4872-833D-850E1D60190D", 1);
        renderer.repaint("YIBIAOPAN", "06FF30A9-50CA-4296-9636-2F7B7EFB6CC4", 1);
        renderer.repaint("YIBIAOPAN", "08333F85-D22B-4EF5-888F-F96CA81C56EF", 1);
        renderer.repaint("YIBIAOPAN", "0A7FC82F-8950-446A-8E10-871EAB94BD7A", 1);
        renderer.repaint("YIBIAOPAN", "0B6D2CBA-6C5E-46AD-B2A5-E6E9CCBEF127", 1);
    }

    vsg::dvec3 centre = {lookat_vector[0], lookat_vector[1], lookat_vector[2]};                    // 固定观察点
    vsg::dvec3 eye = {lookat_vector[3], lookat_vector[4], lookat_vector[5]};// 固定相机位置
    vsg::dvec3 up = {lookat_vector[6], lookat_vector[7], lookat_vector[8]};                       // 固定观察方向
    if(cameara_pos_bool){//停止位姿变化
        renderer.updateCamera(centre, eye, up);
        if(stop_cameara_pos)
            cameara_pos_bool = false;
    }

    std::ifstream color_file(color_path, std::ios::binary | std::ios::app);
    std::vector<uint8_t> color_buffer((std::istreambuf_iterator<char>(color_file)), std::istreambuf_iterator<char>());
    std::ifstream depth_file(depth_path, std::ios::binary | std::ios::app);
    std::vector<uint8_t> depth_buffer((std::istreambuf_iterator<char>(depth_file)), std::istreambuf_iterator<char>());

    std::string real_color1(color_buffer.begin(), color_buffer.end());
    std::string real_depth1(depth_buffer.begin(), depth_buffer.end());

    const std::string& real_color = real_color1;
    const std::string& real_depth = real_depth1;
    uint8_t* color_image;
    if(use_png){
        renderer.setRealColorAndImage(real_color, real_depth);
    }
    else{
        // renderer.setRealColorAndImage(frameData.imgColor.data, frameData.imgDepth.data);
    }

    renderer.render();
    renderer.getEncodeImage(vPacket);
    frame_count ++;
    return 0;
}