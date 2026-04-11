#define _USE_MATH_DEFINES
#include "ImGui.h"
// Keep the renderer header here because the GUI writes directly into the server state.
#include <vsgRendererServer.h>
#include <algorithm>

namespace vsgserver {
    vsgRendererServer* renderer = nullptr;
    SceneRuntimeController* runtime_controller = nullptr;
}

// final = T * original * Rz * Ry * Rx * S
// Apply transforms in the order translation * original * rotationZ * rotationY * rotationX * scale.


namespace gui
{
    vsg::ref_ptr<Params> global_params = Params::create();

    namespace
    {
        std::string getMaterialPersistKey(const std::string& id, const ProtoData* proto_data)
        {
            if (proto_data && !proto_data->material_persist_key.empty())
            {
                return proto_data->material_persist_key;
            }
            return extractMaterialKey(id);
        }

        void applyMaterialJsonToMaterial(const json& material_json, vsg::PbrMaterial* material)
        {
            if (material_json.contains("metallicFactor"))
                material->metallicFactor = material_json["metallicFactor"];
            if (material_json.contains("roughnessFactor"))
                material->roughnessFactor = material_json["roughnessFactor"];
            if (material_json.contains("baseColorFactor"))
            {
                auto& base_color = material_json["baseColorFactor"];
                material->baseColorFactor = vsg::vec4(
                    base_color[0], base_color[1], base_color[2], base_color[3]
                );
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

    // Build the GUI controller and bind its JSON-backed state.
    MyGui::MyGui(vsg::ref_ptr<vsg::Value<GlobalPCData>> pc_data,
                 const std::string& scenes_json_path,
                 const std::string& materials_json_path,
                 const std::string& lightinfo_json_path,
                 vsg::ref_ptr<vsg::Options> options)
        : m_pc_data(pc_data),
          m_scenes_json_path(scenes_json_path), m_materials_json_path(materials_json_path),
          m_lightinfo_json_path(lightinfo_json_path)
    {
        // Initialize the JSON manager, serializers, and controller helpers.
        m_json_manager = std::make_shared<JsonConfigManager>(m_scenes_json_path, m_materials_json_path, m_lightinfo_json_path);
        auto scene_serializer = std::make_shared<SceneConfigSerializer>(m_json_manager);
        m_state_controller = std::make_shared<RenderStateController>(m_json_manager, scene_serializer);
        m_state_persistence = std::make_shared<SceneStatePersistenceCoordinator>(m_json_manager, scene_serializer);
        if (vsgserver::renderer)
        {
            m_runtime_controller = std::make_shared<SceneRuntimeController>(*vsgserver::renderer, m_state_persistence, m_runtime_state);
            vsgserver::runtime_controller = m_runtime_controller.get();
        }
        loadParams();
        // Cache the per-instance transform state used by the pose controls.
        initInstanceStates();
    }

    void MyGui::initInstanceStates()
    {
        m_instance_states.clear();
        for (size_t i = 0; i < CADMesh::scene_instance_names.size(); i++) {
            InstanceTransformState state;
            state.instance_name = CADMesh::scene_instance_names[i];
            m_instance_states.push_back(state);
        }
    }

    void MyGui::resetSharedTransform() const
    {
        m_shared_translate[0] = m_shared_translate[1] = m_shared_translate[2] = 0.0f;
        m_shared_rotate[0] = m_shared_rotate[1] = m_shared_rotate[2] = 0.0f;
        m_shared_scale_percent = 100.0f;
    }

    bool MyGui::hasAnySelectedInstance() const
    {
        for (const auto& state : m_instance_states)
        {
            if (state.selected) return true;
        }
        return false;
    }

    vsg::dmat4 MyGui::computeTransformedMatrix(const InstanceTransformState& state) const
    {
        const vsg::dmat4 original_transform = m_runtime_controller ? m_runtime_controller->sceneTransformOrIdentity(state.instance_name) : vsg::dmat4();
        const double tx = m_shared_translate[0];
        const double ty = m_shared_translate[1];
        const double tz = m_shared_translate[2];
        const double rx = m_shared_rotate[0] * M_PI / 180.0;
        const double ry = m_shared_rotate[1] * M_PI / 180.0;
        const double rz = m_shared_rotate[2] * M_PI / 180.0;
        const double s = m_shared_scale_percent / 100.0;

        auto T = vsg::translate(tx, ty, tz);
        auto Rz = vsg::rotate(rz, 0.0, 0.0, 1.0);
        auto Ry = vsg::rotate(ry, 0.0, 1.0, 0.0);
        auto Rx = vsg::rotate(rx, 1.0, 0.0, 0.0);
        auto S = vsg::scale(s, s, s);

        return T * original_transform * Rz * Ry * Rx * S;
    }

    void MyGui::applyPoseForState(const InstanceTransformState& state) const
    {
        const vsg::dmat4 target = state.selected ? computeTransformedMatrix(state) : (m_runtime_controller ? m_runtime_controller->sceneTransformOrIdentity(state.instance_name) : vsg::dmat4());
        if (m_runtime_controller)
        {
            m_runtime_controller->setInstanceTransform(state.instance_name, target);
        }
    }

    void MyGui::applyPoseForAllStates() const
    {
        for (const auto& state : m_instance_states)
        {
            applyPoseForState(state);
        }
    }

    bool MyGui::applyUiFloatChange(float value,
                                   const std::function<bool(float)>& setter,
                                   const std::function<void(float)>& on_success) const
    {
        if (!m_runtime_controller || !setter || !on_success)
        {
            return false;
        }

        if (!setter(value))
        {
            return false;
        }

        on_success(value);
        return true;
    }

    bool MyGui::applyUiIntChange(int value,
                                 const std::function<bool(int)>& setter,
                                 const std::function<void(int)>& on_success) const
    {
        if (!m_runtime_controller || !setter || !on_success)
        {
            return false;
        }

        if (!setter(value))
        {
            return false;
        }

        on_success(value);
        return true;
    }

    bool MyGui::applyUiVec3Change(const vsg::vec3& value,
                                  const std::function<bool(const vsg::vec3&)>& setter,
                                  const std::function<void(const vsg::vec3&)>& on_success) const
    {
        if (!m_runtime_controller || !setter || !on_success)
        {
            return false;
        }

        if (!setter(value))
        {
            return false;
        }

        on_success(value);
        return true;
    }

    void MyGui::compile(vsg::Context& context)
    {
        // No extra compile-time resources are required for this GUI node.
    }

    // Load both render parameters and material parameters from JSON.
    void MyGui::loadParams()
    {
        loadRenderParams();
        loadMaterialParams();
    }

    // Load the current scene render state from Scenes.json.
    void MyGui::loadRenderParams()
    {
        std::cout << "Loading render params from: " << m_scenes_json_path << std::endl;
        if (!m_runtime_controller)
        {
            std::cerr << "SceneRuntimeController is not initialized." << std::endl;
            return;
        }

        std::string error_message;
        if (!m_runtime_controller->loadSceneState(CADMesh::current_scene_id, &error_message))
        {
            std::cerr << "Failed to load render params: " << error_message << std::endl;
            return;
        }
    }

    // Load material parameters from Materials.json.
    void MyGui::loadMaterialParams()
    {
        try
        {
            if (!m_state_controller) {
                std::cerr << "RenderStateController is not initialized." << std::endl;
                return;
            }

            std::string error_message;
            if (!m_state_controller->loadMaterialParams(&error_message)) {
                std::cerr << "Failed to load material params: " << error_message << std::endl;
                return;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to load material params: " << e.what() << std::endl;
        }
    }

    // Save both render parameters and material parameters back to JSON.
    void MyGui::saveParams() const
    {
        saveRenderParams();
        saveMaterialParams();
    }

    // Persist the current baseBrightness value into LightInfo.json.
    void MyGui::saveBaseBrightnessToLightInfo() const
    {
        if (!m_json_manager)
        {
            std::cerr << "JsonConfigManager is not initialized." << std::endl;
            return;
        }

        try {
            std::string error_message;
            if (!m_runtime_controller || !m_runtime_controller->saveBaseBrightness(&error_message)) {
                std::cerr << "Failed to save LightInfo.json: " << error_message << std::endl;
                return;
            }
            const auto& state = m_runtime_controller->state();
            std::cout << "baseBrightness saved to LightInfo.json (HDR " << state.hdr_image_num << " = " << state.baseBrightness << ")" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to save baseBrightness: " << e.what() << std::endl;
        }
    }

    // Save the current scene render parameters to Scenes.json.
    void MyGui::saveRenderParams() const
    {
        if (!m_json_manager)
        {
            std::cerr << "JsonConfigManager is not initialized." << std::endl;
            return;
        }

        std::string error_message;
        if (!m_runtime_controller || !m_runtime_controller->saveRenderState(&error_message)) {
            std::cerr << "Failed to save render params: " << error_message << std::endl;
            return;
        }
        std::cout << "Render params saved to: " << m_scenes_json_path << " (scene_id=" << CADMesh::current_scene_id << ")" << std::endl;
    }

    // Save material parameters to Materials.json.
    void MyGui::saveMaterialParams() const
    {
        if (!m_state_controller)
        {
            std::cerr << "RenderStateController is not initialized." << std::endl;
            return;
        }

        try {
            std::string error_message;
            if (!m_state_controller->saveMaterialParams(&error_message)) {
                std::cerr << "Failed to load material params: " << error_message << std::endl;
                return;
            }
            std::cout << "Material params saved to: " << m_materials_json_path << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to save material params: " << e.what() << std::endl;
        }
    }

    // Draw the render-parameter control panel.
    void MyGui::drawRenderParams() const
    {
        if (!m_runtime_controller)
        {
            return;
        }

        const SceneRuntimeState& state = m_runtime_controller->state();
        const auto& depth_params = state.depth_completion_params;
        const bool depth_completion_active =
            depth_params.enable_real_depth_occlusion != 0 ||
            depth_params.shadow_mode == SHADOW_REAL_DEPTH;

        if (ImGui::BeginTable("RenderParamsTable", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableNextColumn();
            ImGui::Text("Global Render Params");
            ImGui::Text("hdr num:");
            for (int i = 1; i <= state.hdr_image_max_num; ++i) {
                std::string num_str = std::to_string(i);
                if (i > 1) {
                    ImGui::SameLine(0.0f, 5.0f);
                }
                if (ImGui::Button(num_str.c_str())) {
                    applyUiIntChange(
                        i,
                        [this](int hdr) { return m_runtime_controller->setHdrFromUi(hdr); },
                        [](int) {});
                }
            }

            float base_brightness = state.baseBrightness;
            if (ImGui::SliderFloat("baseBrightness", &base_brightness, 0.0f, 100.0f))
            {
                applyUiFloatChange(
                    base_brightness,
                    [this](float value) { return m_runtime_controller->setBaseBrightnessFromUi(value); },
                    [](float) {});
            }
            if (ImGui::Button("Save baseBrightness"))
                saveBaseBrightnessToLightInfo();

            if (ImGui::RadioButton("PCF", state.shadow_type == 0)){
                applyUiIntChange(
                    0,
                    [this](int type) { return m_runtime_controller->setShadowTypeFromUi(type); },
                    [](int) {});
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("PCSS", state.shadow_type == 1))
            {
                applyUiIntChange(
                    1,
                    [this](int type) { return m_runtime_controller->setShadowTypeFromUi(type); },
                    [](int) {});
            }

            if (state.shadow_type == 0)
            {
                float softness = state.pcf_softness;
                if (ImGui::SliderFloat("pcf_softness", &softness, 0.0f, 100.0f))
                {
                    applyUiFloatChange(
                        softness,
                        [this](float value) { return m_runtime_controller->setPcfSoftnessFromUi(value); },
                        [](float) {});
                }
            }
            else if (state.shadow_type == 1)
            {
                float softness = state.pcss_softness;
                if (ImGui::SliderFloat("pcss_softness", &softness, 0.0f, 0.1f, "%.7f", ImGuiSliderFlags_Logarithmic))
                {
                    applyUiFloatChange(
                        softness,
                        [this](float value) { return m_runtime_controller->setPcssSoftnessFromUi(value); },
                        [](float) {});
                }

                float softness_falloff = state.pcss_softness_falloff;
                if (ImGui::SliderFloat("pcss_softness_falloff", &softness_falloff, 0.0f, 0.005f, "%.7f", ImGuiSliderFlags_Logarithmic))
                {
                    applyUiFloatChange(
                        softness_falloff,
                        [this](float value) { return m_runtime_controller->setPcssSoftnessFalloffFromUi(value); },
                        [](float) {});
                }
            }

            int blocker_sample_num = state.blocker_sample_num;
            if (ImGui::SliderInt("blocker_sample_num", &blocker_sample_num, 1, 64))
            {
                applyUiIntChange(
                    blocker_sample_num,
                    [this](int value) { return m_runtime_controller->setBlockerSampleNumFromUi(value); },
                    [](int) {});
            }

            int pcf_sample_num = state.pcf_sample_num;
            if (ImGui::SliderInt("pcf_sample_num", &pcf_sample_num, 1, 64))
            {
                applyUiIntChange(
                    pcf_sample_num,
                    [this](int value) { return m_runtime_controller->setPcfSampleNumFromUi(value); },
                    [](int) {});
            }

            float shadow_bias = state.shadow_bias;
            if (ImGui::SliderFloat("shadow bias", &shadow_bias, 0.0f, 0.005f, "%.7f", ImGuiSliderFlags_Logarithmic))
            {
                applyUiFloatChange(
                    shadow_bias,
                    [this](float value) { return m_runtime_controller->setShadowBiasFromUi(value); },
                    [](float) {});
            }

            float ssao_radius = state.ssao_radius;
            if (ImGui::SliderFloat("ssao_radius", &ssao_radius, 0.0f, 2.0f))
            {
                applyUiFloatChange(
                    ssao_radius,
                    [this](float value) { return m_runtime_controller->setSsaoRadiusFromUi(value); },
                    [](float) {});
            }

            int ssao_kernel_size = state.ssao_kernel_size;
            if (ImGui::SliderInt("ssao_kernel_size", &ssao_kernel_size, 16, 128))
            {
                applyUiIntChange(
                    ssao_kernel_size,
                    [this](int value) { return m_runtime_controller->setSsaoKernelSizeFromUi(value); },
                    [](int) {});
            }

            int denoise_size = state.denoise_size;
            if (ImGui::SliderInt("denoise_size", &denoise_size, 1, 9))
            {
                applyUiIntChange(
                    denoise_size,
                    [this](int value) { return m_runtime_controller->setDenoiseSizeFromUi(value); },
                    [](int) {});
            }

            float exposure = state.exposure;
            if (ImGui::SliderFloat("exposure", &exposure, 0.0f, 50.f))
            {
                applyUiFloatChange(
                    exposure,
                    [this](float value) { return m_runtime_controller->setExposureFromUi(value); },
                    [](float) {});
            }

            ImGui::TableNextColumn();
            ImGui::Text("Depth Completion");
            bool depth_occlusion_enabled = depth_params.enable_real_depth_occlusion != 0;
            if (ImGui::Checkbox("Depth Occlusion", &depth_occlusion_enabled))
            {
                m_runtime_controller->setDepthOcclusionEnabledFromUi(depth_occlusion_enabled);
            }

            int shadow_mode = (depth_params.shadow_mode == SHADOW_REAL_DEPTH) ? SHADOW_REAL_DEPTH : SHADOW_RECEIVER_PLANE;
            const char* shadow_mode_items[] = {"Receiver Plane", "Real Depth"};
            if (ImGui::Combo("Depth Shadow Mode", &shadow_mode, shadow_mode_items, IM_ARRAYSIZE(shadow_mode_items)))
            {
                m_runtime_controller->setShadowModeFromUi(shadow_mode);
            }

            if (depth_completion_active)
            {
                int valid_depth_min_mm = depth_params.valid_depth_min_mm;
                if (ImGui::SliderInt("valid_depth_min_mm", &valid_depth_min_mm, 1, 1000))
                {
                    applyUiIntChange(
                        valid_depth_min_mm,
                        [this](int value) { return m_runtime_controller->setDepthValidMinMmFromUi(value); },
                        [](int) {});
                }

                int kernel_radius = depth_params.kernel_radius;
                if (ImGui::SliderInt("kernel_radius", &kernel_radius, 1, 64))
                {
                    applyUiIntChange(
                        kernel_radius,
                        [this](int value) { return m_runtime_controller->setDepthKernelRadiusFromUi(value); },
                        [](int) {});
                }

                int top_k = depth_params.top_k;
                if (ImGui::SliderInt("top_k", &top_k, 1, 64))
                {
                    applyUiIntChange(
                        top_k,
                        [this](int value) { return m_runtime_controller->setDepthTopKFromUi(value); },
                        [](int) {});
                }

                float spatial_weight = depth_params.spatial_weight;
                if (ImGui::SliderFloat("spatial_weight", &spatial_weight, 0.0f, 1.0f))
                {
                    applyUiFloatChange(
                        spatial_weight,
                        [this](float value) { return m_runtime_controller->setDepthSpatialWeightFromUi(value); },
                        [](float) {});
                }

                float color_sigma = depth_params.color_sigma;
                if (ImGui::SliderFloat("color_sigma", &color_sigma, 0.001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic))
                {
                    applyUiFloatChange(
                        color_sigma,
                        [this](float value) { return m_runtime_controller->setDepthColorSigmaFromUi(value); },
                        [](float) {});
                }

                float edge_threshold = depth_params.edge_threshold;
                if (ImGui::SliderFloat("edge_threshold", &edge_threshold, 0.0f, 1.0f))
                {
                    applyUiFloatChange(
                        edge_threshold,
                        [this](float value) { return m_runtime_controller->setDepthEdgeThresholdFromUi(value); },
                        [](float) {});
                }

                int max_fill_passes = depth_params.max_fill_passes;
                if (ImGui::SliderInt("max_fill_passes", &max_fill_passes, 1, 15))
                {
                    applyUiIntChange(
                        max_fill_passes,
                        [this](int value) { return m_runtime_controller->setDepthMaxFillPassesFromUi(value); },
                        [](int) {});
                }
            }

            ImGui::EndTable();
        }
    }

    void MyGui::drawPerformanceInfo() const
    {
        ImGui::Text("Current FPS (ms):\t%.3f", global_params->currentFps);
        ImGui::Text("Step\t\tTime");
        ImGui::Text("1. advanceToNextFrame\t%.3f", global_params->render_func_times[0]);
        ImGui::Text("2. fix_depth\t\t%.3f", global_params->render_func_times[1]);
        ImGui::Text("3. copy color/depth\t%.3f", global_params->render_func_times[2]);
        ImGui::Text("4. mark dirty\t\t%.3f", global_params->render_func_times[3]);
        ImGui::Text("5. handleEvents\t\t%.3f", global_params->render_func_times[4]);
        ImGui::Text("6. update\t\t%.3f", global_params->render_func_times[5]);
        ImGui::Text("7. recordAndSubmit\t%.3f", global_params->render_func_times[6]);
        ImGui::Text("8. present\t\t%.3f", global_params->render_func_times[7]);

        ImGui::Separator();
        ImGui::Text("Server Timing:");
        ImGui::Text("render():\t\t%.3f ms", global_params->render_server_times[0]);
        ImGui::Text("getEncodeImage():\t%.3f ms", global_params->render_server_times[1]);
    }

    // Draw material controls for each unique PBR material.
    void MyGui::drawMaterialControls() const
    {
        std::unordered_set<vsg::PbrMaterial*> unique_material;
        for(auto& id_data: CADMesh::proto_id_to_data_map){
            std::string id = id_data.first;
            ProtoData* proto_data = id_data.second;
            if(proto_data->material_index < CADMesh::global_material_buffer->size()){
                vsg::PbrMaterial* pbr_ptr = static_cast<PbrMaterial*>(CADMesh::global_material_buffer->dataPointer(proto_data->material_index));
                if(unique_material.find(pbr_ptr) == unique_material.end()){
                    std::string label_prefix;
                    if (proto_data->material_source == ProtoData::MaterialSource::Fb && !proto_data->fb_color_group_key.empty()) {
                        label_prefix = "FB color " + proto_data->fb_color_group_key;
                    } else {
                        label_prefix = getMaterialPersistKey(id, proto_data);
                    }

                    ImGui::Text("%s", label_prefix.c_str());
                    std::string control_suffix = "##" + std::to_string(proto_data->material_index);
                    std::string metallic_name = "metallic" + control_suffix;
                    ImGui::SliderFloat(metallic_name.c_str(), &(pbr_ptr->metallicFactor), 0.0f, 5.0f);
                    std::string roughness_name = "roughness" + control_suffix;
                    ImGui::SliderFloat(roughness_name.c_str(), &(pbr_ptr->roughnessFactor), 0.0f, 5.0f);
                    std::string basecolor_name = "basecolor" + control_suffix;
                    ImGui::SliderFloat3(basecolor_name.c_str(), pbr_ptr->baseColorFactor.data(), 0.0f, 1.0f);
                    CADMesh::global_material_buffer->dirty();
                    unique_material.insert(pbr_ptr);
                }
            }
        }
    }

    // Draw line and point color controls.
    void MyGui::drawLinePointControls() const
    {
        if (!m_runtime_controller)
        {
            return;
        }

        const SceneRuntimeState& state = m_runtime_controller->state();
        float line_color[3] = {state.line_color.r, state.line_color.g, state.line_color.b};
        if (ImGui::SliderFloat3("line color", line_color, 0.0f, 1.0f))
        {
            applyUiVec3Change(
                vsg::vec3(line_color[0], line_color[1], line_color[2]),
                [this](const vsg::vec3& value) { return m_runtime_controller->setLineColorFromUi(value); },
                [](const vsg::vec3&) {});
        }

        float point_color[3] = {state.point_color.r, state.point_color.g, state.point_color.b};
        if (ImGui::SliderFloat3("point color", point_color, 0.0f, 1.0f))
        {
            applyUiVec3Change(
                vsg::vec3(point_color[0], point_color[1], point_color[2]),
                [this](const vsg::vec3& value) { return m_runtime_controller->setPointColorFromUi(value); },
                [](const vsg::vec3&) {});
        }
    }

    void MyGui::resetSelectedInstances() const
    {
        for (auto& state : m_instance_states) {
            if (!state.selected) continue;
            if (m_runtime_controller)
            {
                m_runtime_controller->setInstanceTransform(state.instance_name, m_runtime_controller->sceneTransformOrIdentity(state.instance_name));
            }
        }
        resetSharedTransform();
    }

    // Save the current instance transforms to Scenes.json.
    // Selected instances use the edited transform; unselected instances keep the original one.
    void MyGui::saveTransformsToScenesJson() const
    {
        if (!m_json_manager)
        {
            std::cerr << "JsonConfigManager is not initialized." << std::endl;
            return;
        }

        try {
            std::vector<SceneModelTransformSave> transforms;
            if (m_runtime_controller)
            {
                transforms = m_runtime_controller->state().scene_transforms;
                for (auto& item : transforms)
                {
                    const auto selected_it = std::find_if(
                        m_instance_states.begin(),
                        m_instance_states.end(),
                        [&item](const InstanceTransformState& ui_state) { return ui_state.instance_name == item.instance_name; });
                    if (selected_it != m_instance_states.end() && selected_it->selected)
                    {
                        item.transform = computeTransformedMatrix(*selected_it);
                    }
                }
            }

            std::string error_message;
            if (!m_runtime_controller || !m_runtime_controller->saveSceneTransforms(transforms, &error_message)) {
                std::cerr << "Failed to save transforms: " << error_message << std::endl;
                return;
            }

            std::cout << "Transforms saved to: " << CADMesh::scenes_json_path << " (scene_id=" << CADMesh::current_scene_id << ")" << std::endl;
            resetSharedTransform();
            applyPoseForAllStates();
        } catch (const std::exception& e) {
            std::cerr << "Failed to save transforms: " << e.what() << std::endl;
        }
    }

    void MyGui::drawInstanceTransformContent() const
    {
        if (m_instance_states.empty()) return;

        if (ImGui::Button("Select All")) {
            for (auto& state : m_instance_states) {
                state.selected = true;
                applyPoseForState(state);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Deselect All")) {
            for (auto& state : m_instance_states) {
                state.selected = false;
                applyPoseForState(state);
            }
            resetSharedTransform();
        }

        ImGui::BeginChild("InstanceList", ImVec2(0, 150), true);
        for (auto& state : m_instance_states) {
            bool selected_before = state.selected;
            if (ImGui::Checkbox(state.instance_name.c_str(), &state.selected)) {
                applyPoseForState(state);
                if (selected_before && !state.selected && !hasAnySelectedInstance()) {
                    resetSharedTransform();
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();

        int selected_count = 0;
        for (const auto& state : m_instance_states) if (state.selected) ++selected_count;
        ImGui::Text("Selected: %d", selected_count);

        bool any_selected = selected_count > 0;
        bool pose_changed = false;

        ImGui::Text("Rotation (deg)");
        for (int i = 0; i < 3; i++) {
            ImGui::PushID(i);
            std::string label = (i == 0) ? "Rx" : (i == 1) ? "Ry" : "Rz";
            ImGui::SetNextItemWidth(120);
            if (ImGui::SliderFloat(label.c_str(), &m_shared_rotate[i], m_rotate_min[i], m_rotate_max[i])) pose_changed = true;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            ImGui::InputFloat("min", &m_rotate_min[i]);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            ImGui::InputFloat("max", &m_rotate_max[i]);
            ImGui::PopID();
        }

        ImGui::Text("Scale (%%)");
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderFloat("Scale", &m_shared_scale_percent, m_scale_min, m_scale_max, "%.1f", ImGuiSliderFlags_Logarithmic)) pose_changed = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        ImGui::InputFloat("min##s", &m_scale_min);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        ImGui::InputFloat("max##s", &m_scale_max);

        ImGui::Text("Translation (m)");
        for (int i = 0; i < 3; i++) {
            ImGui::PushID(i + 3);
            std::string label = (i == 0) ? "Tx" : (i == 1) ? "Ty" : "Tz";
            ImGui::SetNextItemWidth(120);
            if (ImGui::SliderFloat(label.c_str(), &m_shared_translate[i], m_translate_min[i], m_translate_max[i])) pose_changed = true;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            ImGui::InputFloat("min", &m_translate_min[i]);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            ImGui::InputFloat("max", &m_translate_max[i]);
            ImGui::PopID();
        }

        if (pose_changed && any_selected) {
            applyPoseForAllStates();
        } else if (pose_changed && !any_selected) {
            resetSharedTransform();
        }

        if (ImGui::Button("Reset Selected"))
            resetSelectedInstances();
        ImGui::SameLine();
        if (ImGui::Button("Save to Scenes.json"))
            saveTransformsToScenesJson();
    }
    void MyGui::record(vsg::CommandBuffer& cb) const
    {
        if (!global_params->showGui) return;

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(500, 800), ImGuiCond_FirstUseEver);
        ImGui::Begin("AR/MR Engine");

        ImGui::Text("Render FPS:\t%.1f", 1000.0f / global_params->currentFps);
        ImGui::Text("Camera Tracking FPS:\t%.1f", vsgserver::renderer->camera_tracking_fps);
        ImGui::Separator();

        if (ImGui::Button("Save Params"))
            saveParams();

        if (ImGui::CollapsingHeader("Render Params", ImGuiTreeNodeFlags_DefaultOpen))
            drawRenderParams();
        if (ImGui::CollapsingHeader("Performance"))
            drawPerformanceInfo();
        if (ImGui::CollapsingHeader("Materials"))
            drawMaterialControls();
        if (ImGui::CollapsingHeader("Lines & Points"))
            drawLinePointControls();
        if (ImGui::CollapsingHeader("Instance Transforms", ImGuiTreeNodeFlags_DefaultOpen))
            drawInstanceTransformContent();

        ImGui::End();
    }

} // namespace gui






