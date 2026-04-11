#include "SceneInitAssembler.h"

VkSampleCountFlagBits SceneInitAssembler::toSampleCount(int msaa)
{
    switch (msaa)
    {
        case 1: return VK_SAMPLE_COUNT_1_BIT;
        case 2: return VK_SAMPLE_COUNT_2_BIT;
        case 4: return VK_SAMPLE_COUNT_4_BIT;
        case 8: return VK_SAMPLE_COUNT_8_BIT;
        default: return VK_SAMPLE_COUNT_4_BIT;
    }
}

SceneInitPayload SceneInitAssembler::buildSceneInitPayload(const SceneConfig& scene_config, const std::string& rendering_dir)
{
    SceneInitPayload payload;
    payload.rendering_dir = rendering_dir;
    payload.shadow_receiver_path = rendering_dir + scene_config.shadow_receiver_path;
    payload.shadow_receiver_transform = scene_config.shadow_receiver_transform;
    payload.msaa_samples = toSampleCount(scene_config.msaa);

    for (const auto& model : scene_config.models)
    {
        payload.model_paths.push_back(rendering_dir + model.path);
        payload.model_transforms.push_back(model.transform);
        payload.instance_names.push_back(model.instance_name);
    }

    const bool is_legacy_airplane_scene = scene_config.id == 0 || scene_config.name == "airplane_parts";
    if (is_legacy_airplane_scene)
    {
        payload.cull_mode_none_model_paths.insert(rendering_dir + "asset/data/geos/1105/twoAirplaneBody.fb");
        payload.cull_mode_none_model_paths.insert(rendering_dir + "asset/data/geos/1105/twoAirplaneBody_white.fb");
        payload.cull_mode_none_model_paths.insert(rendering_dir + "asset/data/geos/1105/window.fb");
    }

    for (const auto& model_path : scene_config.cull_mode_none_model_paths)
    {
        payload.cull_mode_none_model_paths.insert(rendering_dir + model_path);
    }
    return payload;
}

CameraFrameInput SceneInitAssembler::buildCameraFrameInput(const FrameData& frame_data)
{
    CameraFrameInput input;
    input.lookat = frame_data.lookat;
    input.color = frame_data.image.color.get();
    input.depth = frame_data.image.depth.get();
    return input;
}
