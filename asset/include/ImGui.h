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
#include <json/json.hpp>
#include <fstream>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <cmath>
#include <memory>
#include "JsonConfigManager.h"
#include "RenderStateController.h"
#include "SceneConfigSerializer.h"
#include "MyMask.h"

// 简化JSON命名空间
using json = nlohmann::json;
namespace fs = std::filesystem;

struct GlobalPCData{
    vsg::mat4 last_view;
    vsg::vec3 camera_pos;
    float softness = 1;
    float baseBrightness = 2;
    float ssao_radius = 0.1;
    float exposure = 8;
    float softness_falloff = 1;
    float shadow_bias = 0.0001;
    int ssao_kernel_size = 64;
    int denoise_size = 5;
    int blocker_sample_num = 16;
    int pcf_sample_num = 16;
    int shadow_type = 1;
    uint32_t frame_num = 0;
};

// 实例变换状态
struct InstanceTransformState {
    std::string instance_name;
    vsg::dmat4 original_transform;
    bool selected = false;
};

// 全局renderer指针（initRenderer中设置）
namespace vsgserver {
    extern vsgRendererServer* renderer;
}

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

    struct Params : public Inherit<Object, Params>
    {
        bool showGui = true; // you can toggle this with your own EventHandler and key
        float baseColor[3]{1.0f, 1.0f, 1.0f};
        float roughness = 0.5f;
        float metallic = 0.5f;
        float cubeTransform[4]{0.f, 0.f, 0.f, 0.2f};
        LightParams lightParams;
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
        std::string m_scenes_json_path;     // data/json/Scenes.json
        std::string m_materials_json_path;  // data/json/Materials.json
        std::string m_lightinfo_json_path;  // data/json/LightInfo.json
        std::shared_ptr<JsonConfigManager> m_json_manager;
        std::shared_ptr<RenderStateController> m_state_controller;
        std::shared_ptr<SceneConfigSerializer> m_scene_serializer;
        mutable RenderStateHub m_render_state;
        mutable float pcf_softness;
        mutable float pcss_softness;
        mutable float pcss_softness_falloff;

        // 实例变换状态
        mutable std::vector<InstanceTransformState> m_instance_states;
        // 滑条范围
        mutable float m_rotate_min[3] = {-180.f, -180.f, -180.f};
        mutable float m_rotate_max[3] = {180.f, 180.f, 180.f};
        mutable float m_scale_min = 50.f;
        mutable float m_scale_max = 200.f;
        mutable float m_translate_min[3] = {-2.f, -2.f, -2.f};
        mutable float m_translate_max[3] = {2.f, 2.f, 2.f};
        mutable float m_shared_translate[3] = {0.f, 0.f, 0.f};
        mutable float m_shared_rotate[3] = {0.f, 0.f, 0.f};
        mutable float m_shared_scale_percent = 100.0f;

        // 调整构造函数参数，接收三个JSON路径
        MyGui(vsg::ref_ptr<vsg::Value<GlobalPCData>> pc_data,
              const std::string& scenes_json_path,
              const std::string& materials_json_path,
              const std::string& lightinfo_json_path,
              vsg::ref_ptr<vsg::Options> options = {});

        void compile(vsg::Context& context) override;

        // 声明加载参数函数（实现放cpp）
        void loadParams();

        // 保存参数到JSON文件（保留历史Key）
        void saveParams() const;

        // 声明record函数（实现放cpp）
        void record(vsg::CommandBuffer& cb) const override;

    private:
        void loadRenderParams();
        void loadMaterialParams();
        void saveRenderParams() const;
        void saveMaterialParams() const;
        void initInstanceStates();
        void drawRenderParams() const;
        void drawPerformanceInfo() const;
        void drawMaterialControls() const;
        void drawLinePointControls() const;
        void drawInstanceTransformContent() const;
        void saveBaseBrightnessToLightInfo() const;
        void resetSelectedInstances() const;
        void saveTransformsToScenesJson() const;
        void resetSharedTransform() const;
        bool hasAnySelectedInstance() const;
        vsg::dmat4 computeTransformedMatrix(const InstanceTransformState& state) const;
        void applyPoseForState(const InstanceTransformState& state) const;
        void applyPoseForAllStates() const;
    };

} // namespace gui
#endif
