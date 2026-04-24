#version 450
#extension GL_ARB_separate_shader_objects : enable

#define VIEW_DESCRIPTOR_SET 1
#define MATERIAL_DESCRIPTOR_SET 2

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D cameraImageSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D realDepthSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D sceneDepthSampler;

#define GLOBAL_BUFFER_SET MATERIAL_DESCRIPTOR_SET
#define GLOBAL_BUFFER_BINDING 3
#pragma include "global_buffer.glsl"

layout(push_constant) uniform PushConstants {
    mat4 proj;
    mat4 view;
    mat4 last_view;
    vec3 camera_pos;
    float softness;
    float baseBrightness;
    float ssao_radius;
    float exposure;
    float softness_falloff;
    float shadow_bias;
    int ssao_kernel_size;
    int denoise_size;
    int blocker_sample_num;
    int pcf_sample_num;
    int shadow_type;
    uint frame_num;
} pc;

layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData {
    vec4 values[2048];
} lightData;
layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps;
layout(set = VIEW_DESCRIPTOR_SET, binding = 3) uniform sampler2DArray shadowMapsSampler;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

const float kDepthMatchEpsilon = 5e-4;
const float kBrightnessCutoff = 0.001;

#pragma include "sample_data.glsl"

vec2 Rotate(vec2 pos, vec2 rotationTrig)
{
    return vec2(pos.x * rotationTrig.x - pos.y * rotationTrig.y, pos.y * rotationTrig.x + pos.x * rotationTrig.y);
}

float PCF_sky(vec4 coords, int shadowMapIndex, float area, float random)
{
    float rotationAngle = random * 3.1415926;
    vec2 rotationTrig = vec2(cos(rotationAngle), sin(rotationAngle));

    float linearFrac = sqrt(max(area, 0.0));
    float baseStridePixels = 20.0;
    float stride = baseStridePixels * linearFrac * pc.softness + 0.001;
    float shadowmapSize = 2048.0;
    float visibility = 0.0;

    for (int i = 0; i < pc.pcf_sample_num; i++)
    {
        float sampleValue = texture(shadowMaps, vec4(coords.xy + Rotate(poissonDisk[i * 64 / pc.pcf_sample_num] * stride / shadowmapSize, rotationTrig), shadowMapIndex, coords.z)).r;
        visibility += sampleValue;
    }
    return visibility / float(pc.pcf_sample_num);
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

bool BlockerSearch_Area_sky(inout float closestBlocker, float maxSampleZDistance, vec3 posTCShadowmap, vec2 minCoord, vec2 maxCoord, vec2 sampleJitter, int sampleCount, int shadowMapIndex)
{
    const float nearPlane = 1.0;
    maxSampleZDistance = min(1 - posTCShadowmap.z, maxSampleZDistance);
    float sampleCountInverse = 1.0 / sampleCount;

    vec2 filterScalePos;
    vec2 filterScaleNeg;
    vec2 filterOffset;
    FilterScaleOffset(posTCShadowmap, maxSampleZDistance, filterScalePos, filterScaleNeg, filterOffset);

    closestBlocker = nearPlane;
    for (int i = 0; i < sampleCount && i < 64; ++i)
    {
        float sampleDistNorm;
        vec2 offset = ComputeFibonacciSpiralDiskSampleClumped(i, sampleCountInverse, sampleDistNorm);
        offset = vec2(offset.x * sampleJitter.y + offset.y * sampleJitter.x,
                      offset.x * -sampleJitter.x + offset.y * sampleJitter.y);
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

float PCSS_Area_sky(vec3 posTCShadowmap, float maxSampleZDistance, vec2 minCoord, vec2 maxCoord, vec2 sampleJitter, int sampleCount, int shadowMapIndex)
{
    float biasFactor = 1.0;
    float sampleCountInverse = 1.0 / (sampleCount + biasFactor);
    float sampleBias = biasFactor * sampleCountInverse;

    vec2 filterScalePos;
    vec2 filterScaleNeg;
    vec2 filterOffset;
    FilterScaleOffset(posTCShadowmap, maxSampleZDistance, filterScalePos, filterScaleNeg, filterOffset);

    float sum = 0.0;
    for (int i = 0; i < sampleCount && i < 64; ++i)
    {
        float sampleDistNorm;
        vec2 offset = ComputeFibonacciSpiralDiskSampleUniform(i, sampleCountInverse, sampleBias, sampleDistNorm);
        offset = vec2(offset.x * sampleJitter.y + offset.y * sampleJitter.x,
                      offset.x * -sampleJitter.x + offset.y * sampleJitter.y);
        offset = offset * vec2(offset.x > 0 ? filterScalePos.x : filterScaleNeg.x, offset.y > 0 ? filterScalePos.y : filterScaleNeg.y) + filterOffset * sampleDistNorm;
        float zoffset = maxSampleZDistance * sampleDistNorm;

        vec2 pos = posTCShadowmap.xy + offset;
        sum += (pos.x < minCoord.x || pos.y < minCoord.y || pos.x > maxCoord.x || pos.y > maxCoord.y) ?
            1.0 : texture(shadowMaps, vec4(pos, shadowMapIndex, posTCShadowmap.z + zoffset)).r;
    }
    return sum / sampleCount;
}

float InterleavedGradientNoise_sky(vec2 pixCoord, uint frameCount)
{
    const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    vec2 frameMagicScale = vec2(2.083, 4.867);
    pixCoord += frameCount * frameMagicScale;
    return fract(magic.z * fract(dot(pixCoord, magic.xy)));
}

float PenumbraSizePunctual_sky(float receiver, float blocker)
{
    return abs((receiver - blocker) / blocker);
}

float SampleShadow_PCSS_Area_sky(vec3 posTCShadowmap, vec2 posSS, float shadowSoftness, float minFilterRadius, int blockerSampleCount, int filterSampleCount, float depthBias, int shadowMapIndex, float area)
{
    posTCShadowmap.z += depthBias;

    float maxSampleZDistance = shadowSoftness * 0.1 * sqrt(area);
    float sampleJitterAngle = InterleavedGradientNoise_sky(posSS.xy, pc.frame_num) * 2.0 * 3.14159265359;
    vec2 sampleJitter = vec2(sin(sampleJitterAngle), cos(sampleJitterAngle));
    vec2 minCoord = vec2(0);
    vec2 maxCoord = vec2(1);

    float blocker = 0.0;
    bool blockerFound = BlockerSearch_Area_sky(blocker, maxSampleZDistance, posTCShadowmap, minCoord, maxCoord, sampleJitter, blockerSampleCount, shadowMapIndex);

    maxSampleZDistance *= PenumbraSizePunctual_sky(posTCShadowmap.z, blocker);
    maxSampleZDistance = min(maxSampleZDistance, (blocker - posTCShadowmap.z) * 0.9);
    maxSampleZDistance = max(maxSampleZDistance, minFilterRadius / 100.0);

    bool withinShadowmap = posTCShadowmap.x > 0 && posTCShadowmap.y > 0 && posTCShadowmap.x < 1 && posTCShadowmap.y < 1;
    return blockerFound && withinShadowmap ? PCSS_Area_sky(posTCShadowmap, maxSampleZDistance, minCoord, maxCoord, sampleJitter, filterSampleCount, shadowMapIndex) : 1.0;
}

float ValueNoise_sky(vec3 pos)
{
    vec3 noiseSkew = pos + 0.2127 + pos.x * pos.y * pos.z * 0.3713;
    vec3 noiseRnd = 4.789 * sin(489.123 * noiseSkew);
    return fract(noiseRnd.x * noiseRnd.y * noiseRnd.z * (1.0 + noiseSkew.x) * pc.frame_num);
}

float computeShadowBrightness(vec3 worldPos)
{
    vec4 lightNums = lightData.values[0];
    int numDirectionalLights = int(lightNums[1]);
    int index = 1;

    float sceneBrightness = 1.0;
    int shadowMapIndex = 0;
    if (numDirectionalLights > 0)
    {
        float totalBrightness = pc.baseBrightness;
        float totalRealBrightness = pc.baseBrightness;

        for (int i = 0; i < numDirectionalLights; ++i)
        {
            vec4 lightColor = lightData.values[index++];
            float area = lightData.values[index].w;
            index++;
            vec4 shadowMapSettings = lightData.values[index++];

            float brightness = lightColor.a;
            totalBrightness += brightness;
            float visibility = 0.0;

            bool matched = false;
            while ((shadowMapSettings.r > 0.0 && brightness > kBrightnessCutoff) && !matched)
            {
                mat4 sm_matrix = mat4(lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++]);

                vec4 sm_tc = sm_matrix * vec4(worldPos, 1.0);

                if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0)
                {
                    matched = true;
                    float random = ValueNoise_sky(sm_tc.xyz);
                    if (pc.shadow_type == 0)
                    {
                        visibility = PCF_sky(sm_tc, shadowMapIndex, area, random);
                    }
                    else if (pc.shadow_type == 1)
                    {
                        visibility = SampleShadow_PCSS_Area_sky(sm_tc.xyz, gl_FragCoord.xy, pc.softness, pc.softness_falloff, pc.blocker_sample_num, pc.pcf_sample_num, pc.shadow_bias, shadowMapIndex, area);
                    }
                }
                else
                {
                    visibility = 1.0;
                }

                ++shadowMapIndex;
                shadowMapSettings.r -= 1.0;
            }

            if (shadowMapSettings.r > 0.0)
            {
                index += 4 * int(shadowMapSettings.r);
                shadowMapIndex += int(shadowMapSettings.r);
            }

            totalRealBrightness += brightness * visibility;
        }
        sceneBrightness = totalRealBrightness / totalBrightness;
    }

    return sceneBrightness;
}

void main()
{
    vec2 uv = inUV * 0.5 + 0.5;
    float cameraDepthNorm = texture(realDepthSampler, uv).r;
    float linearZ = cameraDepthNorm * globalBuffer.z_far;

    if (linearZ <= 0.0 || linearZ >= globalBuffer.z_far)
    {
        outColor = texture(cameraImageSampler, uv);
        return;
    }

    float ndcX = uv.x * 2.0 - 1.0;
    float ndcY = uv.y * 2.0 - 1.0;
    vec4 ndcPoint = vec4(ndcX, ndcY, 0.0, 1.0);
    mat4 invProj = inverse(pc.proj);
    vec4 viewRay = invProj * ndcPoint;
    viewRay /= viewRay.w;
    vec3 rayDir = normalize(viewRay.xyz);
    float scale = -linearZ / rayDir.z;
    vec3 actualView = rayDir * scale;

    vec4 clipOut = pc.proj * vec4(actualView, 1.0);
    float realSceneDepth = clipOut.z / clipOut.w;

    if (globalBuffer.enable_real_depth_occlusion != 0)
    {
        float sceneDepth = texture(sceneDepthSampler, uv).r;
        if (abs(sceneDepth - realSceneDepth) > kDepthMatchEpsilon)
        {
            outColor = vec4(0.0);
            return;
        }
    }

    float sceneBrightness = 1.0;
    if (globalBuffer.shadow_mode == 1)
    {
        mat4 invView = inverse(pc.view);
        vec3 worldPos = (invView * vec4(actualView, 1.0)).xyz;
        sceneBrightness = computeShadowBrightness(worldPos);
    }

    vec3 cameraColor = texture(cameraImageSampler, uv).rgb * sceneBrightness;
    outColor = vec4(cameraColor, 1.0);
}
