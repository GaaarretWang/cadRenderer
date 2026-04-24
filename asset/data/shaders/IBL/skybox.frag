#version 450

layout(location = 0) in vec3 inUVW;
layout(set = 0, binding = 0) uniform samplerCube samplerEnv;
layout(set = 0, binding = 1) uniform Params {
    float exposure;
    float gamma;
    float width;
    float height;
    float enableRealDepthOcclusion;
    float shadowMode;
} tonemapParams;

layout(location = 0) out vec4 outColor;

#pragma include "tonemapping.glsl"

void main()
{
    vec3 color = textureLod(samplerEnv, normalize(inUVW), 0).rgb;
    color = Uncharted2Tonemap(color * tonemapParams.exposure);
    color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    color = pow(color, vec3(1.0f / tonemapParams.gamma));
    outColor = vec4(color, 1.0);
}
