// GlobalBuffer uniform declaration
// Before including, define GLOBAL_BUFFER_SET and GLOBAL_BUFFER_BINDING

#ifndef GLOBAL_BUFFER_SET
#error "Define GLOBAL_BUFFER_SET before including global_buffer.glsl"
#endif
#ifndef GLOBAL_BUFFER_BINDING
#error "Define GLOBAL_BUFFER_BINDING before including global_buffer.glsl"
#endif

layout(std140, set = GLOBAL_BUFFER_SET, binding = GLOBAL_BUFFER_BINDING) uniform GlobalBuffer {
    mat4 last_view;
    vec3 camera_pos;
    float softness;
    float baseBrightness;
    float ssao_radius;
    float exposure;
    float softness_falloff;
    float shadow_bias;
    float z_far;
    int width;
    int height;
    int ssao_kernel_size;
    int denoise_size;
    int blocker_sample_num;
    int pcf_sample_num;
    int shadow_type;
    uint frame_num;
    int enable_real_depth_occlusion;
    int shadow_mode;
} globalBuffer;
