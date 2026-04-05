#include "RenderingServer.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace
{
VkSampleCountFlagBits toSampleCount(int msaa)
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
}

RenderingServer::RenderingServer() = default;

RenderingServer::~RenderingServer() = default;

bool RenderingServer::loadSceneFromJSON(const std::string& scene_name_or_id)
{
    JsonConfigManager config_manager(
        rendering_dir + "asset/data/json/Scenes.json",
        rendering_dir + "asset/data/json/Materials.json",
        rendering_dir + "asset/data/json/LightInfo.json");

    std::string error_message;
    if (!config_manager.loadSceneConfig(scene_name_or_id, loaded_scene_config, &error_message))
    {
        vsg::error(error_message);
        return false;
    }

    clearSceneData();
    loaded_scene_id = loaded_scene_config.id;

    for (const auto& model : loaded_scene_config.models)
    {
        model_paths.push_back(rendering_dir + model.path);
        model_transforms.push_back(model.transform);
        instance_names.push_back(model.instance_name);
    }

    renderer.shadow_receiver_path = rendering_dir + loaded_scene_config.shadow_receiver_path;
    renderer.shadow_receiver_transform = loaded_scene_config.shadow_receiver_transform;
    renderer.msaaSamples = toSampleCount(loaded_scene_config.msaa);

    if (!frame_provider.initialize(loaded_scene_config, rendering_dir, width, height, &error_message))
    {
        vsg::error(error_message);
        return false;
    }

    stop_cameara_pos = frame_provider.isStatic();
    cameara_pos_bool = true;

    vsg::info("Scene '", scene_name_or_id, "' loaded: models=", model_paths.size(), ", frames=", frame_provider.frameCount());
    return true;
}

void RenderingServer::applyFrameData(const FrameData& frame_data)
{
    lookat_vector = frame_data.lookat;

    vsg::dvec3 centre = {lookat_vector[0], lookat_vector[1], lookat_vector[2]};
    vsg::dvec3 eye = {lookat_vector[3], lookat_vector[4], lookat_vector[5]};
    vsg::dvec3 up = {lookat_vector[6], lookat_vector[7], lookat_vector[8]};
    renderer.updateCamera(centre, eye, up);

    renderer.setRealColorAndImage(frame_data.image.color.get(), frame_data.image.depth.get());
}

int RenderingServer::Init(int argc, char** argv)
{
    renderer.setWidthAndHeight(width, height, upsample_scale, encode_scale);
    renderer.setKParameters(fx, fy, cx, cy);

    vsg::CommandLine arguments(&argc, argv);

    std::string scene_to_load = "0";
    arguments.read("--scene", scene_to_load);
    arguments.read("-s", scene_to_load);

    if (!loadSceneFromJSON(scene_to_load))
    {
        vsg::error("Failed to load scene '", scene_to_load, "', program will exit");
        return -1;
    }

    CADMesh::scenes_json_path = rendering_dir + "asset/data/json/Scenes.json";
    CADMesh::current_scene_id = loaded_scene_id;

    renderer.cull_mode_none_model_paths.insert(rendering_dir + "asset/data/geos/1105/twoAirplaneBody.fb");
    renderer.cull_mode_none_model_paths.insert(rendering_dir + "asset/data/geos/1105/twoAirplaneBody_white.fb");
    renderer.cull_mode_none_model_paths.insert(rendering_dir + "asset/data/geos/1105/window.fb");

    renderer.initRenderer(rendering_dir, model_transforms, model_paths, instance_names, vsg::dmat4());
    if (frame_provider.hasFrame())
    {
        applyFrameData(frame_provider.currentFrame());
    }

    device = renderer.device;
    return 0;
}

int RenderingServer::Update()
{
    if (cameara_pos_bool)
    {
        if (!frame_provider.hasFrame())
        {
            vsg::error("No frame data available");
            return -1;
        }

        applyFrameData(frame_provider.currentFrame());

        static auto last_frame_time = std::chrono::steady_clock::now();
        auto current_time = std::chrono::steady_clock::now();
        auto frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_frame_time).count();
        if (frame_time < 33)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(33 - frame_time));
        }
        current_time = std::chrono::steady_clock::now();
        last_frame_time = current_time;

        if (!frame_provider.isStatic())
        {
            frame_provider.advance();
        }
        if (stop_cameara_pos)
        {
            cameara_pos_bool = false;
        }
    }

    auto startRender = std::chrono::high_resolution_clock::now();
    if (!renderer.render())
        return -1;
    auto endRender = std::chrono::high_resolution_clock::now();
    gui::global_params->render_server_times[0] = std::chrono::duration<double, std::milli>(endRender - startRender).count();

    auto startEncode = std::chrono::high_resolution_clock::now();
    renderer.getEncodeImage(vPacket);
    auto endEncode = std::chrono::high_resolution_clock::now();
    gui::global_params->render_server_times[1] = std::chrono::duration<double, std::milli>(endEncode - startEncode).count();

    return 0;
}

void RenderingServer::clearSceneData()
{
    model_paths.clear();
    model_transforms.clear();
    instance_names.clear();
    frame_provider.reset();
}
