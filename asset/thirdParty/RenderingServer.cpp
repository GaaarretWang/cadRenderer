#include "RenderingServer.h"

//��create֮������init��ʼ��server����
int RenderingServer::init(RPCServerConnection& con) 
{
    printf("%s. RenderingServer: init...\n", con.name.c_str());

    return STATE_OK;
}


void printPlaneData(
    const std::vector<std::vector<double>>& origins,
    const std::vector<std::vector<double>>& normals,
    const std::vector<std::vector<double>>& u_axes,
    const std::vector<std::vector<double>>& v_axes) {
    
    // 检查所有向量的大小是否一致
    size_t planeCount = origins.size();
    if (normals.size() != planeCount || u_axes.size() != planeCount || v_axes.size() != planeCount) {
        std::cerr << "错误: 平面数据维度不一致!" << std::endl;
        return;
    }
    
    // 设置输出格式
    std::cout << std::fixed << std::setprecision(6);  // 保留6位小数
    
    // 打印每个平面的信息
    for (size_t i = 0; i < planeCount; ++i) {
        std::cout << "平面 " << i << " 信息:" << std::endl;
        
        // 原点
        std::cout << "  原点: (";
        for (double val : origins[i]) {
            std::cout << std::setw(10) << val << ", ";
        }
        std::cout << "\b\b)" << std::endl;
        
        // 法向量
        std::cout << "  法向量: (";
        for (double val : normals[i]) {
            std::cout << std::setw(10) << val << ", ";
        }
        std::cout << "\b\b)" << std::endl;
        
        // U轴
        std::cout << "  U轴: (";
        for (double val : u_axes[i]) {
            std::cout << std::setw(10) << val << ", ";
        }
        std::cout << "\b\b)" << std::endl;
        
        // V轴
        std::cout << "  V轴: (";
        for (double val : v_axes[i]) {
            std::cout << std::setw(10) << val << ", ";
        }
        std::cout << "\b\b)" << std::endl;
        
        // 平面尺寸估计（通过U轴和V轴的模长）
        double u_length = std::sqrt(
            u_axes[i][0] * u_axes[i][0] + 
            u_axes[i][1] * u_axes[i][1] + 
            u_axes[i][2] * u_axes[i][2]);
        
        double v_length = std::sqrt(
            v_axes[i][0] * v_axes[i][0] + 
            v_axes[i][1] * v_axes[i][1] + 
            v_axes[i][2] * v_axes[i][2]);
        
        std::cout << "  估计尺寸: " << u_length << " x " << v_length << " 米" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
    }
}

std::string GetCurrentTimeWithMs() {
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;
	
	std::stringstream ss;
	ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
	<< "." << std::setfill('0') << std::setw(3) << ms.count();
	
	return ss.str();
}

template<typename T>
void printVector(std::vector<T> data){
    for(int i = 0; i < data.size(); ++i){
        std::cout << data[i] << ", ";
    }
    std::cout << std::endl;
}

template<typename T>
void printVectorVector(std::vector<std::vector<T>> data){
    for(int i = 0; i < data.size(); ++i){
        for(int j = 0; j < data[i].size(); ++j){
            std::cout << data[i][j] << ", ";
        }
        std::cout << std::endl;
    }
}



//������Ӧ���ӵ���Ϣ
int RenderingServer::call(RemoteProcPtr proc, FrameDataPtr frameDataPtr, RPCServerConnection& con)
{
    auto& send = proc->send;
    auto cmd = send.getd<std::string>("cmd");

	std::cout << "render cmd: " << cmd << std::endl;
    static std::unordered_map<std::string, vsg::dmat4> instance_names_to_transforms;

    if (cmd == "initCloudRenderer")
    {
        std::cout << "receive params" << std::endl;
        width = send["width"].get<int>();
        height = send["height"].get<int>();
        encode_scale = send["upsample_scale"].get<double>();
        upsample_scale = send["upsample_scale"].get<double>();
        std::vector<double> KParameters = send["KParameters"].getv<double>();
        std::vector<double> plane_transform = send["plane_transform"].getv<double>();
        std::vector<std::vector<double>> model_transforms_vector = send["model_transforms"].getv<std::vector<double>>();
        std::vector<std::string> instance_names = send["instance_names"].getv<std::string>();
        std::vector<std::string> model_paths = send["model_paths"].getv<std::string>();
        std::vector<double> background_transform = send["background_transform"].getv<double>();
        std::string background_path = send["background_path"].get<std::string>();
        int sceneid = send["sceneId"].get<int>();
        renderer.shadow_receiver_transform = VectorTodmat4(background_transform);
        renderer.shadow_receiver_path = background_path;
        renderer.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        std::cout << "width: " << width << std::endl;
        std::cout << "height: " << height << std::endl;
        std::cout << "upsample_scale: " << upsample_scale << std::endl;
        std::cout << "KParameters: ";
        printVector(KParameters);
        std::cout << "plane_transform: ";
        printVector(plane_transform);
        std::cout << "model_transforms_vector: ";
        printVectorVector(model_transforms_vector);
        std::cout << "instance_names: ";
        printVector(instance_names);
        std::cout << "model_paths: ";
        printVector(model_paths);
        std::cout << "background_transform: ";
        printVector(background_transform);
        std::cout << "background_path: " << background_path << std::endl;


// width: 640
// height: 480
// upsample_scale: 3
// KParameters: 385.128, 384.118, 326.371, 243.675, 
// plane_transform: 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 
// model_transforms_vector: 1e-05, 0, 0, 0, 0, 1e-05, 0, 0, 0, 0, 1e-05, 0, -9.15527e-07, -6.10352e-08, 1.52588e-08, 1, 
// instance_names: virtualObj2, 
// model_paths: ../data/airbus/twoAirplaneBody.fb, 
// shader_type: 0
// background_transform: 1.03, 0, 0, 0, 0, 1.03, 0, 0, 0, 0, 1.03, 0, 1.03e+06, 1.03e+06, 1.03e+06, 1, 
// background_path: ../data/SBG(1).obj
// shader_type: 0
// right_hand_view_vector: 0.941480, -0.051212, 0.333155, 0.000000, 0.333346, -0.005021, -0.942791, 0.000000, 0.049955, 0.998675, 0.012344, 0.000000, -0.307542, -0.028693, 0.843575, 1.000000, 

        std::vector<vsg::dmat4> model_transforms_dmat4;
        for(int i = 0; i < model_transforms_vector.size(); i++)
            model_transforms_dmat4.push_back(VectorTodmat4(model_transforms_vector[i]));
        for(int i = 0; i < instance_names.size(); i++){
            instance_names_to_transforms[instance_names[i]] = model_transforms_dmat4[i];
        }
            // std::cout << "objtracking_shader: " << objtracking_shader << std::endl; 
        // std::cout << "width: " << width << std::endl; 
        // std::cout << "height: " << height << std::endl; 
        // std::cout << "upsample_scale: " << upsample_scale << std::endl; 
        // std::cout << "plane_transform: " << plane_transform[0][0] << std::endl; 
        // std::cout << "instance_names[0] = " << instance_names[0] << std::endl;
        // std::cout << "instance_names[1] = " << instance_names[1] << std::endl;
        // std::cout << "model_paths[0] = " << model_paths[0] << std::endl;
        // std::cout << "model_paths[1] = " << model_paths[1] << std::endl;
        // std::cout << "model_transforms.size() = " << model_transforms.size() << std::endl;
        // info("   model_transforms_dmat4[0] = ", model_transforms_dmat4[0]);
        // std::string a;
        // std::cin >> a;
        // info("   model_transforms_dmat4[1] = ", model_transforms_dmat4[1]);
        // info("   plane_transform = ", VectorTodmat4(plane_transform));
        std::cout << "prepare init" << std::endl;
        renderer.setWidthAndHeight(width, height, upsample_scale, encode_scale);
        // std::cout << "setKParameters";
        renderer.setKParameters(KParameters[0], KParameters[1], KParameters[2], KParameters[3]);
        // vsg::vec3 light_direction = vsg::normalize(vsg::vec3(-1.0, 0.2, -1.0));

        //todo
        std::string renderingDir = "../AREngine/Rendering/";

        std::cout << "initRenderer" << std::endl;
        CADMesh::scenes_json_path = renderingDir + "asset/data/json/Scenes.json";
        CADMesh::current_scene_id = sceneid;
        renderer.cull_mode_none_model_paths.insert("../data/airbus/twoAirplaneBody.fb");
        renderer.cull_mode_none_model_paths.insert("../data/airbus/twoAirplaneBody_white.fb");
        renderer.cull_mode_none_model_paths.insert("../data/airbus/window.fb");
        renderer.initRenderer(renderingDir, model_transforms_dmat4, model_paths, instance_names, VectorTodmat4(plane_transform));
        std::cout << "server init done!";
        // img = uchar(val % 256);

        // //���÷��ؽ������������ARModule::ProRemoteReturn�н��ղ����д���
        // proc->ret = {
        //     {"result",Image(img,".png")}
        // };
        init_done = true;
        bool i = true;
        std::cout<<"init done !!!!!!!!!!"<<std::endl;
        proc->ret = {
            {"init_done", Bytes(i)}
        };

    }

    // Mat img = frameDataPtr->image.front().clone();
    // CV_Assert(img.size()==Size(640,480) && img.at<uchar>(0, 0) == uchar(frameDataPtr->frameID % 256)); //��֤����ͼ���С������ֵ

    if (cmd == "drawCommand" && init_done)
    {
        // drawCommand();
        // if(frameDataPtr->frameID % 600 == 0){
        //     renderer.hdr_image_num = 2;
        //     renderer.updateEnvLighting();
        // }
        // else if(frameDataPtr->frameID % 600 == 300){
        //     renderer.hdr_image_num = 3;
        //     renderer.updateEnvLighting();
        // }
        // 日志文件（静态，只打开一次，持续写入）
        static std::ofstream log_file("env_log.txt", std::ios::out | std::ios::app);
        // 获取当前时间戳（方便区分每一帧）
        auto now1 = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now1);

        auto& appData = con.app->appData;
        static int last_environmentalState = -1;
        bool entered_if = false;

        if(appData->environmentalState != last_environmentalState)
        {
            entered_if = true;
            renderer.hdr_image_num = appData->environmentalState;
            renderer.updateEnvLighting();
        }

        // 每一帧都写入文件
        log_file << "[" << std::ctime(&now_time) << "]"
                << " 当前环境状态: " << appData->environmentalState
                << " | 上一帧状态: " << last_environmentalState
                << " | 是否进入if更新: " << (entered_if ? "是" : "否")
                << " | 当前HDR编号: " << renderer.hdr_image_num
                << std::endl;

        // 控制台输出（保留你原来的）
        if(entered_if){
            std::cout << "update env lighting, hdr_image_num: " << renderer.hdr_image_num << std::endl;
        }

        last_environmentalState = appData->environmentalState;
        auto t0 = std::chrono::high_resolution_clock::now();
        if (!decompressImages()) {
            std::cerr << "Failed to decompress images in drawCommand (server side)" << std::endl;
            return STATE_ERROR;
        }

        static auto last_time = std::chrono::steady_clock::now();
        static int frame_count = 0;
        
        frame_count++;
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time).count();
        auto now = std::chrono::system_clock::now();
        auto now_us = std::chrono::time_point_cast<std::chrono::microseconds>(now);
        auto us = now_us.time_since_epoch().count();
		// std::cout <<"time: " << tframe << std::endl;
		// std::cout << "time: " << std::fixed << std::setprecision(6) << frameDataPtr->frameID <<" at "<<us<< std::endl;
        
        // Update FPS every ~1 second
        if (elapsed_ms >= 1000) {
            float average_fps = frame_count * 1000.0f / elapsed_ms;
            gui::global_params->currentFps = average_fps;
            
            // Reset counters
            frame_count = 0;
            last_time = current_time;
        }
        
		auto rgb = frameDataPtr->image.front().clone();
        std::cout << "img size: " << rgb.cols << " " << rgb.rows << std::endl;
        if (frameDataPtr->image.size() > 1) {
            auto m = frameDataPtr->image[1].clone();
            if (!m.empty()) {
                std::cout << "save img..."<< std:: endl;
                rgb = m;
                // cv::imwrite("/home/lab/workspace/Final-AREgine/AR-MR-Algorithm-Engine/data/receive_rgbs/" + std::to_string(frameDataPtr->timestamp) + ".png", m);
        
            }
        }
        // cv::Mat resized;
        // cv::Size targetSize(1280, 960);
        // cv::resize(rgb, resized, targetSize, 0, 0, cv::INTER_LINEAR);
        // rgb = resized;
        cv::Mat depth;
        depth = frameDataPtr->depth.front().clone();
		// auto depth = frameDataPtr->depth.front().clone();
        // cv::Mat depth(rgb.size(), CV_16UC1, cv::Scalar(65535));


        std::cout << "colorCom size : " << frameDataPtr->compressedColor.size() << std::endl;
        std::cout << "color size : " << frameDataPtr->imgColor.cols << std::endl;
        // // //获取解压后的数据
		// auto compressedColor = frameDataPtr->colorCom.front();

        // std::cout << compressedColor.size() << std::endl;
        
        // // 解压缩彩色图像
        // cv::Mat  decompress = cv::imdecode(compressedColor, cv::IMREAD_UNCHANGED);
        // std::cout << "1" << std::endl;
        // if(decompress.empty()) {
            
        //     std::cout << "Failed to decompress color image" << std::endl;
        // }
        
        // // 解压缩深度图像
        // frame.imgDepth = decompressDepthImage(compressed.compressedDepth);
        // if(frame.imgDepth.empty()) {
        //     LOGE("Failed to decompress depth image");
        //     return FrameData();
        // }

        // std::cout << "【render frame id : "<< frameDataPtr->frameID << "】" <<std::endl;

		cv::Mat BGR;
        unsigned short* depth_data = reinterpret_cast<unsigned short*>(depth.data);
        cv::cvtColor(rgb, BGR, cv::COLOR_RGB2BGR);
        BGR = rgb.clone();
        std::vector<double> right_hand_view_vector = send["right_hand_view"].getv<double>();
        std::cout << "right_hand_view_vector: ";
        printVector(right_hand_view_vector);
        // for(int i = 0; i < 4; i++)
        // {
        //     for(int j = 0; j < 4; j++){
        //         std::cout << right_hand_view_vector[i * 4 + j] << " ";
        //     }
        //     std::cout << std::endl;
        // }
        double camera_timestamp = send["camera_timestamp"].get<double>();
        int measureType = send["measureType"].get<int>();
        vsg::dmat4 right_hand_view_dmat4 = VectorTodmat4(right_hand_view_vector);
        
        vsg::dmat4 rotation_transform = {
            1.0,  0.0,  0.0,  0.0,  // X轴不变

            0.0,  0.0,  1.0,  0.0,  // Y→Z
            0.0, -1.0,  0.0,  0.0,  // Z→-Y
            0.0,  0.0,  0.0,  1.0
        };
        
        // 应用旋转到视图矩阵
        vsg::dmat4 transformed_matrix = right_hand_view_dmat4 * rotation_transform;
        
        // std::cout << "1" << std::endl;
        renderer.updateCamera(right_hand_view_dmat4);
        // cv::putText(BGR, std::to_string(camera_timestamp), cv::Point(10, 50), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);  
        // std::cout << "2" << std::endl;
        renderer.setRealColorAndImage(BGR.data, depth_data);


        std::vector<std::vector<double>> model_transforms_vector = send["model_transforms"].getv<std::vector<double>>();
        std::vector<vsg::dmat4> model_transforms_dmat4;
        for(int i = 0; i < model_transforms_vector.size(); i++)
            model_transforms_dmat4.push_back(VectorTodmat4(model_transforms_vector[i]));
        std::vector<std::string> instance_names = send["instance_names"].getv<std::string>();
        for(int i = 0; i < instance_names.size(); i ++){
            vsg::info("update pose for instance ", instance_names[i], ": ", model_transforms_dmat4[i]);
        
            bool update = false;
            if(instance_names_to_transforms.find(instance_names[i]) != instance_names_to_transforms.end()){
                if(instance_names_to_transforms[instance_names[i]] != model_transforms_dmat4[i]){
                    update = true;
                    instance_names_to_transforms[instance_names[i]] = model_transforms_dmat4[i];
                }
            }
            else{
                update = true;
            }
            if(update){
                renderer.updateObjectPose(instance_names[i], model_transforms_dmat4[i]);
            }
        }
            
        // num += 0.002;
        // // PlaneData testPlaneData = createTestPlanes(num); 
        std::vector<std::vector<double>> plane_origin = send["m_plane_origins"].getv<std::vector<double>>();
        std::vector<std::vector<double>> plane_normals = send["m_plane_normals"].getv<std::vector<double>>();
        std::vector<std::vector<double>> plane_u = send["m_plane_u"].getv<std::vector<double>>();
        std::vector<std::vector<double>> plane_v = send["m_plane_v"].getv<std::vector<double>>();

        static std::vector<std::vector<double>> last_plane_origin;
        static std::vector<std::vector<double>> last_plane_normals;
        static std::vector<std::vector<double>> last_plane_u;
        static std::vector<std::vector<double>> last_plane_v;
        
        bool dataChanged = (plane_origin != last_plane_origin) ||
                           (plane_normals != last_plane_normals) ||
                           (plane_u != last_plane_u) ||
                           (plane_v != last_plane_v);
        if(dataChanged){
            if(plane_origin.size() > 0){
                PlaneData planeData;
        
                planeData.normals = plane_normals;
                planeData.origin = plane_origin;
                planeData.u = plane_u;
                planeData.v = plane_v;
                    
                printPlaneData(planeData.origin, planeData.normals, planeData.u, planeData.v);
                // std::string a;
                // std::cin >> a;
                if(planeData.normals.size()!=0)
                {
                    static float subdivisions = 0.1;
                    MeshData mesh = convertPlaneDataToWireframe(planeData, subdivisions);
                    float* vertices_pointer = static_cast<float*>(mesh.vertices->dataPointer(0));
                    size_t vertices_size = mesh.vertices->size() * 3;
                    uint32_t* indices_pointer = static_cast<uint32_t*>(mesh.indices->dataPointer(0));
                    size_t indices_size = mesh.indices->size();
                    renderer.addLineData(vertices_pointer, vertices_size, indices_pointer, indices_size);
                    renderer.addPointData(vertices_pointer, vertices_size, indices_pointer, indices_size);
        
                }
            }    
            else{//bu
                renderer.clearPointAndLineData();
            }

            last_plane_origin = plane_origin;
            last_plane_normals = plane_normals;
            last_plane_u = plane_u;
            last_plane_v = plane_v;
        }

        // switch (measureType)
        // {
        // case 0:
        //     /* code */
        //     break;
        // case 1:
        //     // Draw a point at the origin
        //     drawPoint(-0.0331644, -0.0318135, 0.404163);
        //     break;
        // case 2:
        //     // Draw a line between two points
        //     drawTwoPointsWithLine(-0.0331644, -0.0318135, 0.404163, 0.019051, 0.00408901, 0.4);
        //     break;
        // default:
        //     break;
        // }
        // drawTwoPointsWithLine(-0.0331644, -0.0318135, 0.404163, 0.019051, 0.00408901, 0.4);


         auto e1 = std::chrono::high_resolution_clock::now();

        std::cout << "3" << std::endl;
        renderer.render();

        std::vector<std::vector<uint8_t>> vPacket;
        std::cout << "render done!" << std::endl;
        renderer.getEncodeImage(vPacket);

         auto e3 = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> elapsed2=e3-e1;
		std::cout << "Rendering process time:"<<elapsed2.count()<<" seconds." << std::endl;
        // cv::Mat image(height * upsample_scale, width * upsample_scale, CV_8UC4, color_image.data());
        
        // cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
        // cv::imshow("rgb" , rgb);

        //���÷��ؽ������������ARModule::ProRemoteReturn�н��ղ����д���
        // auto current_time_draw = std::chrono::steady_clock::now();
        // auto elapsed_ms_draw = std::chrono::duration_cast<std::chrono::milliseconds>(current_time_draw - current_time).count();
        // last_time = current_time;
        // std::cout << "draw time: " << elapsed_ms_draw << " ms" << std::endl;
        proc->ret = {
            {"result",vPacket}
        };
        auto e0 = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> elapsed1=e0-t0;
		std::cout << "Rendering process time:"<<elapsed1.count()<<" seconds." << std::endl;
        std::cout << "[" << GetCurrentTimeWithMs() << "] Rendering server ends, frameId : "<<frameDataPtr->frameID << std::endl;
    }


    if (cmd == "initARCloudRenderer")
    {
        std::cout << "receive params" << std::endl;
        width = send["width"].get<int>();
        height = send["height"].get<int>();
        encode_scale = 2.8;
        upsample_scale = 2.8;
        std::vector<double> KParameters = send["KParameters"].getv<double>();
        std::vector<std::vector<double>> model_transforms_vector = send["model_transforms"].getv<std::vector<double>>();
        std::vector<std::string> instance_names = send["instance_names"].getv<std::string>();
        std::vector<std::string> model_paths = send["model_paths"].getv<std::string>();
        renderer.shadow_receiver_transform = vsg::dmat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1000000, 0, 0, 1);
        renderer.shadow_receiver_path = "../data/shadow_receiver2.obj";
        
        std::cout << "width: " << width << std::endl;
        std::cout << "height: " << height << std::endl;
        std::cout << "upsample_scale: " << upsample_scale << std::endl;
        std::cout << "KParameters: ";
        printVector(KParameters);
        std::cout << "model_transforms_vector: ";
        printVectorVector(model_transforms_vector);
        std::cout << "instance_names: ";
        printVector(instance_names);
        std::cout << "model_paths: ";
        printVector(model_paths);
        std::vector<vsg::dmat4> model_transforms_dmat4;
        for(int i = 0; i < model_transforms_vector.size(); i++)
            model_transforms_dmat4.push_back(VectorTodmat4(model_transforms_vector[i]));
        std::cout << "prepare init" << std::endl;
        renderer.setWidthAndHeight(width, height, upsample_scale, encode_scale);
        renderer.setKParameters(KParameters[0], KParameters[1], KParameters[2], KParameters[3]);

        for(int i = 0; i < instance_names.size(); i++){
            instance_names_to_transforms[instance_names[i]] = model_transforms_dmat4[i];
        }

        //todo
        std::string renderingDir = "../AREngine/Rendering/";

        std::cout << "initRenderer" << std::endl;
        CADMesh::scenes_json_path = renderingDir + "asset/data/json/Scenes.json";
        CADMesh::current_scene_id = 6;
        renderer.cull_mode_none_model_paths.insert("../data/airbus/twoAirplaneBody.fb");
        renderer.cull_mode_none_model_paths.insert("../data/airbus/twoAirplaneBody_white.fb");
        renderer.cull_mode_none_model_paths.insert("../data/airbus/window.fb");
        renderer.initRenderer(renderingDir, model_transforms_dmat4, model_paths, instance_names, vsg::dmat4());
        std::cout << "server init done!";

        init_done = true;
        bool i = true;
        std::cout<<"init done !!!!!!!!!!"<<std::endl;
        proc->ret = {
            {"init_done", Bytes(i)}
        };

    }

    // Mat img = frameDataPtr->image.front().clone();
    // CV_Assert(img.size()==Size(640,480) && img.at<uchar>(0, 0) == uchar(frameDataPtr->frameID % 256)); //��֤����ͼ���С������ֵ

    if (cmd == "drawARCommand" && init_done)
    {
        // std::ofstream log_file("send_data_log.txt", std::ios::app);
        // if (!log_file.is_open()) {
        //     std::cerr << "无法打开日志文件！" << std::endl;
        //     return STATE_ERROR;
        // }
    
        // // 记录当前帧
        // log_file << "============================================" << std::endl;
        // log_file << "【新的一帧】frameID: " << (frameDataPtr ? frameDataPtr->frameID : -1) << std::endl;
        // log_file << "============================================" << std::endl;
    
        vsg::info("get image");
        vsg::info("frameDataPtr: ", frameDataPtr);
        if(!frameDataPtr){
            // log_file.close(); // 记得关闭文件
            return STATE_OK;
        }
        if(!proc->send.has("view") || !proc->send.has("model_transforms") || !proc->send.has("instance_names")){
            // log_file.close();
            return STATE_OK;
        }
    
        vsg::info("frameDataPtr->image.size(): ", frameDataPtr->image.size());
        auto rgb = frameDataPtr->image.front().clone();
        vsg::info("image get done");
    
        // ---------------------- view 矩阵 ----------------------
        vsg::info("get view");
        std::vector<double> right_hand_view_vector = send["view"].getv<double>();
        vsg::info("view get done");
    
        // // 【日志：view】
        // log_file << "\n[1] view 矩阵数据" << std::endl;
        // log_file << "size: " << right_hand_view_vector.size() << std::endl;
        // for (int i = 0; i < right_hand_view_vector.size(); ++i) {
        //     log_file << "索引 [" << i << "] = " << right_hand_view_vector[i] << std::endl;
        // }
    
        if(right_hand_view_vector.size() != 16){
            // log_file.close();
            return STATE_OK;
        }
    
        // ---------------------- model_transforms ----------------------
        vsg::info("get model_transforms");
        std::vector<std::vector<double>> model_transforms_vector = send["model_transforms"].getv<std::vector<double>>();
        vsg::info("model_transforms get done");
    
        // 【日志：model_transforms】
        // log_file << "\n[2] model_transforms 数据" << std::endl;
        // log_file << "外层 size: " << model_transforms_vector.size() << std::endl;
        // for (int i = 0; i < model_transforms_vector.size(); ++i) {
        //     log_file << "  索引 [" << i << "] 内部 size: " << model_transforms_vector[i].size() << std::endl;
        //     for (int j = 0; j < model_transforms_vector[i].size(); ++j) {
        //         log_file << "    子索引 [" << i << "][" << j << "] = " << model_transforms_vector[i][j] << std::endl;
        //     }
        // }
    
        std::vector<vsg::dmat4> model_transforms_dmat4;
    
        // ---------------------- instance_names ----------------------
        vsg::info("get instance_names");
        std::vector<std::string> instance_names = send["instance_names"].getv<std::string>();
        vsg::info("instance_names get done");
    
        // // 【日志：instance_names】
        // log_file << "\n[3] instance_names 数据" << std::endl;
        // log_file << "size: " << instance_names.size() << std::endl;
        // for (int i = 0; i < instance_names.size(); ++i) {
        //     log_file << "索引 [" << i << "] = " << instance_names[i] << std::endl;
        // }
        // log_file << "\n===========================================================\n\n" << std::endl;
    
        // // 关闭文件
        // log_file.close();

        // cv::Mat rgb(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));  // 高480，宽640
		cv::Mat BGR;
        cv::cvtColor(rgb, BGR, cv::COLOR_RGB2BGR);
        BGR = rgb.clone();
        cv::Mat depth(480, 640, CV_16UC1, cv::Scalar(65535));
        unsigned char* color_data = BGR.data;            
        unsigned short* depth_data = reinterpret_cast<unsigned short*>(depth.data);
        renderer.setRealColorAndImage(color_data, depth_data);

        vsg::dmat4 right_hand_view_dmat4 = VectorTodmat4(right_hand_view_vector);
        
        vsg::info("right_hand_view_dmat4 = ", right_hand_view_dmat4);
        // std::cout << "1" << std::endl;
        renderer.updateCamera(right_hand_view_dmat4);
        vsg::info("updateCamera ");
        for(int i = 0; i < model_transforms_vector.size(); i++){
            model_transforms_dmat4.push_back(VectorTodmat4(model_transforms_vector[i]));
            vsg::info("model_transforms_dmat4[i] = ", model_transforms_dmat4[i]);
        }
        for(int i = 0; i < instance_names.size(); i ++){
            vsg::info("update pose for instance ", instance_names[i], ": ", model_transforms_dmat4[i]);
        
            bool update = false;
            if(instance_names_to_transforms.find(instance_names[i]) != instance_names_to_transforms.end()){
                if(instance_names_to_transforms[instance_names[i]] != model_transforms_dmat4[i]){
                    update = true;
                    instance_names_to_transforms[instance_names[i]] = model_transforms_dmat4[i];
                }
            }
            else{
                update = true;
            }
            if(update){
                renderer.updateObjectPose(instance_names[i], model_transforms_dmat4[i]);
            }
        }
        // saveAllInstancePoses();
        std::cout << "3" << std::endl;
        renderer.render();

        // std::vector<std::vector<uint8_t>> vPacket;
        std::cout << "render done!" << std::endl;

        // auto e3 = std::chrono::high_resolution_clock::now();

		// std::chrono::duration<double> elapsed2=e3-e1;
		// std::cout << "Rendering process time:"<<elapsed2.count()<<" seconds." << std::endl;
        // auto e0 = std::chrono::high_resolution_clock::now();
		// std::chrono::duration<double> elapsed1=e0-t0;
		// std::cout << "Rendering process time:"<<elapsed1.count()<<" seconds." << std::endl;
        // std::cout << "[" << GetCurrentTimeWithMs() << "] Rendering server ends, frameId : "<<frameDataPtr->frameID << std::endl;
    }

    
	if (cmd == "HDRSwitch"){
		auto& appData = con.app->appData;
		appData->environmentalState = send.getd<int>("environmentalState", 0);
        if(appData->environmentalState != renderer.hdr_image_num)
        {
            renderer.hdr_image_num = appData->environmentalState;
            renderer.updateEnvLighting();
            std::cout << "update env lighting, hdr_image_num: " << renderer.hdr_image_num << std::endl;
        }
		std::cout << "appData:environmentalState" << appData->environmentalState << std::endl;
    }

    // std::cout << "num: " << num << std::endl;
    // num++;
    // if(num == 200)
    // {
    //     renderer.hdr_image_num = 2;
    //     renderer.updateEnvLighting();
    //     std::cout << "update env lighting, hdr_image_num: " << renderer.hdr_image_num << std::endl;
    // }
    // else if(num == 400)
    // {
    //     num = 0;
    //     renderer.hdr_image_num = 3;
    //     renderer.updateEnvLighting();
    //     std::cout << "update env lighting, hdr_image_num: " << renderer.hdr_image_num << std::endl;
    // }

    // if (cmd == "add")
    // {
    //     int val = send.getd<int>("val", 0);

    //     img = uchar((img.at<uchar>(0,0)+val) % 256);

    //     //���÷��ؽ������������ARModule::ProRemoteReturn�н��ղ����д���
    //     proc->ret = {
    //         {"result",Image(img,".png")}
    //     };
    // }

    return STATE_OK;
}

vsg::dmat4 RenderingServer::VectorTodmat4(std::vector<double>& vector){
    vsg::dmat4 dmat4_matrix = vsg::dmat4();
    for(int i = 0; i < 4; i ++)
        for(int j = 0; j < 4; j ++)
            dmat4_matrix[i][j] = vector[i * 4 + j];
    return dmat4_matrix;
}

void RenderingServer::drawPoint(float x, float y, float z) {
    // 1. 定义单个点的顶点数据（XYZ坐标）
    float vertices[] = {x, y, z};
    size_t vertices_size = 3; // 1个点 * 3个坐标分量

    // 2. 定义点的索引（仅需一个索引0）
    uint32_t point_indices[] = {0};
    size_t point_indices_size = 1;

    // 3. 调用渲染器接口（仅传递点数据）
    renderer.addPointData(vertices, vertices_size, point_indices, point_indices_size);
}

void RenderingServer::drawTwoPointsWithLine(float x1, float y1, float z1, float x2, float y2, float z2) {
    // 1. 定义顶点数据（两个点）
    float vertices[] = {x1, y1, z1, x2, y2, z2};
    size_t vertices_size = 6; // 两个点 * 3坐标分量

    // 2. 定义线段索引（连接两个点）
    uint32_t line_indices[] = {0, 1};
    size_t line_indices_size = 2;

    // 3. 定义点索引（绘制两个点）
    uint32_t point_indices[] = {0, 1};
    size_t point_indices_size = 2;

    // 4. 调用渲染器接口
    renderer.addLineData(vertices, vertices_size, line_indices, line_indices_size);
    renderer.addPointData(vertices, vertices_size, point_indices, point_indices_size);
}

bool RenderingServer::decompressImages() {
    // if (compressedColor.empty()) return false;

    // // 解压缩RGB图像
    // imgColor = cv::imdecode(compressedColor, cv::IMREAD_COLOR);

    // // 解压缩深度图像
    // if (!compressedDepth.empty()) {
    //     imgDepth = cv::imdecode(compressedDepth, cv::IMREAD_ANYDEPTH);
    // }

    // return !imgColor.empty();
    return true;
}
