// PCSS (Percentage Closer Soft Shadows) - Standard variant
// Requires: sample_data.glsl, shadowMaps (sampler2DArrayShadow), shadowMapsSampler (sampler2DArray)
// Requires: globalBuffer.frame_num, globalBuffer.softness, globalBuffer.pcf_sample_num,
//           globalBuffer.blocker_sample_num, globalBuffer.softness_falloff, globalBuffer.shadow_bias

vec2 Rotate(vec2 pos, vec2 rotationTrig)
{
    return vec2(pos.x * rotationTrig.x - pos.y * rotationTrig.y, pos.y * rotationTrig.x + pos.x * rotationTrig.y);
}

float ValueNoise(vec3 pos)
{
    vec3 noiseSkew = pos + 0.2127 + pos.x * pos.y * pos.z * 0.3713;
    vec3 noiseRnd = 4.789 * sin(489.123 * noiseSkew);
    return fract(noiseRnd.x * noiseRnd.y * noiseRnd.z * (1.0 + noiseSkew.x) * PCSS_FRAME_NUM);
}

float PCF(vec4 coords, int shadowMapIndex, float area, float random)
{
    float rotationAngle = random * PI;
    vec2 rotationTrig = vec2(cos(rotationAngle), sin(rotationAngle));
    float linearFrac = sqrt(max(area, 0.0));
    float baseStridePixels = 20.0;
    float stride = baseStridePixels * linearFrac * PCSS_SOFTNESS + 0.001;
    float shadowmapSize = 2048.0;
    float visibility = 0.0;
    for (int i = 0; i < PCSS_PCF_SAMPLE_NUM; ++i)
    {
        visibility += texture(shadowMaps, vec4(coords.xy + Rotate(poissonDisk[i * 64 / PCSS_PCF_SAMPLE_NUM] * stride / shadowmapSize, rotationTrig), shadowMapIndex, coords.z)).r;
    }
    return visibility / float(PCSS_PCF_SAMPLE_NUM);
}

vec2 ComputeFibonacciSpiralDiskSampleClumped(const in int sampleIndex, const in float sampleCountInverse, out float sampleDistNorm)
{
    sampleDistNorm = sampleIndex * sampleCountInverse;
    sampleDistNorm = sampleDistNorm * sampleDistNorm * sampleDistNorm;
    return fibonacciSpiralDirection[sampleIndex] * sampleDistNorm;
}

vec2 ComputeFibonacciSpiralDiskSampleUniform(const in int sampleIndex, const in float sampleCountInverse, const in float sampleBias, out float sampleDistNorm)
{
    sampleDistNorm = sampleIndex * sampleCountInverse + sampleBias;
    sampleDistNorm = sqrt(sampleDistNorm);
    return fibonacciSpiralDirection[sampleIndex] * sampleDistNorm;
}

void FilterScaleOffset(vec3 coord, float maxSampleZDistance, out vec2 filterScalePos, out vec2 filterScaleNeg, out vec2 filterOffset)
{
    float d = maxSampleZDistance / coord.z;
    vec2 target = (coord.xy + 0.5) * 0.5;
    filterScalePos = (1 - target) * d;
    filterScaleNeg = target * d;
    filterOffset = (target - coord.xy) * d;
}

bool BlockerSearch_Area(inout float closestBlocker, float maxSampleZDistance, vec3 posTCShadowmap, vec2 minCoord, vec2 maxCoord, vec2 sampleJitter, int sampleCount, int shadowMapIndex)
{
    const float nearPlane = 1.0;
    maxSampleZDistance = min(1 - posTCShadowmap.z, maxSampleZDistance);
    float sampleCountInverse = 1.0 / sampleCount;
    vec2 filterScalePos, filterScaleNeg, filterOffset;
    FilterScaleOffset(posTCShadowmap, maxSampleZDistance, filterScalePos, filterScaleNeg, filterOffset);
    closestBlocker = nearPlane;

    for (int i = 0; i < sampleCount && i < 64; ++i)
    {
        float sampleDistNorm;
        vec2 offset = ComputeFibonacciSpiralDiskSampleClumped(i, sampleCountInverse, sampleDistNorm);
        offset = vec2(offset.x * sampleJitter.y + offset.y * sampleJitter.x, offset.x * -sampleJitter.x + offset.y * sampleJitter.y);
        offset = offset * vec2(offset.x > 0 ? filterScalePos.x : filterScaleNeg.x, offset.y > 0 ? filterScalePos.y : filterScaleNeg.y) + filterOffset * sampleDistNorm;
        float zoffset = maxSampleZDistance * sampleDistNorm;

        vec2 pos = posTCShadowmap.xy + offset;
        float blocker = texture(shadowMapsSampler, vec3(pos, shadowMapIndex)).r;
        if (!(pos.x < minCoord.x || pos.y < minCoord.y || pos.x > maxCoord.x || pos.y > maxCoord.y) &&
            (blocker > posTCShadowmap.z + zoffset) &&
            (closestBlocker > blocker))
        {
            closestBlocker = blocker;
        }
    }

    return nearPlane > closestBlocker;
}

float PCSS_Area(vec3 posTCShadowmap, float maxSampleZDistance, vec2 minCoord, vec2 maxCoord, vec2 sampleJitter, int sampleCount, int shadowMapIndex)
{
    float biasFactor = 1.0;
    float sampleCountInverse = 1.0 / (sampleCount + biasFactor);
    float sampleBias = biasFactor * sampleCountInverse;
    vec2 filterScalePos, filterScaleNeg, filterOffset;
    FilterScaleOffset(posTCShadowmap, maxSampleZDistance, filterScalePos, filterScaleNeg, filterOffset);

    float sum = 0.0;
    for (int i = 0; i < sampleCount && i < 64; ++i)
    {
        float sampleDistNorm;
        vec2 offset = ComputeFibonacciSpiralDiskSampleUniform(i, sampleCountInverse, sampleBias, sampleDistNorm);
        offset = vec2(offset.x * sampleJitter.y + offset.y * sampleJitter.x, offset.x * -sampleJitter.x + offset.y * sampleJitter.y);
        offset = offset * vec2(offset.x > 0 ? filterScalePos.x : filterScaleNeg.x, offset.y > 0 ? filterScalePos.y : filterScaleNeg.y) + filterOffset * sampleDistNorm;
        float zoffset = maxSampleZDistance * sampleDistNorm;

        vec2 pos = posTCShadowmap.xy + offset;
        sum += (pos.x < minCoord.x || pos.y < minCoord.y || pos.x > maxCoord.x || pos.y > maxCoord.y) ? 1.0 : texture(shadowMaps, vec4(pos, shadowMapIndex, posTCShadowmap.z + zoffset)).r;
    }

    return sum / sampleCount;
}

float InterleavedGradientNoise(vec2 pixCoord, uint frameCount)
{
    const vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
    vec2 frameMagicScale = vec2(2.083f, 4.867f);
    pixCoord += frameCount * frameMagicScale;
    return fract(magic.z * fract(dot(pixCoord, magic.xy)));
}

float PenumbraSizePunctual(float receiver, float blocker)
{
    return abs((receiver - blocker) / blocker);
}

float SampleShadow_PCSS_Area(vec3 posTCShadowmap, vec2 posSS, float shadowSoftness, float minFilterRadius, int blockerSampleCount, int filterSampleCount, float depthBias, int shadowMapIndex)
{
    posTCShadowmap.z += depthBias;
    float maxSampleZDistance = shadowSoftness * PCSS_MAX_DISTANCE_SCALE;
    float sampleJitterAngle = InterleavedGradientNoise(posSS.xy, PCSS_FRAME_NUM) * 2.0 * PI;
    vec2 sampleJitter = vec2(sin(sampleJitterAngle), cos(sampleJitterAngle));
    vec2 minCoord = vec2(0);
    vec2 maxCoord = vec2(1);

    float blocker = 0.0;
    bool blockerFound = BlockerSearch_Area(blocker, maxSampleZDistance, posTCShadowmap, minCoord, maxCoord, sampleJitter, blockerSampleCount, shadowMapIndex);
    maxSampleZDistance *= PenumbraSizePunctual(posTCShadowmap.z, blocker);
    maxSampleZDistance = min(maxSampleZDistance, (blocker - posTCShadowmap.z) * PCSS_PENUMBRA_CLAMP);
    maxSampleZDistance = max(maxSampleZDistance, minFilterRadius * PCSS_MIN_FILTER_SCALE);

    bool withinShadowmap = posTCShadowmap.x > 0 && posTCShadowmap.y > 0 && posTCShadowmap.x < 1 && posTCShadowmap.y < 1;
    return blockerFound && withinShadowmap ? PCSS_Area(posTCShadowmap, maxSampleZDistance, minCoord, maxCoord, sampleJitter, filterSampleCount, shadowMapIndex) : 1.0;
}

#ifdef PCSS_WITH_AREA_PARAM
float SampleShadow_PCSS_Area_WithArea(vec3 posTCShadowmap, vec2 posSS, float shadowSoftness, float minFilterRadius, int blockerSampleCount, int filterSampleCount, float depthBias, int shadowMapIndex, float area)
{
    posTCShadowmap.z += depthBias;
    float maxSampleZDistance = shadowSoftness * PCSS_MAX_DISTANCE_SCALE * sqrt(area);
    float sampleJitterAngle = InterleavedGradientNoise(posSS.xy, PCSS_FRAME_NUM) * 2.0 * PI;
    vec2 sampleJitter = vec2(sin(sampleJitterAngle), cos(sampleJitterAngle));
    vec2 minCoord = vec2(0);
    vec2 maxCoord = vec2(1);

    float blocker = 0.0;
    bool blockerFound = BlockerSearch_Area(blocker, maxSampleZDistance, posTCShadowmap, minCoord, maxCoord, sampleJitter, blockerSampleCount, shadowMapIndex);
    maxSampleZDistance *= PenumbraSizePunctual(posTCShadowmap.z, blocker);
    maxSampleZDistance = min(maxSampleZDistance, (blocker - posTCShadowmap.z) * PCSS_PENUMBRA_CLAMP);
    maxSampleZDistance = max(maxSampleZDistance, minFilterRadius * PCSS_MIN_FILTER_SCALE);

    bool withinShadowmap = posTCShadowmap.x > 0 && posTCShadowmap.y > 0 && posTCShadowmap.x < 1 && posTCShadowmap.y < 1;
    return blockerFound && withinShadowmap ? PCSS_Area(posTCShadowmap, maxSampleZDistance, minCoord, maxCoord, sampleJitter, filterSampleCount, shadowMapIndex) : 1.0;
}
#endif
