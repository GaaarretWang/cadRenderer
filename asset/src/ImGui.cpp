#include "ImGui.h"
// 在这里包含完整的vsgRendererServer头文件，此时前向声明已解决依赖问题
#include <vsgRendererServer.h>
#include <iomanip> // 用于std::setw格式化JSON

namespace gui
{
    vsg::ref_ptr<Params> global_params = Params::create();  // ✅ 在 cpp 中初始化

    // 实现构造函数
    MyGui::MyGui(vsgRendererServer* renderer,
                 vsg::ref_ptr<vsg::Value<GlobalPCData>> pc_data, 
                 const std::string& json_path,
                 vsg::ref_ptr<vsg::Options> options)
        : m_renderer(renderer), m_pc_data(pc_data), m_json_path(json_path)
    {
        // 加载JSON文件初始化参数
        loadParams();
    }

    void MyGui::compile(vsg::Context& context)
    {
        // 空实现保持不变
    }

    // 实现加载JSON参数
    void MyGui::loadParams()
    {
        std::cout << m_json_path << std::endl;
        // 1. 读取JSON文件，若无则创建空JSON
        if (fs::exists(m_json_path))
        {
            try
            {
                std::ifstream file(m_json_path);
                if (file.is_open())
                {
                    file >> m_json_data;
                    file.close();
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to load JSON file: " << e.what() << std::endl;
                m_json_data = json::object(); // 加载失败则初始化空对象
            }
        }
        else
        {
            m_json_data = json::object(); // 文件不存在则创建空JSON
        }

        // 2. 加载系统渲染参数
        if (m_json_data.contains("render_params"))
        {
            auto& render_params = m_json_data["render_params"];
            // 加载GlobalPCData中的渲染参数
            if (render_params.contains("baseBrightness"))
                m_pc_data->value().baseBrightness = render_params["baseBrightness"];
            if (render_params.contains("ssao_radius"))
                m_pc_data->value().ssao_radius = render_params["ssao_radius"];
            if (render_params.contains("ssao_kernel_size"))
                m_pc_data->value().ssao_kernel_size = render_params["ssao_kernel_size"];
            if (render_params.contains("exposure"))
                m_pc_data->value().exposure = render_params["exposure"];
            if (render_params.contains("denoise_size"))
                m_pc_data->value().denoise_size = render_params["denoise_size"];
            if (render_params.contains("shadow bias")){
                m_pc_data->value().shadow_bias = render_params["shadow bias"];
            }
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

        // 3. 加载材质参数（按需加载当前场景的material）
        if (m_json_data.contains("material_params"))
        {
            auto& material_params = m_json_data["material_params"];
            for (auto& id_data : CADMesh::proto_id_to_data_map)
            {
                std::string id = id_data.first;
                ProtoData* proto_data = id_data.second;
                if (proto_data->material == nullptr)
                    continue;
                
                // 提取材质Key
                std::string mat_key = extractMaterialKey(id);
                if (material_params.contains(mat_key))
                {
                    auto& mat_data = material_params[mat_key];
                    vsg::PbrMaterial& pbr_mat = proto_data->material->value();
                    
                    // 加载材质参数
                    if (mat_data.contains("metallicFactor"))
                        pbr_mat.metallicFactor = mat_data["metallicFactor"];
                    if (mat_data.contains("roughnessFactor"))
                        pbr_mat.roughnessFactor = mat_data["roughnessFactor"];
                    if (mat_data.contains("baseColorFactor"))
                    {
                        auto& base_color = mat_data["baseColorFactor"];
                        pbr_mat.baseColorFactor = vsg::vec4(
                            base_color[0], base_color[1], base_color[2], base_color[3]
                        );
                    }
                    proto_data->material->dirty(); // 标记脏数据，触发更新
                }
            }
        }

        // 4. 加载动态对象参数
        if (m_json_data.contains("dynamic_objects"))
        {
            auto& dynamic = m_json_data["dynamic_objects"];
            if (dynamic.contains("line_color"))
            {
                auto& color = dynamic["line_color"];
                auto& line_colors = CADMesh::dynamic_lines.colors->value();
                line_colors = vsg::vec4(color[0], color[1], color[2], 1.0f);
                CADMesh::dynamic_lines.colors->dirty();
            }
            if (dynamic.contains("point_color"))
            {
                auto& color = dynamic["point_color"];
                auto& point_colors = CADMesh::dynamic_points.colors->value();
                point_colors = vsg::vec4(color[0], color[1], color[2], 1.0f);
                CADMesh::dynamic_points.colors->dirty();
            }
        }
    }

    // 实现保存参数到JSON文件
    void MyGui::saveParams() const
    {
        try
        {
            json new_json_data;

            // 1. 先加载原有JSON数据（保留历史Key）
            if (fs::exists(m_json_path))
            {
                std::ifstream file(m_json_path);
                if (file.is_open())
                {
                    file >> new_json_data;
                    file.close();
                }
            }

            // 2. 更新系统渲染参数
            json& render_params = new_json_data["render_params"];
            const auto& pc_data = m_pc_data->value();
            render_params["baseBrightness"] = pc_data.baseBrightness;
            render_params["ssao_radius"] = pc_data.ssao_radius;
            render_params["ssao_kernel_size"] = pc_data.ssao_kernel_size;
            render_params["exposure"] = pc_data.exposure;
            render_params["denoise_size"] = pc_data.denoise_size;
            render_params["shadow bias"] = pc_data.shadow_bias;
            render_params["blocker_sample_num"] = pc_data.blocker_sample_num;
            render_params["pcf_sample_num"] = pc_data.pcf_sample_num;
            render_params["shadow_type"] = pc_data.shadow_type;
            render_params["pcf_softness"] = pcf_softness;
            render_params["pcss_softness"] = pcss_softness;
            render_params["pcss_softness_falloff"] = pcss_softness_falloff;

            // 保存Params中的系统参数
            if (global_params)
            {
                render_params["model_scale"] = global_params->model_scale;
                render_params["model_translate"] = {
                    global_params->model_translate[0],
                    global_params->model_translate[1],
                    global_params->model_translate[2]
                };
            }

            // 3. 更新材质参数（保留历史Key，覆盖当前场景的材质）
            json& material_params = new_json_data["material_params"];
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
                
                // 提取材质Key
                std::string mat_key = extractMaterialKey(id);
                vsg::PbrMaterial& pbr_mat = proto_data->material->value();
                
                // 保存材质参数
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

            // 4. 更新动态对象参数
            json& dynamic_objects = new_json_data["dynamic_objects"];
            dynamic_objects["line_color"] = {
                CADMesh::dynamic_lines.colors->value().r,
                CADMesh::dynamic_lines.colors->value().g,
                CADMesh::dynamic_lines.colors->value().b
            };
            dynamic_objects["point_color"] = {
                CADMesh::dynamic_points.colors->value().r,
                CADMesh::dynamic_points.colors->value().g,
                CADMesh::dynamic_points.colors->value().b
            };

            // 5. 写入JSON文件（格式化输出，便于阅读）
            std::ofstream file(m_json_path);
            if (file.is_open())
            {
                file << std::setw(4) << new_json_data << std::endl;
                file.close();
                std::cout << "Params saved to: " << m_json_path << std::endl;
            }
            else
            {
                std::cerr << "Failed to open file for writing: " << m_json_path << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to save params: " << e.what() << std::endl;
        }
    }

    // 实现record函数（核心GUI渲染逻辑）
    void MyGui::record(vsg::CommandBuffer& cb) const
    {
        if (!global_params->showGui) return;

        ImGui::Begin("GUI"); // Create a window called "Hello, world!" and append into it.
        if (ImGui::Button("Save Params"))
            saveParams();

        ImGui::Separator();
        ImGui::Text("hdr num:");
        for(int i = 1; i <= m_renderer->hdr_image_max_num; ++i){
            std::string num_str = std::to_string(i);
            if(i > 1) {
                ImGui::SameLine(0.0f, 5.0f);
            }
            if(ImGui::Button(num_str.c_str())){
                m_renderer->hdr_image_num = i;
                m_renderer->updateEnvLighting();
            }
        }

        ImGui::Separator();
        ImGui::Text("Current FPS (ms):\t%.3f", global_params->currentFps);
        ImGui::Text("Render Timings (ms):");

        ImGui::Separator();
        ImGui::Text("Global Render Params:");
        if (ImGui::RadioButton("PCF", m_pc_data->value().shadow_type == 0)){
            m_pc_data->value().shadow_type = 0;
        }
        ImGui::SameLine(); // 让两个选项并排显示（可选）
        if (ImGui::RadioButton("PCSS", m_pc_data->value().shadow_type == 1))
        {
            m_pc_data->value().shadow_type = 1;
        }

        if (m_pc_data->value().shadow_type == 0)
        {
            ImGui::SliderFloat("baseBrightness", &(m_pc_data->value().baseBrightness), 0.0f, 10.0f);
            ImGui::SliderFloat("pcf_softness", &(pcf_softness), 0.0f, 100.0f);
            m_pc_data->value().softness = pcf_softness;
        }
        else if(m_pc_data->value().shadow_type == 1)
        {
            ImGui::SliderFloat("baseBrightness", &(m_pc_data->value().baseBrightness), 0.0f, 10.0f);
            ImGui::SliderFloat("pcss_softness", &(pcss_softness), 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
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
        
        ImGui::Separator();
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
        ImGui::Separator();

        std::unordered_set<vsg::PbrMaterial*> unique_material;
        for(auto& id_data: CADMesh::proto_id_to_data_map){
            std::string id = id_data.first;
            ProtoData* proto_data = id_data.second;
            ImGui::Text(id.c_str());
            if(proto_data->material != nullptr){
                vsg::PbrMaterial* pbr_ptr = reinterpret_cast<PbrMaterial*>(proto_data->material->dataPointer());
                if(unique_material.find(pbr_ptr) == unique_material.end()){
                    float metallic = pbr_ptr->metallicFactor;
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
        ImGui::Separator();
        ImGui::Text("dynamic objects:");
        std::string line_color_str = "line color";
        ImGui::SliderFloat3(line_color_str.c_str(), CADMesh::dynamic_lines.colors->value().data(), 0.0f, 1.0f);
        CADMesh::dynamic_lines.colors->dirty();
        std::string point_color_str = "point color";
        ImGui::SliderFloat3(point_color_str.c_str(), CADMesh::dynamic_points.colors->value().data(), 0.0f, 1.0f);
        CADMesh::dynamic_points.colors->dirty();
        
        ImGui::End();
    }

} // namespace gui