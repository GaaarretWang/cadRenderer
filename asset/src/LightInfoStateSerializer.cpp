#include "LightInfoStateSerializer.h"

#include <algorithm>

namespace
{
constexpr int kMinHdrIndex = 1;

int normalizeHdrImageMaxNum(int value)
{
    return std::max(value, kMinHdrIndex);
}

int clampHdrImageNum(int value, int hdr_image_max_num)
{
    return std::clamp(value, kMinHdrIndex, normalizeHdrImageMaxNum(hdr_image_max_num));
}
}

LightInfoStateSerializer::LightInfoStateSerializer(std::shared_ptr<JsonConfigManager> json_manager)
    : json_manager_(std::move(json_manager))
{
}

bool LightInfoStateSerializer::load(SceneRuntimeState& io_state, std::string* error_message) const
{
    if (!json_manager_)
    {
        if (error_message) *error_message = "JsonConfigManager is not initialized.";
        return false;
    }

    nlohmann::json light_info_json;
    if (!json_manager_->loadLightInfoJson(light_info_json, error_message))
    {
        return false;
    }

    if (light_info_json.contains("hdr_image_max_num") && light_info_json["hdr_image_max_num"].is_number_integer())
    {
        io_state.hdr_image_max_num = normalizeHdrImageMaxNum(light_info_json["hdr_image_max_num"].get<int>());
    }
    else
    {
        io_state.hdr_image_max_num = normalizeHdrImageMaxNum(io_state.hdr_image_max_num);
    }

    io_state.hdr_base_brightness.clear();
    for (auto it = light_info_json.begin(); it != light_info_json.end(); ++it)
    {
        if (!it.value().is_object() || !it.value().contains("baseBrightness"))
        {
            continue;
        }

        try
        {
            const int hdr_idx = std::stoi(it.key());
            io_state.hdr_base_brightness[hdr_idx] = it.value()["baseBrightness"].get<float>();
        }
        catch (const std::exception&)
        {
        }
    }

    io_state.hdr_image_num = clampHdrImageNum(io_state.hdr_image_num, io_state.hdr_image_max_num);
    const auto brightness_it = io_state.hdr_base_brightness.find(io_state.hdr_image_num);
    if (brightness_it != io_state.hdr_base_brightness.end())
    {
        io_state.baseBrightness = brightness_it->second;
    }

    return true;
}

bool LightInfoStateSerializer::saveBaseBrightness(const SceneRuntimeState& state, std::string* error_message) const
{
    if (!json_manager_)
    {
        if (error_message) *error_message = "JsonConfigManager is not initialized.";
        return false;
    }

    nlohmann::json light_info_json;
    if (!json_manager_->loadLightInfoJson(light_info_json, error_message))
    {
        return false;
    }

    light_info_json["hdr_image_max_num"] = normalizeHdrImageMaxNum(state.hdr_image_max_num);

    const std::string hdr_key = std::to_string(clampHdrImageNum(state.hdr_image_num, state.hdr_image_max_num));
    if (!light_info_json.contains(hdr_key) || !light_info_json[hdr_key].is_object())
    {
        light_info_json[hdr_key] = nlohmann::json::object();
    }

    light_info_json[hdr_key]["baseBrightness"] = state.baseBrightness;
    return json_manager_->saveLightInfoJson(light_info_json, error_message);
}
