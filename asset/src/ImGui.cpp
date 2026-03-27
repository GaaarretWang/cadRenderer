#include "ImGui.h"
// 在这里包含完整的vsgRendererServer头文件，此时前向声明已解决依赖问题
#include <vsgRendererServer.h>
#include <iomanip> // 用于std::setw格式化JSON

namespace vsgserver {
    vsgRendererServer* renderer = nullptr;
}

// 辅助函数：vsg列主序矩阵 → 行主序JSON数组（16个double）
static json matrixToRowMajorJson(const vsg::dmat4& mat)
{
    json arr = json::array();
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            arr.push_back(mat[col][row]);
    return arr;
}

// final = T * original * Rz * Ry * Rx * S
// 平移在左侧（世界空间），旋转和缩放在右侧（局部空间）
vsg::dmat4 InstanceTransformState::computeFinalTransform() const
{
    double tx = translate[0], ty = translate[1], tz = translate[2];
    double rx = rotate[0] * M_PI / 180.0;
    double ry = rotate[1] * M_PI / 180.0;
    double rz = rotate[2] * M_PI / 180.0;
    double s = scale_percent / 100.0;

    auto T = vsg::translate(tx, ty, tz);
    auto Rz = vsg::rotate(rz, 0.0, 0.0, 1.0);
    auto Ry = vsg::rotate(ry, 0.0, 1.0, 0.0);
    auto Rx = vsg::rotate(rx, 1.0, 0.0, 0.0);
    auto S = vsg::scale(s, s, s);

    return T * original_transform * Rz * Ry * Rx * S;
}

namespace gui
{
    vsg::ref_ptr<Params> global_params = Params::create();

    // 实现构造函数
    MyGui::MyGui(vsg::ref_ptr<vsg::Value<GlobalPCData>> pc_data,
                 const std::string& scenes_json_path,
                 const std::string& materials_json_path,
                 const std::string& lightinfo_json_path,
                 vsg::ref_ptr<vsg::Options> options)
        : m_pc_data(pc_data),
          m_scenes_json_path(scenes_json_path), m_materials_json_path(materials_json_path),
          m_lightinfo_json_path(lightinfo_json_path)
    {
        // 加载JSON文件初始化参数
        loadParams();
        // 初始化实例变换状态
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

    void MyGui::compile(vsg::Context& context)
    {
        // 空实现保持不变
    }

    // 实现加载JSON参数
    void MyGui::loadParams()
    {
        loadRenderParams();
        loadMaterialParams();
    }

    // 从Scenes.json加载当前场景的渲染参数
    void MyGui::loadRenderParams()
    {
        std::cout << "Loading render params from: " << m_scenes_json_path << std::endl;
        if (!fs::exists(m_scenes_json_path))
        {
            std::cerr << "Scenes JSON not found: " << m_scenes_json_path << std::endl;
            return;
        }

        try
        {
            json scenes_data;
            std::ifstream file(m_scenes_json_path);
            if (file.is_open())
            {
                file >> scenes_data;
                file.close();
            }

            // 按当前场景ID查找
            int scene_id = CADMesh::current_scene_id;
            json* target_scene = nullptr;
            if (scenes_data.contains("scenes"))
            {
                for (auto& scene : scenes_data["scenes"])
                {
                    if (scene["id"] == scene_id)
                    {
                        target_scene = &scene;
                        break;
                    }
                }
            }

            if (target_scene == nullptr)
            {
                std::cerr << "Scene id " << scene_id << " not found in Scenes.json" << std::endl;
                return;
            }

            // 加载 render_params
            if (target_scene->contains("render_params"))
            {
                auto& render_params = (*target_scene)["render_params"];
                if (render_params.contains("hdr_image_num"))
                    vsgserver::renderer->hdr_image_num = render_params["hdr_image_num"];
                // baseBrightness 不再从scenes.json加载，只从LightInfo.json读取
                if (render_params.contains("ssao_radius"))
                    m_pc_data->value().ssao_radius = render_params["ssao_radius"];
                if (render_params.contains("ssao_kernel_size"))
                    m_pc_data->value().ssao_kernel_size = render_params["ssao_kernel_size"];
                if (render_params.contains("exposure"))
                    m_pc_data->value().exposure = render_params["exposure"];
                if (render_params.contains("denoise_size"))
                    m_pc_data->value().denoise_size = render_params["denoise_size"];
                if (render_params.contains("shadow_bias"))
                    m_pc_data->value().shadow_bias = render_params["shadow_bias"];
                if (render_params.contains("blocker_sample_num"))
                    m_pc_data->value().blocker_sample_num = render_params["blocker_sample_num"];
                if (render_params.contains("pcf_sample_num"))
                    m_pc_data->value().pcf_sample_num = render_params["pcf_sample_num"];
                if (render_params.contains("shadow_type"))
                    m_pc_data->value().shadow_type = render_params["shadow_type"];
                if (render_params.contains("pcf_softness"))
                    pcf_softness = render_params["pcf_softness"];
                if (render_params.contains("pcss_softness"))
                    pcss_softness = render_params["pcss_softness"];
                if (render_params.contains("pcss_softness_falloff"))
                    pcss_softness_falloff = render_params["pcss_softness_falloff"];
            }

            // 加载 line_point_style
            if (target_scene->contains("line_point_style"))
            {
                auto& lp_style = (*target_scene)["line_point_style"];
                if (lp_style.contains("line_color"))
                {
                    auto& color = lp_style["line_color"];
                    auto& line_colors = CADMesh::dynamic_lines.colors->value();
                    line_colors = vsg::vec4(color[0], color[1], color[2], 1.0f);
                    CADMesh::dynamic_lines.colors->dirty();
                }
                if (lp_style.contains("point_color"))
                {
                    auto& color = lp_style["point_color"];
                    auto& point_colors = CADMesh::dynamic_points.colors->value();
                    point_colors = vsg::vec4(color[0], color[1], color[2], 1.0f);
                    CADMesh::dynamic_points.colors->dirty();
                }
            }

            // baseBrightness 从LightInfo.json读取
            int hdr_num = vsgserver::renderer->hdr_image_num;
            auto it = vsgserver::renderer->hdr_base_brightness.find(hdr_num);
            if (it != vsgserver::renderer->hdr_base_brightness.end()) {
                m_pc_data->value().baseBrightness = it->second;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to load render params: " << e.what() << std::endl;
        }
    }

    // 从Materials.json加载材质参数
    void MyGui::loadMaterialParams()
    {
        if (!fs::exists(m_materials_json_path))
        {
            std::cerr << "Materials JSON not found: " << m_materials_json_path << std::endl;
            return;
        }

        try
        {
            json mat_data;
            std::ifstream file(m_materials_json_path);
            if (file.is_open())
            {
                file >> mat_data;
                file.close();
            }

            if (mat_data.contains("material_params"))
            {
                auto& material_params = mat_data["material_params"];
                for (auto& id_data : CADMesh::proto_id_to_data_map)
                {
                    std::string id = id_data.first;
                    ProtoData* proto_data = id_data.second;
                    if (proto_data->material == nullptr)
                        continue;

                    std::string mat_key = extractMaterialKey(id);
                    if (material_params.contains(mat_key))
                    {
                        auto& md = material_params[mat_key];
                        vsg::PbrMaterial& pbr_mat = proto_data->material->value();

                        if (md.contains("metallicFactor"))
                            pbr_mat.metallicFactor = md["metallicFactor"];
                        if (md.contains("roughnessFactor"))
                            pbr_mat.roughnessFactor = md["roughnessFactor"];
                        if (md.contains("baseColorFactor"))
                        {
                            auto& base_color = md["baseColorFactor"];
                            pbr_mat.baseColorFactor = vsg::vec4(
                                base_color[0], base_color[1], base_color[2], base_color[3]
                            );
                        }
                        proto_data->material->dirty();
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to load material params: " << e.what() << std::endl;
        }
    }

    // 实现保存参数到JSON文件
    void MyGui::saveParams() const
    {
        saveRenderParams();
        saveMaterialParams();
    }

    // 保存baseBrightness到LightInfo.json
    void MyGui::saveBaseBrightnessToLightInfo() const
    {
        try {
            json json_data;
            if (fs::exists(m_lightinfo_json_path)) {
                std::ifstream file(m_lightinfo_json_path);
                if (file.is_open()) {
                    file >> json_data;
                    file.close();
                }
            }

            int hdr_num = vsgserver::renderer->hdr_image_num;
            std::string hdr_key = std::to_string(hdr_num);
            float bb = m_pc_data->value().baseBrightness;

            if (json_data.contains(hdr_key)) {
                json_data[hdr_key]["baseBrightness"] = bb;
            }

            std::ofstream file(m_lightinfo_json_path);
            if (file.is_open()) {
                file << std::setw(4) << json_data << std::endl;
                file.close();
                vsgserver::renderer->hdr_base_brightness[hdr_num] = bb;
                std::cout << "baseBrightness saved to LightInfo.json (HDR " << hdr_num << " = " << bb << ")" << std::endl;
            } else {
                std::cerr << "Failed to open LightInfo.json for writing: " << m_lightinfo_json_path << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to save baseBrightness: " << e.what() << std::endl;
        }
    }

    // 保存渲染参数到Scenes.json的当前场景
    void MyGui::saveRenderParams() const
    {
        try
        {
            json scenes_data;
            if (fs::exists(m_scenes_json_path))
            {
                std::ifstream file(m_scenes_json_path);
                if (file.is_open())
                {
                    file >> scenes_data;
                    file.close();
                }
            }

            // 查找当前场景
            int scene_id = CADMesh::current_scene_id;
            json* target_scene = nullptr;
            if (scenes_data.contains("scenes"))
            {
                for (auto& scene : scenes_data["scenes"])
                {
                    if (scene["id"] == scene_id)
                    {
                        target_scene = &scene;
                        break;
                    }
                }
            }

            if (target_scene == nullptr)
            {
                std::cout << "Scene id " << scene_id << " not found, auto-creating new scene" << std::endl;
                json new_scene;
                new_scene["id"] = scene_id;
                new_scene["name"] = "scene_" + std::to_string(scene_id);
                new_scene["description"] = "Auto-created scene";
                new_scene["models"] = json::array();
                new_scene["render_params"] = json::object();
                new_scene["line_point_style"] = {
                    {"line_color", {1.0, 1.0, 1.0}},
                    {"point_color", {1.0, 1.0, 1.0}}
                };
                scenes_data["scenes"].push_back(new_scene);
                target_scene = &scenes_data["scenes"].back();
            }

            // 更新 render_params
            const auto& pc_data = m_pc_data->value();
            json& render_params = (*target_scene)["render_params"];
            render_params["hdr_image_num"] = vsgserver::renderer->hdr_image_num;
            render_params["ssao_radius"] = pc_data.ssao_radius;
            render_params["ssao_kernel_size"] = pc_data.ssao_kernel_size;
            render_params["exposure"] = pc_data.exposure;
            render_params["denoise_size"] = pc_data.denoise_size;
            render_params["shadow_bias"] = pc_data.shadow_bias;
            render_params["blocker_sample_num"] = pc_data.blocker_sample_num;
            render_params["pcf_sample_num"] = pc_data.pcf_sample_num;
            render_params["shadow_type"] = pc_data.shadow_type;
            render_params["pcf_softness"] = pcf_softness;
            render_params["pcss_softness"] = pcss_softness;
            render_params["pcss_softness_falloff"] = pcss_softness_falloff;

            // 更新 line_point_style
            json& lp_style = (*target_scene)["line_point_style"];
            lp_style["line_color"] = {
                CADMesh::dynamic_lines.colors->value().r,
                CADMesh::dynamic_lines.colors->value().g,
                CADMesh::dynamic_lines.colors->value().b
            };
            lp_style["point_color"] = {
                CADMesh::dynamic_points.colors->value().r,
                CADMesh::dynamic_points.colors->value().g,
                CADMesh::dynamic_points.colors->value().b
            };

            // 写回文件
            std::ofstream file(m_scenes_json_path);
            if (file.is_open())
            {
                file << std::setw(4) << scenes_data << std::endl;
                file.close();
                std::cout << "Render params saved to: " << m_scenes_json_path << " (scene_id=" << scene_id << ")" << std::endl;
            }
            else
            {
                std::cerr << "Failed to open file for writing: " << m_scenes_json_path << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to save render params: " << e.what() << std::endl;
        }
    }

    // 保存材质参数到Materials.json
    void MyGui::saveMaterialParams() const
    {
        try
        {
            json mat_json;
            // 先加载原有数据（保留历史Key）
            if (fs::exists(m_materials_json_path))
            {
                std::ifstream file(m_materials_json_path);
                if (file.is_open())
                {
                    file >> mat_json;
                    file.close();
                }
            }

            json& material_params = mat_json["material_params"];
            std::unordered_set<vsg::PbrMaterial*> unique_material;
            for (auto& id_data : CADMesh::proto_id_to_data_map)
            {
                std::string id = id_data.first;
                ProtoData* proto_data = id_data.second;
                if (proto_data->material == nullptr)
                    continue;

                vsg::PbrMaterial* pbr_ptr = reinterpret_cast<PbrMaterial*>(proto_data->material->dataPointer());
                if (unique_material.find(pbr_ptr) != unique_material.end())
                    continue;

                std::string mat_key = extractMaterialKey(id);
                vsg::PbrMaterial& pbr_mat = proto_data->material->value();

                material_params[mat_key]["metallicFactor"] = pbr_mat.metallicFactor;
                material_params[mat_key]["roughnessFactor"] = pbr_mat.roughnessFactor;
                material_params[mat_key]["baseColorFactor"] = {
                    pbr_mat.baseColorFactor.r,
                    pbr_mat.baseColorFactor.g,
                    pbr_mat.baseColorFactor.b,
                    pbr_mat.baseColorFactor.a
                };

                unique_material.insert(pbr_ptr);
            }

            std::ofstream file(m_materials_json_path);
            if (file.is_open())
            {
                file << std::setw(4) << mat_json << std::endl;
                file.close();
                std::cout << "Material params saved to: " << m_materials_json_path << std::endl;
            }
            else
            {
                std::cerr << "Failed to open file for writing: " << m_materials_json_path << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to save material params: " << e.what() << std::endl;
        }
    }

    // 渲染参数面板
    void MyGui::drawRenderParams() const
    {
        ImGui::Text("hdr num:");
        for(int i = 1; i <= vsgserver::renderer->hdr_image_max_num; ++i){
            std::string num_str = std::to_string(i);
            if(i > 1) {
                ImGui::SameLine(0.0f, 5.0f);
            }
            if(ImGui::Button(num_str.c_str())){
                vsgserver::renderer->hdr_image_num = i;
                vsgserver::renderer->updateEnvLighting();
                // 自动更新 baseBrightness 为对应HDR的值
                auto it = vsgserver::renderer->hdr_base_brightness.find(i);
                if (it != vsgserver::renderer->hdr_base_brightness.end()) {
                    m_pc_data->value().baseBrightness = it->second;
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Global Render Params:");
        if (ImGui::RadioButton("PCF", m_pc_data->value().shadow_type == 0)){
            m_pc_data->value().shadow_type = 0;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("PCSS", m_pc_data->value().shadow_type == 1))
        {
            m_pc_data->value().shadow_type = 1;
        }

        if (m_pc_data->value().shadow_type == 0)
        {
            ImGui::SliderFloat("baseBrightness", &(m_pc_data->value().baseBrightness), 0.0f, 100.0f);
            if (ImGui::Button("Save baseBrightness"))
                saveBaseBrightnessToLightInfo();
            ImGui::SliderFloat("pcf_softness", &(pcf_softness), 0.0f, 100.0f);
            m_pc_data->value().softness = pcf_softness;
        }
        else if(m_pc_data->value().shadow_type == 1)
        {
            ImGui::SliderFloat("baseBrightness", &(m_pc_data->value().baseBrightness), 0.0f, 10.0f);
            if (ImGui::Button("Save baseBrightness"))
                saveBaseBrightnessToLightInfo();
            ImGui::SliderFloat("pcss_softness", &(pcss_softness), 0.0f, 2000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("pcss_softness_falloff", &(pcss_softness_falloff), 0.0f, 5.f);
            m_pc_data->value().softness = pcss_softness;
            m_pc_data->value().softness_falloff = pcss_softness_falloff;
        }
        ImGui::SliderInt("blocker_sample_num", &(m_pc_data->value().blocker_sample_num), 1, 64);
        ImGui::SliderInt("pcf_sample_num", &(m_pc_data->value().pcf_sample_num), 1, 64);
        ImGui::SliderFloat("shadow bias", &(m_pc_data->value().shadow_bias), 0.0f, 0.02f, "%.4f");

        ImGui::SliderFloat("ssao_radius", &(m_pc_data->value().ssao_radius), 0.0f, 2.0f);
        ImGui::SliderInt("ssao_kernel_size", &(m_pc_data->value().ssao_kernel_size), 16, 128);
        ImGui::SliderInt("denoise_size", &(m_pc_data->value().denoise_size), 1, 9);

        ImGui::SliderFloat("exposure", &(m_pc_data->value().exposure), 0.0f, 50.f);
    }

    // 性能信息面板
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

    // 材质控制面板
    void MyGui::drawMaterialControls() const
    {
        std::unordered_set<vsg::PbrMaterial*> unique_material;
        for(auto& id_data: CADMesh::proto_id_to_data_map){
            std::string id = id_data.first;
            ProtoData* proto_data = id_data.second;
            ImGui::Text("%s", id.c_str());
            if(proto_data->material != nullptr){
                vsg::PbrMaterial* pbr_ptr = reinterpret_cast<PbrMaterial*>(proto_data->material->dataPointer());
                if(unique_material.find(pbr_ptr) == unique_material.end()){
                    std::string metallic_name = "metallic" + std::to_string(unique_material.size());
                    ImGui::SliderFloat(metallic_name.c_str(), &(proto_data->material->value().metallicFactor), 0.0f, 5.0f);
                    std::string roughness_name = "roughness" + std::to_string(unique_material.size());
                    ImGui::SliderFloat(roughness_name.c_str(), &(proto_data->material->value().roughnessFactor), 0.0f, 5.0f);
                    std::string basecolor_name = "basecolor" + std::to_string(unique_material.size());
                    ImGui::SliderFloat3(basecolor_name.c_str(), proto_data->material->value().baseColorFactor.data(), 0.0f, 1.0f);
                    proto_data->material->dirty();
                    unique_material.insert(pbr_ptr);
                }
            }
        }
    }

    // 线/点样式控制面板
    void MyGui::drawLinePointControls() const
    {
        ImGui::SliderFloat3("line color", CADMesh::dynamic_lines.colors->value().data(), 0.0f, 1.0f);
        CADMesh::dynamic_lines.colors->dirty();
        ImGui::SliderFloat3("point color", CADMesh::dynamic_points.colors->value().data(), 0.0f, 1.0f);
        CADMesh::dynamic_points.colors->dirty();
    }

    void MyGui::resetSelectedInstances() const
    {
        for (auto& state : m_instance_states) {
            if (!state.selected) continue;
            state.translate[0] = state.translate[1] = state.translate[2] = 0.f;
            state.rotate[0] = state.rotate[1] = state.rotate[2] = 0.f;
            state.scale_percent = 100.0f;
            vsgserver::renderer->updateObjectPose(state.instance_name, state.original_transform);
        }
    }

    // 保存变换到Scenes.json
    void MyGui::saveTransformsToScenesJson() const
    {
        try {
            std::string json_path = CADMesh::scenes_json_path;
            json scenes_data;

            // 读取现有文件
            if (fs::exists(json_path)) {
                std::ifstream file(json_path);
                if (file.is_open()) {
                    file >> scenes_data;
                    file.close();
                }
            }

            if (!scenes_data.contains("scenes"))
                scenes_data["scenes"] = json::array();

            int scene_id = CADMesh::current_scene_id;

            // 查找目标场景
            json* target_scene = nullptr;
            int target_idx = -1;
            for (size_t i = 0; i < scenes_data["scenes"].size(); i++) {
                if (scenes_data["scenes"][i]["id"] == scene_id) {
                    target_scene = &scenes_data["scenes"][i];
                    target_idx = static_cast<int>(i);
                    break;
                }
            }

            // ID=-1：完全删除旧条目后重建
            if (scene_id == -1 && target_scene != nullptr) {
                scenes_data["scenes"].erase(scenes_data["scenes"].begin() + target_idx);
                target_scene = nullptr;
            }

            // 如果不存在则创建新条目
            if (target_scene == nullptr) {
                json new_scene;
                new_scene["id"] = scene_id;
                new_scene["models"] = json::array();
                if (scene_id != -1) {
                    new_scene["name"] = "scene_" + std::to_string(scene_id);
                    new_scene["description"] = "Saved from Instance Transform UI";
                }
                scenes_data["scenes"].push_back(new_scene);
                target_scene = &scenes_data["scenes"].back();
            }

            // 更新每个model的transform_sequence（行主序保存）
            auto& models = (*target_scene)["models"];
            for (auto& state : m_instance_states) {
                if (state.instance_name == "shadow_receiver") continue;

                // 查找对应的model条目
                bool found = false;
                for (auto& model : models) {
                    if (model["instance_name"] == state.instance_name) {
                        vsg::dmat4 final_mat = state.computeFinalTransform();
                        model["transform_sequence"] = json::array({matrixToRowMajorJson(final_mat)});
                        found = true;
                        break;
                    }
                }
                if (!found && scene_id == -1) {
                    json new_model;
                    new_model["instance_name"] = state.instance_name;
                    new_model["path"] = "";
                    vsg::dmat4 final_mat = state.computeFinalTransform();
                    new_model["transform_sequence"] = json::array({matrixToRowMajorJson(final_mat)});
                    models.push_back(new_model);
                }
            }

            // 保存shadow_receiver变换（行主序）
            for (auto& state : m_instance_states) {
                if (state.instance_name == "shadow_receiver") {
                    vsg::dmat4 final_mat = state.computeFinalTransform();
                    (*target_scene)["shadow_receiver_transform"] = json::array({matrixToRowMajorJson(final_mat)});
                    break;
                }
            }

            // 写回文件
            std::ofstream file(json_path);
            if (file.is_open()) {
                file << std::setw(4) << scenes_data << std::endl;
                file.close();
                std::cout << "Transforms saved to: " << json_path << " (scene_id=" << scene_id << ")" << std::endl;
            } else {
                std::cerr << "Failed to open Scenes.json for writing: " << json_path << std::endl;
            }

            // 保存后更新original_transform，使Reset恢复到新保存的矩阵
            for (auto& state : m_instance_states) {
                state.original_transform = state.computeFinalTransform();
                state.translate[0] = state.translate[1] = state.translate[2] = 0.f;
                state.rotate[0] = state.rotate[1] = state.rotate[2] = 0.f;
                state.scale_percent = 100.0f;
            }

        } catch (const std::exception& e) {
            std::cerr << "Failed to save transforms: " << e.what() << std::endl;
        }
    }

    // 实例变换面板（独立窗口）
    void MyGui::drawInstanceTransformPanel() const
    {
        if (m_instance_states.empty()) return;

        ImGui::SetNextWindowPos(ImVec2(370, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(500, 700), ImGuiCond_Always);
        ImGui::Begin("Instance Transforms");

        // 全选/全不选按钮
        if (ImGui::Button("Select All")) {
            for (auto& state : m_instance_states)
                state.selected = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Deselect All")) {
            for (auto& state : m_instance_states)
                state.selected = false;
        }

        // 实例列表
        ImGui::BeginChild("InstanceList", ImVec2(0, 150), true);
        for (auto& state : m_instance_states) {
            ImGui::Checkbox(state.instance_name.c_str(), &state.selected);
        }
        ImGui::EndChild();

        ImGui::Separator();

        // 为每个选中实例显示滑条控制
        for (auto& state : m_instance_states) {
            if (!state.selected) continue;

            ImGui::PushID(state.instance_name.c_str());
            ImGui::Text("%s", state.instance_name.c_str());

            // --- Rotation (deg) ---
            ImGui::Text("Rotation (deg)");
            for (int i = 0; i < 3; i++) {
                ImGui::PushID(i);
                std::string label = (i == 0) ? "Rx" : (i == 1) ? "Ry" : "Rz";
                ImGui::SetNextItemWidth(120);
                if (ImGui::SliderFloat(label.c_str(), &state.rotate[i], m_rotate_min[i], m_rotate_max[i]))
                    vsgserver::renderer->updateObjectPose(state.instance_name, state.computeFinalTransform());
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50);
                ImGui::InputFloat("min", &m_rotate_min[i]);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50);
                ImGui::InputFloat("max", &m_rotate_max[i]);
                ImGui::PopID();
            }

            // --- Scale (%) ---
            ImGui::Text("Scale (%%)");
            ImGui::SetNextItemWidth(120);
            if (ImGui::SliderFloat("Scale", &state.scale_percent, m_scale_min, m_scale_max, "%.1f", ImGuiSliderFlags_Logarithmic))
                vsgserver::renderer->updateObjectPose(state.instance_name, state.computeFinalTransform());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            ImGui::InputFloat("min##s", &m_scale_min);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            ImGui::InputFloat("max##s", &m_scale_max);

            // --- Translation (m) ---
            ImGui::Text("Translation (m)");
            for (int i = 0; i < 3; i++) {
                ImGui::PushID(i + 3);
                std::string label = (i == 0) ? "Tx" : (i == 1) ? "Ty" : "Tz";
                ImGui::SetNextItemWidth(120);
                if (ImGui::SliderFloat(label.c_str(), &state.translate[i], m_translate_min[i], m_translate_max[i]))
                    vsgserver::renderer->updateObjectPose(state.instance_name, state.computeFinalTransform());
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50);
                ImGui::InputFloat("min", &m_translate_min[i]);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50);
                ImGui::InputFloat("max", &m_translate_max[i]);
                ImGui::PopID();
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        // 重置和保存按钮
        if (ImGui::Button("Reset Selected"))
            resetSelectedInstances();
        ImGui::SameLine();
        if (ImGui::Button("Save to Scenes.json"))
            saveTransformsToScenesJson();

        ImGui::End();
    }

    // 重构后的record函数
    void MyGui::record(vsg::CommandBuffer& cb) const
    {
        if (!global_params->showGui) return;

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(350, 600), ImGuiCond_Always);
        ImGui::Begin("GUI");
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
        ImGui::End();

        // 独立窗口
        drawInstanceTransformPanel();
    }

} // namespace gui
