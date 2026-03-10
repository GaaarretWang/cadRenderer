#include "Rendering.h"
#include "RenderingServer.h"
#include <iostream>
#include <vsg/all.h>
#include <CADMesh.h>
#include "communication/dataInterface.h"
#include "ImageUtils.h"
#include <string>
#include <chrono>
#include <fstream>
#include <vector>
#include <thread>
#include <future>
#include <memory>
#include <stdexcept>
// #define RENDER_TEST
// simplelogger::Logger* logger = simplelogger::LoggerFactory::CreateConsoleLogger();

ImagePair loadImagePair(const std::string& timestamp, int width, int height) {
    try {
        // 加载颜色图像
        // std::ifstream color_file("../asset/data/dataset3/resized_factory.png", std::ios::binary);
        std::ifstream color_file("../asset/data/dataset3/1711699289.885392.png", std::ios::binary);
        // std::ifstream color_file("../asset/data/dataset3/color/" + timestamp + ".png", std::ios::binary);
        if (!color_file) throw std::runtime_error("Failed to open color file");
        std::vector<uint8_t> color_buffer((std::istreambuf_iterator<char>(color_file)),
                            std::istreambuf_iterator<char>());

        // 加载深度图像
        // std::ifstream depth_file("../asset/data/dataset3/black_depth_1280x960.png", std::ios::binary);
        std::ifstream depth_file("../asset/data/dataset3/depth1711699289.885392.png", std::ios::binary);
        // std::ifstream depth_file("../asset/data/dataset3/depth/" + timestamp + ".png", std::ios::binary);
        if (!depth_file) throw std::runtime_error("Failed to open depth file");
        std::vector<uint8_t> depth_buffer((std::istreambuf_iterator<char>(depth_file)),
                            std::istreambuf_iterator<char>());

        // 转换图像数据
        std::string color_str(color_buffer.begin(), color_buffer.end());
        std::string depth_str(depth_buffer.begin(), depth_buffer.end());

        ImagePair result;
        int w = width, h = height;
        result.color.reset(ImageUtils::convertColor(color_str, w, h));
        result.depth.reset(ImageUtils::convertDepth(depth_str, w, h));

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Error loading " << timestamp << ": " << e.what() << std::endl;
        return {nullptr, nullptr};
    }
}

int main(int argc, char** argv){
    // 手动解析PNG测试参数（不使用vsg::CommandLine，避免消费其他模块需要的参数）
    bool save_ref = false;
    bool compare_ref = false;
    double diff_threshold = 0.01; // 差异率阈值，默认1%
    std::string scene_id_str = "0";
    for(int i = 1; i < argc; i++){
        std::string arg = argv[i];
        if(arg == "--save-ref") save_ref = true;
        else if(arg == "--compare-ref") compare_ref = true;
        else if(arg == "--diff-threshold" && i + 1 < argc) diff_threshold = std::stod(argv[++i]);
        else if((arg == "--scene" || arg == "-s") && i + 1 < argc) scene_id_str = argv[i + 1]; // 不消费，留给RenderingServer
    }

    // 解析命令行参数
    vsg::CommandLine arguments(&argc, argv);
    int max_frames = 0; // 0表示无限运行
    arguments.read("--frames", max_frames);
    arguments.read("-f", max_frames);

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

    double render_scale = 2.8;
    double encode_scale = 2.8;
    RenderingServer rendering_server;
    rendering_server.upsample_scale = render_scale;
    rendering_server.encode_scale = encode_scale;
    rendering_server.Init(argc, argv);
#ifndef RENDER_TEST
    Rendering rendering_client;
    rendering_client.upsample_scale = encode_scale;
    rendering_client.Init(rendering_server.device);
#endif

    int num_images = camera_pos.size();
    // 确定并行线程数 (不超过硬件支持的核心数)
    const unsigned int num_threads = std::min<unsigned int>(
        std::thread::hardware_concurrency(),
        num_images
    );

    // 分批处理图像
    std::vector<std::future<std::vector<ImagePair>>> futures;
    const int batch_size = (num_images + num_threads - 1) / num_threads;

    for (unsigned int t = 0; t < num_threads; ++t) {
        futures.emplace_back(std::async(std::launch::async, [&, t] {
            std::vector<ImagePair> batch_results;
            const int start = t * batch_size;
            const int end = std::min(start + batch_size, num_images);

            for (int i = start; i < end; ++i) {
                batch_results.push_back(loadImagePair(camera_pos_timestamp[i], rendering_server.width, rendering_server.height));
            }
            return batch_results;
        }));
    }
    
    // 收集结果
    for (auto& future : futures) {
        auto batch = future.get();
        rendering_server.all_images.insert(rendering_server.all_images.end(), 
                         std::make_move_iterator(batch.begin()),
                         std::make_move_iterator(batch.end()));
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    int frameCount = 0;
    int total_frames_rendered = 0;
    while((max_frames <= 0 || total_frames_rendered < max_frames)){
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        rendering_server.lookat_vector = camera_pos[frame];
        
        // 增量帧计数器
        frameCount++;
        total_frames_rendered++;

        // 计算经过的时间
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsedTime = currentTime - startTime;

        // 检查是否已过一秒
        if (elapsedTime.count() >= 1.0f)
        {
            // 计算 FPS
            float fps = frameCount / elapsedTime.count();
            gui::global_params->currentFps = fps;

            // 将FPS输出到控制台
            std::cout << "FPS: " << fps << std::endl;

            // 下一秒重置
            frameCount = 0;
            startTime = currentTime;
        }
        
        if(rendering_server.Update() == -1){
            std::cout << "end" << std::endl;
            return 0;
        }

#ifndef RENDER_TEST
        if(rendering_server.vPacket.size() > 0)
            rendering_client.Update(rendering_server.vPacket);
#endif

        frame++;
        if(frame >= num_images)
            frame = 0;
    }

    // 达到指定帧数后，执行PNG参考图保存/对比
    if(save_ref || compare_ref){
        int rw = rendering_server.renderer.render_width;
        int rh = rendering_server.renderer.render_height;
        std::vector<uint8_t> window_image(rw * rh * 4);
        uint8_t* img_ptr = window_image.data();
        rendering_server.renderer.getWindowImage(img_ptr);

        std::string ref_dir = "../test_references";
        std::string ref_path = ref_dir + "/scene_" + scene_id_str + ".png";

        if(save_ref){
            bool ok = ImageUtils::savePNG(ref_path, window_image.data(), rw, rh, 4);
            if(ok){
                std::cout << "Reference saved: " << ref_path << std::endl;
            } else {
                std::cerr << "Failed to save reference: " << ref_path << std::endl;
                return 1;
            }
        }

        if(compare_ref){
            auto cmp = ImageUtils::compareWithRef(window_image.data(), rw, rh, 4, ref_path);
            std::cout << "Compare result: " << cmp.message << std::endl;
            if(!cmp.valid){
                std::cerr << "FAIL: comparison invalid" << std::endl;
                return 1;
            }
            if(cmp.diff_ratio > diff_threshold){
                std::cerr << "FAIL: diff ratio " << (cmp.diff_ratio * 100.0) << "% exceeds threshold "
                          << (diff_threshold * 100.0) << "%" << std::endl;
                return 1;
            } else {
                std::cout << "PASS: diff ratio " << (cmp.diff_ratio * 100.0) << "% within threshold "
                          << (diff_threshold * 100.0) << "%" << std::endl;
            }
        }
    }

    // 达到指定帧数，程序停止
    std::cout << "Program stopped after " << total_frames_rendered << " frames" << std::endl;
    return 0;
}