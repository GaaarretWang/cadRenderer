#include "RenderingServer.h"

#include <algorithm>
#include <chrono>
#include <thread>

RenderingServer::RenderingServer()
{
    json_manager_ = std::make_shared<JsonConfigManager>(
        rendering_dir + "asset/data/json/Scenes.json",
        rendering_dir + "asset/data/json/Materials.json",
        rendering_dir + "asset/data/json/LightInfo.json");
    scene_serializer_ = std::make_shared<SceneConfigSerializer>(json_manager_);
}

RenderingServer::~RenderingServer() = default;

bool RenderingServer::loadSceneFromJSON(const std::string& scene_name_or_id)
{
    std::string error_message;
    if (!scene_serializer_->loadSceneConfig(scene_name_or_id, loaded_scene_config, &error_message))
    {
        vsg::error(error_message);
        return false;
    }

    clearSceneData();
    loaded_scene_id = loaded_scene_config.id;
    scene_payload_ = SceneInitAssembler::buildSceneInitPayload(loaded_scene_config, rendering_dir);
    renderer.shadow_receiver_path = scene_payload_.shadow_receiver_path;
    renderer.shadow_receiver_transform = scene_payload_.shadow_receiver_transform;
    renderer.msaaSamples = scene_payload_.msaa_samples;
    renderer.cull_mode_none_model_paths = scene_payload_.cull_mode_none_model_paths;

    if (!frame_provider.initialize(loaded_scene_config, rendering_dir, width, height, &error_message))
    {
        vsg::error(error_message);
        return false;
    }

    stop_cameara_pos = frame_provider.isStatic();
    cameara_pos_bool = true;

    vsg::info("Scene '", scene_name_or_id, "' loaded: models=", scene_payload_.model_paths.size(), ", frames=", frame_provider.frameCount());
    return true;
}

void RenderingServer::applyFrameData(const FrameData& frame_data)
{
    lookat_vector = frame_data.lookat;
    renderer.applyFrameInput(SceneInitAssembler::buildCameraFrameInput(frame_data));
}

int RenderingServer::Init(const std::string& scene_name_or_id)
{
    renderer.setWidthAndHeight(width, height, upsample_scale, encode_scale);
    renderer.setKParameters(fx, fy, cx, cy);

    if (!loadSceneFromJSON(scene_name_or_id))
    {
        vsg::error("Failed to load scene '", scene_name_or_id, "', program will exit");
        return -1;
    }

    CADMesh::scenes_json_path = rendering_dir + "asset/data/json/Scenes.json";
    CADMesh::current_scene_id = loaded_scene_id;

    renderer.initRenderer(rendering_dir, scene_payload_.model_transforms, scene_payload_.model_paths, scene_payload_.instance_names, vsg::dmat4());
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
    scene_payload_ = SceneInitPayload{};
    frame_provider.reset();
}
