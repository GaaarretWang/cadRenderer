#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_POINT_SPRITE, VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, SHADOWMAP_DEBUG)

#define VIEW_DESCRIPTOR_SET 0
#define MATERIAL_DESCRIPTOR_SET 1

#ifdef VSG_DIFFUSE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D diffuseMap;
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

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 5) uniform sampler2D cameraImage;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 6) uniform sampler2D depthImage;

layout (set = MATERIAL_DESCRIPTOR_SET, binding = 7) uniform Params {
	float width;
	float height;
} extentParams;

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 10) uniform MaterialData
{
    vec4 ambientColor;
    vec4 diffuseColor;
    vec4 specularColor;
    vec4 emissiveColor;
    float shininess;
    float alphaMask;
    float alphaMaskCutoff;
} material;

layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[2048];
} lightData;


layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps; //ָɫԴͰ
//layout(set = 1, binding = 2) uniform sampler2D shadowMap; //

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 normalDir;
layout(location = 2) in vec4 vertexColor;
#ifndef VSG_POINT_SPRITE
layout(location = 3) in vec2 texCoord0;
#endif
layout(location = 5) in vec3 viewDir;

layout(location = 0) out vec4 outColor;

#define NUM_SAMPLES 200 //filterĴСԽĲԽ࣬ӰЧԽ
#define BLOCKER_SEARCH_NUM_SAMPLES NUM_SAMPLES
#define PCF_NUM_SAMPLES NUM_SAMPLES
#define NUM_RINGS 10

#define EPS 1e-2  //ģӰжЧкܴӰ
#define PI 3.141592653589793
#define PI2 6.283185307179586

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

vec3 computeLighting(vec3 ambientColor, vec3 diffuseColor, vec3 specularColor, vec3 emissiveColor, float shininess, float ambientOcclusion, vec3 ld, vec3 nd, vec3 vd)
{
    vec3 color = vec3(0.0);
    color.rgb += ambientColor;

    float diff = max(dot(ld, nd), 0.0);
    color.rgb += diffuseColor * diff;

    if (diff > 0.0)
    {
        vec3 halfDir = normalize(ld + vd);
        color.rgb += specularColor * pow(max(dot(halfDir, nd), 0.0), shininess);
    }

    vec3 result = color + emissiveColor;
    result *= ambientOcclusion;

    return result;
}

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

void main()
{
    float cadDepth = -eyePos.z / 65.535;
    vec2 screen_uv = vec2(gl_FragCoord.x / extentParams.width, gl_FragCoord.y / extentParams.height);
    float cameraDepth = texture(depthImage, screen_uv).r;
    if(cadDepth > cameraDepth){
        outColor = texture(cameraImage, screen_uv);
        return;
    }

    float brightnessCutoff = 0.0001;
    vec4 diffuseColor = vertexColor * material.diffuseColor;

    vec4 ambientColor = diffuseColor * material.ambientColor * material.ambientColor.a; //߹
    vec4 specularColor = material.specularColor; //߹
    vec4 emissiveColor = material.emissiveColor; //Է
    float shininess = material.shininess; //⻬
    float ambientOcclusion = 1.0; //--------------------ڱ----------------------//

    if (material.alphaMask == 1.0f) //͸ȼ
    {
        if (diffuseColor.a < material.alphaMaskCutoff)
            discard;
    }

    vec3 nd = getNormal();
    vec3 vd = normalize(viewDir);

    vec3 color = vec3(0.0, 0.0, 0.0); //ɫ ʼΪ0 ҶΪʽ
    float visibility=0.0f;

    vec4 lightNums = lightData.values[0];
    int numAmbientLights = int(lightNums[0]);
    int numDirectionalLights = int(lightNums[1]);
    int numPointLights = int(lightNums[2]);
    int numSpotLights = int(lightNums[3]);
    int index = 1;

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
    color.rgb = vec3(scene_brightness * 0.5 + 0.5);

    //----------------------------------------ɫ----------------------------------------//
    //outColor.rgb = (color * ambientOcclusion) + emissiveColor.rgb; //ambientOcclusionڱ ɼ
	  outColor.rgb = texture(cameraImage, screen_uv).rgb * color;
    outColor.a = diffuseColor.a;
}
