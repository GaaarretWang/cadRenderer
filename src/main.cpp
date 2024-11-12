#define STB_IMAGE_IMPLEMENTATION
#include "Rendering.h"
#include "RenderingServer.h"
#include <iostream>
#include <vsg/all.h>
#include <CADMesh.h>
#include "communication/dataInterface.h"
#include <string>
simplelogger::Logger* logger = simplelogger::LoggerFactory::CreateConsoleLogger();

int main(int argc, char** argv){
    global_argc = argc;
    global_argv = argv;
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
    rendering_server.Init();
    Rendering rendering_client;
    rendering_client.Init(rendering_server.device);
    while(true){
        rendering_server.color_path = "../asset/data/dataset3/color/" + camera_pos_timestamp[frame] + ".png";
        rendering_server.depth_path = "../asset/data/dataset3/depth/" + camera_pos_timestamp[frame] + ".png";
        rendering_server.lookat_vector = camera_pos[frame];
        rendering_server.Update();
        if(rendering_server.vPacket.size() > 0)
            std::cout << rendering_server.vPacket[0].size() << std::endl;

        rendering_client.Update(rendering_server.vPacket);
        frame++;
        if(frame > 698)
            frame = 0;
    }
}