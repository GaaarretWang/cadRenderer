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

#ifdef VSG_DIFFUSE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D diffuseMap;
#endif

#ifdef VSG_METALLROUGHNESS_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D mrMap;
#endif

#ifdef VSG_NORMAL_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D normalMap;
#endif

#ifdef VSG_LIGHTMAP_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D aoMap;
#endif

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

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 normalDir;
layout(location = 2) in vec4 vertexColor;
layout(location = 3) in vec2 texCoord0;
layout(location = 4) in float highlight;

layout(location = 6) in vec3 worldNormal;
layout(location = 7) in vec3 worldViewDir;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(1, 1, 1, 1);
    return;
}
