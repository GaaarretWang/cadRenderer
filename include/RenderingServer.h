#pragma once

#include <array>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <vsgRendererServer.h>

#include "SceneFrameProvider.h"
#include "SceneConfigSerializer.h"
#include "SceneInitAssembler.h"

class RenderingServer {
public:
    vsgRendererServer renderer;
    int width = 640;
    int height = 480;
    bool cameara_pos_bool = true;
    bool stop_cameara_pos = true;
    std::array<double, 9> lookat_vector = {0.000588, 0.739846, -0.903124, 0.087284, 1.51642, -0.279094, 0.163545, -0.628984, 0.760021};
    float fx = 386.52199190267083;
    float fy = 387.32300428823663;
    float cx = 326.5103569741365;
    float cy = 237.40293732598795;
    std::string rendering_dir = "../";
    double upsample_scale = 2;
    double encode_scale = 2;

    vsg::ref_ptr<vsg::Device> device;
    std::vector<std::vector<uint8_t>> vPacket;

private:
    bool loadSceneFromJSON(const std::string& scene_name_or_id);
    void clearSceneData();
    void applyFrameData(const FrameData& frame_data);

    SceneConfig loaded_scene_config;
    int loaded_scene_id = -1;
    SceneFrameProvider frame_provider;
    SceneInitPayload scene_payload_;
    std::shared_ptr<JsonConfigManager> json_manager_;
    std::shared_ptr<SceneConfigSerializer> scene_serializer_;

public:
    RenderingServer();
    ~RenderingServer();

    int Init(int argc, char** argv);
    int Update();
};
