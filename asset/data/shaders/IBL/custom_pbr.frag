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

#define EPS 1e-2  //模影卸效泻艽影
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

struct PbrMaterial {
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 diffuseFactor;
    vec4 specularFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaMask;
    float alphaMaskCutoff;
};

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 14) buffer MaterialArray {
    PbrMaterial materials[];
} materialArray;

#define GLOBAL_BUFFER_SET MATERIAL_DESCRIPTOR_SET
#define GLOBAL_BUFFER_BINDING 12
#pragma include "global_buffer.glsl"

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 13) uniform sampler2D shadowInputAttachment;

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
layout(location = 7) in vec3 worldPos;
layout(location = 8) in vec3 lastWorldPos;
layout(location = 9) in flat uint materialIndex;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outWorldPos;
layout(location = 3) out vec4 outShadow;
layout(location = 4) out vec4 outMaterial;
layout(location = 5) out float outMask;

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
} pc;

highp float rand_1to1(highp float x ) {                              //
  // float偷一维x一[-1,1]围float
  // -1 -1
  return fract(sin(x)*10000.0);
}

highp float rand_2to1(vec2 uv ) {                              //
  // 一维uv一围[0,1]float
  // 0 - 1
	const highp float a = 12.9898, b = 78.233, c = 43758.5453;
	highp float dt = dot( uv.xy, vec2( a,b ) ), sn = mod( dt, PI );
	return fract(sin(sn) * c);
}

float unpack(vec4 rgbaDepth) {                             //
    //
    //unpack()钥shadowFragment.glsl械pack()姆转
    // RGBA值转[0,1]母
    const vec4 bitShift = vec4(1.0, 1.0/256.0, 1.0/(256.0*256.0), 1.0/(256.0*256.0*256.0));
    return dot(rgbaDepth, bitShift);
}

#pragma include "sample_data.glsl"

#define PCSS_FRAME_NUM globalBuffer.frame_num
#define PCSS_SOFTNESS globalBuffer.softness
#define PCSS_PCF_SAMPLE_NUM globalBuffer.pcf_sample_num
#define PCSS_MAX_DISTANCE_SCALE 65.0
#define PCSS_PENUMBRA_CLAMP 1.8
#define PCSS_MIN_FILTER_SCALE 10.0
#pragma include "pcss.glsl"

// --- custom_pbr unique functions below ---
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

    vec3 Q1 = dFdx(worldPos);
    vec3 Q2 = dFdy(worldPos);
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

// [Gotanda 2012, "Beyond a Simple Physically Based Blinn-Phong Model in float-Time"]
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
    vec3 emissive = SRGBtoLINEAR(texture(emissiveMap, texCoord0)).rgb * materialArray.materials[materialIndex].emissiveFactor.rgb;
#else
    vec3 emissive = materialArray.materials[materialIndex].emissiveFactor.rgb;
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

    float brightnessCutoff = 0.001;

    float perceptualRoughness = 0.0;
    float metallic;
    vec3 diffuseColor;
    vec4 baseColor;

    float ambientOcclusion = 1.0;

    vec3 f0 = vec3(0.04);

#ifdef VSG_DIFFUSE_MAP
    #ifdef VSG_GREYSCALE_DIFFUSE_MAP
        float v = texture(diffuseMap, texCoord0.st).s * materialArray.materials[materialIndex].baseColorFactor;
        baseColor = vertexColor * vec4(v, v, v, 1.0);
    #else
        baseColor = vertexColor * SRGBtoLINEAR(texture(diffuseMap, texCoord0)) * materialArray.materials[materialIndex].baseColorFactor;
    #endif
#else
    baseColor = vertexColor * materialArray.materials[materialIndex].baseColorFactor;
#endif

    if (materialArray.materials[materialIndex].alphaMask == 1.0f)
    {
        if (baseColor.a < materialArray.materials[materialIndex].alphaMaskCutoff)
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
        vec3 baseColorDiffusePart = diffuse.rgb * ((1.0 - maxSpecular) / (1 - c_MinRoughness) / max(1 - metallic, epsilon)) * materialArray.materials[materialIndex].diffuseFactor.rgb;
        vec3 baseColorSpecularPart = specular - (vec3(c_MinRoughness) * (1 - metallic) * (1 / max(metallic, epsilon))) * materialArray.materials[materialIndex].specularFactor.rgb;
        baseColor = vec4(mix(baseColorDiffusePart, baseColorSpecularPart, metallic * metallic), diffuse.a);
#else
        perceptualRoughness = materialArray.materials[materialIndex].roughnessFactor;
        metallic = materialArray.materials[materialIndex].metallicFactor;

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
    vec3 worldCamPos = globalBuffer.camera_pos;
    vec3 worldV = normalize(worldCamPos - worldPos);

    vec3 color = vec3(0.0, 0.0, 0.0);
    vec4 lightNums = lightData.values[0];
    int numDirectionalLights = int(lightNums[1]);
    int index = 1;


    vec3 iblColor = IBL(worldV, worldN, perceptualRoughness, metallic, specularEnvironmentR0, specularEnvironmentR90, diffuseColor);
    color += iblColor * envmapData.param.a;

    float scene_brightness = 1.0f;
    if (numDirectionalLights>0){
        int shadowMapIndex = 0;
        float totalBrigtness = globalBuffer.baseBrightness;
        float totalfloatBrightness = globalBuffer.baseBrightness;
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
                vec4 sm_tc = (sm_matrix) * vec4(worldPos, 1.0);
                if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0)
                {
                    matched = true;
                    // poissonDiskSamples(sm_tc.xy);
                    float random = ValueNoise(sm_tc.xyz);
                    if(globalBuffer.shadow_type == 0){
                        visibility = PCF(sm_tc,shadowMapIndex,area, random);
                    }else if(globalBuffer.shadow_type == 1){
                        // visibility = 1 - PCSS(sm_tc,shadowMapIndex, area, random);
                        visibility = SampleShadow_PCSS_Area(sm_tc.xyz, vec2(gl_FragCoord.xy), globalBuffer.softness, globalBuffer.softness_falloff, globalBuffer.blocker_sample_num, globalBuffer.pcf_sample_num, globalBuffer.shadow_bias, shadowMapIndex);
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

            totalfloatBrightness += brightness * visibility;
        }
        scene_brightness = totalfloatBrightness / totalBrigtness;
    }
    vec4 last_ndc = pc.projection * globalBuffer.last_view * vec4(lastWorldPos, 1);
    ivec2 last_coord = ivec2(((last_ndc.x / last_ndc.w) / 2 + 0.5) * globalBuffer.width, ((last_ndc.y / last_ndc.w) / 2 + 0.5) * globalBuffer.height);
    float old_shadow = 1;
    float oldInstanceID = -1;
    if(last_coord.x >= 0 && last_coord.y >= 0 && last_coord.x < globalBuffer.width && last_coord.y < globalBuffer.height){
        float shadowSum = 0.0;
        int count = 0;
        for(int dy = -1; dy <= 1; dy++){
            for(int dx = -1; dx <= 1; dx++){
                ivec2 sampleCoord = last_coord + ivec2(dx, dy);
                if(sampleCoord.x >= 0 && sampleCoord.y >= 0 && sampleCoord.x < globalBuffer.width && sampleCoord.y < globalBuffer.height){
                    vec2 data = texelFetch(shadowInputAttachment, sampleCoord, 0).rg;
                    if(abs(data.y - InstanceID) < 0.1){
                        shadowSum += data.x;
                        count++;
                    }
                }
            }
        }
        old_shadow = count > 0 ? shadowSum / float(count) : scene_brightness;
        if(count > 0) oldInstanceID = InstanceID;
    }
    else{
        old_shadow = scene_brightness;
    }

    float current_shadow_value = scene_brightness; // 鏆傛椂淇濆瓨褰撳墠甯х殑闃村奖鍊?
    if (abs(oldInstanceID - InstanceID) < 0.1 && abs(old_shadow - scene_brightness) < 0.1)
    {
        float historyLuma = old_shadow;
        float currentLuma = current_shadow_value;

        float diff = abs(currentLuma - historyLuma) / max(max(currentLuma, historyLuma), 0.2); // 璁＄畻鐩稿宸紓

        float weight_sq = (1.0 - diff);
        weight_sq = weight_sq * weight_sq;

        const float feedbackMin = 0.96; // 鏈€灏忓弽棣?(褰撳墠甯у樊寮傚ぇ鏃?
        const float feedbackMax = 0.91; // 鏈€澶у弽棣?(褰撳墠甯у樊寮傚皬鏃?

        float feedback = (1.0 - weight_sq) * feedbackMin + weight_sq * feedbackMax;

        scene_brightness = mix(current_shadow_value, old_shadow, feedback);

        // 閽冲埗鏈€缁堢粨鏋?
        scene_brightness = clamp(scene_brightness, 0.0, 1.0);
    }
    if(highlight > 0){
        outColor = vec4(scene_brightness, 0, 0, 1);
    }
    else{
        outColor = vec4(color * scene_brightness, baseColor.w);
    }

    outNormal = vec4(worldN, perceptualRoughness);
    outWorldPos = vec4(worldPos, metallic);

    outShadow = vec4(scene_brightness, InstanceID, (1 - gl_FragCoord.z), 1);
    outMaterial = vec4(baseColor.rgb, ambientOcclusion);
    outMask = 1.0;
}
