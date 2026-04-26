int SelectMaxDepthSampleIndex(sampler2DMS sourceDepth, ivec2 coord, out float maxDepth)
{
    int sampleCount = textureSamples(sourceDepth);
    maxDepth = texelFetch(sourceDepth, coord, 0).r;
    int maxSampleIndex = 0;

    for (int sampleIndex = 1; sampleIndex < sampleCount; ++sampleIndex)
    {
        float sampleDepth = texelFetch(sourceDepth, coord, sampleIndex).r;
        if (sampleDepth > maxDepth)
        {
            maxDepth = sampleDepth;
            maxSampleIndex = sampleIndex;
        }
    }

    return maxSampleIndex;
}

int SelectMaxDepthSampleIndex(sampler2DMS sourceDepth, ivec2 coord)
{
    float ignoredDepth = 0.0;
    return SelectMaxDepthSampleIndex(sourceDepth, coord, ignoredDepth);
}
