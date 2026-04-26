#version 450

layout(set = 2, binding = 0) uniform sampler2DMS depthSampler;
layout(set = 2, binding = 1) uniform sampler2DMS worldPosSampler;

layout(location = 0) out vec4 outColor;

#pragma include "gbuffer_resolve_common.glsl"

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    int maxSampleIndex = SelectMaxDepthSampleIndex(depthSampler, coord);

    outColor = texelFetch(worldPosSampler, coord, maxSampleIndex);
}
