#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_METALLROUGHNESS_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, VSG_WORKFLOW_SPECGLOSS, SHADOWMAP_DEBUG)

#define VIEW_DESCRIPTOR_SET 0
#define MATERIAL_DESCRIPTOR_SET 1

const float PI = 3.14159265359;
const float RECIPROCAL_PI = 0.31830988618;
const float RECIPROCAL_PI2 = 0.15915494;
const float EPSILON = 1e-6;
const float c_MinRoughness = 0.04;

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

layout (set = MATERIAL_DESCRIPTOR_SET, binding = 9) uniform params {
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

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 normalDir;
layout(location = 2) in vec4 vertexColor;
layout(location = 3) in vec2 texCoord0;
layout(location = 5) in vec3 viewDir;

layout(location = 0) out vec4 outColor;


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


#define NUM_SAMPLES 32 
#define BLOCKER_SEARCH_NUM_SAMPLES NUM_SAMPLES
#define PCF_NUM_SAMPLES NUM_SAMPLES
#define NUM_RINGS 10

#define EPS 1e-2 
#define PI 3.141592653589793
#define PI2 6.283185307179586

highp float rand_1to1(highp float x ) {         
  return fract(sin(x)*10000.0);
}

highp float rand_2to1(vec2 uv ) {       
	const highp float a = 12.9898, b = 78.233, c = 43758.5453;
	highp float dt = dot( uv.xy, vec2( a,b ) ), sn = mod( dt, PI );
	return fract(sin(sn) * c);
}

float unpack(vec4 rgbaDepth) {       
    const vec4 bitShift = vec4(1.0, 1.0/256.0, 1.0/(256.0*256.0), 1.0/(256.0*256.0*256.0));
    return dot(rgbaDepth, bitShift);
}

vec2 poissonDisk[NUM_SAMPLES];
void uniformDiskSamples( const in vec2 randomSeed ) {     
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

void poissonDiskSamples( const in vec2 randomSeed ) {         
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

float PCF(sampler2DArrayShadow shadowMap, vec4 coords,int shadowMapIndex) {          
    float Stride = 10.0; 
    float shadowmapSize = 2048.;
    float visibility = 0.0;
    float cur_depth = coords.z;
    
    poissonDiskSamples(coords.xy);

    //uniformDiskSamples(coords.xy);

    float ctrl = 1.0;
        
    for(int i =0 ; i < NUM_SAMPLES; i++)
    {
        //vec4 shadow_color = texture2D(shadowMap, coords.xy + poissonDisk[i] * Stride / shadowmapSize); 
        //float shadow_depth = unpack(shadow_color);
        //float res = cur_depth < shadow_depth + EPS ? 1. : 0. ;
        //visibility += res;
        float res  = texture(shadowMap, vec4(coords.xy + poissonDisk[i] * Stride / shadowmapSize, shadowMapIndex, coords.z)).r;
        visibility += res;
    }

    return visibility / float(NUM_SAMPLES);
}

float findBlocker(sampler2DArrayShadow shadowMap,  vec4 coords, int shadowMapIndex) {
    int blockerNum = 0;
    float block_depth = 0.;
    float shadowmapSize = 2048.;
    float Stride = 20.;

    poissonDiskSamples(coords.xy);

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
    if(blockerNum == 0){
        return 1.;
    }
    return float(block_depth) / float(blockerNum);
}

float PCSS(sampler2DArrayShadow shadowMap, vec4 coords,int shadowMapIndex){
    float d_Blocker = findBlocker(shadowMap, coords,shadowMapIndex);
    float w_Light = 1.; 
    float d_Receiver = coords.z;

    float w_penumbra = w_Light * (d_Receiver - d_Blocker) / d_Blocker;

    float Stride = 20.;
    float shadowmapSize = 2048.;
    float visibility = 0.;
    float cur_depth = coords.z;

    //poissonDiskSamples(coords.xy); 

    //float ctrl = 1.0;
    //float bias = getBias(ctrl);

    for(int i = 0; i < NUM_SAMPLES; i++){
        float res  = texture(shadowMap, vec4(coords.xy + poissonDisk[i] * Stride / shadowmapSize* w_penumbra, shadowMapIndex, coords.z)).r;
        visibility += res;
    }

    return visibility / float(NUM_SAMPLES);
}

void main()
{
    vec2 screen_uv = vec2(gl_FragCoord.x / extraParams.width, gl_FragCoord.y / extraParams.height);
    if(extraParams.shader_type != 0){
        float cadDepth = -eyePos.z / extraParams.z_far;
        float cameraDepth = texture(depthImage, screen_uv).r;
        if(cadDepth > cameraDepth){
            outColor = texture(cameraImage, screen_uv);
            return;
        }
    }

    float brightnessCutoff = 0.001;

    vec4 lightNums = lightData.values[0];
    int numAmbientLights = int(lightNums[0]);
    int numDirectionalLights = int(lightNums[1]);
    int numPointLights = int(lightNums[2]);
    int numSpotLights = int(lightNums[3]);
    int index = 1;

    float scene_brightness = 0.0;
    // index used to step through the shadowMaps array
    int shadowMapIndex = 0;
    if (numDirectionalLights>0)
    {
        float totalBrigtness = 0.0f;
        float totalRealBrightness = 0.0f;
        // directional lights
        for(int i = 0; i<numDirectionalLights; ++i)
        {
            vec4 lightColor = lightData.values[index++];
            vec3 direction = -lightData.values[index++].xyz;
            vec4 shadowMapSettings = lightData.values[index++];

            float brightness = lightColor.a;
            totalBrigtness += brightness;
            float visibility = 0.0f;

            // check shadow maps if required
            bool matched = false;
            while ((shadowMapSettings.r > 0.0 && brightness > brightnessCutoff) && !matched)
            {
                mat4 sm_matrix = mat4(lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++]);

                vec4 sm_tc = (sm_matrix) * vec4(eyePos, 1.0);

                if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0 /* && sm_tc.z <= 1.0*/)
                {
                    //visibility = 1 - texture(shadowMaps, vec4(sm_tc.st, shadowMapIndex, sm_tc.z)).r; //����ǰƬ�ε�������������Ӱ��ͼ�е����ֵ���бȽ� ����Ӱ0 ������Ӱ1

                    matched = true;
                    visibility = 1 - PCF(shadowMaps,sm_tc,shadowMapIndex);
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
    vec3 color = vec3(scene_brightness * 0.5 + 0.5);
    outColor.rgb = texture(cameraImage, screen_uv).rgb * color;
    outColor.a = 1;
}
