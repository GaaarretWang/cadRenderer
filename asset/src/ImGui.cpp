#define _USE_MATH_DEFINES
#include "ImGui.h"
// Keep the renderer header here because the GUI writes directly into the server state.
#include <vsgRendererServer.h>

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
        m_scene_serializer = std::make_shared<SceneConfigSerializer>(m_json_manager);
        m_state_controller = std::make_shared<RenderStateController>(m_json_manager, m_scene_serializer);
        if (vsgserver::renderer)
        {
            m_runtime_controller = std::make_shared<SceneRuntimeController>(*vsgserver::renderer, m_state_controller, m_render_state, m_line_point_style, m_base_brightness);
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
            state.original_transform = CADMesh::scene_original_transforms[i];
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

        return T * state.original_transform * Rz * Ry * Rx * S;
    }

    void MyGui::applyPoseForState(const InstanceTransformState& state) const
    {
        const vsg::dmat4 target = state.selected ? computeTransformedMatrix(state) : state.original_transform;
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
        if (!m_json_manager)
        {
            std::cerr << "JsonConfigManager is not initialized." << std::endl;
            return;
        }
        if (!m_scene_serializer)
        {
            std::cerr << "SceneConfigSerializer is not initialized." << std::endl;
            return;
        }
        if (!m_state_controller)
        {
            std::cerr << "RenderStateController is not initialized." << std::endl;
            return;
        }

        RenderStateHub state;
        SceneLinePointStyle style;
        std::string error_message;
        if (!m_state_controller->loadRenderState(CADMesh::current_scene_id, state, style, &error_message))
        {
            std::cerr << "Failed to load render params: " << error_message << std::endl;
            return;
        }

        m_render_state = state;
        m_line_point_style = style;
        m_pc_data->value().ssao_radius = m_render_state.pipeline.frame_params.ssao_radius;
        m_pc_data->value().ssao_kernel_size = m_render_state.pipeline.frame_params.ssao_kernel_size;
        m_pc_data->value().exposure = m_render_state.pipeline.frame_params.exposure;
        m_pc_data->value().denoise_size = m_render_state.pipeline.frame_params.denoise_size;
        m_pc_data->value().shadow_bias = m_render_state.pipeline.frame_params.shadow_bias;
        m_pc_data->value().blocker_sample_num = m_render_state.pipeline.frame_params.blocker_sample_num;
        m_pc_data->value().pcf_sample_num = m_render_state.pipeline.frame_params.pcf_sample_num;
        m_pc_data->value().shadow_type = m_render_state.pipeline.frame_params.shadow_type;
        pcf_softness = m_render_state.pipeline.pcf_softness;
        pcss_softness = m_render_state.pipeline.pcss_softness;
        pcss_softness_falloff = m_render_state.pipeline.pcss_softness_falloff;

        CADMesh::dynamic_lines.colors->value() = vsg::vec4(m_line_point_style.line_color.r, m_line_point_style.line_color.g, m_line_point_style.line_color.b, 1.0f);
        CADMesh::dynamic_lines.colors->dirty();
        CADMesh::dynamic_points.colors->value() = vsg::vec4(m_line_point_style.point_color.r, m_line_point_style.point_color.g, m_line_point_style.point_color.b, 1.0f);
        CADMesh::dynamic_points.colors->dirty();

        if (m_runtime_controller)
        {
            m_runtime_controller->initializeForScene(CADMesh::current_scene_id);
            m_runtime_controller->applyLoadedState();
        }

        int hdr_num = m_render_state.pipeline.hdr_image_num;
        auto it = vsgserver::renderer->hdr_base_brightness.find(hdr_num);
        if (it != vsgserver::renderer->hdr_base_brightness.end())
        {
            m_base_brightness = it->second;
        }
        m_pc_data->value().baseBrightness = m_base_brightness;
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
            vsgserver::renderer->hdr_base_brightness[m_render_state.pipeline.hdr_image_num] = m_base_brightness;
            std::cout << "baseBrightness saved to LightInfo.json (HDR " << m_render_state.pipeline.hdr_image_num << " = " << m_base_brightness << ")" << std::endl;
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

    void MyGui::syncStateFromRuntime() const
    {
        if (!vsgserver::renderer)
        {
            return;
        }

        m_render_state.pipeline.hdr_image_num = vsgserver::renderer->hdr_image_num;
        m_render_state.pipeline.enable_real_depth_occlusion = vsgserver::renderer->enable_real_depth_occlusion;
        m_render_state.pipeline.shadow_mode = vsgserver::renderer->shadow_mode;

        m_render_state.pipeline.frame_params.ssao_radius = m_pc_data->value().ssao_radius;
        m_render_state.pipeline.frame_params.ssao_kernel_size = m_pc_data->value().ssao_kernel_size;
        m_render_state.pipeline.frame_params.exposure = m_pc_data->value().exposure;
        m_render_state.pipeline.frame_params.denoise_size = m_pc_data->value().denoise_size;
        m_render_state.pipeline.frame_params.shadow_bias = m_pc_data->value().shadow_bias;
        m_render_state.pipeline.frame_params.blocker_sample_num = m_pc_data->value().blocker_sample_num;
        m_render_state.pipeline.frame_params.pcf_sample_num = m_pc_data->value().pcf_sample_num;
        m_render_state.pipeline.frame_params.shadow_type = m_pc_data->value().shadow_type;
        m_render_state.pipeline.frame_params.baseBrightness = m_pc_data->value().baseBrightness;

        m_base_brightness = m_pc_data->value().baseBrightness;
        if (m_render_state.pipeline.frame_params.shadow_type == 0)
        {
            pcf_softness = m_pc_data->value().softness;
        }
        else
        {
            pcss_softness = m_pc_data->value().softness;
            pcss_softness_falloff = m_pc_data->value().softness_falloff;
        }

        if (CADMesh::dynamic_lines.colors)
        {
            const auto& line_color = CADMesh::dynamic_lines.colors->value();
            m_line_point_style.line_color = vsg::vec3(line_color.r, line_color.g, line_color.b);
        }
        if (CADMesh::dynamic_points.colors)
        {
            const auto& point_color = CADMesh::dynamic_points.colors->value();
            m_line_point_style.point_color = vsg::vec3(point_color.r, point_color.g, point_color.b);
        }
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
        ImGui::Text("hdr num:");
        for (int i = 1; i <= vsgserver::renderer->hdr_image_max_num; ++i) {
            std::string num_str = std::to_string(i);
            if (i > 1) {
                ImGui::SameLine(0.0f, 5.0f);
            }
            if (ImGui::Button(num_str.c_str())) {
                applyUiIntChange(
                    i,
                    [this](int hdr) { return m_runtime_controller->setHdrFromUi(hdr); },
                    [this](int) { m_pc_data->value().baseBrightness = m_base_brightness; });
            }
        }

        ImGui::Separator();
        ImGui::Text("Global Render Params:");
        bool depth_occlusion_enabled = m_render_state.pipeline.enable_real_depth_occlusion != 0;
        if (ImGui::Checkbox("Depth Occlusion", &depth_occlusion_enabled))
        {
            if (m_runtime_controller) m_runtime_controller->setDepthOcclusionEnabledFromUi(depth_occlusion_enabled);
        }

        int shadow_mode = (m_render_state.pipeline.shadow_mode == SHADOW_REAL_DEPTH) ? SHADOW_REAL_DEPTH : SHADOW_RECEIVER_PLANE;
        const char* shadow_mode_items[] = {"Receiver Plane", "Real Depth"};
        if (ImGui::Combo("Shadow Mode", &shadow_mode, shadow_mode_items, IM_ARRAYSIZE(shadow_mode_items)))
        {
            if (m_runtime_controller) m_runtime_controller->setShadowModeFromUi(shadow_mode);
        }

        if (ImGui::RadioButton("PCF", m_render_state.pipeline.frame_params.shadow_type == 0)){
            applyUiIntChange(
                0,
                [this](int type) { return m_runtime_controller->setShadowTypeFromUi(type); },
                [this](int type) { m_pc_data->value().shadow_type = type; });
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("PCSS", m_render_state.pipeline.frame_params.shadow_type == 1))
        {
            applyUiIntChange(
                1,
                [this](int type) { return m_runtime_controller->setShadowTypeFromUi(type); },
                [this](int type) { m_pc_data->value().shadow_type = type; });
        }

        if (ImGui::SliderFloat("baseBrightness", &m_base_brightness, 0.0f, 100.0f))
        {
            applyUiFloatChange(
                m_base_brightness,
                [this](float value) { return m_runtime_controller->setBaseBrightnessFromUi(value); },
                [this](float value) { m_pc_data->value().baseBrightness = value; });
        }
        if (ImGui::Button("Save baseBrightness"))
            saveBaseBrightnessToLightInfo();
        if (m_render_state.pipeline.frame_params.shadow_type == 0)
        {
            float softness = pcf_softness;
            if (ImGui::SliderFloat("pcf_softness", &softness, 0.0f, 100.0f))
            {
                applyUiFloatChange(
                    softness,
                    [this](float value) { return m_runtime_controller->setPcfSoftnessFromUi(value); },
                    [this](float value) {
                        pcf_softness = value;
                        m_pc_data->value().softness = value;
                    });
            }
        }
        else if(m_render_state.pipeline.frame_params.shadow_type == 1)
        {
            float softness = pcss_softness;
            if (ImGui::SliderFloat("pcss_softness", &softness, 0.0f, 0.1f, "%.7f", ImGuiSliderFlags_Logarithmic))
            {
                applyUiFloatChange(
                    softness,
                    [this](float value) { return m_runtime_controller->setPcssSoftnessFromUi(value); },
                    [this](float value) {
                        pcss_softness = value;
                        m_pc_data->value().softness = value;
                    });
            }

            float softness_falloff = pcss_softness_falloff;
            if (ImGui::SliderFloat("pcss_softness_falloff", &softness_falloff, 0.0f, 0.005f, "%.7f", ImGuiSliderFlags_Logarithmic))
            {
                applyUiFloatChange(
                    softness_falloff,
                    [this](float value) { return m_runtime_controller->setPcssSoftnessFalloffFromUi(value); },
                    [this](float value) {
                        pcss_softness_falloff = value;
                        m_pc_data->value().softness_falloff = value;
                    });
            }
        }
        int blocker_sample_num = m_pc_data->value().blocker_sample_num;
        if (ImGui::SliderInt("blocker_sample_num", &blocker_sample_num, 1, 64))
        {
            applyUiIntChange(
                blocker_sample_num,
                [this](int value) { return m_runtime_controller->setBlockerSampleNumFromUi(value); },
                [this](int value) { m_pc_data->value().blocker_sample_num = value; });
        }

        int pcf_sample_num = m_pc_data->value().pcf_sample_num;
        if (ImGui::SliderInt("pcf_sample_num", &pcf_sample_num, 1, 64))
        {
            applyUiIntChange(
                pcf_sample_num,
                [this](int value) { return m_runtime_controller->setPcfSampleNumFromUi(value); },
                [this](int value) { m_pc_data->value().pcf_sample_num = value; });
        }

        float shadow_bias = m_pc_data->value().shadow_bias;
        if (ImGui::SliderFloat("shadow bias", &shadow_bias, 0.0f, 0.005f, "%.7f", ImGuiSliderFlags_Logarithmic))
        {
            applyUiFloatChange(
                shadow_bias,
                [this](float value) { return m_runtime_controller->setShadowBiasFromUi(value); },
                [this](float value) { m_pc_data->value().shadow_bias = value; });
        }

        float ssao_radius = m_pc_data->value().ssao_radius;
        if (ImGui::SliderFloat("ssao_radius", &ssao_radius, 0.0f, 2.0f))
        {
            applyUiFloatChange(
                ssao_radius,
                [this](float value) { return m_runtime_controller->setSsaoRadiusFromUi(value); },
                [this](float value) { m_pc_data->value().ssao_radius = value; });
        }

        int ssao_kernel_size = m_pc_data->value().ssao_kernel_size;
        if (ImGui::SliderInt("ssao_kernel_size", &ssao_kernel_size, 16, 128))
        {
            applyUiIntChange(
                ssao_kernel_size,
                [this](int value) { return m_runtime_controller->setSsaoKernelSizeFromUi(value); },
                [this](int value) { m_pc_data->value().ssao_kernel_size = value; });
        }

        int denoise_size = m_pc_data->value().denoise_size;
        if (ImGui::SliderInt("denoise_size", &denoise_size, 1, 9))
        {
            applyUiIntChange(
                denoise_size,
                [this](int value) { return m_runtime_controller->setDenoiseSizeFromUi(value); },
                [this](int value) { m_pc_data->value().denoise_size = value; });
        }

        float exposure = m_pc_data->value().exposure;
        if (ImGui::SliderFloat("exposure", &exposure, 0.0f, 50.f))
        {
            applyUiFloatChange(
                exposure,
                [this](float value) { return m_runtime_controller->setExposureFromUi(value); },
                [this](float value) { m_pc_data->value().exposure = value; });
        }

        m_render_state.pipeline.frame_params.baseBrightness = m_base_brightness;
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
        float line_color[3] = {
            CADMesh::dynamic_lines.colors->value().r,
            CADMesh::dynamic_lines.colors->value().g,
            CADMesh::dynamic_lines.colors->value().b};
        if (ImGui::SliderFloat3("line color", line_color, 0.0f, 1.0f))
        {
            applyUiVec3Change(
                vsg::vec3(line_color[0], line_color[1], line_color[2]),
                [this](const vsg::vec3& value) { return m_runtime_controller->setLineColorFromUi(value); },
                [](const vsg::vec3& value) {
                    CADMesh::dynamic_lines.colors->value() = vsg::vec4(value.r, value.g, value.b, 1.0f);
                    CADMesh::dynamic_lines.colors->dirty();
                });
        }

        float point_color[3] = {
            CADMesh::dynamic_points.colors->value().r,
            CADMesh::dynamic_points.colors->value().g,
            CADMesh::dynamic_points.colors->value().b};
        if (ImGui::SliderFloat3("point color", point_color, 0.0f, 1.0f))
        {
            applyUiVec3Change(
                vsg::vec3(point_color[0], point_color[1], point_color[2]),
                [this](const vsg::vec3& value) { return m_runtime_controller->setPointColorFromUi(value); },
                [](const vsg::vec3& value) {
                    CADMesh::dynamic_points.colors->value() = vsg::vec4(value.r, value.g, value.b, 1.0f);
                    CADMesh::dynamic_points.colors->dirty();
                });
        }
    }

    void MyGui::resetSelectedInstances() const
    {
        for (auto& state : m_instance_states) {
            if (!state.selected) continue;
            if (m_runtime_controller)
            {
                m_runtime_controller->setInstanceTransform(state.instance_name, state.original_transform);
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
            transforms.reserve(m_instance_states.size());
            for (const auto& state : m_instance_states) {
                SceneModelTransformSave item;
                item.instance_name = state.instance_name;
                item.transform = state.selected ? computeTransformedMatrix(state) : state.original_transform;
                item.is_shadow_receiver = (state.instance_name == "shadow_receiver");
                if (!item.is_shadow_receiver) {
                    auto it = CADMesh::instance_name_to_rel_path.find(state.instance_name);
                    item.path = (it != CADMesh::instance_name_to_rel_path.end()) ? it->second : "";
                }
                transforms.push_back(std::move(item));
            }

            std::string error_message;
            if (!m_runtime_controller || !m_runtime_controller->saveSceneTransforms(transforms, &error_message)) {
                std::cerr << "Failed to save transforms: " << error_message << std::endl;
                return;
            }

            std::cout << "Transforms saved to: " << CADMesh::scenes_json_path << " (scene_id=" << CADMesh::current_scene_id << ")" << std::endl;
            for (auto& state : m_instance_states) {
                state.original_transform = state.selected ? computeTransformedMatrix(state) : state.original_transform;
            }
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
        syncStateFromRuntime();

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






