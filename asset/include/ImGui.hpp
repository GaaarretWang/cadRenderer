#pragma once

#include <vsg/all.h>
#include <vsgImGui/imgui.h>
#include <vsgImGui/Texture.h>

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

        Params() {
            lightParams.lightColor[0][0] = 1.0f;
            lightParams.lightColor[0][1] = 1.0f;
            lightParams.lightColor[0][2] = 1.0f;
            lightParams.lightColor[0][3] = 1.0f;
        }
    };

    class MyGui : public Inherit<Command, MyGui>
    {
    public:
        vsg::ref_ptr<Params> params;

        MyGui(vsg::ref_ptr<Params> in_params, vsg::ref_ptr<vsg::Options> options = {}) :
            params(in_params)
        {
        }

        void compile(vsg::Context& context) override
        {
        }

        // Example here taken from the Dear imgui comments (mostly)
        void record(vsg::CommandBuffer& cb) const override
        {
            if (!params->showGui) return;

            ImGui::Begin("GUI"); // Create a window called "Hello, world!" and append into it.

            ImGui::Text("Material Control");
            ImGui::ColorEdit3("base color", (float*)&params->baseColor);
            ImGui::SliderFloat("metallic", &params->metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("roughness", &params->roughness, 0.01f, 1.0f);

            ImGui::InputFloat4("cube translate scale", (float*) & params->cubeTransform);

            ImGui::End();

            ImGui::Begin("Light Control");
            auto& lightParams = params->lightParams;
            if (ImGui::CollapsingHeader("Environment"))
            {
                ImGui::Checkbox("enabled", &lightParams.envmapEnabled);
                if (lightParams.envmapEnabled)
                    ImGui::SliderFloat("intensity", &lightParams.envmapStrength, 0.0f, 2.0f);
            }
            if (ImGui::CollapsingHeader("Directional"))
            {
                ImGui::SliderInt("count", &lightParams.directionalLightCount, 0, 4);
                for (int i = 0; i < lightParams.directionalLightCount; i++)
                {
                    ImGui::Text("light #%d", i + 1);
                    ImGui::PushID(i);
                    ImGui::ColorEdit4("color(rgb) intensity(a)", lightParams.lightColor[i]);
                    ImGui::SliderFloat("shadowIntensity", &lightParams.lightShadowStrength[i], 0.0f, 1.0f);
                    ImGui::SliderFloat2("dir theta/phi", lightParams.lightDirectionsThetaPhi[i], 0, 1);
                    ImGui::PopID();
                }
            }
            if (ImGui::CollapsingHeader("Debug Output"))
            {
                //ImGui::Text("%.3f")
            }
            ImGui::End();
        }
    };

} // namespace gui