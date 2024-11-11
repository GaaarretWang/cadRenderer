#include "RenderingServer.h"

RenderingServer::RenderingServer() = default;

RenderingServer::~RenderingServer() = default;

int RenderingServer::Init(int argc, char** argv){
    renderer.setWidthAndHeight(width, height, upsample_scale);
    renderer.setKParameters(fx, fy, cx, cy);

    init_model_transforms.push_back(vsg::dmat4(0.000132165, 0, 0, 0, 0, 0.000132165, 0, 0, 0, 0, 0.000132165, 0, -0.00434349, 8.06674e-09, 0.0100961, 1));
    init_model_transforms.push_back(vsg::dmat4(0.000132165, 0, 0, 0, 0, 0.000132165, 0, 0, 0, 0, 0.000132165, 0, -0.00434349, 8.06674e-09, 0.0100961, 1));
    model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.24006, 1.01482, -0.591005, 1) * init_model_transforms[0]);
    model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.30006, 1.01482, -0.591005, 1) * init_model_transforms[0]);
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.24006, 0.81482, -0.991005, 1) * init_model_transforms[1]);
    model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.24006, 0.81482, -0.691005, 1) * init_model_transforms[1]);
    model_paths.push_back(rendering_dir + "asset/data/geos/3ED_827.fb");
    model_paths.push_back(rendering_dir + "asset/data/geos/3ED_827.fb");
    // model_paths.push_back(rendering_dir + "asset/data/obj/小舱壁-ASM-修改焊接后.obj");
    model_paths.push_back(rendering_dir + "asset/data/obj/小舱壁-ASM-修改焊接后.obj");
    instance_names.push_back("3ED_827_0");
    instance_names.push_back("3ED_827_1");
    // instance_names.push_back("小舱壁-ASM-修改焊接后0");
    instance_names.push_back("小舱壁-ASM-修改焊接后1");

    vsg::CommandLine arguments(&argc, argv);

    renderer.setUpShader(rendering_dir, trackingShader);
    // vsg::dmat4 plane_transform = vsg::translate(0.0, 1.5, 0.0) * vsg::rotate(90.0, 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, 1.0);
    vsg::dmat4 plane_transform = vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -0.78, 1);
    // vsg::vec3 light_direction = vsg::normalize(vsg::vec3(0, 1, 0));
    //vsg::vec3 light_direction = vsg::normalize(vsg::vec3(-1.0, 0.2, -1.0));
    renderer.initRenderer(rendering_dir, model_transforms, model_paths, instance_names, plane_transform, arguments);
    
    device = renderer.device;
    return 0;
}

int RenderingServer::Update(){
    model_transforms[2][3][2] += 0.005;
    if(model_transforms[2][3][2] > -0.45)
        model_transforms[2][3][2] = -0.679909;

    renderer.updateObjectPose("小舱壁-ASM-修改焊接后1", model_transforms[2]);

    if(trackingViewMatrix){
        // auto vobj = sceneData.getObject("virtualObj1");
        // cv::Matx44f initPose = vobj->transform.GetMatrix();
        // // cv::Matx44f initPose(-0.982719, -0.0361663, 0.181536, 0.191284, 0.163545, -0.628986, 0.76002, 1.15165, 0.0866961, 0.776575, 0.624031, -1.01102, 0, 0, 0, 1); // for test
        // vsg::dmat4 left_hand_view = vsg::dmat4(initPose(0, 0), initPose(1, 0), initPose(2, 0), initPose(3, 0),
        //                                         initPose(0, 1), initPose(1, 1), initPose(2, 1), initPose(3, 1),
        //                                         initPose(0, 2), initPose(1, 2), initPose(2, 2), initPose(3, 2),
        //                                         initPose(0, 3), initPose(1, 3), initPose(2, 3), initPose(3, 3));
        // vsg::dmat4 left_to_right = vsg::dmat4(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1);
        // vsg::dmat4 right_hand_view = left_to_right * left_hand_view;
        // renderer.updateCamera(right_hand_view);
    }else{
        vsg::dvec3 centre = {lookat_vector[0], lookat_vector[1], lookat_vector[2]};                    // 固定观察点
        vsg::dvec3 eye = {lookat_vector[3], lookat_vector[4], lookat_vector[5]};// 固定相机位置
        vsg::dvec3 up = {lookat_vector[6], lookat_vector[7], lookat_vector[8]};                       // 固定观察方向
        renderer.updateCamera(centre, eye, up);
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
    return 0;
}