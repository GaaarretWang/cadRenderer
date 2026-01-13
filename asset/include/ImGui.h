#ifndef IMGUI_H
#pragma once
#define IMGUI_H
#include <vsg/all.h>
#include <vsgImGui/imgui.h>
#include <vsgImGui/Texture.h>
#include <vsgImGui/RenderImGui.h>
#include <vsgImGui/SendEventsToImGui.h>
// 移除直接包含 vsgRendererServer.h，改为前向声明
class vsgRendererServer;
#include <CADMesh.h>
// 引入JSON库
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <string>
#include <unordered_map>

// 简化JSON命名空间
using json = nlohmann::json;
namespace fs = std::filesystem;

struct GlobalPCData{
    vsg::vec3 camera_pos;
    float z_far;
    float lightSizeScale = 20;
    float baseBrightness = 2;
    float ssao_radius = 0.1;
    float exposure = 8;
    int ssao_kernel_size = 64;
    int shader_type;
    int width;
    int height;
    int denoise_size = 5;
};

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

    // 工具函数：提取路径最后一个/后的内容作为Key
    inline std::string extractMaterialKey(const std::string& full_path)
    {
        size_t last_slash = full_path.find_last_of('/');
        if (last_slash == std::string::npos)
            return full_path;
        return full_path.substr(last_slash + 1);
    }

    class MyGui : public Inherit<Command, MyGui>
    {
    public:
        vsg::ref_ptr<vsg::Value<GlobalPCData>> m_pc_data;
        std::string m_json_path; // JSON文件路径
        json m_json_data;        // 存储JSON数据
        vsgRendererServer* m_renderer; // 仅声明指针，前向声明已足够

        // 调整构造函数参数顺序，匹配你的创建代码：MyGui::create(this, pc_data, json_path)
        MyGui(vsgRendererServer* renderer,
              vsg::ref_ptr<vsg::Value<GlobalPCData>> pc_data, 
              const std::string& json_path,
              vsg::ref_ptr<vsg::Options> options = {});

        void compile(vsg::Context& context) override;

        // 声明加载参数函数（实现放cpp）
        void loadParams();

        // 保存参数到JSON文件（保留历史Key）
        void saveParams() const;

        // 声明record函数（实现放cpp）
        void record(vsg::CommandBuffer& cb) const override;
    };

} // namespace gui
#endif