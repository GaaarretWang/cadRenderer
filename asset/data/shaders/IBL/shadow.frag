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

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 5) uniform sampler2D cameraImageSampler;

layout (set = MATERIAL_DESCRIPTOR_SET, binding = 6) uniform Params {
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


layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps; //����ָ����ɫ����Դ�������������Ͱ�����
//layout(set = 1, binding = 2) uniform sampler2D shadowMap; //����������������

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 normalDir;
layout(location = 2) in vec4 vertexColor;
#ifndef VSG_POINT_SPRITE
layout(location = 3) in vec2 texCoord0;
#endif
layout(location = 5) in vec3 viewDir;

layout(location = 0) out vec4 outColor;

#define NUM_SAMPLES 200 //����������������filter�Ĵ�С��Խ��������Ĳ���Խ�࣬��Ӱ���Ч��Խ����
#define BLOCKER_SEARCH_NUM_SAMPLES NUM_SAMPLES
#define PCF_NUM_SAMPLES NUM_SAMPLES
#define NUM_RINGS 10

#define EPS 1e-2  //��ģ������Ӱ�ж�Ч���кܴ��Ӱ��
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

highp float rand_1to1(highp float x ) {                              //��
  // ��float���͵�һά�������x����һ��[-1,1]��Χ��float 
  // -1 -1
  return fract(sin(x)*10000.0);
}

highp float rand_2to1(vec2 uv ) {                              //��
  // ��һ����ά�������uv�������һ����Χ[0,1]��float����
  // 0 - 1
	const highp float a = 12.9898, b = 78.233, c = 43758.5453;
	highp float dt = dot( uv.xy, vec2( a,b ) ), sn = mod( dt, PI );
	return fract(sin(sn) * c);
}

float unpack(vec4 rgbaDepth) {                             //��
    //ʵ����ɫֵ�����ֵ��ӳ��
    //���unpack()���Կ�����shadowFragment.glsl�е�pack()�ķ�ת
    // ��������RGBAֵת����[0,1]�ĸ�����
    const vec4 bitShift = vec4(1.0, 1.0/256.0, 1.0/(256.0*256.0), 1.0/(256.0*256.0*256.0));
    return dot(rgbaDepth, bitShift);
}

vec2 poissonDisk[NUM_SAMPLES];
void uniformDiskSamples( const in vec2 randomSeed ) {                              //��
  //����Բ�̲���

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

void poissonDiskSamples( const in vec2 randomSeed ) {                              //��
  //����Բ�̲���

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

float PCF(sampler2DArrayShadow shadowMap, vec4 coords,int shadowMapIndex) {                              //��
  //1.��������Stride��shadow map�ֱ��ʡ���ʼ���ֵ��������Χ��ǰ�����
  //StrideԽ�󣬱�ԵԽģ��
  float Stride = 3.0; //�˲��Ĳ�����ÿ�ξ����л���������/����
  float shadowmapSize = 2048.; //shadow map�Ĵ�С
  float visibility = 0.0;
  float cur_depth = coords.z;
  
  //2.����Բ�̲����õ�������
  poissonDiskSamples(coords.xy);

  //2.����Բ�̲����õ�������
  //uniformDiskSamples(coords.xy);

  //3.��ÿ������бȽ����ֵ���ۼ�  NUM_SAMPLES�ڿ�ͷ��ʼ��Ϊ20
  float ctrl = 1.0;
     
  for(int i =0 ; i < NUM_SAMPLES; i++)
  {
    //vec4 shadow_color = texture2D(shadowMap, coords.xy + poissonDisk[i] * Stride / shadowmapSize); 
    //float shadow_depth = unpack(shadow_color);
    //float res = cur_depth < shadow_depth + EPS ? 1. : 0. ;//û�н�����ڵ������Ը��õع۲���Ӱ����������Ų���
    //visibility += res;
     float res  = texture(shadowMap, vec4(coords.xy + poissonDisk[i] * Stride / shadowmapSize, shadowMapIndex, coords.z)).r;
     visibility += res;
  }

  //4.���ؾ�ֵ
  return visibility / float(NUM_SAMPLES);
}

float findBlocker(sampler2DArrayShadow shadowMap,  vec4 coords, int shadowMapIndex) {
  //1.��������
  int blockerNum = 0;
  float block_depth = 0.;
  float shadowmapSize = 2048.;
  float Stride = 20.;

  //2.���ɲ����õ���
  poissonDiskSamples(coords.xy);

  //3.�жϻ����blocker���ۼ�
  for(int i = 0; i < NUM_SAMPLES; i++){ //�Ƽ��˰汾
      vec2 xy=vec2(coords.xy + poissonDisk[i] * Stride / shadowmapSize);
	  float dp = 1.0; //�������
	  while (texture(shadowMap, vec4(xy, shadowMapIndex, dp)).r < 1.0 && dp>=0.0)
		{
			dp -= 0.01;
		}
		if (dp >=0.0) {
			blockerNum++;
			block_depth += dp;
		}
  }
  //���shading point�ڳ��������յ��ĵط�����ҪҲ��һ������ֵ����Ȼ������ȫ�ڵ�
  //����/0���� �������ֵ�������е㶼<������������Ӱ��
  if(blockerNum == 0){
    return 1.;
  }
  return float(block_depth) / float(blockerNum);
}

float PCSS(sampler2DArrayShadow shadowMap, vec4 coords,int shadowMapIndex){

  // STEP 1: avgblocker depth
  float d_Blocker = findBlocker(shadowMap, coords,shadowMapIndex);
  float w_Light = 1.; // ��Դ��С���������ǣ�ֱ�ӿ������Դ������
  float d_Receiver = coords.z;

  // STEP 2: penumbra size
  //���ݹ�ʽ�������������Σ�����wpenumbra��Ӱ��Χ
  float w_penumbra = w_Light * (d_Receiver - d_Blocker) / d_Blocker;

  // STEP 3: filtering
  //����ʵ����һ��PCF��ֻ������PCF����һ��w_penumbra������d_Receiber��Ӱ��
  //1
  float Stride = 20.;
  float shadowmapSize = 2048.;
  float visibility = 0.;
  float cur_depth = coords.z;

  //2 ����
  //poissonDiskSamples(coords.xy); findBlocker�Ѿ�������

  //3
  //float ctrl = 1.0;
  //float bias = getBias(ctrl);//���Դ˽�����ڵ����⣬��ͬ�����²�����Ӱ��ʧ����

  for(int i = 0; i < NUM_SAMPLES; i++){
     float res  = texture(shadowMap, vec4(coords.xy + poissonDisk[i] * Stride / shadowmapSize* w_penumbra, shadowMapIndex, coords.z)).r;
     visibility += res;
  }

  //4.���ؾ�ֵ
  return visibility / float(NUM_SAMPLES);
}

void main()
{
    float brightnessCutoff = 0.0001;
    vec4 diffuseColor = vertexColor * material.diffuseColor;

    vec4 ambientColor = diffuseColor * material.ambientColor * material.ambientColor.a; //�߹�
    vec4 specularColor = material.specularColor; //�߹�
    vec4 emissiveColor = material.emissiveColor; //�Է���
    float shininess = material.shininess; //�⻬��
    float ambientOcclusion = 1.0; //--------------------�������ڱ�----------------------//

    if (material.alphaMask == 1.0f) //͸���ȼ���
    {
        if (diffuseColor.a < material.alphaMaskCutoff)
            discard;
    }

    vec3 nd = getNormal();
    vec3 vd = normalize(viewDir);

    vec3 color = vec3(0.0, 0.0, 0.0); //���������ɫ ��ʼΪ0 �Ҷ�Ϊ��������ʽ
    float visibility=0.0f;

    vec4 lightNums = lightData.values[0];
    int numAmbientLights = int(lightNums[0]);
    int numDirectionalLights = int(lightNums[1]);
    int numPointLights = int(lightNums[2]);
    int numSpotLights = int(lightNums[3]);
    int index = 1;

    // index used to step through the shadowMaps array
    int shadowMapIndex = 0;

    //--------------------------�����----------------------------//

    if (numDirectionalLights>0)
    {
        float totalAttenuation = 1.0f;
        float totalBrigtness = 0.0f;
        float totalVisibility = 0.0f;
        // directional lights
        for(int i = 0; i<numDirectionalLights; ++i)
        {
            vec4 lightColor = lightData.values[index++]; //��ȡ��Դ����ɫֵ
            vec3 direction = -lightData.values[index++].xyz; //��ȡ��Դ��������
            vec4 shadowMapSettings = lightData.values[index++]; //��ȡ��Ӱ��ͼ�����ò���

            float brightness = lightColor.a; //�ӹ�Դ��ɫ����ȡ����ֵ
            totalBrigtness += brightness;
            float lightShadowStrength = 0.7f;
            float attenuation = 1.0f;

            // check shadow maps if required
            bool matched = false;
            while ((shadowMapSettings.r > 0.0 && brightness > brightnessCutoff) && !matched)
            {//����Ƿ���Ҫ������Ӱ��ͼ�Ĵ������Լ��Ƿ����ҵ�ƥ�����Ӱ��ͼ��
                mat4 sm_matrix = mat4(lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++]); //��ȡ��Ӱ��ͼ�ı任����

                vec4 sm_tc = (sm_matrix) * vec4(eyePos, 1.0); //������ռ䵽��Դ�ռ�ı任���Ա㽫��Ӱ��ͼ�����������볡���еĵ��Ӧ������

                if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0 /* && sm_tc.z <= 1.0*/)
                {//������������Ƿ�����Ч��Χ��
                    matched = true;

                    //����ǰ�����������������꣬��������������Ӱ��ͼ���������ĸ����������������������,ͨ�����ڽ���͸���ȡ�
                    float coverage = texture(shadowMaps, vec4(sm_tc.st, shadowMapIndex, sm_tc.z)).r; //����ǰƬ�ε�������������Ӱ��ͼ�е����ֵ���бȽ� ����Ӱ0 ������Ӱ1
                    // float coverage = PCF(shadowMaps,sm_tc,shadowMapIndex);
                    // attenuation *= mix(1.0, lightShadowStrength, coverage); //������Ӱ��ͼ�ĸ����ʵ�������ֵ��

                    //attenuation *= (1-lightShadowStrength) + lightShadowStrength * (1-coverage); //������Ӱ��ͼ�ĸ����ʵ�������ֵ��
                    visibility = PCF(shadowMaps,sm_tc,shadowMapIndex);
                    //float visibility = PCSS(shadowMaps,sm_tc,shadowMapIndex);
                    //float d_Blocker = findBlocker(shadowMaps, sm_tc,shadowMapIndex);
                    //brightness *= (1.0-visibility);
                    visibility = (1.0 - visibility);
                    // brightness=1-visibility;
                }else{
                  visibility = 1.0;
                }

                ++shadowMapIndex;
                shadowMapSettings.r -= 1.0; //������Ӱ��ͼ���������ò���
            }
            totalVisibility += brightness * visibility;

            if (shadowMapSettings.r > 0.0) //����Ƿ���δ���ʵ���Ӱ��ͼ
            { 
                // skip lightData and shadowMap entries for shadow maps that we haven't visited for this light
                // so subsequent light pointions are correct.
                index += 4 * int(shadowMapSettings.r); //����δ���ʵĹ�Դ���ݺ���Ӱ��ͼ����Ŀ���Ա�֤��һ����Դ��λ������ȷ�ġ�
                shadowMapIndex += int(shadowMapSettings.r); //������Ӱ��ͼ������
            }
            totalAttenuation *= attenuation;
            // float unclamped_LdotN = dot(direction, nd); //�����Դ���������뷨�������ĵ��

            // float diff = max(unclamped_LdotN, 0.0); //�������������ǿ��
            // color.rgb += (diffuseColor.rgb * lightColor.rgb) * (diff * brightness); //�������������ǿ�ȡ���Դ��ɫ������ֵ�������յ���ɫ

            // if (shininess > 0.0 && diff > 0.0) //����Ƿ���ڸ߹ⷴ�䣬�������������ǿ�ȴ����㡣
            // { //���������
            //     vec3 halfDir = normalize(direction + vd);
            //     color.rgb += specularColor.rgb * (pow(max(dot(halfDir, nd), 0.0), shininess) * brightness); //���ݸ߹ⷴ��ļ��㹫ʽ���������յ���ɫ��
            // }
        }
        // if light is too dim/shadowed to effect the rendering skip it �����Դ������ֵС�ڵ���������ֵ����������ǰ��Դ�ļ������Ⱦ��
        // if (totalAttenuation >= brightnessCutoff ) discard; //******************
        color.rgb = (totalVisibility / totalBrigtness).xxx;
    }

    //----------------------------------------�����ɫ----------------------------------------//
    //outColor.rgb = (color * ambientOcclusion) + emissiveColor.rgb; //ambientOcclusion�����ڱ� �����ɼ���
    vec2 screen_uv = vec2(gl_FragCoord.x / extentParams.width, gl_FragCoord.y / extentParams.height);
	  outColor.rgb = texture(cameraImageSampler, screen_uv).rgb * color;
    outColor.a = diffuseColor.a;
}
