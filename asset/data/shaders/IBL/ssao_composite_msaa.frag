#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MATERIAL_DESCRIPTOR_SET 2

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2DMS colorSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D ssaoSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D realSceneSampler;
#define GLOBAL_BUFFER_SET MATERIAL_DESCRIPTOR_SET
#define GLOBAL_BUFFER_BINDING 3
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

vec3 compositeSample(vec4 colorData, vec4 realSceneData, bool preferRealScene, float occlusion)
{
    bool isEmptySample = abs(colorData.a) < 1e-6 && length(colorData.rgb) < 1e-6;
    bool hasRealScene = realSceneData.a > 0.5;

    if (isEmptySample && hasRealScene)
    {
        return realSceneData.rgb;
    }

    if (colorData.a < 0.0)
    {
        return colorData.rgb * occlusion;
    }

    if (preferRealScene)
    {
        return hasRealScene ? realSceneData.rgb : colorData.rgb;
    }

    vec3 color = Uncharted2Tonemap(colorData.rgb * occlusion * globalBuffer.exposure);
    color = color * (vec3(1.0) / Uncharted2Tonemap(vec3(11.2)));
    return LINEARtoSRGB(vec4(color, 1.0)).rgb;
}

void main()
{
    vec2 uv = inUV * 0.5 + 0.5;

    ivec2 colorSize = textureSize(colorSampler);
    ivec2 colorCoord = clamp(ivec2(uv * vec2(colorSize)), ivec2(0), colorSize - 1);
    int sampleCount = textureSamples(colorSampler);

    vec4 realSceneData = texture(realSceneSampler, uv);
    vec4 centerSSAO = texture(ssaoSampler, uv);
    bool preferRealScene = centerSSAO.w > 0.5;
    float occlusion = computeOcclusion(uv);

    vec3 accumulated = vec3(0.0);
    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        vec4 colorData = texelFetch(colorSampler, colorCoord, sampleIndex);
        accumulated += compositeSample(colorData, realSceneData, preferRealScene, occlusion);
    }

    outColor = vec4(accumulated / float(sampleCount), 1.0);
}
