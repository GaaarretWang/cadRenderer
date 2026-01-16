#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_METALLROUGHNESS_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, VSG_WORKFLOW_SPECGLOSS, SHADOWMAP_DEBUG)

#define VIEW_DESCRIPTOR_SET 1
#define MATERIAL_DESCRIPTOR_SET 2

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
layout(set = VIEW_DESCRIPTOR_SET, binding = 3) uniform sampler2DArray shadowMapsSampler;

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 normalDir;
layout(location = 2) in vec4 vertexColor;
layout(location = 3) in vec2 texCoord0;
layout(location = 4) in vec3 worldViewDir;
layout(location = 5) in vec3 viewDir;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
    vec3 camera_pos;
    float z_far;
    float softness;
    float baseBrightness;
    float ssao_radius;
    float exposure;
    float softness_falloff;
    int ssao_kernel_size;
    int shader_type;
    int width;
    int height;
    int denoise_size;
    int blocker_sample_num;
    int pcf_sample_num;
    int shadow_type;
    uint frame_num;
} pc;

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

// void uniformDiskSamples( const in vec2 randomSeed ) {     
//     float randNum = rand_2to1(randomSeed);
//     float sampleX = rand_1to1( randNum ) ;
//     float sampleY = rand_1to1( sampleX ) ;

//     float angle = sampleX * PI2;
//     float radius = sqrt(sampleY);

//     for( int i = 0; i < pc.shadow_sample_num; i ++ ) {
//         poissonDisk[i] = vec2( radius * cos(angle) , radius * sin(angle)  );

//         sampleX = rand_1to1( sampleY ) ;
//         sampleY = rand_1to1( sampleX ) ;

//         angle = sampleX * PI2;
//         radius = sqrt(sampleY);
//     }
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

void main()
{
    vec2 screen_uv = vec2(gl_FragCoord.x / pc.width, gl_FragCoord.y / pc.height);
    if(pc.shader_type != 0){
        float cadDepth = -eyePos.z / pc.z_far;
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

    float scene_brightness = 1.0f;
    int shadowMapIndex = 0;
    if (numDirectionalLights>0)
    {
        float totalBrigtness = pc.baseBrightness;
        float totalRealBrightness = pc.baseBrightness;
        // directional lights
        for(int i = 0; i<numDirectionalLights; ++i)
        {
            vec4 lightColor = lightData.values[index++];
            float area = lightData.values[index].w;
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

                vec4 sm_tc = (sm_matrix) * vec4(worldViewDir, 1.0);

                if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0 /* && sm_tc.z <= 1.0*/)
                {
                    //visibility = 1 - texture(shadowMaps, vec4(sm_tc.st, shadowMapIndex, sm_tc.z)).r; //����ǰƬ�ε�������������Ӱ��ͼ�е����ֵ���бȽ� ����Ӱ0 ������Ӱ1

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
    vec3 color = vec3(scene_brightness);
    outColor.rgb = texture(cameraImage, screen_uv).rgb * color;
    outColor.a = 1;
}
