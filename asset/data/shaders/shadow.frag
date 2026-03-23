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

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 normalDir;
layout(location = 2) in vec4 vertexColor;
layout(location = 3) in vec2 texCoord0;
layout(location = 4) in vec3 worldViewDir;
layout(location = 5) in vec3 viewDir;
layout(location = 6) in float InstanceID;
layout(location = 8) in vec3 lastWorldPos;

layout(location = 0) out vec4 outColor;
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
    float shadow_bias;
    int ssao_kernel_size;
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
vec2 fibonacciSpiralDirection[64] =
{
    vec2 (1, 0),
    vec2 (-0.7373688780783197, 0.6754902942615238),
    vec2 (0.08742572471695988, -0.9961710408648278),
    vec2 (0.6084388609788625, 0.793600751291696),
    vec2 (-0.9847134853154288, -0.174181950379311),
    vec2 (0.8437552948123969, -0.5367280526263233),
    vec2 (-0.25960430490148884, 0.9657150743757782),
    vec2 (-0.46090702471337114, -0.8874484292452536),
    vec2 (0.9393212963241182, 0.3430386308741014),
    vec2 (-0.924345556137805, 0.3815564084749356),
    vec2 (0.423845995047909, -0.9057342725556143),
    vec2 (0.29928386444487326, 0.9541641203078969),
    vec2 (-0.8652112097532296, -0.501407581232427),
    vec2 (0.9766757736281757, -0.21471942904125949),
    vec2 (-0.5751294291397363, 0.8180624302199686),
    vec2 (-0.12851068979899202, -0.9917081236973847),
    vec2 (0.764648995456044, 0.6444469828838233),
    vec2 (-0.9991460540072823, 0.04131782619737919),
    vec2 (0.7088294143034162, -0.7053799411794157),
    vec2 (-0.04619144594036213, 0.9989326054954552),
    vec2 (-0.6407091449636957, -0.7677836880006569),
    vec2 (0.9910694127331615, 0.1333469877603031),
    vec2 (-0.8208583369658855, 0.5711318504807807),
    vec2 (0.21948136924637865, -0.9756166914079191),
    vec2 (0.4971808749652937, 0.8676469198750981),
    vec2 (-0.952692777196691, -0.30393498034490235),
    vec2 (0.9077911335843911, -0.4194225289437443),
    vec2 (-0.38606108220444624, 0.9224732195609431),
    vec2 (-0.338452279474802, -0.9409835569861519),
    vec2 (0.8851894374032159, 0.4652307598491077),
    vec2 (-0.9669700052147743, 0.25489019011123065),
    vec2 (0.5408377383579945, -0.8411269468800827),
    vec2 (0.16937617250387435, 0.9855514761735877),
    vec2 (-0.7906231749427578, -0.6123030256690173),
    vec2 (0.9965856744766464, -0.08256508601054027),
    vec2 (-0.6790793464527829, 0.7340648753490806),
    vec2 (0.0048782771634473775, -0.9999881011351668),
    vec2 (0.6718851669348499, 0.7406553331023337),
    vec2 (-0.9957327006438772, -0.09228428288961682),
    vec2 (0.7965594417444921, -0.6045602168251754),
    vec2 (-0.17898358311978044, 0.9838520605119474),
    vec2 (-0.5326055939855515, -0.8463635632843003),
    vec2 (0.9644371617105072, 0.26431224169867934),
    vec2 (-0.8896863018294744, 0.4565723210368687),
    vec2 (0.34761681873279826, -0.9376366819478048),
    vec2 (0.3770426545691533, 0.9261958953890079),
    vec2 (-0.9036558571074695, -0.4282593745796637),
    vec2 (0.9556127564793071, -0.2946256262683552),
    vec2 (-0.50562235513749, 0.8627549095688868),
    vec2 (-0.2099523790012021, -0.9777116131824024),
    vec2 (0.8152470554454873, 0.5791133210240138),
    vec2 (-0.9923232342597708, 0.12367133357503751),
    vec2 (0.6481694844288681, -0.7614961060013474),
    vec2 (0.036443223183926, 0.9993357251114194),
    vec2 (-0.7019136816142636, -0.7122620188966349),
    vec2 (0.998695384655528, 0.05106396643179117),
    vec2 (-0.7709001090366207, 0.6369560596205411),
    vec2 (0.13818011236605823, -0.9904071165669719),
    vec2 (0.5671206801804437, 0.8236347091470047),
    vec2 (-0.9745343917253847, -0.22423808629319533),
    vec2 (0.8700619819701214, -0.49294233692210304),
    vec2 (-0.30857886328244405, 0.9511987621603146),
    vec2 (-0.4149890815356195, -0.9098263912451776),
    vec2 (0.9205789302157817, 0.3905565685566777)
};
vec2 ComputeFibonacciSpiralDiskSampleClumped(const in int sampleIndex, const in float sampleCountInverse, out float sampleDistNorm)
{
    // Samples not biased away from the center - sample 0 at (0, 0) is important for blocker search near shadow contact points.
    sampleDistNorm = sampleIndex * sampleCountInverse;

    // Third power chosen arbitrarily - center area is floatly that much more important
    sampleDistNorm = sampleDistNorm * sampleDistNorm * sampleDistNorm;

    return fibonacciSpiralDirection[sampleIndex] * sampleDistNorm;
}

// Samples uniformly spread across the disk kernel
vec2 ComputeFibonacciSpiralDiskSampleUniform(const in int sampleIndex, const in float sampleCountInverse, const in float sampleBias, out float sampleDistNorm)
{
    // Samples biased away from the center, so that sample 0 doesn't fall at (0, 0), or it will not be affected by sample jitter and create a visible edge.
    sampleDistNorm = sampleIndex * sampleCountInverse + sampleBias;

    // sqrt results in uniform distribution
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

bool BlockerSearch_Area(inout float closestBlocker, float maxSampleZDistance, vec3 posTCShadowmap, vec2 minCoord, vec2 maxCoord, vec2 sampleJitter, int sampleCount, int shadowMapIndex)
{
    #define NEARPLANE 1
    maxSampleZDistance = min(1 - posTCShadowmap.z, maxSampleZDistance);

    float sampleCountInverse = 1.0f / sampleCount;

    vec2 filterScalePos, filterScaleNeg;
    vec2 filterOffset;
    FilterScaleOffset(posTCShadowmap, maxSampleZDistance, filterScalePos, filterScaleNeg, filterOffset);

    closestBlocker = NEARPLANE;
    for (int i = 0; i < sampleCount && i < 64; ++i)
    {
        float sampleDistNorm;
        vec2 offset = ComputeFibonacciSpiralDiskSampleClumped(i, sampleCountInverse, sampleDistNorm);
        offset = vec2(offset.x *  sampleJitter.y + offset.y * sampleJitter.x,
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

    return NEARPLANE > closestBlocker;
}

float PCSS_Area(vec3 posTCShadowmap, float maxSampleZDistance, vec2 minCoord, vec2 maxCoord, vec2 sampleJitter, int sampleCount, int shadowMapIndex)
{
    float biasFactor = 1;
    float sampleCountInverse = 1.0f / (sampleCount + biasFactor);
    float sampleBias = biasFactor * sampleCountInverse;

    vec2 filterScalePos, filterScaleNeg;
    vec2 filterOffset;
    FilterScaleOffset(posTCShadowmap, maxSampleZDistance, filterScalePos, filterScaleNeg, filterOffset);

    float sum = 0.0;
    for (int i = 0; i < sampleCount && i < 64; ++i)
    {
        float sampleDistNorm;
        vec2 offset = ComputeFibonacciSpiralDiskSampleUniform(i, sampleCountInverse, sampleBias, sampleDistNorm);
        offset = vec2(offset.x *  sampleJitter.y + offset.y * sampleJitter.x,
                       offset.x * -sampleJitter.x + offset.y * sampleJitter.y);

        offset = offset * vec2(offset.x > 0 ? filterScalePos.x : filterScaleNeg.x, offset.y > 0 ? filterScalePos.y : filterScaleNeg.y) + filterOffset * sampleDistNorm;
        float zoffset = maxSampleZDistance * sampleDistNorm;

        vec2 pos = posTCShadowmap.xy + offset;
        sum += (pos.x < minCoord.x || pos.y < minCoord.y || pos.x > maxCoord.x || pos.y > maxCoord.y) ? 
                1.0 : texture(shadowMaps, vec4(pos, shadowMapIndex, posTCShadowmap.z + zoffset)).r;
    }

    return sum / sampleCount;
}

//From  Next Generation Post Processing in Call of Duty: Advanced Warfare [Jimenez 2014]
// http://advances.realtimerendering.com/s2014/index.html
float InterleavedGradientNoise(vec2 pixCoord, uint frameCount)
{
    const vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
    vec2 frameMagicScale = vec2(2.083f, 4.867f);
    pixCoord += frameCount * frameMagicScale;
    return fract(magic.z * fract(dot(pixCoord, magic.xy)));
}

float PenumbraSizePunctual(float Reciever, float Blocker)
{
    return abs((Reciever - Blocker) / Blocker);
}

float PenumbraSizeDirectional(float Reciever, float Blocker, float rangeScale)
{
    return abs(Reciever - Blocker) * rangeScale;
}

// TODO: This PCSS variant works for other types of lights as well, but is not well tested there, so we're introducing it only for area lights for now.
float SampleShadow_PCSS_Area(vec3 posTCShadowmap, vec2 posSS, float shadowSoftness, float minFilterRadius, int blockerSampleCount, int filterSampleCount, float depthBias, int shadowMapIndex, float area)
{
    posTCShadowmap.z += depthBias;

    // This is a modified PCSS. Instead of performing both the blocker search and filtering phases using a flat disc of samples centered around
    // the shaded point, it adds a z offset to sample points extruding them in a cone shape - pyramid, actually - towards the light. The base of the pyramid
    // is the near plane of the area light (surface of the area light when near plane is at 0), the apex at the shaded point, and samples lie on the 4 sides
    // of the pyramid.
    //
    // The idea is that only casters within the volume of that pyramid would contribute to the shadow. In other words any casters caught by a sample with
    // z further away from the light than z of that sample don't contribute to the shadow.
    //
    // The maximum heigh of the pyramid is the z distance between the shaded point and the near plane. Lowering that height is necessary to keep
    // the sampling kernel sizes reasonable and is controlled by maxSampleZDistance. Higher maxSampleZDistance values result in wider penumbras.

    // Rescale the softness param so that the default 1 gives a very soft shadow without pushing it to edge, where artifacts start to show up.
    // This way setting softness to slightly more than 1 will get the shadow close to the raytraced reference, but with a more stable default.
    float maxSampleZDistance = shadowSoftness * 0.1 * sqrt(area);

    float sampleJitterAngle = InterleavedGradientNoise(posSS.xy, pc.frame_num) * 2.0 * PI;
    vec2 sampleJitter = vec2(sin(sampleJitterAngle), cos(sampleJitterAngle));

    vec2 minCoord = vec2(0);
    vec2 maxCoord = vec2(1);

    //1) Blocker Search
    float blocker = 0.0;
    bool blockerFound = BlockerSearch_Area(blocker, maxSampleZDistance, posTCShadowmap, minCoord, maxCoord, sampleJitter, blockerSampleCount, shadowMapIndex);

    //2) Penumbra Estimation
    maxSampleZDistance *= PenumbraSizePunctual(posTCShadowmap.z, blocker);
    // Extend the sampling cone only up to a certain margin before the blocker. Extending it past that distance will make samples miss the blocker and the shadow will fade.
    maxSampleZDistance = min(maxSampleZDistance, (blocker - posTCShadowmap.z) * 0.9);
    // minFilterRadius can extend the cone past the above, so min&max instead of clamp.
    maxSampleZDistance = max(maxSampleZDistance, minFilterRadius / 100);

    //3) Filter
    // We can't early out of the function if blockers are not found since Vulkan triggers a warning otherwise
    bool withinShadowmap = posTCShadowmap.x > 0 && posTCShadowmap.y > 0 && posTCShadowmap.x < 1 && posTCShadowmap.y < 1;
    return blockerFound && withinShadowmap ? PCSS_Area(posTCShadowmap, maxSampleZDistance, minCoord, maxCoord, sampleJitter, filterSampleCount, shadowMapIndex) : 1.0f;
}
float ValueNoise(vec3 pos)
{
	vec3 Noise_skew = pos + 0.2127 + pos.x * pos.y * pos.z * 0.3713;
	vec3 Noise_rnd = 4.789 * sin(489.123 * (Noise_skew));
	return fract(Noise_rnd.x * Noise_rnd.y * Noise_rnd.z * (1.0 + Noise_skew.x) * pc.frame_num);
}

void main()
{
    vec2 screen_uv = vec2(gl_FragCoord.x / constantBuffer.width, gl_FragCoord.y / constantBuffer.height);
    if(constantBuffer.shader_type != 0){
        float cadDepth = -eyePos.z / constantBuffer.z_far;
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
                        visibility = PCF(shadowMaps,sm_tc,shadowMapIndex,area, random);
                    }else if(pc.shadow_type == 1){
                        // visibility = 1 - PCSS(sm_tc,shadowMapIndex, area, random);
                        visibility = SampleShadow_PCSS_Area(sm_tc.xyz, vec2(gl_FragCoord.xy), pc.softness, pc.softness_falloff, pc.blocker_sample_num, pc.pcf_sample_num, pc.shadow_bias, shadowMapIndex, area);
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
    vec4 last_ndc = pc.projection * pc.last_view * vec4(lastWorldPos, 1);
    ivec2 last_coord = ivec2(((last_ndc.x / last_ndc.w) / 2 + 0.5) * constantBuffer.width, ((last_ndc.y / last_ndc.w) / 2 + 0.5) * constantBuffer.height);
    float old_shadow = 1;
    float oldInstanceID = -1;
    if(last_coord.x >= 0 && last_coord.y >= 0 && last_coord.x < constantBuffer.width && last_coord.y < constantBuffer.height){
        vec2 shadowdataold_shadow = texelFetch(shadowInputAttachment, last_coord, gl_SampleID).rg;
        oldInstanceID = shadowdataold_shadow.y;
        old_shadow = shadowdataold_shadow.x;
    }

    float current_shadow_value = scene_brightness; // 暂时保存当前帧的阴影值
    if (abs(oldInstanceID - InstanceID) < 0.1 && abs(old_shadow - scene_brightness) < 0.1)
    {
        float historyLuma = old_shadow;
        float currentLuma = current_shadow_value;

        float diff = abs(currentLuma - historyLuma) / max(max(currentLuma, historyLuma), 0.2); // 计算相对差异

        float weight_sq = (1.0 - diff);
        weight_sq = weight_sq * weight_sq;
        
        const float feedbackMin = 0.96; // 最小反馈 (当前帧差异大时)
        const float feedbackMax = 0.91; // 最大反馈 (当前帧差异小时)

        float feedback = (1.0 - weight_sq) * feedbackMin + weight_sq * feedbackMax;

        scene_brightness = mix(current_shadow_value, old_shadow, feedback);

        // 钳制最终结果
        scene_brightness = clamp(scene_brightness, 0.0, 1.0);
    }

    outColor.rgb = texture(cameraImage, screen_uv).rgb * scene_brightness;
    outColor.a = 1;
    outShadow = vec4(scene_brightness, InstanceID, gl_FragCoord.z, 1);
}
