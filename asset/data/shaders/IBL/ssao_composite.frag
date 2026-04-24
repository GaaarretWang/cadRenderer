#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MATERIAL_DESCRIPTOR_SET 2

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D colorSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D ssaoSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D realSceneSampler;
#define GLOBAL_BUFFER_SET MATERIAL_DESCRIPTOR_SET
#define GLOBAL_BUFFER_BINDING 3
#pragma include "global_buffer.glsl"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

#pragma include "tonemapping.glsl"

void main()
{
    vec2 uv = inUV * 0.5 + 0.5;

    ivec2 colorSize = textureSize(colorSampler, 0);
    ivec2 colorCoord = clamp(ivec2(uv * vec2(colorSize)), ivec2(0), colorSize - 1);
    vec4 colorData = texelFetch(colorSampler, colorCoord, 0);
    vec4 realSceneData = texture(realSceneSampler, uv);
    bool isShadowReceiver = colorData.a < 0.0;

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

    float kernelWidth = float(denoiseRadius * 2 + 1);
    float occlusion = result / (kernelWidth * kernelWidth);

    if (isShadowReceiver)
    {
        outColor = vec4(colorData.rgb * occlusion, 1.0);
    }
    else if (centerSSAO.w > 0.5)
    {
        outColor = realSceneData.a > 0.5 ? realSceneData : vec4(colorData.rgb, 1.0);
    }
    else
    {
        vec3 color = Uncharted2Tonemap(colorData.rgb * occlusion * globalBuffer.exposure);
        color = color * (vec3(1.0) / Uncharted2Tonemap(vec3(11.2)));
        outColor = LINEARtoSRGB(vec4(color, 1.0));
    }

}
