#pragma once

#include <memory>
#include <string>
#include <vector>

#include "JsonConfigManager.h"
#include "RenderState.h"
#include "SceneConfigSerializer.h"

class RenderStateController
{
public:
    RenderStateController(std::shared_ptr<JsonConfigManager> json_manager, std::shared_ptr<SceneConfigSerializer> scene_serializer);

    bool loadRenderState(int scene_id, RenderStateHub& out_state, SceneLinePointStyle& out_style, std::string* error_message = nullptr) const;
    bool saveRenderState(int scene_id, const RenderStateHub& state, const SceneLinePointStyle& style, std::string* error_message = nullptr) const;
    bool loadMaterialParams(std::string* error_message = nullptr) const;
    bool saveMaterialParams(std::string* error_message = nullptr) const;
    bool saveBaseBrightnessToLightInfo(int hdr_num, float base_brightness, std::string* error_message = nullptr) const;
    bool saveSceneTransforms(int scene_id, const std::vector<SceneModelTransformSave>& transforms, std::string* error_message = nullptr) const;
    void applyHdrSelection(const RenderStateHub& state, float& inout_base_brightness) const;
    void applyDepthOcclusionState(const RenderStateHub& state) const;
    void applyShadowModeState(const RenderStateHub& state) const;

private:
    std::shared_ptr<JsonConfigManager> json_manager_;
    std::shared_ptr<SceneConfigSerializer> scene_serializer_;
};
