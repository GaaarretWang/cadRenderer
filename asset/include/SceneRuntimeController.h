#pragma once

#include <memory>
#include <string>
#include <vector>

#include "RenderStateController.h"

class vsgRendererServer;

enum class RuntimeParam
{
    Hdr,
    BaseBrightness,
    DepthOcclusion,
    ShadowMode,
    ShadowType,
    Exposure,
    SsaoRadius,
    SsaoKernelSize,
    DenoiseSize,
    ShadowBias,
    BlockerSampleNum,
    PcfSampleNum,
    PcfSoftness,
    PcssSoftness,
    PcssSoftnessFalloff,
    LineColor,
    PointColor
};

class SceneRuntimeController
{
public:
    SceneRuntimeController(vsgRendererServer& renderer,
                           std::shared_ptr<RenderStateController> persistence_controller,
                           RenderStateHub& render_state,
                           SceneLinePointStyle& line_point_style,
                           float& base_brightness);

    void initializeForScene(int scene_id);
    void applyLoadedState();
    void clearServerDirtyFlags();
    void markServerDirty(RuntimeParam param);
    bool isServerDirty(RuntimeParam param) const;

    bool setHdrImageNum(int hdr_num);
    bool setHdrFromUi(int hdr_num);
    bool setBaseBrightness(float value);
    bool setBaseBrightnessFromUi(float value);
    bool setDepthOcclusionEnabled(bool enabled);
    bool setDepthOcclusionEnabledFromUi(bool enabled);
    bool setShadowMode(int mode);
    bool setShadowModeFromUi(int mode);
    bool setShadowType(int type);
    bool setShadowTypeFromUi(int type);
    bool setExposure(float value);
    bool setExposureFromUi(float value);
    bool setSsaoRadius(float value);
    bool setSsaoRadiusFromUi(float value);
    bool setSsaoKernelSize(int value);
    bool setSsaoKernelSizeFromUi(int value);
    bool setDenoiseSize(int value);
    bool setDenoiseSizeFromUi(int value);
    bool setShadowBias(float value);
    bool setShadowBiasFromUi(float value);
    bool setBlockerSampleNum(int value);
    bool setBlockerSampleNumFromUi(int value);
    bool setPcfSampleNum(int value);
    bool setPcfSampleNumFromUi(int value);
    bool setPcfSoftness(float value);
    bool setPcfSoftnessFromUi(float value);
    bool setPcssSoftness(float value);
    bool setPcssSoftnessFromUi(float value);
    bool setPcssSoftnessFalloff(float value);
    bool setPcssSoftnessFalloffFromUi(float value);
    bool setLineColor(const vsg::vec3& color);
    bool setLineColorFromUi(const vsg::vec3& color);
    bool setPointColor(const vsg::vec3& color);
    bool setPointColorFromUi(const vsg::vec3& color);
    bool setInstanceTransform(const std::string& instance_name, const vsg::dmat4& transform) const;

    bool saveRenderState(std::string* error_message = nullptr) const;
    bool saveBaseBrightness(std::string* error_message = nullptr) const;
    bool saveSceneTransforms(const std::vector<SceneModelTransformSave>& transforms, std::string* error_message = nullptr) const;

private:
    static bool nearlyEqual(float lhs, float rhs, float epsilon = 1e-6f);
    static bool sameVec3(const vsg::vec3& lhs, const vsg::vec3& rhs);
    bool canApplyUiChange(RuntimeParam param) const;

    vsgRendererServer& renderer_;
    std::shared_ptr<RenderStateController> persistence_controller_;
    RenderStateHub& render_state_;
    SceneLinePointStyle& line_point_style_;
    float& base_brightness_;
    int scene_id_ = -1;
    int applied_hdr_image_num_ = -1;
    bool hdr_server_dirty_ = false;
    bool base_brightness_server_dirty_ = false;
    bool depth_occlusion_server_dirty_ = false;
    bool shadow_mode_server_dirty_ = false;
    bool shadow_type_server_dirty_ = false;
    bool exposure_server_dirty_ = false;
    bool ssao_radius_server_dirty_ = false;
    bool ssao_kernel_size_server_dirty_ = false;
    bool denoise_size_server_dirty_ = false;
    bool shadow_bias_server_dirty_ = false;
    bool blocker_sample_num_server_dirty_ = false;
    bool pcf_sample_num_server_dirty_ = false;
    bool pcf_softness_server_dirty_ = false;
    bool pcss_softness_server_dirty_ = false;
    bool pcss_softness_falloff_server_dirty_ = false;
    bool line_color_server_dirty_ = false;
    bool point_color_server_dirty_ = false;
};
