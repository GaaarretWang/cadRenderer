#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_METALLROUGHNESS_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, VSG_WORKFLOW_SPECGLOSS, SHADOWMAP_DEBUG)

#define IBL_DESCRIPTOR_SET 0
#define VIEW_DESCRIPTOR_SET 1
#define MATERIAL_DESCRIPTOR_SET 2

const float PI = 3.14159265359;
const float RECIPROCAL_PI = 0.31830988618;
const float RECIPROCAL_PI2 = 0.15915494;
const float EPSILON = 1e-6;
const float c_MinRoughness = 0.04;

#define NUM_SAMPLES 16
#define BLOCKER_SEARCH_NUM_SAMPLES NUM_SAMPLES
#define PCF_NUM_SAMPLES NUM_SAMPLES
#define NUM_RINGS 10

#define EPS 1e-2  //ģӰжЧкܴӰ
#define PI 3.141592653589793
#define PI2 6.283185307179586

// 关键修正：添加 input_attachment_index = 1（对应子通道 1 输入附件列表的索引 1 → colorRef1_Read）
layout(
    input_attachment_index = 1,  // 强制要求：子通道输入附件列表中的索引
    set = MATERIAL_DESCRIPTOR_SET,  // 和 CPU 侧一致（比如 2）
    binding = 0  // 和 CPU 侧一致（比如 0）
) uniform subpassInput colorInputAttachment;  // color attachment 1（法线）

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D normalInputAttachment;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D worldPosInputAttachment;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D samplerNoise;

#ifdef VSG_EMISSIVE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 4) uniform sampler2D emissiveMap;
#endif

#ifdef VSG_SPECULAR_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 5) uniform sampler2D specularMap;
#endif

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 7) uniform sampler2D cameraImage;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 8) uniform sampler2D depthImage;

layout (set = MATERIAL_DESCRIPTOR_SET, binding = 9) uniform customParams {
	float semitransparent;
	int width;
	int height;
    float z_far;
    int shader_type;
} extraParams;

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 10) uniform PbrData
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 diffuseFactor;
    vec4 specularFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaMask;
    float alphaMaskCutoff;
} pbr;

// ViewDependentState
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[2048];
} lightData;


layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps;

layout(set = IBL_DESCRIPTOR_SET, binding = 0) uniform sampler2D samplerBRDFLUT;
layout(set = IBL_DESCRIPTOR_SET, binding = 1) uniform samplerCube samplerIrradiance;
layout(set = IBL_DESCRIPTOR_SET, binding = 2) uniform samplerCube samplerPrefilteredEnv;
layout(set = IBL_DESCRIPTOR_SET, binding = 3) uniform EnvmapParams{
    vec4 param;
}envmapData;

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
    mat4 invView;
    mat4 cameraData;
} pc;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

#define SSAO_WHOLE_KERNEL_SIZE 128
#define SSAO_KERNEL_SIZE 64
#define SSAO_RADIUS 0.1

const vec4 ssaoKernel[SSAO_WHOLE_KERNEL_SIZE] = vec4[](
    vec4(0.006057f, -0.020636f, 0.005974f, 0.0f),
    vec4(0.003845f, 0.002873f, 0.007254f, 0.0f),
    vec4(-0.008086f, -0.048698f, 0.011322f, 0.0f),
    vec4(-0.039987f, -0.025435f, 0.027443f, 0.0f),
    vec4(-0.000367f, 0.000117f, 0.000531f, 0.0f),
    vec4(0.011986f, 0.007766f, 0.006668f, 0.0f),
    vec4(0.009245f, -0.003304f, 0.000938f, 0.0f),
    vec4(0.047995f, 0.014326f, 0.055739f, 0.0f),
    vec4(0.004053f, 0.052923f, 0.021172f, 0.0f),
    vec4(0.035782f, 0.012874f, 0.046802f, 0.0f),
    vec4(0.012222f, -0.027134f, 0.006808f, 0.0f),
    vec4(-0.024888f, -0.015826f, 0.002991f, 0.0f),
    vec4(0.011517f, -0.011473f, 0.015711f, 0.0f),
    vec4(-0.026221f, 0.049136f, 0.036461f, 0.0f),
    vec4(-0.033791f, 0.023543f, 0.008395f, 0.0f),
    vec4(0.064887f, 0.018557f, 0.036912f, 0.0f),
    vec4(0.002760f, 0.002222f, 0.000922f, 0.0f),
    vec4(-0.064042f, -0.080599f, 0.036608f, 0.0f),
    vec4(0.032949f, -0.016224f, 0.028690f, 0.0f),
    vec4(0.028027f, -0.002782f, 0.008954f, 0.0f),
    vec4(0.017619f, -0.068117f, 0.083917f, 0.0f),
    vec4(-0.010958f, -0.030573f, 0.054328f, 0.0f),
    vec4(-0.053027f, -0.058704f, 0.007107f, 0.0f),
    vec4(0.047330f, -0.012614f, 0.005147f, 0.0f),
    vec4(0.080913f, 0.004748f, 0.079187f, 0.0f),
    vec4(-0.055471f, 0.025063f, 0.038704f, 0.0f),
    vec4(-0.049986f, 0.030218f, 0.011957f, 0.0f),
    vec4(-0.002699f, 0.026472f, 0.025545f, 0.0f),
    vec4(0.000131f, -0.071709f, 0.101826f, 0.0f),
    vec4(-0.011527f, 0.007947f, 0.017414f, 0.0f),
    vec4(0.044154f, 0.006624f, 0.065482f, 0.0f),
    vec4(-0.133877f, -0.047137f, 0.002610f, 0.0f),
    vec4(0.006512f, 0.005703f, 0.002644f, 0.0f),
    vec4(0.050024f, 0.059147f, 0.005667f, 0.0f),
    vec4(-0.014296f, 0.008649f, 0.012708f, 0.0f),
    vec4(-0.025101f, 0.050577f, 0.134586f, 0.0f),
    vec4(-0.023886f, -0.089564f, 0.083798f, 0.0f),
    vec4(-0.055764f, -0.035133f, 0.092845f, 0.0f),
    vec4(-0.028239f, 0.008018f, 0.027601f, 0.0f),
    vec4(-0.030108f, 0.016421f, 0.021395f, 0.0f),
    vec4(-0.157641f, 0.048175f, 0.042063f, 0.0f),
    vec4(0.080841f, -0.096465f, 0.026750f, 0.0f),
    vec4(-0.048680f, -0.062636f, 0.079683f, 0.0f),
    vec4(-0.002120f, 0.022080f, 0.031322f, 0.0f),
    vec4(-0.084348f, -0.014429f, 0.044320f, 0.0f),
    vec4(0.008358f, 0.006325f, 0.017954f, 0.0f),
    vec4(-0.011140f, -0.018383f, 0.049285f, 0.0f),
    vec4(-0.050490f, -0.008375f, 0.034379f, 0.0f),
    vec4(-0.090535f, 0.153162f, 0.080175f, 0.0f),
    vec4(0.014477f, -0.129286f, 0.143736f, 0.0f),
    vec4(0.024269f, 0.022063f, 0.021958f, 0.0f),
    vec4(-0.000585f, -0.011654f, 0.008164f, 0.0f),
    vec4(-0.045578f, 0.182765f, 0.049937f, 0.0f),
    vec4(-0.023391f, -0.040029f, 0.248857f, 0.0f),
    vec4(0.018064f, 0.070744f, 0.025070f, 0.0f),
    vec4(0.170523f, 0.028807f, 0.098629f, 0.0f),
    vec4(-0.199192f, 0.037864f, 0.113094f, 0.0f),
    vec4(-0.030799f, 0.041427f, 0.003601f, 0.0f),
    vec4(0.014020f, 0.025848f, 0.017349f, 0.0f),
    vec4(0.127451f, -0.082875f, 0.097072f, 0.0f),
    vec4(-0.078577f, 0.081396f, 0.254280f, 0.0f),
    vec4(-0.092477f, 0.067603f, 0.037318f, 0.0f),
    vec4(0.130682f, -0.152232f, 0.120329f, 0.0f),
    vec4(-0.205620f, -0.020066f, 0.240143f, 0.0f),
    vec4(-0.243751f, -0.163845f, 0.075741f, 0.0f),
    vec4(0.035104f, 0.034957f, 0.017029f, 0.0f),
    vec4(0.225226f, 0.137358f, 0.206393f, 0.0f),
    vec4(0.024284f, -0.077621f, 0.064433f, 0.0f),
    vec4(0.014111f, 0.037907f, 0.005799f, 0.0f),
    vec4(-0.204939f, 0.027757f, 0.071018f, 0.0f),
    vec4(0.043675f, -0.059488f, 0.063646f, 0.0f),
    vec4(-0.000681f, 0.024063f, 0.025115f, 0.0f),
    vec4(-0.096064f, -0.280709f, 0.002228f, 0.0f),
    vec4(0.064395f, -0.111798f, 0.174060f, 0.0f),
    vec4(-0.051494f, -0.349162f, 0.026790f, 0.0f),
    vec4(0.165189f, 0.018644f, 0.170656f, 0.0f),
    vec4(-0.246678f, -0.261152f, 0.108041f, 0.0f),
    vec4(0.040871f, 0.049784f, 0.062035f, 0.0f),
    vec4(-0.157513f, -0.249790f, 0.245295f, 0.0f),
    vec4(-0.225237f, 0.290287f, 0.185912f, 0.0f),
    vec4(0.200464f, 0.261823f, 0.222886f, 0.0f),
    vec4(-0.366142f, 0.182268f, 0.127971f, 0.0f),
    vec4(0.060745f, 0.073171f, 0.081474f, 0.0f),
    vec4(0.180788f, -0.246548f, 0.274341f, 0.0f),
    vec4(-0.086081f, 0.098183f, 0.071377f, 0.0f),
    vec4(0.070509f, -0.065033f, 0.002825f, 0.0f),
    vec4(-0.038568f, 0.081824f, 0.108568f, 0.0f),
    vec4(0.075201f, -0.053323f, 0.260752f, 0.0f),
    vec4(0.054282f, -0.047537f, 0.059962f, 0.0f),
    vec4(0.206278f, -0.104596f, 0.024172f, 0.0f),
    vec4(0.150625f, -0.122798f, 0.199764f, 0.0f),
    vec4(-0.240435f, 0.160398f, 0.266732f, 0.0f),
    vec4(-0.285876f, 0.244150f, 0.154453f, 0.0f),
    vec4(0.028012f, 0.019755f, 0.021084f, 0.0f),
    vec4(0.287455f, -0.297191f, 0.274714f, 0.0f),
    vec4(0.159708f, -0.145117f, 0.112387f, 0.0f),
    vec4(-0.105759f, -0.221429f, 0.068965f, 0.0f),
    vec4(0.255234f, 0.102773f, 0.261632f, 0.0f),
    vec4(-0.175052f, 0.042158f, 0.000178f, 0.0f),
    vec4(-0.060465f, 0.068980f, 0.282313f, 0.0f),
    vec4(-0.090042f, -0.445691f, 0.368313f, 0.0f),
    vec4(0.226141f, -0.252332f, 0.032389f, 0.0f),
    vec4(0.145534f, -0.180424f, 0.447976f, 0.0f),
    vec4(0.008476f, -0.013507f, 0.004884f, 0.0f),
    vec4(-0.025994f, -0.002533f, 0.043284f, 0.0f),
    vec4(-0.229313f, 0.347794f, 0.260560f, 0.0f),
    vec4(-0.000054f, -0.002447f, 0.003135f, 0.0f),
    vec4(0.208791f, 0.224666f, 0.044338f, 0.0f),
    vec4(-0.019510f, 0.027568f, 0.015590f, 0.0f),
    vec4(-0.311185f, 0.432203f, 0.283180f, 0.0f),
    vec4(0.204393f, 0.595077f, 0.363133f, 0.0f),
    vec4(0.282473f, 0.081296f, 0.259532f, 0.0f),
    vec4(0.346819f, 0.050225f, 0.470656f, 0.0f),
    vec4(-0.047605f, -0.452653f, 0.232371f, 0.0f),
    vec4(0.144376f, 0.011569f, 0.170208f, 0.0f),
    vec4(-0.226528f, -0.114880f, 0.072839f, 0.0f),
    vec4(0.061245f, -0.551594f, 0.176373f, 0.0f),
    vec4(0.182285f, -0.384821f, 0.179972f, 0.0f),
    vec4(-0.177966f, -0.619447f, 0.443873f, 0.0f),
    vec4(0.118142f, 0.274733f, 0.601905f, 0.0f),
    vec4(-0.149286f, -0.616648f, 0.219500f, 0.0f),
    vec4(0.390443f, 0.500890f, 0.231440f, 0.0f),
    vec4(0.058805f, 0.131617f, 0.140561f, 0.0f),
    vec4(-0.080475f, -0.590698f, 0.210787f, 0.0f),
    vec4(-0.027500f, -0.096268f, 0.067165f, 0.0f),
    vec4(0.125520f, -0.485657f, 0.202268f, 0.0f),
    vec4(-0.425669f, 0.128493f, 0.061074f, 0.0f),
    vec4(-0.303480f, -0.081584f, 0.071417f, 0.0f)
);

void main()
{
    vec4 colorData = subpassLoad(colorInputAttachment);
    mat4 cameraData = pc.cameraData;
    vec3 worldCamPos = vec3(cameraData[0][0], cameraData[0][1], cameraData[0][2]);
    outColor = colorData;


    vec2 uv = inUV * 0.5 + 0.5;
    vec3 worldPosition = texture(worldPosInputAttachment, uv).rgb;
    vec3 normal = texture(normalInputAttachment, uv).rgb;
    if(length(normal) < 0.001){
        outColor = vec4(1);
        return;
    }

    float originDist = length(worldPosition - worldCamPos);
    vec3 randDir = texture(samplerNoise, uv).rgb * 2.0f - 1.0f;

    vec3 tangent = normalize(randDir - normal * dot(randDir, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Calculate occlusion value.
	float occlusion = 0.0f;
    for(uint i = 0; i < SSAO_KERNEL_SIZE; i++) {
        vec3 samplePos = TBN * ssaoKernel[i * (SSAO_WHOLE_KERNEL_SIZE / SSAO_KERNEL_SIZE)].xyz;
        samplePos = samplePos * SSAO_RADIUS + worldPosition;

        float sampleDepth = length(samplePos - worldCamPos);

        vec4 samplePosProj = pc.projection * pc.view * vec4(samplePos, 1.0f);
        samplePosProj /= samplePosProj.w;

        vec2 sampleUV = vec2(samplePosProj.x, samplePosProj.y) * 0.5f + 0.5f;

        vec3 sceneWorldPos = texture(worldPosInputAttachment, sampleUV).rgb;
        if(length(sceneWorldPos) < 0.001)
            continue;
        float sceneDepth = length(sceneWorldPos - worldCamPos);

        float rangeCheck = step(abs(sampleDepth - sceneDepth), SSAO_RADIUS);
        occlusion += step(sceneDepth, sampleDepth) * rangeCheck;
    }

    float factor = 1 - (occlusion / float(SSAO_KERNEL_SIZE));
    outColor.xyz = vec3(factor);
    
    return;
}
