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

#ifdef VSG_SPECULAR_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 5) uniform sampler2D specularMap;
#endif

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


layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps;//用于指定着色器资源的描述集索引和绑定索引
//layout(set = 1, binding = 2) uniform sampler2D shadowMap; //声明采样器？？？

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 normalDir;
layout(location = 2) in vec4 vertexColor;
#ifndef VSG_POINT_SPRITE
layout(location = 3) in vec2 texCoord0;
#endif
layout(location = 5) in vec3 viewDir;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 fragColor;   // 输出color map
layout(location = 2) out float fragDepth;  // 输出depth map

#define NUM_SAMPLES 200 //样本数量，决定了filter的大小。越大参与计算的参数越多，阴影柔和效果越明显
#define BLOCKER_SEARCH_NUM_SAMPLES NUM_SAMPLES
#define PCF_NUM_SAMPLES NUM_SAMPLES
#define NUM_RINGS 10

#define EPS 1e-2  //对模型上阴影判断效果有很大的影响
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

highp float rand_1to1(highp float x ) {                              //√
  // 从float类型的一维随机变量x产生一个[-1,1]范围的float 
  // -1 -1
  return fract(sin(x)*10000.0);
}

highp float rand_2to1(vec2 uv ) {                              //√
  // 从一个二维随机变量uv随机产生一个范围[0,1]的float变量
  // 0 - 1
	const highp float a = 12.9898, b = 78.233, c = 43758.5453;
	highp float dt = dot( uv.xy, vec2( a,b ) ), sn = mod( dt, PI );
	return fract(sin(sn) * c);
}

float unpack(vec4 rgbaDepth) {                             //√
    //实现颜色值到深度值的映射
    //这个unpack()可以看成是shadowFragment.glsl中的pack()的反转
    // 将纹理的RGBA值转换成[0,1]的浮点数
    const vec4 bitShift = vec4(1.0, 1.0/256.0, 1.0/(256.0*256.0), 1.0/(256.0*256.0*256.0));
    return dot(rgbaDepth, bitShift);
}

vec2 poissonDisk[NUM_SAMPLES];
void uniformDiskSamples( const in vec2 randomSeed ) {                              //√
  //均匀圆盘采样

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

void poissonDiskSamples( const in vec2 randomSeed ) {                              //√
  //泊松圆盘采样

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

float PCF(sampler2DArrayShadow shadowMap, vec4 coords,int shadowMapIndex) {                              //√
  //1.给定步长Stride、shadow map分辨率、初始输出值、卷积范围当前的深度
  //Stride越大，边缘越模糊
  float Stride = 4.0; //滤波的步长，每次卷积盒互动的行数/列数
  float shadowmapSize = 2048.; //shadow map的大小
  float visibility = 0.0;
  float cur_depth = coords.z;
  
  //2.泊松圆盘采样得到采样点
  poissonDiskSamples(coords.xy);

  //2.均匀圆盘采样得到采样点
  //uniformDiskSamples(coords.xy);

  //3.对每个点进行比较深度值并累加  NUM_SAMPLES在开头初始化为20
  float ctrl = 1.0;
     
  for(int i =0 ; i < NUM_SAMPLES; i++)
  {
    //vec4 shadow_color = texture2D(shadowMap, coords.xy + poissonDisk[i] * Stride / shadowmapSize); 
    //float shadow_depth = unpack(shadow_color);
    //float res = cur_depth < shadow_depth + EPS ? 1. : 0. ;//没有解决自遮挡，可以更好地观察阴影情况（包括脚部）
    //visibility += res;
     float res  = texture(shadowMap, vec4(coords.xy + poissonDisk[i] * Stride / shadowmapSize, shadowMapIndex, coords.z)).r;
     visibility += res;
  }

  //4.返回均值
  return visibility / float(NUM_SAMPLES);
}

float findBlocker(sampler2DArrayShadow shadowMap, vec4 coords, int shadowMapIndex) {
  //1.给定参数
  int blockerNum = 0;
  float block_depth = 0.;
  float shadowmapSize = 2048.;
  float Stride = 20.;

  //2.泊松采样得到点
  poissonDiskSamples(coords.xy);

  //3.判断会否是blocker并累加
  for(int i = 0; i < NUM_SAMPLES; i++){ //逼急了版本
      vec2 xy=vec2(coords.xy + poissonDisk[i] * Stride / shadowmapSize);
	  float dp = 1.0; //测试深度
	  while (texture(shadowMap, vec4(xy, shadowMapIndex, dp)).r < 1.0 && dp>=0.0)
		{
			dp -= 0.01;
		}
		if (dp >=0.0) {
			blockerNum++;
			block_depth += dp;
		}
  }
  //如果shading point在场景被光照到的地方，需要也给一个返回值，不然环境是全黑的
  //避免/0报错 返回最大值，则所有点都<它，都不在阴影里
  if(blockerNum == 0){
    return 1.;
  }
  return float(block_depth) / float(blockerNum);
}

float PCSS(sampler2DArrayShadow shadowMap, vec4 coords,int shadowMapIndex){

  // STEP 1: avgblocker depth
  float d_Blocker = findBlocker(shadowMap, coords,shadowMapIndex);
  float w_Light = 1.; // 光源大小，不做考虑，直接看作点光源（？）
  float d_Receiver = coords.z;

  // STEP 2: penumbra size
  //根据公式（相似性三角形）计算wpenumbra半影范围
  float w_penumbra = w_Light * (d_Receiver - d_Blocker) / d_Blocker;

  // STEP 3: filtering
  //这其实又是一次PCF，只不过比PCF多了一重w_penumbra加入了d_Receiber的影响
  //1
  float Stride = 40.;
  float shadowmapSize = 2048.;
  float visibility = 0.;
  float cur_depth = coords.z;

  //2 采样
  //poissonDiskSamples(coords.xy); findBlocker已经进行了

  //3
  //float ctrl = 1.0;
  //float bias = getBias(ctrl);//若以此解决自遮挡问题，则同样导致部分阴影消失现象

  for(int i = 0; i < NUM_SAMPLES; i++){
     float res  = texture(shadowMap, vec4(coords.xy + poissonDisk[i] * Stride / shadowmapSize* w_penumbra, shadowMapIndex, coords.z)).r;
     visibility += res;
  }

  //4.返回均值
  return visibility / float(NUM_SAMPLES);
}

void main()
{
    float brightnessCutoff = 0.7;

#ifdef VSG_POINT_SPRITE
    vec2 texCoord0 = gl_PointCoord.xy;
#endif

    vec4 diffuseColor = vertexColor * material.diffuseColor;

#ifdef VSG_DIFFUSE_MAP
    #ifdef VSG_GREYSCALE_DIFFUSE_MAP
        float v = texture(diffuseMap, texCoord0.st).s;
        diffuseColor *= vec4(v, v, v, 1.0);
    #else
        diffuseColor *= texture(diffuseMap, texCoord0.st);
    #endif
#endif

    vec4 ambientColor = diffuseColor * material.ambientColor * material.ambientColor.a; //高光
    vec4 specularColor = material.specularColor; //高光
    vec4 emissiveColor = material.emissiveColor; //自发光
    float shininess = material.shininess; //光滑度
    float ambientOcclusion = 1.0; //--------------------环境光遮蔽----------------------//

    if (material.alphaMask == 1.0f) //透明度剪切
    {
        if (diffuseColor.a < material.alphaMaskCutoff)
            discard;
    }

#ifdef VSG_EMISSIVE_MAP
    emissiveColor *= texture(emissiveMap, texCoord0.st);
#endif

#ifdef VSG_LIGHTMAP_MAP
    ambientOcclusion *= texture(aoMap, texCoord0.st).r; //未执行
#endif

#ifdef VSG_SPECULAR_MAP
    specularColor *= texture(specularMap, texCoord0.st);
#endif

    vec3 nd = getNormal();
    vec3 vd = normalize(viewDir);

    vec3 color = vec3(0.0, 0.0, 0.0); //最终输出颜色 初始为0 且都为浮点数形式
    float visibility=0.0f;

    vec4 lightNums = lightData.values[0];
    int numAmbientLights = int(lightNums[0]);
    int numDirectionalLights = int(lightNums[1]);
    int numPointLights = int(lightNums[2]);
    int numSpotLights = int(lightNums[3]);
    int index = 1;

    //--------------------------环境光----------------------------//
    if (numAmbientLights>0 )
    {
        // ambient lights
        for(int i = 0; i<numAmbientLights; ++i)
        {
            vec4 lightColor = lightData.values[index++];
            color += (ambientColor.rgb * lightColor.rgb) * (lightColor.a);
        }
    }

    // index used to step through the shadowMaps array
    int shadowMapIndex = 0;

 //--------------------------定向光----------------------------//

 if (numDirectionalLights>0)
    {
        // directional lights
        for(int i = 0; i<numDirectionalLights; ++i)
        {
            vec4 lightColor = lightData.values[index++]; //获取光源的颜色值
            vec3 direction = -lightData.values[index++].xyz; //获取光源方向向量
            vec4 shadowMapSettings = lightData.values[index++]; //获取阴影贴图的设置参数

            float brightness = lightColor.a; //从光源颜色中提取亮度值

            // check shadow maps if required
            bool matched = false;
            while ((shadowMapSettings.r > 0.0 && brightness > brightnessCutoff) && !matched)
            {//检查是否需要进行阴影贴图的处理，以及是否已找到匹配的阴影贴图。
                mat4 sm_matrix = mat4(lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++]); //获取阴影贴图的变换矩阵

                vec4 sm_tc = (sm_matrix) * vec4(eyePos, 1.0); //从世界空间到光源空间的变换，以便将阴影贴图的纹理坐标与场景中的点对应起来。

                if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0 /* && sm_tc.z <= 1.0*/)
                {//检查纹理坐标是否在有效范围内
                    matched = true;

                    //其中前两个分量是纹理坐标，第三个分量是阴影贴图索引，第四个分量是其他纹理坐标分量,通常用于进行透明度。
                    //float coverage = texture(shadowMaps, vec4(sm_tc.st, shadowMapIndex, sm_tc.z)).r; //将当前片段的纹理坐标与阴影贴图中的深度值进行比较 在阴影0 不在阴影1
                    //brightness *= (1.0-coverage); //根据阴影贴图的覆盖率调整亮度值。
                    visibility = PCF(shadowMaps,sm_tc,shadowMapIndex);
                    //if(visibility<0.7) discard;  //0.7是一个比较合适的数值
                    //else brightness=1-visibility;

                    //if(coverage<=0.5) discard;  //0.7是一个比较合适的数值
                    //else brightness=1-coverage;
                    //float visibility = PCSS(shadowMaps,sm_tc,shadowMapIndex);
                    //float d_Blocker = findBlocker(shadowMaps, sm_tc,shadowMapIndex);
                    brightness *= (1.0-visibility);
                    //brightness=1-visibility;
                }

                ++shadowMapIndex;
                shadowMapSettings.r -= 1.0; //更新阴影贴图索引和设置参数
            }

            if (shadowMapSettings.r > 0.0) //检查是否还有未访问的阴影贴图
            { 
                // skip lightData and shadowMap entries for shadow maps that we haven't visited for this light
                // so subsequent light pointions are correct.
                index += 4 * int(shadowMapSettings.r); //跳过未访问的光源数据和阴影贴图的条目，以保证下一个光源的位置是正确的。
                shadowMapIndex += int(shadowMapSettings.r); //更新阴影贴图的索引
            }

            // if light is too dim/shadowed to effect the rendering skip it 如果光源的亮度值小于等于亮度阈值，则跳过当前光源的计算和渲染。
            //if (brightness <= brightnessCutoff ) continue;
            if (brightness >= brightnessCutoff ) discard;   //可以通过亮度剪切进行控制

            float unclamped_LdotN = dot(direction, nd); //计算光源方向向量与法线向量的点积

            float diff = max(unclamped_LdotN, 0.0); //计算漫反射光照强度
            color.rgb += (diffuseColor.rgb * lightColor.rgb) * (diff * brightness); //根据漫反射光照强度、光源颜色和亮度值计算最终的颜色

            if (shininess > 0.0 && diff > 0.0) //检查是否存在高光反射，并且漫反射光照强度大于零。
            { //计算半向量
                vec3 halfDir = normalize(direction + vd);
                color.rgb += specularColor.rgb * (pow(max(dot(halfDir, nd), 0.0), shininess) * brightness); //根据高光反射的计算公式，计算最终的颜色。
            }
        }
    }

    //----------------------------------------输出颜色----------------------------------------//
    //outColor.rgb = (color * ambientOcclusion) + emissiveColor.rgb; //ambientOcclusion环境遮蔽 代表可见性
    outColor.rgb = color; //ambientOcclusion环境遮蔽 代表可见性
    //outColor.rgb =vec3(1-visibility, 1-visibility, 1-visibility);
    outColor.a = diffuseColor.a;

    //float depth = 0;  // 计算的深度值
    // 将颜色和深度值写入到纹理中
    //fragColor = color;
    //fragDepth = 1;
}
