#version 450

layout(set = 2, binding = 0) uniform sampler2DMS depthSampler;
layout(set = 2, binding = 1) uniform sampler2DMS maskSampler;

layout(location = 0) out float outMask;

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    int sampleCount = textureSamples(depthSampler);

    float maxDepth = texelFetch(depthSampler, coord, 0).r;
    int maxSampleIndex = 0;
    for (int sampleIndex = 1; sampleIndex < sampleCount; ++sampleIndex)
    {
        float sampleDepth = texelFetch(depthSampler, coord, sampleIndex).r;
        if (sampleDepth > maxDepth)
        {
            maxDepth = sampleDepth;
            maxSampleIndex = sampleIndex;
        }
    }

    outMask = texelFetch(maskSampler, coord, maxSampleIndex).r;
}
