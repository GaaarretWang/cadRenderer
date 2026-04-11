#pragma once

#include <memory>
#include <string>

#include "JsonConfigManager.h"
#include "SceneConfigSerializer.h"

class RenderStateController
{
public:
    RenderStateController(std::shared_ptr<JsonConfigManager> json_manager, std::shared_ptr<SceneConfigSerializer> scene_serializer);

    bool loadMaterialParams(std::string* error_message = nullptr) const;
    bool saveMaterialParams(std::string* error_message = nullptr) const;

private:
    std::shared_ptr<JsonConfigManager> json_manager_;
    std::shared_ptr<SceneConfigSerializer> scene_serializer_;
};
