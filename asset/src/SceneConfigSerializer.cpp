#include "SceneConfigSerializer.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

SceneConfigSerializer::SceneConfigSerializer(std::shared_ptr<JsonConfigManager> json_manager)
    : json_manager_(std::move(json_manager))
{
}

bool SceneConfigSerializer::parseInt(const std::string& input, int& value)
{
    if (input.empty()) return false;
    const bool negative = input[0] == '-';
    const size_t start = negative ? 1 : 0;
    if (start >= input.size()) return false;
    if (!std::all_of(input.begin() + static_cast<std::string::difference_type>(start), input.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
    {
        return false;
    }
    value = std::stoi(input);
    return true;
}

bool SceneConfigSerializer::findSceneByIdOrName(json& scenes_root, const std::string& scene_name_or_id, json*& out_scene, std::string* error_message) const
{
    out_scene = nullptr;
    if (!scenes_root.contains("scenes") || !scenes_root["scenes"].is_array())
    {
        if (error_message) *error_message = "Invalid scenes file: missing scenes array";
        return false;
    }

    int scene_id = 0;
    if (parseInt(scene_name_or_id, scene_id))
    {
        for (auto& scene : scenes_root["scenes"])
        {
            if (scene.contains("id") && scene["id"] == scene_id)
            {
                out_scene = &scene;
                return true;
            }
        }
    }

    for (auto& scene : scenes_root["scenes"])
    {
        if (scene.contains("name") && scene["name"] == scene_name_or_id)
        {
            out_scene = &scene;
            return true;
        }
    }

    if (error_message) *error_message = "Scene not found: " + scene_name_or_id;
    return false;
}

bool SceneConfigSerializer::findSceneById(json& scenes_root, int scene_id, json*& out_scene) const
{
    out_scene = nullptr;
    if (!scenes_root.contains("scenes") || !scenes_root["scenes"].is_array())
    {
        return false;
    }

    for (auto& scene : scenes_root["scenes"])
    {
        if (scene.contains("id") && scene["id"] == scene_id)
        {
            out_scene = &scene;
            return true;
        }
    }

    return false;
}

vsg::dmat4 SceneConfigSerializer::parseMatrixFromJson(const json& matrix_array)
{
    if (!matrix_array.is_array() || matrix_array.size() != 4)
    {
        throw std::runtime_error("Matrix must be 4x4 array");
    }

    vsg::dmat4 mat;
    for (int row = 0; row < 4; ++row)
    {
        if (!matrix_array[row].is_array() || matrix_array[row].size() != 4)
        {
            throw std::runtime_error("Matrix row must have 4 columns");
        }
        for (int col = 0; col < 4; ++col)
        {
            mat[col][row] = matrix_array[row][col].get<double>();
        }
    }
    return mat;
}

SceneConfigSerializer::json SceneConfigSerializer::matrixToRowMajorJson(const vsg::dmat4& mat)
{
    json rows = json::array();
    for (int row = 0; row < 4; ++row)
    {
        json row_json = json::array();
        for (int col = 0; col < 4; ++col)
        {
            row_json.push_back(mat[col][row]);
        }
        rows.push_back(std::move(row_json));
    }
    return rows;
}

SceneConfigSerializer::json& SceneConfigSerializer::ensureSceneForSave(json& scenes_root, int scene_id) const
{
    if (!scenes_root.contains("scenes") || !scenes_root["scenes"].is_array())
    {
        scenes_root["scenes"] = json::array();
    }

    json* target_scene = nullptr;
    if (findSceneById(scenes_root, scene_id, target_scene) && target_scene != nullptr)
    {
        return *target_scene;
    }

    json new_scene;
    new_scene["id"] = scene_id;
    new_scene["name"] = "scene_" + std::to_string(scene_id);
    new_scene["description"] = "Auto-created scene";
    new_scene["models"] = json::array();
    new_scene["msaa"] = 4;
    new_scene["scene_media"] = {
        {"type", "dynamic_video"},
        {"pose_path", "asset/data/cameraPose/vsg_pose.txt"},
        {"color_dir", "asset/data/dataset3/color"},
        {"depth_dir", "asset/data/dataset3/depth"}
    };
    scenes_root["scenes"].push_back(std::move(new_scene));
    return scenes_root["scenes"].back();
}

int SceneConfigSerializer::normalizeDepthOcclusionFlag(int value)
{
    return value != 0 ? 1 : 0;
}

int SceneConfigSerializer::normalizeShadowModeValue(int value)
{
    return value == CAMERA_DEPTH ? CAMERA_DEPTH : FULL_MODEL;
}

void SceneConfigSerializer::normalizeDepthCompletionParams(SceneRuntimeState::DepthCompletionParams& params)
{
    params.enable_real_depth_occlusion = normalizeDepthOcclusionFlag(params.enable_real_depth_occlusion);
    params.shadow_mode = normalizeShadowModeValue(params.shadow_mode);
    params.valid_depth_min_mm = std::clamp(params.valid_depth_min_mm, 1, 1000);
    params.kernel_radius = std::clamp(params.kernel_radius, 1, 64);
    params.top_k = std::clamp(params.top_k, 1, 64);
    params.spatial_weight = std::clamp(params.spatial_weight, 0.0f, 1.0f);
    params.color_sigma = std::clamp(params.color_sigma, 0.001f, 1.0f);
    params.edge_threshold = std::clamp(params.edge_threshold, 0.0f, 1.0f);
    params.max_fill_passes = std::clamp(params.max_fill_passes, 1, 15);
}

bool SceneConfigSerializer::loadSceneConfig(const std::string& scene_name_or_id, SceneConfig& out_scene, std::string* error_message) const
{
    json scenes_root;
    if (!json_manager_->loadScenesJson(scenes_root, error_message))
    {
        return false;
    }

    json* scene_json = nullptr;
    if (!findSceneByIdOrName(scenes_root, scene_name_or_id, scene_json, error_message))
    {
        return false;
    }

    out_scene = SceneConfig{};
    out_scene.id = (*scene_json).value("id", -1);
    out_scene.name = (*scene_json).value("name", "");
    out_scene.description = (*scene_json).value("description", "");
    out_scene.msaa = (*scene_json).value("msaa", 4);

    if (scene_json->contains("scene_media") && (*scene_json)["scene_media"].is_object())
    {
        const auto& media_json = (*scene_json)["scene_media"];
        const std::string type_str = media_json.value("type", "dynamic_video");
        out_scene.media.type = (type_str == "static_image") ? SceneMediaConfig::Type::StaticImage : SceneMediaConfig::Type::DynamicVideo;
        out_scene.media.pose_path = media_json.value("pose_path", "asset/data/cameraPose/vsg_pose.txt");
        out_scene.media.color_path = media_json.value("color_path", "");
        out_scene.media.depth_path = media_json.value("depth_path", "");
        out_scene.media.color_dir = media_json.value("color_dir", "asset/data/dataset3/color");
        out_scene.media.depth_dir = media_json.value("depth_dir", "asset/data/dataset3/depth");
    }
    else
    {
        out_scene.media.type = SceneMediaConfig::Type::DynamicVideo;
        out_scene.media.pose_path = "asset/data/cameraPose/vsg_pose.txt";
        out_scene.media.color_dir = "asset/data/dataset3/color";
        out_scene.media.depth_dir = "asset/data/dataset3/depth";
    }

    if (scene_json->contains("models") && (*scene_json)["models"].is_array())
    {
        for (const auto& model : (*scene_json)["models"])
        {
            SceneModelConfig model_cfg;
            model_cfg.instance_name = model.value("instance_name", "");
            model_cfg.path = model.value("path", "");
            if (model.contains("transform_sequence") &&
                model["transform_sequence"].is_array() &&
                !model["transform_sequence"].empty())
            {
                model_cfg.transform = parseMatrixFromJson(model["transform_sequence"][0]);
            }
            out_scene.models.push_back(std::move(model_cfg));
        }
    }

    if (scene_json->contains("cull_mode_none_model_paths") && (*scene_json)["cull_mode_none_model_paths"].is_array())
    {
        for (const auto& path : (*scene_json)["cull_mode_none_model_paths"])
        {
            if (path.is_string())
            {
                out_scene.cull_mode_none_model_paths.push_back(path.get<std::string>());
            }
        }
    }

    if (scene_json->contains("shadow_receiver_path"))
    {
        out_scene.shadow_receiver_path = (*scene_json)["shadow_receiver_path"].get<std::string>();
    }

    if (scene_json->contains("shadow_receiver_transform") &&
        (*scene_json)["shadow_receiver_transform"].is_array() &&
        !(*scene_json)["shadow_receiver_transform"].empty())
    {
        out_scene.shadow_receiver_transform = parseMatrixFromJson((*scene_json)["shadow_receiver_transform"][0]);
    }

    return true;
}

bool SceneConfigSerializer::loadSceneRuntimeState(int scene_id, SceneRuntimeState& out_state, std::string* error_message) const
{
    json scenes_root;
    if (!json_manager_->loadScenesJson(scenes_root, error_message))
    {
        return false;
    }

    json* target_scene = nullptr;
    if (!findSceneById(scenes_root, scene_id, target_scene) || target_scene == nullptr)
    {
        if (error_message) *error_message = "Scene id not found: " + std::to_string(scene_id);
        return false;
    }

    out_state = SceneRuntimeState{};

    if (target_scene->contains("render_params") && (*target_scene)["render_params"].is_object())
    {
        auto& rp = (*target_scene)["render_params"];
        out_state.hdr_image_num = rp.value("hdr_image_num", out_state.hdr_image_num);
        out_state.ssao_radius = rp.value("ssao_radius", out_state.ssao_radius);
        out_state.ssao_kernel_size = rp.value("ssao_kernel_size", out_state.ssao_kernel_size);
        out_state.exposure = rp.value("exposure", out_state.exposure);
        out_state.denoise_size = rp.value("denoise_size", out_state.denoise_size);
        out_state.shadow_bias = rp.value("shadow_bias", out_state.shadow_bias);
        out_state.blocker_sample_num = rp.value("blocker_sample_num", out_state.blocker_sample_num);
        out_state.pcf_sample_num = rp.value("pcf_sample_num", out_state.pcf_sample_num);
        out_state.shadow_type = rp.value("shadow_type", out_state.shadow_type);
        out_state.pcf_softness = rp.value("pcf_softness", out_state.pcf_softness);
        out_state.pcss_softness = rp.value("pcss_softness", out_state.pcss_softness);
        out_state.pcss_softness_falloff = rp.value("pcss_softness_falloff", out_state.pcss_softness_falloff);
    }

    if (target_scene->contains("depth_completion_params") && (*target_scene)["depth_completion_params"].is_object())
    {
        auto& dcp = (*target_scene)["depth_completion_params"];
        out_state.depth_completion_params.enable_real_depth_occlusion =
            dcp.value("enable_real_depth_occlusion", out_state.depth_completion_params.enable_real_depth_occlusion);
        out_state.depth_completion_params.shadow_mode =
            dcp.value("shadow_mode", out_state.depth_completion_params.shadow_mode);
        out_state.depth_completion_params.valid_depth_min_mm =
            dcp.value("valid_depth_min_mm", out_state.depth_completion_params.valid_depth_min_mm);
        out_state.depth_completion_params.kernel_radius =
            dcp.value("kernel_radius", out_state.depth_completion_params.kernel_radius);
        out_state.depth_completion_params.top_k =
            dcp.value("top_k", out_state.depth_completion_params.top_k);
        out_state.depth_completion_params.spatial_weight =
            dcp.value("spatial_weight", out_state.depth_completion_params.spatial_weight);
        out_state.depth_completion_params.color_sigma =
            dcp.value("color_sigma", out_state.depth_completion_params.color_sigma);
        out_state.depth_completion_params.edge_threshold =
            dcp.value("edge_threshold", out_state.depth_completion_params.edge_threshold);
        out_state.depth_completion_params.max_fill_passes =
            dcp.value("max_fill_passes", out_state.depth_completion_params.max_fill_passes);
    }

    normalizeDepthCompletionParams(out_state.depth_completion_params);

    if (target_scene->contains("line_point_style") && (*target_scene)["line_point_style"].is_object())
    {
        auto& style = (*target_scene)["line_point_style"];
        if (style.contains("line_color") && style["line_color"].is_array() && style["line_color"].size() >= 3)
        {
            out_state.line_color = vsg::vec3(style["line_color"][0], style["line_color"][1], style["line_color"][2]);
        }
        if (style.contains("point_color") && style["point_color"].is_array() && style["point_color"].size() >= 3)
        {
            out_state.point_color = vsg::vec3(style["point_color"][0], style["point_color"][1], style["point_color"][2]);
        }
    }

    out_state.scene_transforms.clear();
    if (target_scene->contains("models") && (*target_scene)["models"].is_array())
    {
        for (const auto& model : (*target_scene)["models"])
        {
            SceneModelTransformSave transform_item;
            transform_item.instance_name = model.value("instance_name", "");
            transform_item.path = model.value("path", "");
            if (model.contains("transform_sequence") &&
                model["transform_sequence"].is_array() &&
                !model["transform_sequence"].empty())
            {
                transform_item.transform = parseMatrixFromJson(model["transform_sequence"][0]);
            }
            out_state.scene_transforms.push_back(std::move(transform_item));
        }
    }

    if (target_scene->contains("shadow_receiver_transform") &&
        (*target_scene)["shadow_receiver_transform"].is_array() &&
        !(*target_scene)["shadow_receiver_transform"].empty())
    {
        SceneModelTransformSave shadow_receiver;
        shadow_receiver.instance_name = "shadow_receiver";
        shadow_receiver.is_shadow_receiver = true;
        shadow_receiver.transform = parseMatrixFromJson((*target_scene)["shadow_receiver_transform"][0]);
        out_state.scene_transforms.push_back(std::move(shadow_receiver));
    }

    return true;
}

bool SceneConfigSerializer::saveSceneRenderState(int scene_id, const SceneRuntimeState& state, std::string* error_message) const
{
    json scenes_root;
    if (!json_manager_->loadScenesJson(scenes_root, error_message))
    {
        return false;
    }

    json& target_scene = ensureSceneForSave(scenes_root, scene_id);

    SceneRuntimeState normalized_state = state;
    normalizeDepthCompletionParams(normalized_state.depth_completion_params);

    target_scene["render_params"] = {
        {"hdr_image_num", normalized_state.hdr_image_num},
        {"ssao_radius", normalized_state.ssao_radius},
        {"ssao_kernel_size", normalized_state.ssao_kernel_size},
        {"exposure", normalized_state.exposure},
        {"denoise_size", normalized_state.denoise_size},
        {"shadow_bias", normalized_state.shadow_bias},
        {"blocker_sample_num", normalized_state.blocker_sample_num},
        {"pcf_sample_num", normalized_state.pcf_sample_num},
        {"shadow_type", normalized_state.shadow_type},
        {"pcf_softness", normalized_state.pcf_softness},
        {"pcss_softness", normalized_state.pcss_softness},
        {"pcss_softness_falloff", normalized_state.pcss_softness_falloff}
    };

    target_scene["depth_completion_params"] = {
        {"enable_real_depth_occlusion", normalized_state.depth_completion_params.enable_real_depth_occlusion},
        {"shadow_mode", normalized_state.depth_completion_params.shadow_mode},
        {"valid_depth_min_mm", normalized_state.depth_completion_params.valid_depth_min_mm},
        {"kernel_radius", normalized_state.depth_completion_params.kernel_radius},
        {"top_k", normalized_state.depth_completion_params.top_k},
        {"spatial_weight", normalized_state.depth_completion_params.spatial_weight},
        {"color_sigma", normalized_state.depth_completion_params.color_sigma},
        {"edge_threshold", normalized_state.depth_completion_params.edge_threshold},
        {"max_fill_passes", normalized_state.depth_completion_params.max_fill_passes}
    };

    target_scene["line_point_style"] = {
        {"line_color", {normalized_state.line_color.r, normalized_state.line_color.g, normalized_state.line_color.b}},
        {"point_color", {normalized_state.point_color.r, normalized_state.point_color.g, normalized_state.point_color.b}}
    };

    return json_manager_->saveScenesJson(scenes_root, error_message);
}

bool SceneConfigSerializer::saveSceneTransforms(int scene_id, const SceneRuntimeState& state, std::string* error_message) const
{
    json scenes_root;
    if (!json_manager_->loadScenesJson(scenes_root, error_message))
    {
        return false;
    }

    json& target_scene = ensureSceneForSave(scenes_root, scene_id);

    target_scene["models"] = json::array();
    auto& models = target_scene["models"];
    for (const auto& t : state.scene_transforms)
    {
        if (t.is_shadow_receiver)
        {
            target_scene["shadow_receiver_transform"] = json::array({matrixToRowMajorJson(t.transform)});
            continue;
        }
        json model_json;
        model_json["instance_name"] = t.instance_name;
        model_json["path"] = t.path;
        model_json["transform_sequence"] = json::array({matrixToRowMajorJson(t.transform)});
        models.push_back(std::move(model_json));
    }

    return json_manager_->saveScenesJson(scenes_root, error_message);
}
