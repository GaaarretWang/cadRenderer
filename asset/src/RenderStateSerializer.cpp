#include "RenderStateSerializer.h"

void RenderStateSerializer::applySceneRenderParams(const SceneRenderParams& params, RenderStateHub& state)
{
    state.pipeline.hdr_image_num = params.hdr_image_num;
    state.pipeline.enable_real_depth_occlusion = params.enable_real_depth_occlusion;
    state.pipeline.shadow_mode = params.shadow_mode;
    state.pipeline.frame_params.ssao_radius = params.ssao_radius;
    state.pipeline.frame_params.ssao_kernel_size = params.ssao_kernel_size;
    state.pipeline.frame_params.exposure = params.exposure;
    state.pipeline.frame_params.denoise_size = params.denoise_size;
    state.pipeline.frame_params.shadow_bias = params.shadow_bias;
    state.pipeline.frame_params.blocker_sample_num = params.blocker_sample_num;
    state.pipeline.frame_params.pcf_sample_num = params.pcf_sample_num;
    state.pipeline.frame_params.shadow_type = params.shadow_type;
    state.pipeline.pcf_softness = params.pcf_softness;
    state.pipeline.pcss_softness = params.pcss_softness;
    state.pipeline.pcss_softness_falloff = params.pcss_softness_falloff;
}

SceneRenderParams RenderStateSerializer::toSceneRenderParams(const RenderStateHub& state)
{
    SceneRenderParams params;
    params.hdr_image_num = state.pipeline.hdr_image_num;
    params.enable_real_depth_occlusion = state.pipeline.enable_real_depth_occlusion;
    params.shadow_mode = state.pipeline.shadow_mode;
    params.ssao_radius = state.pipeline.frame_params.ssao_radius;
    params.ssao_kernel_size = state.pipeline.frame_params.ssao_kernel_size;
    params.exposure = state.pipeline.frame_params.exposure;
    params.denoise_size = state.pipeline.frame_params.denoise_size;
    params.shadow_bias = state.pipeline.frame_params.shadow_bias;
    params.blocker_sample_num = state.pipeline.frame_params.blocker_sample_num;
    params.pcf_sample_num = state.pipeline.frame_params.pcf_sample_num;
    params.shadow_type = state.pipeline.frame_params.shadow_type;
    params.pcf_softness = state.pipeline.pcf_softness;
    params.pcss_softness = state.pipeline.pcss_softness;
    params.pcss_softness_falloff = state.pipeline.pcss_softness_falloff;
    return params;
}
