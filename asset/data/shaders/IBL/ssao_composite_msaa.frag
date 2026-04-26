#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MATERIAL_DESCRIPTOR_SET 2

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2DMS colorSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D ssaoSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D realSceneSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D maskSampler;
#define GLOBAL_BUFFER_SET MATERIAL_DESCRIPTOR_SET
#define GLOBAL_BUFFER_BINDING 4
#pragma include "global_buffer.glsl"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

#pragma include "tonemapping.glsl"
#pragma include "fallback_composite_common.glsl"

void main()
{
    vec2 uv = inUV * 0.5 + 0.5;

    ivec2 colorSize = textureSize(colorSampler);
    ivec2 colorCoord = clamp(ivec2(uv * vec2(colorSize)), ivec2(0), colorSize - 1);
    int sampleCount = textureSamples(colorSampler);

    vec4 realSceneData = texture(realSceneSampler, uv);
    float maskValue = texture(maskSampler, uv).r;
    vec4 centerSSAO = texture(ssaoSampler, uv);
    bool preferRealScene = centerSSAO.w > 0.5;
    float occlusion = ComputeDenoisedOcclusion(ssaoSampler, uv, globalBuffer.denoise_size);

    vec4 accumulated = vec4(0.0);
    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        vec4 colorData = texelFetch(colorSampler, colorCoord, sampleIndex);
        accumulated += CompositeFallbackSample(colorData, realSceneData, maskValue, preferRealScene, occlusion, globalBuffer.exposure);
    }

    outColor = accumulated / float(sampleCount);
}
