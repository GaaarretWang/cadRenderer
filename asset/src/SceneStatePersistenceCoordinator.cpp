#include "SceneStatePersistenceCoordinator.h"

SceneStatePersistenceCoordinator::SceneStatePersistenceCoordinator(std::shared_ptr<JsonConfigManager> json_manager,
                                                                   std::shared_ptr<SceneConfigSerializer> scene_serializer)
    : json_manager_(std::move(json_manager)),
      scene_serializer_(std::move(scene_serializer)),
      lightinfo_serializer_(std::make_shared<LightInfoStateSerializer>(json_manager_))
{
}

bool SceneStatePersistenceCoordinator::loadSceneState(int scene_id, SceneRuntimeState& out_state, std::string* error_message) const
{
    if (!scene_serializer_)
    {
        if (error_message) *error_message = "SceneConfigSerializer is not initialized.";
        return false;
    }

    if (!scene_serializer_->loadSceneRuntimeState(scene_id, out_state, error_message))
    {
        return false;
    }

    if (!lightinfo_serializer_)
    {
        if (error_message) *error_message = "LightInfoStateSerializer is not initialized.";
        return false;
    }

    return lightinfo_serializer_->load(out_state, error_message);
}

bool SceneStatePersistenceCoordinator::saveSceneRenderState(int scene_id, const SceneRuntimeState& state, std::string* error_message) const
{
    if (!scene_serializer_)
    {
        if (error_message) *error_message = "SceneConfigSerializer is not initialized.";
        return false;
    }

    return scene_serializer_->saveSceneRenderState(scene_id, state, error_message);
}

bool SceneStatePersistenceCoordinator::saveSceneTransforms(int scene_id, const SceneRuntimeState& state, std::string* error_message) const
{
    if (!scene_serializer_)
    {
        if (error_message) *error_message = "SceneConfigSerializer is not initialized.";
        return false;
    }

    return scene_serializer_->saveSceneTransforms(scene_id, state, error_message);
}

bool SceneStatePersistenceCoordinator::saveBaseBrightness(const SceneRuntimeState& state, std::string* error_message) const
{
    if (!lightinfo_serializer_)
    {
        if (error_message) *error_message = "LightInfoStateSerializer is not initialized.";
        return false;
    }

    return lightinfo_serializer_->saveBaseBrightness(state, error_message);
}
