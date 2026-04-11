#pragma once

#include <memory>
#include <string>

#include "JsonConfigManager.h"
#include "LightInfoStateSerializer.h"
#include "RenderState.h"
#include "SceneConfigSerializer.h"

class SceneStatePersistenceCoordinator
{
public:
    SceneStatePersistenceCoordinator(std::shared_ptr<JsonConfigManager> json_manager,
                                     std::shared_ptr<SceneConfigSerializer> scene_serializer);

    bool loadSceneState(int scene_id, SceneRuntimeState& out_state, std::string* error_message = nullptr) const;
    bool saveSceneRenderState(int scene_id, const SceneRuntimeState& state, std::string* error_message = nullptr) const;
    bool saveSceneTransforms(int scene_id, const SceneRuntimeState& state, std::string* error_message = nullptr) const;
    bool saveBaseBrightness(const SceneRuntimeState& state, std::string* error_message = nullptr) const;

private:
    std::shared_ptr<JsonConfigManager> json_manager_;
    std::shared_ptr<SceneConfigSerializer> scene_serializer_;
    std::shared_ptr<LightInfoStateSerializer> lightinfo_serializer_;
};
