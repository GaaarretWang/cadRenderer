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

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D samplerSSAO;

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

// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec4 LINEARtoSRGB(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(1.0 / 2.2));
    return vec4(linOut, srgbIn.w);
}

void main()
{
    vec4 colorAttachment = subpassLoad(colorInputAttachment);
    vec3 colorData = colorAttachment.xyz;
    vec2 uv = inUV * 0.5 + 0.5;

    vec2 texelSize = 1.0 / vec2(textureSize(samplerSSAO, 0));
    float result = 0.0;
    for (int x = -2; x <= 2; ++x) 
    {
        for (int y = -2; y <= 2; ++y) 
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(samplerSSAO, uv + offset).r;
        }
    }
    if(texture(samplerSSAO, uv).w > 0.5){
        outColor = vec4(colorData, 1);
    }else{
        float exposure = 5.0f;
        vec3 color = Uncharted2Tonemap(colorData * result / 25.0 * result / 25.0 * exposure);
        color = color * (vec3(1.0f) / Uncharted2Tonemap(vec3(11.2f)));
        outColor = LINEARtoSRGB(vec4(color, 1));
    }

    return;
}
