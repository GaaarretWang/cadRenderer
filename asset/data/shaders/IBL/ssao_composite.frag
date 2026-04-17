#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MATERIAL_DESCRIPTOR_SET 2

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D colorSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D ssaoSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D realSceneSampler;
layout(std140, set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform GlobalBuffer {
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

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

vec3 Uncharted2Tonemap(vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec4 LINEARtoSRGB(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(1.0 / 2.2));
    return vec4(linOut, srgbIn.w);
}

void main()
{
    vec2 uv = inUV * 0.5 + 0.5;

    ivec2 colorSize = textureSize(colorSampler, 0);
    ivec2 colorCoord = clamp(ivec2(uv * vec2(colorSize)), ivec2(0), colorSize - 1);
    vec3 colorData = texelFetch(colorSampler, colorCoord, 0).xyz;
    vec4 realSceneData = texture(realSceneSampler, uv);

    ivec2 ssaoSize = textureSize(ssaoSampler, 0);
    vec4 centerSSAO = texture(ssaoSampler, uv);

    vec2 texelSize = 1.0 / vec2(ssaoSize);
    float result = 0.0;
    int denoiseRadius = globalBuffer.denoise_size / 2;
    for (int x = -denoiseRadius; x <= denoiseRadius; ++x)
    {
        for (int y = -denoiseRadius; y <= denoiseRadius; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoSampler, uv + offset).r;
        }
    }

    if (centerSSAO.w > 0.5)
    {
        outColor = realSceneData.a > 0.5 ? realSceneData : vec4(colorData, 1.0);
    }
    else
    {
        float kernelWidth = float(denoiseRadius * 2 + 1);
        float occlusion = result / (kernelWidth * kernelWidth);
        vec3 color = Uncharted2Tonemap(colorData * occlusion * globalBuffer.exposure);
        color = color * (vec3(1.0) / Uncharted2Tonemap(vec3(11.2)));
        outColor = LINEARtoSRGB(vec4(color, 1.0));
    }

}
