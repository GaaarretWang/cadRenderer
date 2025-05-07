#pragma once

#include <vsg/all.h>
#include <vsgImGui/imgui.h>
#include <vsgImGui/Texture.h>
#include <vsgImGui/RenderImGui.h>
#include <vsgImGui/SendEventsToImGui.h>
#include <CADMesh.h>

namespace gui
{
    using namespace vsg;
    template<typename T>
    using ptr = vsg::ref_ptr<T>;
    
    struct LightParams
    {
        bool envmapEnabled = true;
        float envmapStrength = 1.0f;
        int directionalLightCount;
        float lightDirectionsThetaPhi[4][2];
        float lightShadowStrength[4];
        float lightColor[4][4];
    };

    struct SceneParams : public Inherit<Object, SceneParams>
    {
    };

    struct Params : public Inherit<Object, Params>
    {
        bool showGui = true; // you can toggle this with your own EventHandler and key
        float baseColor[3]{1.0f, 1.0f, 1.0f};
        float roughness = 0.5f;
        float metallic = 0.5f;
        float cubeTransform[4]{0.f, 0.f, 0.f, 0.2f};
        LightParams lightParams;
        float model_translate[3]{180000.0f, -180000.0f, 60000.0f};
        float model_scale = 100.0f;
        float currentFps = 0.0f;
        float render_server_times[2];
        float render_func_times[8];
        Params() {
            lightParams.lightColor[0][0] = 1.0f;
            lightParams.lightColor[0][1] = 1.0f;
            lightParams.lightColor[0][2] = 1.0f;
            lightParams.lightColor[0][3] = 1.0f;
        }
    };
    extern vsg::ref_ptr<Params> global_params;

    class MyGui : public Inherit<Command, MyGui>
    {
    public:
        MyGui(vsg::ref_ptr<vsg::Options> options = {})
        {
        }

        void compile(vsg::Context& context) override
        {
        }

        // Example here taken from the Dear imgui comments (mostly)
        void record(vsg::CommandBuffer& cb) const override
        {
            if (!global_params->showGui) return;

            ImGui::Begin("GUI"); // Create a window called "Hello, world!" and append into it.
            
            ImGui::Text("Current FPS (ms):\t%.3f", global_params->currentFps);
            ImGui::Text("Render Timings (ms):");
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
                        // vsg::PbrMaterial a;
                        // a.metallicFactor = 0;
                        std::string metallic_name = "metallic" + std::to_string(unique_material.size());
                        ImGui::SliderFloat(metallic_name.c_str(), &(proto_data->material->value().metallicFactor), 0.0f, 1.0f);
                        std::string roughness_name = "roughness" + std::to_string(unique_material.size());
                        ImGui::SliderFloat(roughness_name.c_str(), &(proto_data->material->value().roughnessFactor), 0.0f, 1.0f);
                        std::string basecolor_name = "basecolor" + std::to_string(unique_material.size());
                        // float basecolor[3] = {};
                        ImGui::SliderFloat3(basecolor_name.c_str(), proto_data->material->value().baseColorFactor.data(), 0.0f, 1.0f);  
                        // proto_data->material->value().baseColorFactor = vec4(basecolor[0], basecolor[1], basecolor[2], 1.0);
                                              // pbr_ptr->metallicFactor = metallic;
                        // std::cout << "proto_data->material->value().metallicFactor " << proto_data->material->value().metallicFactor << std::endl;
                        // proto_data->material->value().metallicFactor = 0.f;
                        // std::cout << "proto_data->material->value().metallicFactor " << proto_data->material->value().metallicFactor << std::endl;
                        proto_data->material->dirty();
                        // ImGui::SliderFloat("roughness", &(pbr_ptr->roughnessFactor), 0.01f, 1.0f);
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
            // ImGui::Text("Material Control");
            // ImGui::ColorEdit3("base color", (float*)&params->baseColor);
            // ImGui::SliderFloat("metallic", &params->metallic, 0.0f, 1.0f);
            // ImGui::SliderFloat("roughness", &params->roughness, 0.01f, 1.0f);

            // ImGui::InputFloat4("cube translate scale", (float*) & params->cubeTransform);

            // ImGui::End();

            // ImGui::Begin("Light Control");

            // // ImGui::Text("Resolotion is : 720*425");
            // //ImGui::Text("Current FPS is : ");
            // //ImGui::InputFloat("Current FPS is : ", &currentFps);
            // auto& lightParams = params->lightParams;
            // if (ImGui::CollapsingHeader("Environment"))
            // {
            //     ImGui::Checkbox("enabled", &lightParams.envmapEnabled);
            //     if (lightParams.envmapEnabled)
            //         ImGui::SliderFloat("intensity", &lightParams.envmapStrength, 0.0f, 2.0f);
            // }
            // if (ImGui::CollapsingHeader("Directional"))
            // {
            //     ImGui::SliderInt("count", &lightParams.directionalLightCount, 0, 4);
            //     for (int i = 0; i < lightParams.directionalLightCount; i++)
            //     {
            //         ImGui::Text("light #%d", i + 1);
            //         ImGui::PushID(i);
            //         ImGui::ColorEdit4("color(rgb) intensity(a)", lightParams.lightColor[i]);
            //         ImGui::SliderFloat("shadowIntensity", &lightParams.lightShadowStrength[i], 0.0f, 1.0f);
            //         ImGui::SliderFloat2("dir theta/phi", lightParams.lightDirectionsThetaPhi[i], 0, 1);
            //         ImGui::PopID();
            //     }
            // }
            // if (ImGui::CollapsingHeader("Debug Output"))
            // {
            //     // ImGui::SliderFloat3("model translate1", params->model_translate, 0.0f, 100.0f);

            //     ImGui::InputFloat3("model translate", params->model_translate);
            //     ImGui::SliderFloat("model scale", &params->model_scale, 1.00f, 100.0f);
            //     ImGui::Text("Resolotion is : 1440*850");
            //     ImGui::Text("%.2f", params->currentFps);
            // }
            ImGui::End();
        }
    };

} // namespace gui