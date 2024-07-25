#include "Rendering.h"

Rendering::Rendering() = default;

Rendering::~Rendering() = default;

int Rendering::Init(){
    renderer.setWidthAndHeight(width, height);
    renderer.setKParameters(fx, fy, cx, cy);

    init_model_transforms.push_back(vsg::dmat4(0.000132165, 0, 0, 0, 0, 0.000132165, 0, 0, 0, 0, 0.000132165, 0, -0.00434349, 8.06674e-09, 0.0100961, 1));
    init_model_transforms.push_back(vsg::dmat4(0.000132165, 0, 0, 0, 0, 0.000132165, 0, 0, 0, 0, 0.000132165, 0, -0.00434349, 8.06674e-09, 0.0100961, 1));
    model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.24006, 1.01482, -0.591005, 1) * init_model_transforms[0]);
    // model_transforms.push_back(vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.24006, 0.81482, -0.991005, 1) * init_model_transforms[1]);
    // model_paths.push_back(renderingDir + "asset/data/geos/3ED_827.fb");
    model_paths.push_back(renderingDir + "asset/data/geos/3ED_827.fb");
    // model_paths.push_back(renderingDir + "asset/data/obj/小舱壁-ASM-修改焊接后.obj");
    // model_paths.push_back(renderingDir + "asset/data/obj/小舱壁-ASM-修改焊接后.obj");

    renderer.initRenderer(renderingDir, model_transforms, model_paths);

    return 0;
}

int Rendering::Update(){
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
        vsg::dvec3 centre = {lookAtVector[0], lookAtVector[1], lookAtVector[2]};                    // 固定观察点
        vsg::dvec3 eye = {lookAtVector[3], lookAtVector[4], lookAtVector[5]};// 固定相机位置
        vsg::dvec3 up = {lookAtVector[6], lookAtVector[7], lookAtVector[8]};                       // 固定观察方向
        renderer.updateCamera(centre, eye, up);
    }


    std::ifstream color_file(colorPath, std::ios::binary | std::ios::app);
    std::vector<uint8_t> color_buffer((std::istreambuf_iterator<char>(color_file)), std::istreambuf_iterator<char>());
    std::ifstream depth_file(depthPath, std::ios::binary | std::ios::app);
    std::vector<uint8_t> depth_buffer((std::istreambuf_iterator<char>(depth_file)), std::istreambuf_iterator<char>());

    std::string real_color1(color_buffer.begin(), color_buffer.end());
    std::string real_depth1(depth_buffer.begin(), depth_buffer.end());

    const std::string& real_color = real_color1;
    const std::string& real_depth = real_depth1;
    std::vector<std::vector<uint8_t>> color_image;
    if(use_png){
        renderer.setRealColorAndImage(real_color, real_depth);
    }
    else{
        // renderer.setRealColorAndImage(frameData.imgColor.data, frameData.imgDepth.data);
    }
    
    renderer.render(color_image);

    return 0;
}

int main(){
    std::vector<std::vector<double>> camera_pos;
    std::vector<std::string> camera_pos_timestamp;
    int frame =  0;
    
    std::ifstream inf;//文件读操作
    std::string line;
    inf.open("../asset/data/cameraPose/vsg_pose.txt");         
    while (getline(inf, line))      //getline(inf,s)是逐行读取inf中的文件信息
    {    
        std::istringstream Linestream(line);
        std::vector<double> values;
        std::vector<double> lookatvalues;
        std::string value;
        int count = 0;
        while(Linestream >> value){
            // std::cout << "value" << value << "=" << std::stof(value);
            if(count == 0)
                camera_pos_timestamp.push_back(value);
            else
                values.push_back(std::stod(value));
            count ++;
        }
        // pos, up, forward => centre, eye, up
        lookatvalues.push_back(values[0] + values[6]);
        lookatvalues.push_back(values[1] + values[7]);
        lookatvalues.push_back(values[2] + values[8]);
        lookatvalues.push_back(values[0]);
        lookatvalues.push_back(values[1]);
        lookatvalues.push_back(values[2]);
        lookatvalues.push_back(values[3]);
        lookatvalues.push_back(values[4]);
        lookatvalues.push_back(values[5]);

        camera_pos.push_back(lookatvalues);
    }
    inf.close();

    Rendering rendering;
    rendering.Init();
    while(true){
        rendering.colorPath = "../asset/data/dataset3/color/" + camera_pos_timestamp[frame] + ".png";
        rendering.depthPath = "../asset/data/dataset3/depth/" + camera_pos_timestamp[frame] + ".png";
        rendering.lookAtVector = camera_pos[frame];
        rendering.Update();
        frame++;
        if(frame > 698)
            frame = 0;
    }
}