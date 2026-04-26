#version 450

layout(set = 2, binding = 0) uniform sampler2DMS depthSampler;
layout(set = 2, binding = 1) uniform sampler2DMS maskSampler;

layout(location = 0) out float outMask;

#pragma include "gbuffer_resolve_common.glsl"

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    int maxSampleIndex = SelectMaxDepthSampleIndex(depthSampler, coord);

    outMask = texelFetch(maskSampler, coord, maxSampleIndex).r;
}
