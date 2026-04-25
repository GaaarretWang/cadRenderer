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

float computeOcclusion(vec2 uv)
{
    ivec2 ssaoSize = textureSize(ssaoSampler, 0);
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

    float kernelWidth = float(denoiseRadius * 2 + 1);
    return result / (kernelWidth * kernelWidth);
}

vec4 compositeSample(vec4 colorData, vec4 realSceneData, float maskValue, bool preferRealScene, float occlusion)
{
    bool isEmptySample = abs(colorData.a) < 1e-6 && length(colorData.rgb) < 1e-6;
    bool hasRealScene = realSceneData.a > 0.5;
    bool isOpaqueVirtual = abs(maskValue - 1.0) < 0.5;

    if (isEmptySample && hasRealScene)
    {
        return realSceneData;
    }

    if (isOpaqueVirtual)
    {
        return vec4(0.0);
    }

    if (maskValue > 1.5)
    {
        return vec4(colorData.rgb * occlusion, 1.0);
    }

    if (preferRealScene)
    {
        return hasRealScene ? realSceneData : colorData;
    }

    vec3 color = Uncharted2Tonemap(colorData.rgb * occlusion * globalBuffer.exposure);
    color = color * (vec3(1.0) / Uncharted2Tonemap(vec3(11.2)));
    return LINEARtoSRGB(vec4(color, colorData.a));
}

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
    float occlusion = computeOcclusion(uv);

    vec4 accumulated = vec4(0.0);
    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        vec4 colorData = texelFetch(colorSampler, colorCoord, sampleIndex);
        accumulated += compositeSample(colorData, realSceneData, maskValue, preferRealScene, occlusion);
    }

    outColor = accumulated / float(sampleCount);
}
