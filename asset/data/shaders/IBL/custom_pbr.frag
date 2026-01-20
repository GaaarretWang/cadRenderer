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

layout(std430, set = MATERIAL_DESCRIPTOR_SET, binding = 12) buffer ConstantBuffer {
    float z_far;
    int shader_type;
    int width;
    int height;
}constantBuffer;

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 13) uniform sampler2DMS shadowInputAttachment;

// ViewDependentState
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[2048];
} lightData;

layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps;
layout(set = VIEW_DESCRIPTOR_SET, binding = 3) uniform sampler2DArray shadowMapsSampler;

layout(set = IBL_DESCRIPTOR_SET, binding = 0) uniform sampler2D samplerBRDFLUT;
layout(set = IBL_DESCRIPTOR_SET, binding = 1) uniform samplerCube samplerIrradiance;
layout(set = IBL_DESCRIPTOR_SET, binding = 2) uniform samplerCube samplerPrefilteredEnv;
layout(set = IBL_DESCRIPTOR_SET, binding = 3) uniform EnvmapParams{
    vec4 param;
}envmapData;
// layout(set = IBL_DESCRIPTOR_SET, binding = 3) uniform EnvmapData
// {
//     vec4 params;
// } envmapData;

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 normalDir;
layout(location = 2) in vec4 vertexColor;
layout(location = 3) in vec2 texCoord0;
layout(location = 4) in float highlight;
layout(location = 5) in float InstanceID;

layout(location = 6) in vec3 worldNormal;
layout(location = 7) in vec3 worldViewDir;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outWorldPos;
layout(location = 3) out vec4 outShadow;

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
    mat4 last_view;
    vec3 camera_pos;
    float softness;
    float baseBrightness;
    float ssao_radius;
    float exposure;
    float softness_falloff;
    int ssao_kernel_size;
    int denoise_size;
    int blocker_sample_num;
    int pcf_sample_num;
    int shadow_type;
    uint frame_num;
} pc;

highp float rand_1to1(highp float x ) {                              //
  // float͵һάxһ[-1,1]Χfloat 
  // -1 -1
  return fract(sin(x)*10000.0);
}

highp float rand_2to1(vec2 uv ) {                              //
  // һάuvһΧ[0,1]float
  // 0 - 1
	const highp float a = 12.9898, b = 78.233, c = 43758.5453;
	highp float dt = dot( uv.xy, vec2( a,b ) ), sn = mod( dt, PI );
	return fract(sin(sn) * c);
}

float unpack(vec4 rgbaDepth) {                             //
    //
    //unpack()ԿshadowFragment.glslеpack()ķת
    // RGBAֵת[0,1]ĸ
    const vec4 bitShift = vec4(1.0, 1.0/256.0, 1.0/(256.0*256.0), 1.0/(256.0*256.0*256.0));
    return dot(rgbaDepth, bitShift);
}

vec2 poissonDisk[64] = {
	vec2(0.0617981, 0.07294159),
	vec2(0.6470215, 0.7474022),
	vec2(-0.5987766, -0.7512833),
	vec2(-0.693034, 0.6913887),
	vec2(0.6987045, -0.6843052),
	vec2(-0.9402866, 0.04474335),
	vec2(0.8934509, 0.07369385),
	vec2(0.1592735, -0.9686295),
	vec2(-0.05664673, 0.995282),
	vec2(-0.1203411, -0.1301079),
	vec2(0.1741608, -0.1682285),
	vec2(-0.09369049, 0.3196758),
	vec2(0.185363, 0.3213367),
	vec2(-0.1493771, -0.3147511),
	vec2(0.4452095, 0.2580113),
	vec2(-0.1080467, -0.5329178),
	vec2(0.1604507, 0.5460774),
	vec2(-0.4037193, -0.2611179),
	vec2(0.5947998, -0.2146744),
	vec2(0.3276062, 0.9244621),
	vec2(-0.6518704, -0.2503952),
	vec2(-0.3580975, 0.2806469),
	vec2(0.8587891, 0.4838005),
	vec2(-0.1596546, -0.8791054),
	vec2(-0.3096867, 0.5588146),
	vec2(-0.5128918, 0.1448544),
	vec2(0.8581337, -0.424046),
	vec2(0.1562584, -0.5610626),
	vec2(-0.7647934, 0.2709858),
	vec2(-0.3090832, 0.9020988),
	vec2(0.3935608, 0.4609676),
	vec2(0.3929337, -0.5010948),
	vec2(-0.8682281, -0.1990303),
	vec2(-0.01973724, 0.6478714),
	vec2(-0.3897587, -0.4665619),
	vec2(-0.7416366, -0.4377831),
	vec2(-0.5523247, 0.4272514),
	vec2(-0.5325066, 0.8410385),
	vec2(0.3085465, -0.7842533),
	vec2(0.8400612, -0.200119),
	vec2(0.6632416, 0.3067062),
	vec2(-0.4462856, -0.04265022),
	vec2(0.06892014, 0.812484),
	vec2(0.5149567, -0.7502338),
	vec2(0.6464897, -0.4666451),
	vec2(-0.159861, 0.1038342),
	vec2(0.6455986, 0.04419327),
	vec2(-0.7445076, 0.5035095),
	vec2(0.9430245, 0.3139912),
	vec2(0.0349884, -0.7968109),
	vec2(-0.9517487, 0.2963554),
	vec2(-0.7304786, -0.01006928),
	vec2(-0.5862702, -0.5531025),
	vec2(0.3029106, 0.09497032),
	vec2(0.09025345, -0.3503742),
	vec2(0.4356628, -0.0710125),
	vec2(0.4112572, 0.7500054),
	vec2(0.3401214, -0.3047142),
	vec2(-0.2192158, -0.6911137),
	vec2(-0.4676369, 0.6570358),
	vec2(0.6295372, 0.5629555),
	vec2(0.1253822, 0.9892166),
	vec2(-0.1154335, 0.8248222),
	vec2(-0.4230408, -0.7129914),
};

// void uniformDiskSamples( const in vec2 randomSeed ) {                              //
//   //Բ̲

//   float randNum = rand_2to1(randomSeed);
//   float sampleX = rand_1to1( randNum ) ;
//   float sampleY = rand_1to1( sampleX ) ;

//   float angle = sampleX * PI2;
//   float radius = sqrt(sampleY);

//   for( int i = 0; i < pc.shadow_sample_num; i ++ ) {
//     poissonDisk[i] = vec2( radius * cos(angle) , radius * sin(angle)  );

//     sampleX = rand_1to1( sampleY ) ;
//     sampleY = rand_1to1( sampleX ) ;

//     angle = sampleX * PI2;
//     radius = sqrt(sampleY);
//   }
// }

// void poissonDiskSamples( const in vec2 randomSeed ) {         
//     float ANGLE_STEP = PI2 * float( NUM_RINGS ) / float( pc.shadow_sample_num );
//     float INV_NUM_SAMPLES = 1.0 / float( pc.shadow_sample_num );

//     float angle = rand_2to1( randomSeed ) * PI2;
//     float radius = INV_NUM_SAMPLES;
//     float radiusStep = radius;

//     for( int i = 0; i < pc.shadow_sample_num; i ++ ) {
//         poissonDisk[i] = vec2( cos( angle ), sin( angle ) ) * pow( radius, 0.75 );
//         radius += radiusStep;
//         angle += ANGLE_STEP;
//     }
// }

vec2 Rotate(vec2 pos, vec2 rotationTrig)
{
	return vec2(pos.x * rotationTrig.x - pos.y * rotationTrig.y, pos.y * rotationTrig.x + pos.x * rotationTrig.y);
}

float PCF(sampler2DArrayShadow shadowMap, vec4 coords,int shadowMapIndex, float area, float random) {
    float rotationAngle = random * 3.1415926;
	vec2 rotationTrig = vec2(cos(rotationAngle), sin(rotationAngle));

    float linearFrac = sqrt(max(area, 0.0));//将area映射为线性尺寸
    float baseStridePixels = 20.0; //基础步长
    const float softness = pc.softness; // 调节此值来放大/缩小基于 area 的影响
    float Stride = baseStridePixels * linearFrac * softness + 0.001; // 最小非零避免 0
    float shadowmapSize = 2048.;
    float visibility = 0.0;
    float cur_depth = coords.z;
    
    float ctrl = 1.0;
        
    for(int i =0 ; i < pc.pcf_sample_num; i++)
    {
        float res  = texture(shadowMap, vec4(coords.xy + Rotate(poissonDisk[i * 64 / pc.pcf_sample_num] * Stride / shadowmapSize, rotationTrig), shadowMapIndex, coords.z)).r;
        visibility += res;
    }

    return visibility / float(pc.pcf_sample_num);
}

vec2 findBlocker(sampler2DArrayShadow shadowMap,  vec4 coords, int shadowMapIndex, float search_size, vec2 rotationTrig) {
    float blockerNum = 0;
    float block_depth = 0.;

    for(int i = 0; i < pc.blocker_sample_num; i++){ //Ƽ˰汾
        vec2 xy=coords.xy + Rotate(poissonDisk[i * 64 / pc.blocker_sample_num] * search_size, rotationTrig);
        float depthInShadowmap = texture(shadowMapsSampler, vec3(xy, shadowMapIndex)).r;
        if(depthInShadowmap - 0.0001 > coords.z){
            block_depth += depthInShadowmap;
            blockerNum += 1.0;
        }
    }
    return vec2(1 - block_depth / blockerNum, blockerNum);
}

float PCSS(sampler2DArrayShadow shadowMap, vec4 coords,int shadowMapIndex, float area, float random){
    float d_Receiver = 1 - coords.z;
	float rotationAngle = random * 3.1415926;
	vec2 rotationTrig = vec2(cos(rotationAngle), sin(rotationAngle));

    //todo: pc.softness
    float searchSize = pc.softness * clamp(d_Receiver - 0.02, 0.0, 1.0) / d_Receiver;
    vec2 blockerInfo = findBlocker(shadowMap, coords,shadowMapIndex, searchSize, rotationTrig);
	if (blockerInfo.y < 1)
	{
		//There are no occluders so early out (this saves filtering)
		return 0.0;
	}
    float d_Blocker = blockerInfo.x;
    float w_penumbra = d_Receiver - d_Blocker;

    //todo: pc.softness_falloff
    w_penumbra = 1.0 - pow(1.0 - w_penumbra, sqrt(area) * pc.softness_falloff);
    float filterRadiusUV = w_penumbra * pc.softness;

    float Stride = 20.;
    float shadowmapSize = 2048.;
    float visibility = 0.;
    float cur_depth = coords.z;

    //float ctrl = 1.0;
    //float bias = getBias(ctrl);

    for(int i = 0; i < pc.pcf_sample_num; i++){
        float res  = texture(shadowMap, vec4(coords.xy + Rotate(poissonDisk[i * 64 / pc.pcf_sample_num] * filterRadiusUV, rotationTrig), shadowMapIndex, coords.z)).r;
        visibility += res;
    }

    return visibility / float(pc.pcf_sample_num);
}

float ValueNoise(vec3 pos)
{
	vec3 Noise_skew = pos + 0.2127 + pos.x * pos.y * pos.z * 0.3713;
	vec3 Noise_rnd = 4.789 * sin(489.123 * (Noise_skew));
	return fract(Noise_rnd.x * Noise_rnd.y * Noise_rnd.z * (1.0 + Noise_skew.x) * pc.frame_num);
}


// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct PBRInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector
    float VdotH;                  // cos angle between view direction and half vector
    float VdotL;                  // cos angle between view direction and light direction
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    float metalness;              // metallic value at the surface
    vec3 reflectance0;            // full reflectance color (normal incidence angle)
    vec3 reflectance90;           // reflectance color at grazing angle
    float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 diffuseColor;            // color contribution from diffuse lighting
    vec3 specularColor;           // color contribution from specular lighting
};



vec4 SRGBtoLINEAR(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
    return vec4(linOut,srgbIn.w);
}

vec4 LINEARtoSRGB(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(1.0 / 2.2));
    return vec4(linOut, srgbIn.w);
}

float rcp(const in float value)
{
    return 1.0 / value;
}

float pow5(const in float value)
{
    return value * value * value * value * value;
}

vec3 getWorldNormal()
{
    vec3 result;
#ifdef VSG_NORMAL_MAP
    vec3 tangentNormal = texture(normalMap, texCoord0).xyz * 2.0 - 1.0;
    
    vec3 Q1 = dFdx(worldViewDir);
    vec3 Q2 = dFdy(worldViewDir);
    vec2 st1 = dFdx(texCoord0);
    vec2 st2 = dFdy(texCoord0);

    vec3 N = normalize(worldNormal);
    vec3 T = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    result = normalize(TBN * tangentNormal);
#else
    result = normalize(worldNormal);
#endif
    if (!gl_FrontFacing)
        result = -result;
    return result;
}
// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
vec3 BRDF_Diffuse_Lambert(PBRInfo pbrInputs)
{
    return pbrInputs.diffuseColor * RECIPROCAL_PI;
}

vec3 BRDF_Diffuse_Custom_Lambert(PBRInfo pbrInputs)
{
    return pbrInputs.diffuseColor * RECIPROCAL_PI * pow(pbrInputs.NdotV, 0.5 + 0.3 * pbrInputs.perceptualRoughness);
}

// [Gotanda 2012, "Beyond a Simple Physically Based Blinn-Phong Model in Real-Time"]
vec3 BRDF_Diffuse_OrenNayar(PBRInfo pbrInputs)
{
    float a = pbrInputs.alphaRoughness;
    float s = a;// / ( 1.29 + 0.5 * a );
    float s2 = s * s;
    float VoL = 2 * pbrInputs.VdotH * pbrInputs.VdotH - 1;		// double angle identity
    float Cosri = pbrInputs.VdotL - pbrInputs.NdotV * pbrInputs.NdotL;
    float C1 = 1 - 0.5 * s2 / (s2 + 0.33);
    float C2 = 0.45 * s2 / (s2 + 0.09) * Cosri * ( Cosri >= 0 ? 1.0 / max(pbrInputs.NdotL, pbrInputs.NdotV) : 1 );
    return pbrInputs.diffuseColor / PI * ( C1 + C2 ) * ( 1 + pbrInputs.perceptualRoughness * 0.5 );
}

// [Gotanda 2014, "Designing Reflectance Models for New Consoles"]
vec3 BRDF_Diffuse_Gotanda(PBRInfo pbrInputs)
{
    float a = pbrInputs.alphaRoughness;
    float a2 = a * a;
    float F0 = 0.04;
    float VoL = 2 * pbrInputs.VdotH * pbrInputs.VdotH - 1;		// double angle identity
    float Cosri = VoL - pbrInputs.NdotV * pbrInputs.NdotL;
    float a2_13 = a2 + 1.36053;
    float Fr = ( 1 - ( 0.542026*a2 + 0.303573*a ) / a2_13 ) * ( 1 - pow( 1 - pbrInputs.NdotV, 5 - 4*a2 ) / a2_13 ) * ( ( -0.733996*a2*a + 1.50912*a2 - 1.16402*a ) * pow( 1 - pbrInputs.NdotV, 1 + rcp(39*a2*a2+1) ) + 1 );
    //float Fr = ( 1 - 0.36 * a ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( -2.5 * Roughness * ( 1 - NoV ) + 1 );
    float Lm = ( max( 1 - 2*a, 0 ) * ( 1 - pow5( 1 - pbrInputs.NdotL ) ) + min( 2*a, 1 ) ) * ( 1 - 0.5*a * (pbrInputs.NdotL - 1) ) * pbrInputs.NdotL;
    float Vd = ( a2 / ( (a2 + 0.09) * (1.31072 + 0.995584 * pbrInputs.NdotV) ) ) * ( 1 - pow( 1 - pbrInputs.NdotL, ( 1 - 0.3726732 * pbrInputs.NdotV * pbrInputs.NdotV ) / ( 0.188566 + 0.38841 * pbrInputs.NdotV ) ) );
    float Bp = Cosri < 0 ? 1.4 * pbrInputs.NdotV * pbrInputs.NdotL * Cosri : Cosri;
    float Lr = (21.0 / 20.0) * (1 - F0) * ( Fr * Lm + Vd + Bp );
    return pbrInputs.diffuseColor * RECIPROCAL_PI * Lr;
}

vec3 BRDF_Diffuse_Burley(PBRInfo pbrInputs)
{
    float energyBias = mix(pbrInputs.perceptualRoughness, 0.0, 0.5);
    float energyFactor = mix(pbrInputs.perceptualRoughness, 1.0, 1.0 / 1.51);
    float fd90 = energyBias + 2.0 * pbrInputs.VdotH * pbrInputs.VdotH * pbrInputs.perceptualRoughness;
    float f0 = 1.0;
    float lightScatter = f0 + (fd90 - f0) * pow(1.0 - pbrInputs.NdotL, 5.0);
    float viewScatter = f0 + (fd90 - f0) * pow(1.0 - pbrInputs.NdotV, 5.0);

    return pbrInputs.diffuseColor * lightScatter * viewScatter * energyFactor;
}

vec3 BRDF_Diffuse_Disney(PBRInfo pbrInputs)
{
	float Fd90 = 0.5 + 2.0 * pbrInputs.perceptualRoughness * pbrInputs.VdotH * pbrInputs.VdotH;
    vec3 f0 = vec3(0.1);
	vec3 invF0 = vec3(1.0, 1.0, 1.0) - f0;
	float dim = min(invF0.r, min(invF0.g, invF0.b));
	float result = ((1.0 + (Fd90 - 1.0) * pow(1.0 - pbrInputs.NdotL, 5.0 )) * (1.0 + (Fd90 - 1.0) * pow(1.0 - pbrInputs.NdotV, 5.0 ))) * dim;
	return pbrInputs.diffuseColor * result;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(PBRInfo pbrInputs)
{
    //return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance90*pbrInputs.reflectance0) * exp2((-5.55473 * pbrInputs.VdotH - 6.98316) * pbrInputs.VdotH);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(PBRInfo pbrInputs)
{
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r + (1.0 - r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r + (1.0 - r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(PBRInfo pbrInputs)
{
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (PI * f * f);
}

vec3 BRDF(vec3 u_LightColor, vec3 v, vec3 n, vec3 l, vec3 h, float perceptualRoughness, float metallic, vec3 specularEnvironmentR0, vec3 specularEnvironmentR90, float alphaRoughness, vec3 diffuseColor, vec3 specularColor, float ao)
{
    float unclmapped_NdotL = dot(n, l);

    vec3 reflection = -normalize(reflect(v, n));
    reflection.y *= -1.0f;

    float NdotL = clamp(unclmapped_NdotL, 0.001, 1.0);
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);
    float VdotL = clamp(dot(v, l), 0.0, 1.0);

    PBRInfo pbrInputs = PBRInfo(NdotL,
                                NdotV,
                                NdotH,
                                LdotH,
                                VdotH,
                                VdotL,
                                perceptualRoughness,
                                metallic,
                                specularEnvironmentR0,
                                specularEnvironmentR90,
                                alphaRoughness,
                                diffuseColor,
                                specularColor);

    // Calculate the shading terms for the microfacet specular shading model
    vec3 F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution
    vec3 diffuseContrib = (1.0 - F) * BRDF_Diffuse_Disney(pbrInputs);
    vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    vec3 color = NdotL * u_LightColor * (diffuseContrib + specContrib);

    color *= ao;

#ifdef VSG_EMISSIVE_MAP
    vec3 emissive = SRGBtoLINEAR(texture(emissiveMap, texCoord0)).rgb * pbr.emissiveFactor.rgb;
#else
    vec3 emissive = pbr.emissiveFactor.rgb;
#endif
    color += emissive;

    return color;
}

float convertMetallic(vec3 diffuse, vec3 specular, float maxSpecular)
{
    float perceivedDiffuse = sqrt(0.299 * diffuse.r * diffuse.r + 0.587 * diffuse.g * diffuse.g + 0.114 * diffuse.b * diffuse.b);
    float perceivedSpecular = sqrt(0.299 * specular.r * specular.r + 0.587 * specular.g * specular.g + 0.114 * specular.b * specular.b);

    if (perceivedSpecular < c_MinRoughness)
    {
        return 0.0;
    }

    float a = c_MinRoughness;
    float b = perceivedDiffuse * (1.0 - maxSpecular) / (1.0 - c_MinRoughness) + perceivedSpecular - 2.0 * c_MinRoughness;
    float c = c_MinRoughness - perceivedSpecular;
    float D = max(b * b - 4.0 * a * c, 0.0);
    return clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
}

vec3 specularFresnel(vec3 f0, vec3 f90, float NdotV)
{
    //return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
    return f0 + (f90 - f90 * f0) * exp2((-5.55473 * NdotV - 6.98316) * NdotV);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0; // todo: param/const
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	return textureLod(samplerPrefilteredEnv, R, lod).rgb;
	// vec3 b = textureLod(samplerPrefilteredEnv, RFixed, lodc).rgb;
	// return mix(a, b, lod - lodf);
}

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


vec3 IBL(vec3 v, vec3 n, float perceptualRoughness, float metallic, vec3 specularEnvironmentR0, vec3 specularEnvironmentR90, vec3 diffuseColor){
    vec3 R = normalize(reflect(-v, n));

    float NdotV = clamp(dot(n, v), 0.001, 1.0);

    vec3 color = vec3(0);
    vec2 brdf = texture(samplerBRDFLUT, vec2(NdotV, perceptualRoughness)).rg;
    vec3 F = fresnelSchlickRoughness(NdotV, specularEnvironmentR0, perceptualRoughness);
    vec3 kD = 1.0 - F;
    kD *= 1.0 - metallic;     

	vec3 irradiance = texture(samplerIrradiance, n).rgb;
    color += irradiance * diffuseColor * pow(NdotV, 0.5 + 0.3 * perceptualRoughness) * kD;
	vec3 reflection = prefilteredReflection(R, perceptualRoughness).rgb;	
    color += reflection * (F * brdf.x + brdf.y);

    return color;
}

float computeF0Base_Merged(float f0) {
    float sqrtF0 = sqrt(f0);
    float numerator = 1.0 - 5.0 * sqrtF0;
    float denominator = 5.0 - sqrtF0;
    return (numerator * numerator) / (denominator * denominator);
}

void main()
{
    if(highlight > 0){
        outColor = vec4(1, 1, 1, 1);
        return;
    }
    
    if(constantBuffer.shader_type == 1){
        float cadDepth = -eyePos.z / 65.535;
        vec2 screen_uv = vec2(gl_FragCoord.x / constantBuffer.width, gl_FragCoord.y / constantBuffer.height);
        float cameraDepth = texture(depthImage, screen_uv).r;
        if(cadDepth > cameraDepth){
            outColor = texture(cameraImage, screen_uv);
            return;
        }
    }

    float brightnessCutoff = 0.001;

    float perceptualRoughness = 0.0;
    float metallic;
    vec3 diffuseColor;
    vec4 baseColor;

    float ambientOcclusion = 1.0;

    vec3 f0 = vec3(0.04);

#ifdef VSG_DIFFUSE_MAP
    #ifdef VSG_GREYSCALE_DIFFUSE_MAP
        float v = texture(diffuseMap, texCoord0.st).s * pbr.baseColorFactor;
        baseColor = vertexColor * vec4(v, v, v, 1.0);
    #else
        baseColor = vertexColor * SRGBtoLINEAR(texture(diffuseMap, texCoord0)) * pbr.baseColorFactor;
    #endif
#else
    baseColor = vertexColor * pbr.baseColorFactor;
#endif

    if (pbr.alphaMask == 1.0f)
    {
        if (baseColor.a < pbr.alphaMaskCutoff)
            discard;
    }

#ifdef VSG_WORKFLOW_SPECGLOSS
    #ifdef VSG_DIFFUSE_MAP
        vec4 diffuse = SRGBtoLINEAR(texture(diffuseMap, texCoord0));
    #else
        vec4 diffuse = vec4(1.0);
    #endif

    #ifdef VSG_SPECULAR_MAP
        vec4 specular_texel = texture(specularMap, texCoord0);
        vec3 specular = SRGBtoLINEAR(specular_texel).rgb;
        perceptualRoughness = 1.0 - specular_texel.a;
    #else
        vec3 specular = vec3(0.0);
        perceptualRoughness = 0.0;
    #endif

        float maxSpecular = max(max(specular.r, specular.g), specular.b);

        // Convert metallic value from specular glossiness inputs
        metallic = convertMetallic(diffuse.rgb, specular, maxSpecular);

        const float epsilon = 1e-6;
        vec3 baseColorDiffusePart = diffuse.rgb * ((1.0 - maxSpecular) / (1 - c_MinRoughness) / max(1 - metallic, epsilon)) * pbr.diffuseFactor.rgb;
        vec3 baseColorSpecularPart = specular - (vec3(c_MinRoughness) * (1 - metallic) * (1 / max(metallic, epsilon))) * pbr.specularFactor.rgb;
        baseColor = vec4(mix(baseColorDiffusePart, baseColorSpecularPart, metallic * metallic), diffuse.a);
#else
        perceptualRoughness = pbr.roughnessFactor;
        metallic = pbr.metallicFactor;

    #ifdef VSG_METALLROUGHNESS_MAP
        vec4 mrSample = texture(mrMap, texCoord0);
        perceptualRoughness = mrSample.g * perceptualRoughness;
        metallic = mrSample.r * metallic;
    #endif
#endif

#ifdef VSG_LIGHTMAP_MAP
    ambientOcclusion = texture(aoMap, texCoord0).r;
#endif

    diffuseColor = baseColor.rgb * (vec3(1.0) - f0);
    diffuseColor *= 1.0 - metallic;

    float alphaRoughness = perceptualRoughness * perceptualRoughness;

    vec3 specularColor = mix(f0, baseColor.rgb, metallic);

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    vec3 worldN = getWorldNormal();
    vec3 worldCamPos = pc.camera_pos;
    vec3 worldV = normalize(worldCamPos - worldViewDir);    

    vec3 color = vec3(0.0, 0.0, 0.0);
    vec4 lightNums = lightData.values[0];
    int numDirectionalLights = int(lightNums[1]);
    int index = 1;


    vec3 iblColor = IBL(worldV, worldN, perceptualRoughness, metallic, specularEnvironmentR0, specularEnvironmentR90, diffuseColor);
    color += iblColor * envmapData.param.a;

    float scene_brightness = 1.0f;
    if (numDirectionalLights>0){
        int shadowMapIndex = 0;
        float totalBrigtness = pc.baseBrightness;
        float totalRealBrightness = pc.baseBrightness;
        for(int i = 0; i<numDirectionalLights; ++i){
            vec4 lightColor = lightData.values[index++];
            float area = lightData.values[index].w;
            vec3 direction = -lightData.values[index++].xyz;
            vec4 shadowMapSettings = lightData.values[index++];

            float brightness = lightColor.a;
            totalBrigtness += brightness;
            bool matched = false;
            float visibility = 0.0f;
            
            while ((shadowMapSettings.r > 0.0 && brightness > brightnessCutoff) && !matched)
            {
                mat4 sm_matrix = mat4(lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++]);
                vec4 sm_tc = (sm_matrix) * vec4(worldViewDir, 1.0);
                if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0)
                {
                    matched = true;
                    // poissonDiskSamples(sm_tc.xy); 
                    float random = ValueNoise(sm_tc.xyz);
                    if(pc.shadow_type == 0){
                        visibility = 1 - PCF(shadowMaps,sm_tc,shadowMapIndex,area, random);
                    }else if(pc.shadow_type == 1){
                        visibility = 1 - PCSS(shadowMaps,sm_tc,shadowMapIndex, area, random);
                    }
                }else{
                  visibility = 1.0;
                }
                ++shadowMapIndex;
                shadowMapSettings.r -= 1.0;
            }

            if (shadowMapSettings.r > 0.0)
            {
                // skip lightData and shadowMap entries for shadow maps that we haven't visited for this light
                // so subsequent light pointions are correct.
                index += 4 * int(shadowMapSettings.r);
                shadowMapIndex += int(shadowMapSettings.r);
            }

            totalRealBrightness += brightness * visibility;
        }
        scene_brightness = totalRealBrightness / totalBrigtness;
    }
    vec4 last_ndc = pc.projection * pc.last_view * vec4(worldViewDir, 1);
    ivec2 last_coord = ivec2(((last_ndc.x / last_ndc.w) / 2 + 0.5) * constantBuffer.width, ((last_ndc.y / last_ndc.w) / 2 + 0.5) * constantBuffer.height);
    float old_shadow = 1;
    float oldInstanceID = -1;
    if(last_coord.x >= 0 && last_coord.y >= 0 && last_coord.x < constantBuffer.width && last_coord.y < constantBuffer.height){
        vec2 shadowdataold_shadow = texelFetch(shadowInputAttachment, last_coord, gl_SampleID).rg;
        oldInstanceID = shadowdataold_shadow.y;
        old_shadow = shadowdataold_shadow.x;
    }

    float old_weight = max(0.0, 0.7 - length(last_coord - gl_FragCoord.xy) / 50.0);
    if(abs(oldInstanceID - InstanceID) > 0.1)
        scene_brightness = scene_brightness;
    else
        scene_brightness = old_shadow * old_weight + scene_brightness * (1 - old_weight);

    outColor = vec4(color * scene_brightness, baseColor.w);
    if(baseColor.w > 0.8){
        outNormal = vec4(worldN, 1);
        outWorldPos = vec4(worldViewDir, 1);
    }
    else{
        outNormal = vec4(worldN, 0);
        outWorldPos = vec4(worldViewDir, 0);
    }

    outShadow = vec4(scene_brightness, InstanceID, (1 - gl_FragCoord.z), 1);
}
