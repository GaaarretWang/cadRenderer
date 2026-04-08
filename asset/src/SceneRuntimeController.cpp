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

bool SceneRuntimeController::setShadowType(int type)
{
    if (render_state_.pipeline.frame_params.shadow_type == type)
    {
        return false;
    }

    render_state_.pipeline.frame_params.shadow_type = type;
    return true;
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

bool SceneRuntimeController::setSsaoRadius(float value)
{
    if (nearlyEqual(render_state_.pipeline.frame_params.ssao_radius, value))
    {
        return false;
    }

    render_state_.pipeline.frame_params.ssao_radius = value;
    return true;
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

bool SceneRuntimeController::setDenoiseSize(int value)
{
    if (render_state_.pipeline.frame_params.denoise_size == value)
    {
        return false;
    }

    render_state_.pipeline.frame_params.denoise_size = value;
    return true;
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

bool SceneRuntimeController::setBlockerSampleNum(int value)
{
    if (render_state_.pipeline.frame_params.blocker_sample_num == value)
    {
        return false;
    }

    render_state_.pipeline.frame_params.blocker_sample_num = value;
    return true;
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

bool SceneRuntimeController::setPcfSoftness(float value)
{
    if (nearlyEqual(render_state_.pipeline.pcf_softness, value))
    {
        return false;
    }

    render_state_.pipeline.pcf_softness = value;
    return true;
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

bool SceneRuntimeController::setPcssSoftnessFalloff(float value)
{
    if (nearlyEqual(render_state_.pipeline.pcss_softness_falloff, value))
    {
        return false;
    }

    render_state_.pipeline.pcss_softness_falloff = value;
    return true;
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

bool SceneRuntimeController::setPointColor(const vsg::vec3& color)
{
    if (sameVec3(line_point_style_.point_color, color))
    {
        return false;
    }

    line_point_style_.point_color = color;
    return true;
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
