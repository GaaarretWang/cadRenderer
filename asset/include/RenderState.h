#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <vsg/all.h>

#include "JsonConfigManager.h"
#include "MyMask.h"

struct SceneRuntimeState
{
    int hdr_image_num = 4;
    int hdr_image_max_num = 7;
    int enable_real_depth_occlusion = 0;
    int shadow_mode = SHADOW_RECEIVER_PLANE;
    int shadow_type = 1;
    float baseBrightness = 2.0f;
    float ssao_radius = 0.1f;
    int ssao_kernel_size = 64;
    float exposure = 8.0f;
    int denoise_size = 5;
    float shadow_bias = 0.0001f;
    int blocker_sample_num = 16;
    int pcf_sample_num = 16;
    float pcf_softness = 1.0f;
    float pcss_softness = 1.0f;
    float pcss_softness_falloff = 1.0f;
    vsg::vec3 line_color = vsg::vec3(1.0f, 1.0f, 1.0f);
    vsg::vec3 point_color = vsg::vec3(1.0f, 1.0f, 1.0f);
    std::unordered_map<int, float> hdr_base_brightness;
    std::vector<SceneModelTransformSave> scene_transforms;
};

struct RenderPerformanceState
{
    float current_fps = 0.0f;
    std::array<float, 2> render_server_times = {0.0f, 0.0f};
    std::array<float, 8> render_func_times = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
};

struct SceneInitPayload
{
    std::string rendering_dir;
    std::vector<vsg::dmat4> model_transforms;
    std::vector<std::string> model_paths;
    std::vector<std::string> instance_names;
    std::string shadow_receiver_path;
    vsg::dmat4 shadow_receiver_transform = vsg::dmat4();
    VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_4_BIT;
    std::unordered_set<std::string> cull_mode_none_model_paths;
};

struct CameraFrameInput
{
    std::array<double, 9> lookat = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    unsigned char* color = nullptr;
    unsigned short* depth = nullptr;
};

using SceneRuntimeStatePtr = std::shared_ptr<SceneRuntimeState>;
