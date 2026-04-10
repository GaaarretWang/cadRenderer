#include "SceneRuntimeController.h"

#include <cmath>

#include "vsgRendererServer.h"

SceneRuntimeController::SceneRuntimeController(vsgRendererServer& renderer,
                                               std::shared_ptr<RenderStateController> persistence_controller,
                                               RenderStateHub& render_state,
                                               SceneLinePointStyle& line_point_style,
                                               float& base_brightness)
    : renderer_(renderer),
      persistence_controller_(std::move(persistence_controller)),
      render_state_(render_state),
      line_point_style_(line_point_style),
      base_brightness_(base_brightness)
{
}

void SceneRuntimeController::initializeForScene(int scene_id)
{
    scene_id_ = scene_id;
    applied_hdr_image_num_ = -1;
    clearServerDirtyFlags();
}

void SceneRuntimeController::applyLoadedState()
{
    renderer_.hdr_image_num = render_state_.pipeline.hdr_image_num;
    renderer_.setRealDepthOcclusion(render_state_.pipeline.enable_real_depth_occlusion);
    renderer_.setShadowMode(render_state_.pipeline.shadow_mode);
    renderer_.syncConstantData();

    const auto it = renderer_.hdr_base_brightness.find(render_state_.pipeline.hdr_image_num);
    if (it != renderer_.hdr_base_brightness.end())
    {
        base_brightness_ = it->second;
    }
    render_state_.pipeline.frame_params.baseBrightness = base_brightness_;
}

void SceneRuntimeController::clearServerDirtyFlags()
{
    hdr_server_dirty_ = false;
    base_brightness_server_dirty_ = false;
    depth_occlusion_server_dirty_ = false;
    shadow_mode_server_dirty_ = false;
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
    if (hdr_num <= 0)
    {
        hdr_num = 1;
    }

    const bool changed = applied_hdr_image_num_ != hdr_num;
    render_state_.pipeline.hdr_image_num = hdr_num;
    renderer_.hdr_image_num = hdr_num;

    if (changed && renderer_.view && renderer_.window && renderer_.device)
    {
        renderer_.updateEnvLighting();
        applied_hdr_image_num_ = hdr_num;
    }

    const auto it = renderer_.hdr_base_brightness.find(render_state_.pipeline.hdr_image_num);
    if (it != renderer_.hdr_base_brightness.end())
    {
        base_brightness_ = it->second;
        render_state_.pipeline.frame_params.baseBrightness = base_brightness_;
    }

    return changed;
}

bool SceneRuntimeController::setHdrFromUi(int hdr_num)
{
    return canApplyUiChange(RuntimeParam::Hdr) ? setHdrImageNum(hdr_num) : false;
}

bool SceneRuntimeController::setBaseBrightness(float value)
{
    if (nearlyEqual(base_brightness_, value))
    {
        return false;
    }

    base_brightness_ = value;
    render_state_.pipeline.frame_params.baseBrightness = value;
    return true;
}

bool SceneRuntimeController::setBaseBrightnessFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::BaseBrightness) ? setBaseBrightness(value) : false;
}

bool SceneRuntimeController::setDepthOcclusionEnabled(bool enabled)
{
    const int normalized = enabled ? 1 : 0;
    if (render_state_.pipeline.enable_real_depth_occlusion == normalized)
    {
        return false;
    }

    render_state_.pipeline.enable_real_depth_occlusion = normalized;
    renderer_.setRealDepthOcclusion(normalized);
    renderer_.syncConstantData();
    return true;
}

bool SceneRuntimeController::setDepthOcclusionEnabledFromUi(bool enabled)
{
    return canApplyUiChange(RuntimeParam::DepthOcclusion) ? setDepthOcclusionEnabled(enabled) : false;
}

bool SceneRuntimeController::setShadowMode(int mode)
{
    const int normalized = (mode == SHADOW_REAL_DEPTH) ? SHADOW_REAL_DEPTH : SHADOW_RECEIVER_PLANE;
    if (render_state_.pipeline.shadow_mode == normalized)
    {
        return false;
    }

    render_state_.pipeline.shadow_mode = normalized;
    renderer_.setShadowMode(normalized);
    renderer_.syncConstantData();
    return true;
}

bool SceneRuntimeController::setShadowModeFromUi(int mode)
{
    return canApplyUiChange(RuntimeParam::ShadowMode) ? setShadowMode(mode) : false;
}

bool SceneRuntimeController::setShadowType(int type)
{
    if (render_state_.pipeline.frame_params.shadow_type == type)
    {
        return false;
    }

    render_state_.pipeline.frame_params.shadow_type = type;
    return true;
}

bool SceneRuntimeController::setShadowTypeFromUi(int type)
{
    return canApplyUiChange(RuntimeParam::ShadowType) ? setShadowType(type) : false;
}

bool SceneRuntimeController::setExposure(float value)
{
    if (nearlyEqual(render_state_.pipeline.frame_params.exposure, value))
    {
        return false;
    }

    render_state_.pipeline.frame_params.exposure = value;
    return true;
}

bool SceneRuntimeController::setExposureFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::Exposure) ? setExposure(value) : false;
}

bool SceneRuntimeController::setSsaoRadius(float value)
{
    if (nearlyEqual(render_state_.pipeline.frame_params.ssao_radius, value))
    {
        return false;
    }

    render_state_.pipeline.frame_params.ssao_radius = value;
    return true;
}

bool SceneRuntimeController::setSsaoRadiusFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::SsaoRadius) ? setSsaoRadius(value) : false;
}

bool SceneRuntimeController::setSsaoKernelSize(int value)
{
    if (render_state_.pipeline.frame_params.ssao_kernel_size == value)
    {
        return false;
    }

    render_state_.pipeline.frame_params.ssao_kernel_size = value;
    return true;
}

bool SceneRuntimeController::setSsaoKernelSizeFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::SsaoKernelSize) ? setSsaoKernelSize(value) : false;
}

bool SceneRuntimeController::setDenoiseSize(int value)
{
    if (render_state_.pipeline.frame_params.denoise_size == value)
    {
        return false;
    }

    render_state_.pipeline.frame_params.denoise_size = value;
    return true;
}

bool SceneRuntimeController::setDenoiseSizeFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::DenoiseSize) ? setDenoiseSize(value) : false;
}

bool SceneRuntimeController::setShadowBias(float value)
{
    if (nearlyEqual(render_state_.pipeline.frame_params.shadow_bias, value))
    {
        return false;
    }

    render_state_.pipeline.frame_params.shadow_bias = value;
    return true;
}

bool SceneRuntimeController::setShadowBiasFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::ShadowBias) ? setShadowBias(value) : false;
}

bool SceneRuntimeController::setBlockerSampleNum(int value)
{
    if (render_state_.pipeline.frame_params.blocker_sample_num == value)
    {
        return false;
    }

    render_state_.pipeline.frame_params.blocker_sample_num = value;
    return true;
}

bool SceneRuntimeController::setBlockerSampleNumFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::BlockerSampleNum) ? setBlockerSampleNum(value) : false;
}

bool SceneRuntimeController::setPcfSampleNum(int value)
{
    if (render_state_.pipeline.frame_params.pcf_sample_num == value)
    {
        return false;
    }

    render_state_.pipeline.frame_params.pcf_sample_num = value;
    return true;
}

bool SceneRuntimeController::setPcfSampleNumFromUi(int value)
{
    return canApplyUiChange(RuntimeParam::PcfSampleNum) ? setPcfSampleNum(value) : false;
}

bool SceneRuntimeController::setPcfSoftness(float value)
{
    if (nearlyEqual(render_state_.pipeline.pcf_softness, value))
    {
        return false;
    }

    render_state_.pipeline.pcf_softness = value;
    return true;
}

bool SceneRuntimeController::setPcfSoftnessFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::PcfSoftness) ? setPcfSoftness(value) : false;
}

bool SceneRuntimeController::setPcssSoftness(float value)
{
    if (nearlyEqual(render_state_.pipeline.pcss_softness, value))
    {
        return false;
    }

    render_state_.pipeline.pcss_softness = value;
    return true;
}

bool SceneRuntimeController::setPcssSoftnessFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::PcssSoftness) ? setPcssSoftness(value) : false;
}

bool SceneRuntimeController::setPcssSoftnessFalloff(float value)
{
    if (nearlyEqual(render_state_.pipeline.pcss_softness_falloff, value))
    {
        return false;
    }

    render_state_.pipeline.pcss_softness_falloff = value;
    return true;
}

bool SceneRuntimeController::setPcssSoftnessFalloffFromUi(float value)
{
    return canApplyUiChange(RuntimeParam::PcssSoftnessFalloff) ? setPcssSoftnessFalloff(value) : false;
}

bool SceneRuntimeController::setLineColor(const vsg::vec3& color)
{
    if (sameVec3(line_point_style_.line_color, color))
    {
        return false;
    }

    line_point_style_.line_color = color;
    return true;
}

bool SceneRuntimeController::setLineColorFromUi(const vsg::vec3& color)
{
    return canApplyUiChange(RuntimeParam::LineColor) ? setLineColor(color) : false;
}

bool SceneRuntimeController::setPointColor(const vsg::vec3& color)
{
    if (sameVec3(line_point_style_.point_color, color))
    {
        return false;
    }

    line_point_style_.point_color = color;
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

bool SceneRuntimeController::saveRenderState(std::string* error_message) const
{
    if (!persistence_controller_ || scene_id_ < 0)
    {
        if (error_message) *error_message = "SceneRuntimeController is not initialized.";
        return false;
    }

    return persistence_controller_->saveRenderState(scene_id_, render_state_, line_point_style_, error_message);
}

bool SceneRuntimeController::saveBaseBrightness(std::string* error_message) const
{
    if (!persistence_controller_)
    {
        if (error_message) *error_message = "SceneRuntimeController is not initialized.";
        return false;
    }

    return persistence_controller_->saveBaseBrightnessToLightInfo(render_state_.pipeline.hdr_image_num, base_brightness_, error_message);
}

bool SceneRuntimeController::saveSceneTransforms(const std::vector<SceneModelTransformSave>& transforms, std::string* error_message) const
{
    if (!persistence_controller_ || scene_id_ < 0)
    {
        if (error_message) *error_message = "SceneRuntimeController is not initialized.";
        return false;
    }

    return persistence_controller_->saveSceneTransforms(scene_id_, transforms, error_message);
}

bool SceneRuntimeController::nearlyEqual(float lhs, float rhs, float epsilon)
{
    return std::fabs(lhs - rhs) <= epsilon;
}

bool SceneRuntimeController::sameVec3(const vsg::vec3& lhs, const vsg::vec3& rhs)
{
    return nearlyEqual(lhs.r, rhs.r) &&
           nearlyEqual(lhs.g, rhs.g) &&
           nearlyEqual(lhs.b, rhs.b);
}

bool SceneRuntimeController::canApplyUiChange(RuntimeParam param) const
{
    return !isServerDirty(param);
}
