#version 450

layout(set = 2, binding = 0) uniform sampler2DMS depthSampler;

layout(location = 0) out vec4 outColor;

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

    outColor = vec4(maxDepth, float(maxSampleIndex), 0.0, 1.0);
}
