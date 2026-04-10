#include "RenderStateController.h"

#include <unordered_set>

#include "CADMesh.h"
#include "RenderStateSerializer.h"
#include "SceneRuntimeController.h"
#include "vsgRendererServer.h"

using json = nlohmann::json;

namespace vsgserver
{
    extern vsgRendererServer* renderer;
    extern SceneRuntimeController* runtime_controller;
}

namespace
{
std::string extractMaterialKey(const std::string& full_path)
{
    const size_t last_slash = full_path.find_last_of('/');
    if (last_slash == std::string::npos) return full_path;
    return full_path.substr(last_slash + 1);
}

std::string getMaterialPersistKey(const std::string& id, const ProtoData* proto_data)
{
    if (proto_data && !proto_data->material_persist_key.empty()) return proto_data->material_persist_key;
    return extractMaterialKey(id);
}

void applyMaterialJsonToMaterial(const json& material_json, vsg::PbrMaterial* material)
{
    if (material_json.contains("metallicFactor")) material->metallicFactor = material_json["metallicFactor"];
    if (material_json.contains("roughnessFactor")) material->roughnessFactor = material_json["roughnessFactor"];
    if (material_json.contains("baseColorFactor"))
    {
        auto& base_color = material_json["baseColorFactor"];
        material->baseColorFactor = vsg::vec4(base_color[0], base_color[1], base_color[2], base_color[3]);
    }
}

void writeMaterialJson(json& material_json, const vsg::PbrMaterial* material)
{
    material_json["metallicFactor"] = material->metallicFactor;
    material_json["roughnessFactor"] = material->roughnessFactor;
    material_json["baseColorFactor"] = {
        material->baseColorFactor.r,
        material->baseColorFactor.g,
        material->baseColorFactor.b,
        material->baseColorFactor.a
    };
}
}

RenderStateController::RenderStateController(std::shared_ptr<JsonConfigManager> json_manager, std::shared_ptr<SceneConfigSerializer> scene_serializer)
    : json_manager_(std::move(json_manager)),
      scene_serializer_(std::move(scene_serializer))
{
}

bool RenderStateController::loadRenderState(int scene_id, RenderStateHub& out_state, SceneLinePointStyle& out_style, std::string* error_message) const
{
    if (!scene_serializer_)
    {
        if (error_message) *error_message = "SceneConfigSerializer is not initialized.";
        return false;
    }

    SceneRenderParams params;
    if (!scene_serializer_->loadSceneRenderParamsAndStyle(scene_id, params, out_style, error_message))
    {
        return false;
    }

    out_state = RenderStateHub{};
    RenderStateSerializer::applySceneRenderParams(params, out_state);
    return true;
}

bool RenderStateController::saveRenderState(int scene_id, const RenderStateHub& state, const SceneLinePointStyle& style, std::string* error_message) const
{
    if (!scene_serializer_)
    {
        if (error_message) *error_message = "SceneConfigSerializer is not initialized.";
        return false;
    }

    return scene_serializer_->saveSceneRenderParamsAndStyle(scene_id, RenderStateSerializer::toSceneRenderParams(state), style, error_message);
}

bool RenderStateController::loadMaterialParams(std::string* error_message) const
{
    if (!json_manager_)
    {
        if (error_message) *error_message = "JsonConfigManager is not initialized.";
        return false;
    }

    json mat_data;
    if (!json_manager_->loadMaterialsJson(mat_data, error_message))
    {
        return false;
    }

    if (!mat_data.contains("material_params"))
    {
        return true;
    }

    auto& material_params = mat_data["material_params"];
    std::unordered_set<std::string> loaded_fb_groups;
    bool material_changed = false;
    for (auto& id_data : CADMesh::proto_id_to_data_map)
    {
        const std::string& id = id_data.first;
        ProtoData* proto_data = id_data.second;
        if (proto_data->material_index >= CADMesh::global_material_buffer->size()) continue;

        vsg::PbrMaterial* pbr_ptr = static_cast<vsg::PbrMaterial*>(CADMesh::global_material_buffer->dataPointer(proto_data->material_index));
        std::string mat_key = getMaterialPersistKey(id, proto_data);

        if (proto_data->material_source == ProtoData::MaterialSource::Fb)
        {
            if (proto_data->fb_color_group_key.empty()) continue;
            if (loaded_fb_groups.find(proto_data->fb_color_group_key) != loaded_fb_groups.end()) continue;
            loaded_fb_groups.insert(proto_data->fb_color_group_key);

            auto leader_it = CADMesh::fb_color_to_leader_material_key.find(proto_data->fb_color_group_key);
            if (leader_it != CADMesh::fb_color_to_leader_material_key.end()) mat_key = leader_it->second;
        }

        if (!material_params.contains(mat_key)) continue;

        applyMaterialJsonToMaterial(material_params[mat_key], pbr_ptr);
        material_changed = true;
    }

    if (material_changed) CADMesh::global_material_buffer->dirty();
    return true;
}

bool RenderStateController::saveMaterialParams(std::string* error_message) const
{
    if (!json_manager_)
    {
        if (error_message) *error_message = "JsonConfigManager is not initialized.";
        return false;
    }

    json mat_json;
    if (!json_manager_->loadMaterialsJson(mat_json, error_message))
    {
        return false;
    }

    json& material_params = mat_json["material_params"];
    for (auto& id_data : CADMesh::proto_id_to_data_map)
    {
        const std::string& id = id_data.first;
        ProtoData* proto_data = id_data.second;
        if (proto_data->material_index >= CADMesh::global_material_buffer->size()) continue;

        vsg::PbrMaterial* pbr_ptr = static_cast<vsg::PbrMaterial*>(CADMesh::global_material_buffer->dataPointer(proto_data->material_index));
        writeMaterialJson(material_params[getMaterialPersistKey(id, proto_data)], pbr_ptr);
    }

    return json_manager_->saveMaterialsJson(mat_json, error_message);
}

bool RenderStateController::saveBaseBrightnessToLightInfo(int hdr_num, float base_brightness, std::string* error_message) const
{
    if (!json_manager_)
    {
        if (error_message) *error_message = "JsonConfigManager is not initialized.";
        return false;
    }

    json json_data;
    if (!json_manager_->loadLightInfoJson(json_data, error_message))
    {
        return false;
    }

    const std::string hdr_key = std::to_string(hdr_num);
    if (json_data.contains(hdr_key))
    {
        json_data[hdr_key]["baseBrightness"] = base_brightness;
    }

    return json_manager_->saveLightInfoJson(json_data, error_message);
}

bool RenderStateController::saveSceneTransforms(int scene_id, const std::vector<SceneModelTransformSave>& transforms, std::string* error_message) const
{
    if (!scene_serializer_)
    {
        if (error_message) *error_message = "SceneConfigSerializer is not initialized.";
        return false;
    }

    return scene_serializer_->saveSceneTransforms(scene_id, transforms, error_message);
}

void RenderStateController::applyHdrSelection(const RenderStateHub& state, float& inout_base_brightness) const
{
    if (!vsgserver::renderer) return;

    vsgserver::renderer->hdr_image_num = state.pipeline.hdr_image_num;
    vsgserver::renderer->updateEnvLighting();
    if (vsgserver::runtime_controller)
    {
        vsgserver::runtime_controller->markServerDirty(RuntimeParam::Hdr);
    }

    const auto it = vsgserver::renderer->hdr_base_brightness.find(state.pipeline.hdr_image_num);
    if (it != vsgserver::renderer->hdr_base_brightness.end())
    {
        inout_base_brightness = it->second;
        if (vsgserver::runtime_controller)
        {
            vsgserver::runtime_controller->markServerDirty(RuntimeParam::BaseBrightness);
        }
    }
}

void RenderStateController::applyDepthOcclusionState(const RenderStateHub& state) const
{
    if (!vsgserver::renderer) return;

    vsgserver::renderer->setRealDepthOcclusion(state.pipeline.enable_real_depth_occlusion);
    vsgserver::renderer->syncConstantData();
    if (vsgserver::runtime_controller)
    {
        vsgserver::runtime_controller->markServerDirty(RuntimeParam::DepthOcclusion);
    }
}

void RenderStateController::applyShadowModeState(const RenderStateHub& state) const
{
    if (!vsgserver::renderer) return;

    vsgserver::renderer->setShadowMode(state.pipeline.shadow_mode);
    vsgserver::renderer->syncConstantData();
    if (vsgserver::runtime_controller)
    {
        vsgserver::runtime_controller->markServerDirty(RuntimeParam::ShadowMode);
    }
}
