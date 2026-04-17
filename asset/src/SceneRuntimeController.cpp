#include "SceneRuntimeController.h"

#include <algorithm>

#include "CADMesh.h"
#include "vsgRendererServer.h"

namespace
{
constexpr int kDefaultHdrMin = 1;

int normalizeHdrImageMaxNum(int value)
{
    return std::max(value, kDefaultHdrMin);
}

int clampHdrImageNum(int value, int hdr_image_max_num)
{
    return std::clamp(value, kDefaultHdrMin, normalizeHdrImageMaxNum(hdr_image_max_num));
}

SceneRuntimeState::DepthCompletionParams normalizeDepthCompletionParams(SceneRuntimeState::DepthCompletionParams params)
{
    params.enable_real_depth_occlusion = params.enable_real_depth_occlusion != 0 ? 1 : 0;
    params.shadow_mode = (params.shadow_mode == SHADOW_REAL_DEPTH) ? SHADOW_REAL_DEPTH : SHADOW_RECEIVER_PLANE;
    params.valid_depth_min_mm = std::clamp(params.valid_depth_min_mm, 1, 1000);
    params.kernel_radius = std::clamp(params.kernel_radius, 1, 64);
    params.top_k = std::clamp(params.top_k, 1, 64);
    params.spatial_weight = std::clamp(params.spatial_weight, 0.0f, 1.0f);
    params.color_sigma = std::clamp(params.color_sigma, 0.001f, 1.0f);
    params.edge_threshold = std::clamp(params.edge_threshold, 0.0f, 1.0f);
    params.max_fill_passes = std::clamp(params.max_fill_passes, 1, 15);
    return params;
}
}

SceneRuntimeController::SceneRuntimeController(vsgRendererServer& renderer,
                                               std::shared_ptr<SceneStatePersistenceCoordinator> persistence_coordinator,
                                               SceneRuntimeState& state)
    : renderer_(renderer),
      persistence_coordinator_(std::move(persistence_coordinator)),
      state_(state)
{
}

bool SceneRuntimeController::loadSceneState(int scene_id, std::string* error_message)
{
    if (!persistence_coordinator_)
    {
        if (error_message) *error_message = "SceneStatePersistenceCoordinator is not initialized.";
        return false;
    }

    SceneRuntimeState loaded_state;
    if (!persistence_coordinator_->loadSceneState(scene_id, loaded_state, error_message))
    {
        return false;
    }

    initializeForScene(scene_id);
    state_ = std::move(loaded_state);
    applyLoadedState();
    clearServerDirtyFlags();
    return true;
}

void SceneRuntimeController::initializeForScene(int scene_id)
{
    scene_id_ = scene_id;
    applied_hdr_image_num_ = -1;
    clearServerDirtyFlags();
}

void SceneRuntimeController::applyHdrStateToRenderer(bool refresh_env_lighting)
{
    state_.hdr_image_max_num = normalizeHdrImageMaxNum(state_.hdr_image_max_num);
    state_.hdr_image_num = clampHdrImageNum(state_.hdr_image_num, state_.hdr_image_max_num);
    renderer_.hdr_image_max_num = state_.hdr_image_max_num;
    renderer_.hdr_image_num = state_.hdr_image_num;

    syncHdrBrightnessFromState();

    if (refresh_env_lighting &&
        applied_hdr_image_num_ != state_.hdr_image_num &&
        renderer_.view &&
        renderer_.window &&
        renderer_.device)
    {
        renderer_.requestEnvLightingUpdate();
        applied_hdr_image_num_ = state_.hdr_image_num;
    }
}

void SceneRuntimeController::applyDepthCompletionParamsToRenderer()
{
    state_.depth_completion_params = normalizeDepthCompletionParams(state_.depth_completion_params);
    renderer_.setDepthCompletionParams(state_.depth_completion_params);
}

void SceneRuntimeController::applyRenderModesToRenderer()
{
    applyDepthCompletionParamsToRenderer();
    renderer_.syncGlobalBufferData();
}

void SceneRuntimeController::syncHdrBrightnessFromState()
{
    state_.hdr_image_max_num = normalizeHdrImageMaxNum(state_.hdr_image_max_num);
    state_.hdr_image_num = clampHdrImageNum(state_.hdr_image_num, state_.hdr_image_max_num);
    renderer_.hdr_base_brightness = state_.hdr_base_brightness;
    const auto it = state_.hdr_base_brightness.find(state_.hdr_image_num);
    if (it != state_.hdr_base_brightness.end())
    {
        state_.baseBrightness = it->second;
    }
}

void SceneRuntimeController::applyFrameParamsToPcData()
{
    if (!renderer_.pc_data)
    {
        return;
    }

    renderer_.pc_data->value().baseBrightness = state_.baseBrightness;
    renderer_.pc_data->value().ssao_radius = state_.ssao_radius;
    renderer_.pc_data->value().ssao_kernel_size = state_.ssao_kernel_size;
    renderer_.pc_data->value().exposure = state_.exposure;
    renderer_.pc_data->value().denoise_size = state_.denoise_size;
    renderer_.pc_data->value().shadow_bias = state_.shadow_bias;
    renderer_.pc_data->value().blocker_sample_num = state_.blocker_sample_num;
    renderer_.pc_data->value().pcf_sample_num = state_.pcf_sample_num;
    renderer_.pc_data->value().shadow_type = state_.shadow_type;
    renderer_.pc_data->value().softness = (state_.shadow_type == 0) ? state_.pcf_softness : state_.pcss_softness;
    renderer_.pc_data->value().softness_falloff = state_.pcss_softness_falloff;
}

void SceneRuntimeController::applyLinePointColors()
{
    if (CADMesh::dynamic_lines.colors)
    {
        CADMesh::dynamic_lines.colors->value() = vsg::vec4(state_.line_color.r, state_.line_color.g, state_.line_color.b, 1.0f);
        CADMesh::dynamic_lines.colors->dirty();
    }
    if (CADMesh::dynamic_points.colors)
    {
        CADMesh::dynamic_points.colors->value() = vsg::vec4(state_.point_color.r, state_.point_color.g, state_.point_color.b, 1.0f);
        CADMesh::dynamic_points.colors->dirty();
    }
}

void SceneRuntimeController::applyLoadedState()
{
    applyHdrStateToRenderer(true);
    applyRenderModesToRenderer();
    applyFrameParamsToPcData();
    applyLinePointColors();
}

void SceneRuntimeController::clearServerDirtyFlags()
{
    hdr_server_dirty_ = false;
    base_brightness_server_dirty_ = false;
    depth_occlusion_server_dirty_ = false;
    shadow_mode_server_dirty_ = false;
    depth_valid_min_mm_server_dirty_ = false;
    depth_kernel_radius_server_dirty_ = false;
    depth_top_k_server_dirty_ = false;
    depth_spatial_weight_server_dirty_ = false;
    depth_color_sigma_server_dirty_ = false;
    depth_edge_threshold_server_dirty_ = false;
    depth_max_fill_passes_server_dirty_ = false;
    shadow_type_server_dirty_ = false;
    exposure_server_dirty_ = false;
    ssao_radius_server_dirty_ = false;
    ssao_kernel_size_server_dirty_ = false;
    denoise_size_server_dirty_ = false;
    shadow_bias_server_dirty_ = false;
    blocker_sample_num_server_dirty_ = false;
    pcf_sample_num_server_dirty_ = false;
    pcf_softness_server_dirty_ = false;
    pcss_softness_server_dirty_ = false;
    pcss_softness_falloff_server_dirty_ = false;
    line_color_server_dirty_ = false;
    point_color_server_dirty_ = false;
}

void SceneRuntimeController::markServerDirty(RuntimeParam param)
{
    switch (param)
    {
    case RuntimeParam::Hdr: hdr_server_dirty_ = true; break;
    case RuntimeParam::BaseBrightness: base_brightness_server_dirty_ = true; break;
    case RuntimeParam::DepthOcclusion: depth_occlusion_server_dirty_ = true; break;
    case RuntimeParam::ShadowMode: shadow_mode_server_dirty_ = true; break;
    case RuntimeParam::DepthValidMinMm: depth_valid_min_mm_server_dirty_ = true; break;
    case RuntimeParam::DepthKernelRadius: depth_kernel_radius_server_dirty_ = true; break;
    case RuntimeParam::DepthTopK: depth_top_k_server_dirty_ = true; break;
    case RuntimeParam::DepthSpatialWeight: depth_spatial_weight_server_dirty_ = true; break;
    case RuntimeParam::DepthColorSigma: depth_color_sigma_server_dirty_ = true; break;
    case RuntimeParam::DepthEdgeThreshold: depth_edge_threshold_server_dirty_ = true; break;
    case RuntimeParam::DepthMaxFillPasses: depth_max_fill_passes_server_dirty_ = true; break;
    case RuntimeParam::ShadowType: shadow_type_server_dirty_ = true; break;
    case RuntimeParam::Exposure: exposure_server_dirty_ = true; break;
    case RuntimeParam::SsaoRadius: ssao_radius_server_dirty_ = true; break;
    case RuntimeParam::SsaoKernelSize: ssao_kernel_size_server_dirty_ = true; break;
    case RuntimeParam::DenoiseSize: denoise_size_server_dirty_ = true; break;
    case RuntimeParam::ShadowBias: shadow_bias_server_dirty_ = true; break;
    case RuntimeParam::BlockerSampleNum: blocker_sample_num_server_dirty_ = true; break;
    case RuntimeParam::PcfSampleNum: pcf_sample_num_server_dirty_ = true; break;
    case RuntimeParam::PcfSoftness: pcf_softness_server_dirty_ = true; break;
    case RuntimeParam::PcssSoftness: pcss_softness_server_dirty_ = true; break;
    case RuntimeParam::PcssSoftnessFalloff: pcss_softness_falloff_server_dirty_ = true; break;
    case RuntimeParam::LineColor: line_color_server_dirty_ = true; break;
    case RuntimeParam::PointColor: point_color_server_dirty_ = true; break;
    }
}

bool SceneRuntimeController::isServerDirty(RuntimeParam param) const
{
    switch (param)
    {
    case RuntimeParam::Hdr: return hdr_server_dirty_;
    case RuntimeParam::BaseBrightness: return base_brightness_server_dirty_;
    case RuntimeParam::DepthOcclusion: return depth_occlusion_server_dirty_;
    case RuntimeParam::ShadowMode: return shadow_mode_server_dirty_;
    case RuntimeParam::DepthValidMinMm: return depth_valid_min_mm_server_dirty_;
    case RuntimeParam::DepthKernelRadius: return depth_kernel_radius_server_dirty_;
    case RuntimeParam::DepthTopK: return depth_top_k_server_dirty_;
    case RuntimeParam::DepthSpatialWeight: return depth_spatial_weight_server_dirty_;
    case RuntimeParam::DepthColorSigma: return depth_color_sigma_server_dirty_;
    case RuntimeParam::DepthEdgeThreshold: return depth_edge_threshold_server_dirty_;
    case RuntimeParam::DepthMaxFillPasses: return depth_max_fill_passes_server_dirty_;
    case RuntimeParam::ShadowType: return shadow_type_server_dirty_;
    case RuntimeParam::Exposure: return exposure_server_dirty_;
    case RuntimeParam::SsaoRadius: return ssao_radius_server_dirty_;
    case RuntimeParam::SsaoKernelSize: return ssao_kernel_size_server_dirty_;
    case RuntimeParam::DenoiseSize: return denoise_size_server_dirty_;
    case RuntimeParam::ShadowBias: return shadow_bias_server_dirty_;
    case RuntimeParam::BlockerSampleNum: return blocker_sample_num_server_dirty_;
    case RuntimeParam::PcfSampleNum: return pcf_sample_num_server_dirty_;
    case RuntimeParam::PcfSoftness: return pcf_softness_server_dirty_;
    case RuntimeParam::PcssSoftness: return pcss_softness_server_dirty_;
    case RuntimeParam::PcssSoftnessFalloff: return pcss_softness_falloff_server_dirty_;
    case RuntimeParam::LineColor: return line_color_server_dirty_;
    case RuntimeParam::PointColor: return point_color_server_dirty_;
    }

    return false;
}

bool SceneRuntimeController::setHdrImageNum(int hdr_num)
{
    hdr_num = clampHdrImageNum(hdr_num, state_.hdr_image_max_num);

    const bool changed = state_.hdr_image_num != hdr_num;
    if (!changed)
    {
        return false;
    }

    state_.hdr_image_num = hdr_num;
    applyHdrStateToRenderer(true);
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setHdrFromUi(int hdr_num)
{
    return canApplyUiChange(RuntimeParam::Hdr) ? setHdrImageNum(hdr_num) : false;
}

bool SceneRuntimeController::setBaseBrightness(float value)
{
    if (state_.baseBrightness == value)
    {
        return false;
    }

    state_.baseBrightness = value;
    state_.hdr_base_brightness[state_.hdr_image_num] = value;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setBaseBrightnessFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::BaseBrightness) ? setBaseBrightness(value) : false;
}

bool SceneRuntimeController::setDepthOcclusionEnabled(bool enabled)
{
    const int normalized = enabled ? 1 : 0;
    if (state_.depth_completion_params.enable_real_depth_occlusion == normalized)
    {
        return false;
    }

    state_.depth_completion_params.enable_real_depth_occlusion = normalized;
    applyRenderModesToRenderer();
    return true;
}

bool SceneRuntimeController::setDepthOcclusionEnabledFromUi(bool enabled)
{
    return canApplyUiChange(RuntimeParam::DepthOcclusion) ? setDepthOcclusionEnabled(enabled) : false;
}

bool SceneRuntimeController::setShadowMode(int mode)
{
    const int normalized = (mode == SHADOW_REAL_DEPTH) ? SHADOW_REAL_DEPTH : SHADOW_RECEIVER_PLANE;
    if (state_.depth_completion_params.shadow_mode == normalized)
    {
        return false;
    }

    state_.depth_completion_params.shadow_mode = normalized;
    applyRenderModesToRenderer();
    return true;
}

bool SceneRuntimeController::setShadowModeFromUi(int mode)
{
    return canApplyUiChange(RuntimeParam::ShadowMode) ? setShadowMode(mode) : false;
}

bool SceneRuntimeController::setDepthValidMinMm(int value)
{
    const int normalized = normalizeDepthCompletionParams(state_.depth_completion_params).valid_depth_min_mm;
    const int clamped = std::clamp(value, 1, 1000);
    if (normalized == clamped)
    {
        return false;
    }

    state_.depth_completion_params.valid_depth_min_mm = clamped;
    applyDepthCompletionParamsToRenderer();
    return true;
}

bool SceneRuntimeController::setDepthValidMinMmFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::DepthValidMinMm) ? setDepthValidMinMm(value) : false;
}

bool SceneRuntimeController::setDepthKernelRadius(int value)
{
    const int clamped = std::clamp(value, 1, 64);
    if (state_.depth_completion_params.kernel_radius == clamped)
    {
        return false;
    }

    state_.depth_completion_params.kernel_radius = clamped;
    applyDepthCompletionParamsToRenderer();
    return true;
}

bool SceneRuntimeController::setDepthKernelRadiusFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::DepthKernelRadius) ? setDepthKernelRadius(value) : false;
}

bool SceneRuntimeController::setDepthTopK(int value)
{
    const int clamped = std::clamp(value, 1, 64);
    if (state_.depth_completion_params.top_k == clamped)
    {
        return false;
    }

    state_.depth_completion_params.top_k = clamped;
    applyDepthCompletionParamsToRenderer();
    return true;
}

bool SceneRuntimeController::setDepthTopKFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::DepthTopK) ? setDepthTopK(value) : false;
}

bool SceneRuntimeController::setDepthSpatialWeight(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    if (state_.depth_completion_params.spatial_weight == clamped)
    {
        return false;
    }

    state_.depth_completion_params.spatial_weight = clamped;
    applyDepthCompletionParamsToRenderer();
    return true;
}

bool SceneRuntimeController::setDepthSpatialWeightFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::DepthSpatialWeight) ? setDepthSpatialWeight(value) : false;
}

bool SceneRuntimeController::setDepthColorSigma(float value)
{
    const float clamped = std::clamp(value, 0.001f, 1.0f);
    if (state_.depth_completion_params.color_sigma == clamped)
    {
        return false;
    }

    state_.depth_completion_params.color_sigma = clamped;
    applyDepthCompletionParamsToRenderer();
    return true;
}

bool SceneRuntimeController::setDepthColorSigmaFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::DepthColorSigma) ? setDepthColorSigma(value) : false;
}

bool SceneRuntimeController::setDepthEdgeThreshold(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    if (state_.depth_completion_params.edge_threshold == clamped)
    {
        return false;
    }

    state_.depth_completion_params.edge_threshold = clamped;
    applyDepthCompletionParamsToRenderer();
    return true;
}

bool SceneRuntimeController::setDepthEdgeThresholdFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::DepthEdgeThreshold) ? setDepthEdgeThreshold(value) : false;
}

bool SceneRuntimeController::setDepthMaxFillPasses(int value)
{
    const int clamped = std::clamp(value, 1, 15);
    if (state_.depth_completion_params.max_fill_passes == clamped)
    {
        return false;
    }

    state_.depth_completion_params.max_fill_passes = clamped;
    applyDepthCompletionParamsToRenderer();
    return true;
}

bool SceneRuntimeController::setDepthMaxFillPassesFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::DepthMaxFillPasses) ? setDepthMaxFillPasses(value) : false;
}

bool SceneRuntimeController::setShadowType(int type)
{
    if (state_.shadow_type == type)
    {
        return false;
    }

    state_.shadow_type = type;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setShadowTypeFromUi(int type)
{
    return canApplyUiChange(RuntimeParam::ShadowType) ? setShadowType(type) : false;
}

bool SceneRuntimeController::setExposure(float value)
{
    if (state_.exposure == value)
    {
        return false;
    }

    state_.exposure = value;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setExposureFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::Exposure) ? setExposure(value) : false;
}

bool SceneRuntimeController::setSsaoRadius(float value)
{
    if (state_.ssao_radius == value)
    {
        return false;
    }

    state_.ssao_radius = value;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setSsaoRadiusFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::SsaoRadius) ? setSsaoRadius(value) : false;
}

bool SceneRuntimeController::setSsaoKernelSize(int value)
{
    if (state_.ssao_kernel_size == value)
    {
        return false;
    }

    state_.ssao_kernel_size = value;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setSsaoKernelSizeFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::SsaoKernelSize) ? setSsaoKernelSize(value) : false;
}

bool SceneRuntimeController::setDenoiseSize(int value)
{
    if (state_.denoise_size == value)
    {
        return false;
    }

    state_.denoise_size = value;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setDenoiseSizeFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::DenoiseSize) ? setDenoiseSize(value) : false;
}

bool SceneRuntimeController::setShadowBias(float value)
{
    if (state_.shadow_bias == value)
    {
        return false;
    }

    state_.shadow_bias = value;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setShadowBiasFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::ShadowBias) ? setShadowBias(value) : false;
}

bool SceneRuntimeController::setBlockerSampleNum(int value)
{
    if (state_.blocker_sample_num == value)
    {
        return false;
    }

    state_.blocker_sample_num = value;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setBlockerSampleNumFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::BlockerSampleNum) ? setBlockerSampleNum(value) : false;
}

bool SceneRuntimeController::setPcfSampleNum(int value)
{
    if (state_.pcf_sample_num == value)
    {
        return false;
    }

    state_.pcf_sample_num = value;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setPcfSampleNumFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::PcfSampleNum) ? setPcfSampleNum(value) : false;
}

bool SceneRuntimeController::setPcfSoftness(float value)
{
    if (state_.pcf_softness == value)
    {
        return false;
    }

    state_.pcf_softness = value;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setPcfSoftnessFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::PcfSoftness) ? setPcfSoftness(value) : false;
}

bool SceneRuntimeController::setPcssSoftness(float value)
{
    if (state_.pcss_softness == value)
    {
        return false;
    }

    state_.pcss_softness = value;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setPcssSoftnessFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::PcssSoftness) ? setPcssSoftness(value) : false;
}

bool SceneRuntimeController::setPcssSoftnessFalloff(float value)
{
    if (state_.pcss_softness_falloff == value)
    {
        return false;
    }

    state_.pcss_softness_falloff = value;
    applyFrameParamsToPcData();
    return true;
}

bool SceneRuntimeController::setPcssSoftnessFalloffFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::PcssSoftnessFalloff) ? setPcssSoftnessFalloff(value) : false;
}

bool SceneRuntimeController::setLineColor(const vsg::vec3& color)
{
    if (sameVec3(state_.line_color, color))
    {
        return false;
    }

    state_.line_color = color;
    applyLinePointColors();
    return true;
}

bool SceneRuntimeController::setLineColorFromUi(const vsg::vec3& color)
{
    return canApplyUiChange(RuntimeParam::LineColor) ? setLineColor(color) : false;
}

bool SceneRuntimeController::setPointColor(const vsg::vec3& color)
{
    if (sameVec3(state_.point_color, color))
    {
        return false;
    }

    state_.point_color = color;
    applyLinePointColors();
    return true;
}

bool SceneRuntimeController::setPointColorFromUi(const vsg::vec3& color)
{
    return canApplyUiChange(RuntimeParam::PointColor) ? setPointColor(color) : false;
}

bool SceneRuntimeController::setInstanceTransform(const std::string& instance_name, const vsg::dmat4& transform) const
{
    renderer_.updateObjectPose(instance_name, transform);
    return true;
}

void SceneRuntimeController::replaceSceneTransforms(const std::vector<SceneModelTransformSave>& transforms)
{
    state_.scene_transforms = transforms;
}

bool SceneRuntimeController::saveRenderState(std::string* error_message) const
{
    if (!persistence_coordinator_ || scene_id_ < 0)
    {
        if (error_message) *error_message = "SceneRuntimeController is not initialized.";
        return false;
    }

    return persistence_coordinator_->saveSceneRenderState(scene_id_, state_, error_message);
}

bool SceneRuntimeController::saveBaseBrightness(std::string* error_message)
{
    if (!persistence_coordinator_)
    {
        if (error_message) *error_message = "SceneRuntimeController is not initialized.";
        return false;
    }

    state_.hdr_base_brightness[state_.hdr_image_num] = state_.baseBrightness;
    renderer_.hdr_base_brightness[state_.hdr_image_num] = state_.baseBrightness;
    return persistence_coordinator_->saveBaseBrightness(state_, error_message);
}

bool SceneRuntimeController::saveSceneTransforms(const std::vector<SceneModelTransformSave>& transforms, std::string* error_message)
{
    if (!persistence_coordinator_ || scene_id_ < 0)
    {
        if (error_message) *error_message = "SceneRuntimeController is not initialized.";
        return false;
    }

    replaceSceneTransforms(transforms);
    return persistence_coordinator_->saveSceneTransforms(scene_id_, state_, error_message);
}

bool SceneRuntimeController::findSceneTransform(const std::string& instance_name, SceneModelTransformSave& out_transform) const
{
    for (const auto& transform : state_.scene_transforms)
    {
        if (transform.instance_name == instance_name)
        {
            out_transform = transform;
            return true;
        }
    }
    return false;
}

vsg::dmat4 SceneRuntimeController::sceneTransformOrIdentity(const std::string& instance_name) const
{
    SceneModelTransformSave transform;
    if (findSceneTransform(instance_name, transform))
    {
        return transform.transform;
    }
    for (size_t i = 0; i < CADMesh::scene_instance_names.size(); ++i)
    {
        if (CADMesh::scene_instance_names[i] == instance_name && i < CADMesh::scene_original_transforms.size())
        {
            return CADMesh::scene_original_transforms[i];
        }
    }
    return vsg::dmat4();
}

bool SceneRuntimeController::sameVec3(const vsg::vec3& lhs, const vsg::vec3& rhs)
{
    return lhs.r == rhs.r &&
           lhs.g == rhs.g &&
           lhs.b == rhs.b;
}

bool SceneRuntimeController::canApplyUiChange(RuntimeParam param) const
{
    return !isServerDirty(param);
}
