#version 450

// Define DEFERRED_MSAA before including this file for MSAA variant
// #define DEFERRED_MSAA

#define IBL_DESCRIPTOR_SET 0
#define VIEW_DESCRIPTOR_SET 1
#define MATERIAL_DESCRIPTOR_SET 2

const float PI = 3.14159265359;
const float RECIPROCAL_PI = 0.31830988618;

layout(set = IBL_DESCRIPTOR_SET, binding = 0) uniform sampler2D samplerBRDFLUT;
layout(set = IBL_DESCRIPTOR_SET, binding = 1) uniform samplerCube samplerIrradiance;
layout(set = IBL_DESCRIPTOR_SET, binding = 2) uniform samplerCube samplerPrefilteredEnv;
layout(set = IBL_DESCRIPTOR_SET, binding = 3) uniform EnvmapParams {
    vec4 param;
} envmapData;

layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData {
    vec4 values[2048];
} lightData;
layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps;
layout(set = VIEW_DESCRIPTOR_SET, binding = 3) uniform sampler2DArray shadowMapsSampler;

#ifdef DEFERRED_MSAA
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2DMS colorSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2DMS normalSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2DMS worldPosSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2DMS materialSampler;
#else
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D colorSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D normalSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D worldPosSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D materialSampler;
#endif
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 4) uniform sampler2D ssaoSampler;

#define GLOBAL_BUFFER_SET MATERIAL_DESCRIPTOR_SET
#define GLOBAL_BUFFER_BINDING 5
#pragma include "global_buffer.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

#pragma include "sample_data.glsl"

#define PCSS_FRAME_NUM globalBuffer.frame_num
#define PCSS_SOFTNESS globalBuffer.softness
#define PCSS_PCF_SAMPLE_NUM globalBuffer.pcf_sample_num
#define PCSS_MAX_DISTANCE_SCALE 65.0
#define PCSS_PENUMBRA_CLAMP 1.8
#define PCSS_MIN_FILTER_SCALE 10.0
#pragma include "pcss.glsl"

#pragma include "pbr_brdf.glsl"
#pragma include "ibl.glsl"
#pragma include "tonemapping.glsl"

void main()
{
    vec2 uv = inUV * 0.5 + 0.5;
#ifdef DEFERRED_MSAA
    ivec2 inputSize = textureSize(normalSampler);
    ivec2 coord = clamp(ivec2(uv * vec2(inputSize)), ivec2(0), inputSize - 1);
    int sampleIndex = gl_SampleID;
#else
    ivec2 inputSize = textureSize(normalSampler, 0);
    ivec2 coord = clamp(ivec2(uv * vec2(inputSize)), ivec2(0), inputSize - 1);
    int sampleIndex = 0;
#endif

    vec4 normalData = texelFetch(normalSampler, coord, sampleIndex);
    vec4 worldPosData = texelFetch(worldPosSampler, coord, sampleIndex);
    vec4 materialData = texelFetch(materialSampler, coord, sampleIndex);
    vec4 colorData = texelFetch(colorSampler, coord, sampleIndex);

    vec3 worldN = normalData.xyz;
    vec3 worldPos = worldPosData.xyz;
    if (length(worldN) < 0.001 || length(worldPos) < 0.001)
    {
        outColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    worldN = normalize(worldN);
    vec3 worldV = normalize(globalBuffer.camera_pos - worldPos);
    vec3 baseColor = max(materialData.rgb, vec3(0.0));
    float perceptualRoughness = clamp(normalData.w, 0.04, 1.0);
    float metallic = clamp(worldPosData.w, 0.0, 1.0);
    float occlusion = clamp(texture(ssaoSampler, uv).r, 0.0, 1.0);

    vec3 diffuseColor = baseColor.rgb * (vec3(1.0) - vec3(0.04));
    diffuseColor *= 1.0 - metallic;
    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    vec3 specularColor = mix(vec3(0.04), baseColor.rgb, metallic);
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor;
    vec3 specularEnvironmentR90 = vec3(1.0) * reflectance90;

    vec3 color = IBL(worldV, worldN, perceptualRoughness, metallic, specularEnvironmentR0, diffuseColor) * envmapData.param.a;

    vec4 lightNums = lightData.values[0];
    int numDirectionalLights = int(lightNums[1]);
    int index = 1;
    float scene_brightness = 1.0;
    if (numDirectionalLights > 0)
    {
        int shadowMapIndex = 0;
        float totalBrightness = globalBuffer.baseBrightness;
        float totalRealBrightness = globalBuffer.baseBrightness;
        for (int i = 0; i < numDirectionalLights; ++i)
        {
            vec4 lightColor = lightData.values[index++];
            float area = lightData.values[index].w;
            vec3 direction = -lightData.values[index++].xyz;
            vec4 shadowMapSettings = lightData.values[index++];
            float brightness = lightColor.a;
            totalBrightness += brightness;
            bool matched = false;
            float visibility = 0.0;

            while ((shadowMapSettings.r > 0.0 && brightness > 0.001) && !matched)
            {
                mat4 sm_matrix = mat4(lightData.values[index++], lightData.values[index++], lightData.values[index++], lightData.values[index++]);
                vec4 sm_tc = sm_matrix * vec4(worldPos, 1.0);
                if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0)
                {
                    matched = true;
                    float random = ValueNoise(sm_tc.xyz);
                    if (globalBuffer.shadow_type == 0)
                    {
                        visibility = PCF(sm_tc, shadowMapIndex, area, random);
                    }
                    else
                    {
                        visibility = SampleShadow_PCSS_Area(sm_tc.xyz, vec2(gl_FragCoord.xy), globalBuffer.softness, globalBuffer.softness_falloff, globalBuffer.blocker_sample_num, globalBuffer.pcf_sample_num, globalBuffer.shadow_bias, shadowMapIndex);
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
        scene_brightness = totalRealBrightness / totalBrightness;
    }

    vec3 mapped = Uncharted2Tonemap(color * scene_brightness * occlusion * globalBuffer.exposure);
    mapped *= vec3(1.0) / Uncharted2Tonemap(vec3(11.2));
    float opaqueCoverage = colorData.a > 0.999 ? 1.0 : 0.0;
    outColor = LINEARtoSRGB(vec4(mapped, opaqueCoverage));
}
