#version 450

layout(set = 2, binding = 0) uniform sampler2DMS depthSampler;

layout(location = 0) out vec4 outColor;

#pragma include "gbuffer_resolve_common.glsl"

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    float maxDepth = 0.0;
    int maxSampleIndex = SelectMaxDepthSampleIndex(depthSampler, coord, maxDepth);

    outColor = vec4(maxDepth, float(maxSampleIndex), 0.0, 1.0);
}
