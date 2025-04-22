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

#define NUM_SAMPLES 32
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

layout(set = VIEW_DESCRIPTOR_SET, binding = 3) uniform ViewMatrixData{
    mat4 view;
    mat4 invView;
    mat4 unused[2];
} viewMatrixData;

layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps;

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
layout(location = 5) in vec3 viewDir;

layout(location = 6) in vec3 worldNormal;
layout(location = 7) in vec3 worldViewDir;
layout(location = 8) in mat4 project;

layout(location = 0) out vec4 outColor;

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

vec2 poissonDisk[NUM_SAMPLES];
void uniformDiskSamples( const in vec2 randomSeed ) {                              //
  //Բ̲

  float randNum = rand_2to1(randomSeed);
  float sampleX = rand_1to1( randNum ) ;
  float sampleY = rand_1to1( sampleX ) ;

  float angle = sampleX * PI2;
  float radius = sqrt(sampleY);

  for( int i = 0; i < NUM_SAMPLES; i ++ ) {
    poissonDisk[i] = vec2( radius * cos(angle) , radius * sin(angle)  );

    sampleX = rand_1to1( sampleY ) ;
    sampleY = rand_1to1( sampleX ) ;

    angle = sampleX * PI2;
    radius = sqrt(sampleY);
  }
}

void poissonDiskSamples( const in vec2 randomSeed ) {                              //
  //Բ̲

  float ANGLE_STEP = PI2 * float( NUM_RINGS ) / float( NUM_SAMPLES );
  float INV_NUM_SAMPLES = 1.0 / float( NUM_SAMPLES );

  float angle = rand_2to1( randomSeed ) * PI2;
  float radius = INV_NUM_SAMPLES;
  float radiusStep = radius;

  for( int i = 0; i < NUM_SAMPLES; i ++ ) {
    poissonDisk[i] = vec2( cos( angle ), sin( angle ) ) * pow( radius, 0.75 );
    radius += radiusStep;
    angle += ANGLE_STEP;
  }
}

float PCF(sampler2DArrayShadow shadowMap, vec4 coords,int shadowMapIndex) {                              //
  //1.Strideshadow mapֱʡʼֵΧǰ
  //StrideԽ󣬱ԵԽģ
  float Stride = 3.0; //˲Ĳÿξл/
  float shadowmapSize = 2048.; //shadow mapĴС
  float visibility = 0.0;
  float cur_depth = coords.z;
  
  //2.Բ̲õ
  poissonDiskSamples(coords.xy);

  //2.Բ̲õ
  //uniformDiskSamples(coords.xy);

  //3.ÿбȽֵۼ  NUM_SAMPLESڿͷʼΪ20
  float ctrl = 1.0;
     
  for(int i =0 ; i < NUM_SAMPLES; i++)
  {
    //vec4 shadow_color = texture2D(shadowMap, coords.xy + poissonDisk[i] * Stride / shadowmapSize); 
    //float shadow_depth = unpack(shadow_color);
    //float res = cur_depth < shadow_depth + EPS ? 1. : 0. ;//ûнڵԸõع۲ӰŲ
    //visibility += res;
     float res  = texture(shadowMap, vec4(coords.xy + poissonDisk[i] * Stride / shadowmapSize, shadowMapIndex, coords.z)).r;
     visibility += res;
  }

  //4.ؾֵ
  return visibility / float(NUM_SAMPLES);
}

float findBlocker(sampler2DArrayShadow shadowMap,  vec4 coords, int shadowMapIndex) {
  //1.
  int blockerNum = 0;
  float block_depth = 0.;
  float shadowmapSize = 2048.;
  float Stride = 20.;

  //2.ɲõ
  poissonDiskSamples(coords.xy);

  //3.жϻblockerۼ
  for(int i = 0; i < NUM_SAMPLES; i++){ //Ƽ˰汾
      vec2 xy=vec2(coords.xy + poissonDisk[i] * Stride / shadowmapSize);
	  float dp = 1.0; //
	  while (texture(shadowMap, vec4(xy, shadowMapIndex, dp)).r < 1.0 && dp>=0.0)
		{
			dp -= 0.01;
		}
		if (dp >=0.0) {
			blockerNum++;
			block_depth += dp;
		}
  }
  //shading pointڳյĵطҪҲһֵȻȫڵ
  ///0 ֵе㶼<Ӱ
  if(blockerNum == 0){
    return 1.;
  }
  return float(block_depth) / float(blockerNum);
}

float PCSS(sampler2DArrayShadow shadowMap, vec4 coords,int shadowMapIndex){

  // STEP 1: avgblocker depth
  float d_Blocker = findBlocker(shadowMap, coords,shadowMapIndex);
  float w_Light = 1.; // ԴСǣֱӿԴ
  float d_Receiver = coords.z;

  // STEP 2: penumbra size
  //ݹʽΣwpenumbraӰΧ
  float w_penumbra = w_Light * (d_Receiver - d_Blocker) / d_Blocker;

  // STEP 3: filtering
  //ʵһPCFֻPCFһw_penumbrad_ReceiberӰ
  //1
  float Stride = 20.;
  float shadowmapSize = 2048.;
  float visibility = 0.;
  float cur_depth = coords.z;

  //2 
  //poissonDiskSamples(coords.xy); findBlockerѾ

  //3
  //float ctrl = 1.0;
  //float bias = getBias(ctrl);//Դ˽ڵ⣬ͬ²Ӱʧ

  for(int i = 0; i < NUM_SAMPLES; i++){
     float res  = texture(shadowMap, vec4(coords.xy + poissonDisk[i] * Stride / shadowmapSize* w_penumbra, shadowMapIndex, coords.z)).r;
     visibility += res;
  }

  //4.ؾֵ
  return visibility / float(NUM_SAMPLES);
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

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelView;
} pc;


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

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal()
{
    vec3 result;
#ifdef VSG_NORMAL_MAP
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    vec3 tangentNormal = texture(normalMap, texCoord0).xyz * 2.0 - 1.0;

    //tangentNormal *= vec3(2,2,1);

    vec3 q1 = dFdx(eyePos);
    vec3 q2 = dFdy(eyePos);
    vec2 st1 = dFdx(texCoord0);
    vec2 st2 = dFdy(texCoord0);

    vec3 N = normalize(normalDir);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    result = normalize(TBN * tangentNormal);
#else
    result = normalize(normalDir);
#endif
#ifdef VSG_TWO_SIDED_LIGHTING
    if (!gl_FrontFacing)
        result = -result;
#endif
    return result;
}
vec3 getWorldNormal()
{
    vec3 result;
#ifdef VSG_NORMAL_MAP
    vec3 tangentNormal = texture(normalMap, texCoord0 * 4).xyz * 2.0 - 1.0;
    tangentNormal = tangentNormal * 0.3 + vec3(0, 0, 1) * 0.7;
    //vec3 tangentNormal = texture(normalMap, texCoord0).xyz * 2.0 - 1.0;
    
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
    result = normalize(normalDir);
#endif
#ifdef VSG_TWO_SIDED_LIGHTING
    if (!gl_FrontFacing)
        result = -result;
#endif
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

vec3 specularFresnel(vec3 f0, vec3 f90, float NdotL)
{
    //return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
    return f0 + (f90 - f0) * exp2((-5.55473 * NdotL - 6.98316) * NdotL);
}

vec3 fixCubeDir(vec3 v){
	return vec3(v.x, -v.z, v.y);
}

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0; // todo: param/const
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
    vec3 RFixed = fixCubeDir(R);
	vec3 a = textureLod(samplerPrefilteredEnv, RFixed, lodf).rgb;
	vec3 b = textureLod(samplerPrefilteredEnv, RFixed, lodc).rgb;
	return mix(a, b, lod - lodf);
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

    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);

    vec3 color = vec3(0);
    vec2 brdf = texture(samplerBRDFLUT, vec2(NdotV, perceptualRoughness)).rg;
    vec3 F = specularFresnel(specularEnvironmentR0, vec3(1.0), NdotV);

    vec3 N = fixCubeDir(n);
    N.y *= -1.0f;
    // N.xz *= -1.0f;
    // vec3 lutR = fixCubeDir(R);
    vec3 lutR = R;
    lutR.y *= -1.0f;
	vec3 irradiance = texture(samplerIrradiance, N).rgb;
    color += irradiance * diffuseColor;
	vec3 reflection = prefilteredReflection(lutR, perceptualRoughness).rgb;	
    color += reflection * (F * brdf.x + brdf.y);

    // float exposure = 3.0f;
    // float gamma = 2.2f;

	// // Tone mapping
	// color = Uncharted2Tonemap(color * exposure);
	// color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    return color;
}

void main()
{
    if(extraParams.shader_type == 1){
        float cadDepth = -eyePos.z / 65.535;
        vec2 screen_uv = vec2(gl_FragCoord.x / 1280.0, gl_FragCoord.y / 960.0);
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
        metallic = mrSample.b * metallic;
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
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    vec3 n = getNormal();
    vec3 v = normalize(viewDir);    // Vector from surface point to camera

    float shininess = 100.0f;

    vec3 color = vec3(0.0, 0.0, 0.0);

    vec4 lightNums = lightData.values[0];
    int numAmbientLights = int(lightNums[0]);
    int numDirectionalLights = int(lightNums[1]);
    int numPointLights = int(lightNums[2]);
    int numSpotLights = int(lightNums[3]);
    int index = 1;

    if (numAmbientLights>0)
    {
        // ambient lights
        for(int i = 0; i<numAmbientLights; ++i)
        {
            vec4 ambient_color = lightData.values[index++];
            color += (baseColor.rgb * ambient_color.rgb) * (ambient_color.a * ambientOcclusion);
        }
    }
    
    vec3 worldN = normalize(getWorldNormal());
    // vec3 worldPos = worldViewDir;
    
    // vec3 worldCamPos = vec3(-viewMatrixData.view[0][3], -viewMatrixData.view[1][3], -viewMatrixData.view[2][3]);
    mat4 cameraData = viewMatrixData.unused[0];

    vec3 worldCamPos = vec3(cameraData[0][0], cameraData[0][1], cameraData[0][2]);
    vec3 worldPos = worldViewDir;
    // vec3 worldV = vec3(viewMatrixData.view[3][0], viewMatrixData.view[3][1], viewMatrixData.view[3][2]) - worldPos;
    // worldV = normalize(worldV);
    vec3 worldV = normalize(worldCamPos - worldPos);    

    // metallic = 0.0;
    vec3 iblColor = IBL(worldV, worldN, perceptualRoughness, metallic, specularEnvironmentR0, specularEnvironmentR90, diffuseColor);
    // iblColor *= ambientOcclusion;
    // iblColor.xyz = worldN;
    color += iblColor * envmapData.param.a;
    // color += iblColor;
    // color.xyz = worldV;

    // color.xyz = fract(worldPos / 500);
    // color.xy = worldPos.xy / 2000.0 * 0.5 + 0.5;
    // color.xy *= step(-2000.0, worldPos.x) * step(worldPos.x, 2000.0);
    // color.xy *= step(-2000.0, worldPos.y) * step(worldPos.y, 2000.0);
    // color.z = 0;
    float scene_brightness;
    if (numDirectionalLights>0){
        int shadowMapIndex = 0;
        float totalBrigtness = 0.0f;
        float totalRealBrightness = 0.0f;
        for(int i = 0; i<numDirectionalLights; ++i){
            vec4 lightColor = lightData.values[index++];
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
                vec4 sm_tc = (sm_matrix) * vec4(eyePos, 1.0);
                if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0)
                {
                    matched = true;
                    visibility = 1 - PCF(shadowMaps,sm_tc,shadowMapIndex);

                }else{
                  visibility = 1.0;
                }
                ++shadowMapIndex;
                shadowMapSettings.r -= 1.0;
            }
            totalRealBrightness += brightness * visibility;
        }
        scene_brightness = totalRealBrightness / totalBrigtness;
    }
    scene_brightness = scene_brightness * 0.5 + 0.5;


    float exposure = 3.0f;
    color = Uncharted2Tonemap(color * scene_brightness * exposure);
	color = color * (vec3(1.0f) / Uncharted2Tonemap(vec3(11.2f)));
    outColor = LINEARtoSRGB(vec4(color, baseColor.a * extraParams.semitransparent));
}
