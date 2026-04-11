#pragma once

#include <memory>
#include <string>

#include "JsonConfigManager.h"
#include "RenderState.h"

class LightInfoStateSerializer
{
public:
    explicit LightInfoStateSerializer(std::shared_ptr<JsonConfigManager> json_manager);

    bool load(SceneRuntimeState& io_state, std::string* error_message = nullptr) const;
    bool saveBaseBrightness(const SceneRuntimeState& state, std::string* error_message = nullptr) const;

private:
    std::shared_ptr<JsonConfigManager> json_manager_;
};
