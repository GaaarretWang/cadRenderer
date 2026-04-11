#pragma once

#include <memory>
#include <string>
#include <vector>

#include "RenderState.h"
#include "SceneStatePersistenceCoordinator.h"

class vsgRendererServer;

enum class RuntimeParam
{
    Hdr,
    BaseBrightness,
    DepthOcclusion,
    ShadowMode,
    DepthValidMinMm,
    DepthKernelRadius,
    DepthTopK,
    DepthSpatialWeight,
    DepthColorSigma,
    DepthEdgeThreshold,
    DepthMaxFillPasses,
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
                           std::shared_ptr<SceneStatePersistenceCoordinator> persistence_coordinator,
                           SceneRuntimeState& state);

    bool loadSceneState(int scene_id, std::string* error_message = nullptr);
    void initializeForScene(int scene_id);
    void applyLoadedState();
    void clearServerDirtyFlags();
    void markServerDirty(RuntimeParam param);
    bool isServerDirty(RuntimeParam param) const;

    const SceneRuntimeState& state() const { return state_; }
    SceneRuntimeState& state() { return state_; }

    bool setHdrImageNum(int hdr_num);
    bool setHdrFromUi(int hdr_num);
    bool setBaseBrightness(float value);
    bool setBaseBrightnessFromUi(float value);
    bool setDepthOcclusionEnabled(bool enabled);
    bool setDepthOcclusionEnabledFromUi(bool enabled);
    bool setShadowMode(int mode);
    bool setShadowModeFromUi(int mode);
    bool setDepthValidMinMm(int value);
    bool setDepthValidMinMmFromUi(int value);
    bool setDepthKernelRadius(int value);
    bool setDepthKernelRadiusFromUi(int value);
    bool setDepthTopK(int value);
    bool setDepthTopKFromUi(int value);
    bool setDepthSpatialWeight(float value);
    bool setDepthSpatialWeightFromUi(float value);
    bool setDepthColorSigma(float value);
    bool setDepthColorSigmaFromUi(float value);
    bool setDepthEdgeThreshold(float value);
    bool setDepthEdgeThresholdFromUi(float value);
    bool setDepthMaxFillPasses(int value);
    bool setDepthMaxFillPassesFromUi(int value);
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
    bool saveBaseBrightness(std::string* error_message = nullptr);
    bool saveSceneTransforms(const std::vector<SceneModelTransformSave>& transforms, std::string* error_message = nullptr);
    bool findSceneTransform(const std::string& instance_name, SceneModelTransformSave& out_transform) const;
    vsg::dmat4 sceneTransformOrIdentity(const std::string& instance_name) const;

private:
    static bool sameVec3(const vsg::vec3& lhs, const vsg::vec3& rhs);
    void applyDepthCompletionParamsToRenderer();
    bool canApplyUiChange(RuntimeParam param) const;
    void applyHdrStateToRenderer(bool refresh_env_lighting);
    void applyRenderModesToRenderer();
    void applyFrameParamsToPcData();
    void applyLinePointColors();
    void syncHdrBrightnessFromState();
    void replaceSceneTransforms(const std::vector<SceneModelTransformSave>& transforms);

    vsgRendererServer& renderer_;
    std::shared_ptr<SceneStatePersistenceCoordinator> persistence_coordinator_;
    SceneRuntimeState& state_;
    int scene_id_ = -1;
    int applied_hdr_image_num_ = -1;
    bool hdr_server_dirty_ = false;
    bool base_brightness_server_dirty_ = false;
    bool depth_occlusion_server_dirty_ = false;
    bool shadow_mode_server_dirty_ = false;
    bool depth_valid_min_mm_server_dirty_ = false;
    bool depth_kernel_radius_server_dirty_ = false;
    bool depth_top_k_server_dirty_ = false;
    bool depth_spatial_weight_server_dirty_ = false;
    bool depth_color_sigma_server_dirty_ = false;
    bool depth_edge_threshold_server_dirty_ = false;
    bool depth_max_fill_passes_server_dirty_ = false;
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
