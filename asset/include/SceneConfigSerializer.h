#pragma once

#include <memory>
#include <string>

#include "JsonConfigManager.h"
#include "RenderState.h"

class SceneConfigSerializer
{
public:
    explicit SceneConfigSerializer(std::shared_ptr<JsonConfigManager> json_manager);

    bool loadSceneConfig(const std::string& scene_name_or_id, SceneConfig& out_scene, std::string* error_message = nullptr) const;
    bool loadSceneRuntimeState(int scene_id, SceneRuntimeState& out_state, std::string* error_message = nullptr) const;
    bool saveSceneRenderState(int scene_id, const SceneRuntimeState& state, std::string* error_message = nullptr) const;
    bool saveSceneTransforms(int scene_id, const SceneRuntimeState& state, std::string* error_message = nullptr) const;

private:
    using json = nlohmann::json;

    std::shared_ptr<JsonConfigManager> json_manager_;

    bool findSceneByIdOrName(json& scenes_root, const std::string& scene_name_or_id, json*& out_scene, std::string* error_message) const;
    bool findSceneById(json& scenes_root, int scene_id, json*& out_scene) const;
    static bool parseInt(const std::string& input, int& value);
    static vsg::dmat4 parseMatrixFromJson(const json& matrix_array);
    static json matrixToRowMajorJson(const vsg::dmat4& mat);
    json& ensureSceneForSave(json& scenes_root, int scene_id) const;
    static int normalizeDepthOcclusionFlag(int value);
    static int normalizeShadowModeValue(int value);
};
