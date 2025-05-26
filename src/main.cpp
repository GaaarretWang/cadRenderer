#define STB_IMAGE_IMPLEMENTATION
#include "Rendering.h"
#include "RenderingServer.h"
#include <iostream>
#include <vsg/all.h>
#include <CADMesh.h>
#include "communication/dataInterface.h"
#include <string>
#include <chrono>
#include <fstream>
#include <vector>
#include <thread>
#include <future>
#include <memory>
#include <stdexcept>

// simplelogger::Logger* logger = simplelogger::LoggerFactory::CreateConsoleLogger();

ImagePair loadImagePair(const std::string& timestamp, ConvertImage* converter) {
    try {
        // 加载颜色图像
        std::ifstream color_file("../asset/data/dataset3/color/" + timestamp + ".png", std::ios::binary);
        if (!color_file) throw std::runtime_error("Failed to open color file");
        std::vector<uint8_t> color_buffer((std::istreambuf_iterator<char>(color_file)), 
                            std::istreambuf_iterator<char>());
        
        // 加载深度图像
        std::ifstream depth_file("../asset/data/dataset3/depth/" + timestamp + ".png", std::ios::binary);
        if (!depth_file) throw std::runtime_error("Failed to open depth file");
        std::vector<uint8_t> depth_buffer((std::istreambuf_iterator<char>(depth_file)), 
                            std::istreambuf_iterator<char>());
        
        // 转换图像数据
        std::string color_str(color_buffer.begin(), color_buffer.end());
        std::string depth_str(depth_buffer.begin(), depth_buffer.end());
        
        ImagePair result;
        result.color.reset(converter->convertColor(color_str));
        result.depth.reset(converter->convertDepth(depth_str));
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Error loading " << timestamp << ": " << e.what() << std::endl;
        return {nullptr, nullptr};
    }
}

int main(int argc, char** argv){
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

    RenderingServer rendering_server;
    Rendering rendering_client;
    rendering_server.Init(argc, argv);
    rendering_client.Init(rendering_server.device);
    ConvertImage *convert_image = new ConvertImage(rendering_server.width, rendering_server.height);

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
                batch_results.push_back(loadImagePair(camera_pos_timestamp[i], convert_image));
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
    while(true){
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        rendering_server.color_path = "../asset/data/dataset3/color/" + camera_pos_timestamp[frame] + ".png";
        rendering_server.depth_path = "../asset/data/dataset3/depth/" + camera_pos_timestamp[frame] + ".png";
        rendering_server.lookat_vector = camera_pos[frame];
        
        // 增量帧计数器
        frameCount++;

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

        rendering_server.Update();
        if(rendering_server.vPacket.size() > 0)
            rendering_client.Update(rendering_server.vPacket);
        frame++;
        if(frame >= num_images)
            frame = 0;
    }
}