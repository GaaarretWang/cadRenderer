#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <vsg/all.h>
#include <json/json.hpp>
#include "MyMask.h"

struct SceneMediaConfig
{
    enum class Type
    {
        DynamicVideo,
        StaticImage
    };

    Type type = Type::DynamicVideo;
    std::string pose_path;
    std::string color_path;
    std::string depth_path;
    std::string color_dir;
    std::string depth_dir;
};

struct SceneModelConfig
{
    std::string instance_name;
    std::string path;
    vsg::dmat4 transform = vsg::dmat4();
};

struct SceneConfig
{
    int id = -1;
    std::string name;
    std::string description;
    int msaa = 4;
    SceneMediaConfig media;
    std::vector<SceneModelConfig> models;
    std::vector<std::string> cull_mode_none_model_paths;
    std::string shadow_receiver_path = "asset/data/obj/shadow_receiver2.obj";
    vsg::dmat4 shadow_receiver_transform = vsg::dmat4();
};

struct SceneModelTransformSave
{
    std::string instance_name;
    std::string path;
    vsg::dmat4 transform = vsg::dmat4();
    bool is_shadow_receiver = false;
};

class JsonConfigManager
{
public:
    JsonConfigManager(std::string scenes_json_path, std::string materials_json_path, std::string lightinfo_json_path);

    bool loadScenesJson(nlohmann::json& out_json, std::string* error_message = nullptr) const;
    bool saveScenesJson(const nlohmann::json& in_json, std::string* error_message = nullptr) const;
    bool loadSceneConfig(const std::string& scene_name_or_id, SceneConfig& out_scene, std::string* error_message = nullptr) const;

    bool loadMaterialsJson(nlohmann::json& out_json, std::string* error_message = nullptr) const;
    bool saveMaterialsJson(const nlohmann::json& in_json, std::string* error_message = nullptr) const;

    bool loadLightInfoJson(nlohmann::json& out_json, std::string* error_message = nullptr) const;
    bool saveLightInfoJson(const nlohmann::json& in_json, std::string* error_message = nullptr) const;

    const std::string& scenesPath() const { return scenes_json_path_; }
    const std::string& materialsPath() const { return materials_json_path_; }
    const std::string& lightInfoPath() const { return lightinfo_json_path_; }

private:
    using json = nlohmann::json;

    std::string scenes_json_path_;
    std::string materials_json_path_;
    std::string lightinfo_json_path_;

    bool loadJsonFromFile(const std::string& path, json& out_json, std::string* error_message) const;
    bool saveJsonToFile(const std::string& path, const json& in_json, std::string* error_message) const;

    bool findSceneByIdOrName(json& scenes_root, const std::string& scene_name_or_id, json*& out_scene, std::string* error_message) const;
    bool findSceneById(json& scenes_root, int scene_id, json*& out_scene) const;
    static bool parseInt(const std::string& input, int& value);

    static vsg::dmat4 parseMatrixFromJson(const json& matrix_array);
    static json matrixToRowMajorJson(const vsg::dmat4& mat);
};
