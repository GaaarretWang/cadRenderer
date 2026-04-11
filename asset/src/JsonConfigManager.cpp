#include "JsonConfigManager.h"

#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace
{
int normalizeDepthOcclusionFlag(int value)
{
    return value != 0 ? 1 : 0;
}

int normalizeShadowModeValue(int value)
{
    return value == CAMERA_DEPTH ? CAMERA_DEPTH : FULL_MODEL;
}
}

JsonConfigManager::JsonConfigManager(std::string scenes_json_path, std::string materials_json_path, std::string lightinfo_json_path)
    : scenes_json_path_(std::move(scenes_json_path)),
      materials_json_path_(std::move(materials_json_path)),
      lightinfo_json_path_(std::move(lightinfo_json_path))
{
}

bool JsonConfigManager::loadScenesJson(nlohmann::json& out_json, std::string* error_message) const
{
    return loadJsonFromFile(scenes_json_path_, out_json, error_message);
}

bool JsonConfigManager::saveScenesJson(const nlohmann::json& in_json, std::string* error_message) const
{
    return saveJsonToFile(scenes_json_path_, in_json, error_message);
}

bool JsonConfigManager::loadJsonFromFile(const std::string& path, json& out_json, std::string* error_message) const
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        if (error_message) *error_message = "Failed to open file: " + path;
        return false;
    }

    try
    {
        file >> out_json;
    }
    catch (const std::exception& e)
    {
        if (error_message) *error_message = "Failed to parse JSON file: " + path + " error: " + e.what();
        return false;
    }
    return true;
}

bool JsonConfigManager::saveJsonToFile(const std::string& path, const json& in_json, std::string* error_message) const
{
    std::ofstream file(path);
    if (!file.is_open())
    {
        if (error_message) *error_message = "Failed to open file for writing: " + path;
        return false;
    }
    file << std::setw(4) << in_json << std::endl;
    return true;
}

bool JsonConfigManager::parseInt(const std::string& input, int& value)
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

bool JsonConfigManager::findSceneByIdOrName(json& scenes_root, const std::string& scene_name_or_id, json*& out_scene, std::string* error_message) const
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

bool JsonConfigManager::findSceneById(json& scenes_root, int scene_id, json*& out_scene) const
{
    out_scene = nullptr;
    if (!scenes_root.contains("scenes") || !scenes_root["scenes"].is_array())
    {
        scenes_root["scenes"] = json::array();
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

vsg::dmat4 JsonConfigManager::parseMatrixFromJson(const json& matrix_array)
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

JsonConfigManager::json JsonConfigManager::matrixToRowMajorJson(const vsg::dmat4& mat)
{
    json mat2d = json::array();
    for (int row = 0; row < 4; ++row)
    {
        json row_arr = json::array();
        for (int col = 0; col < 4; ++col)
        {
            row_arr.push_back(mat[col][row]);
        }
        mat2d.push_back(row_arr);
    }
    return mat2d;
}

bool JsonConfigManager::loadSceneConfig(const std::string& scene_name_or_id, SceneConfig& out_scene, std::string* error_message) const
{
    json scenes_root;
    if (!loadJsonFromFile(scenes_json_path_, scenes_root, error_message))
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

bool JsonConfigManager::loadMaterialsJson(nlohmann::json& out_json, std::string* error_message) const
{
    return loadJsonFromFile(materials_json_path_, out_json, error_message);
}

bool JsonConfigManager::saveMaterialsJson(const nlohmann::json& in_json, std::string* error_message) const
{
    return saveJsonToFile(materials_json_path_, in_json, error_message);
}

bool JsonConfigManager::loadLightInfoJson(nlohmann::json& out_json, std::string* error_message) const
{
    return loadJsonFromFile(lightinfo_json_path_, out_json, error_message);
}

bool JsonConfigManager::saveLightInfoJson(const nlohmann::json& in_json, std::string* error_message) const
{
    return saveJsonToFile(lightinfo_json_path_, in_json, error_message);
}
