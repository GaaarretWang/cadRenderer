// ============================================================
// PBR Fragment Shader - 基于物理的渲染(PBR)片段着色器
// 功能: 支持 IBL (Image-Based Lighting / 基于图像的光照)、
//       PCF/PCSS 软阴影、多材质纹理采样、时域阴影滤波
// 输出: 多渲染目标(MRT) - 颜色、法线、世界坐标、阴影
// ============================================================

#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_METALLROUGHNESS_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, VSG_WORKFLOW_SPECGLOSS, SHADOWMAP_DEBUG)

// ---- 描述符集编号定义 ----
// set=0: IBL 纹理 (BRDF LUT, 漫反射辐照度, 预滤波环境贴图)
// set=1: 视图相关状态 (灯光数据, 阴影贴图数组)
// set=2: 材质纹理 (漫反射贴图, 法线贴图, 金属度-粗糙度贴图等)
// 注意: 实际 set 编号可能在 C++ 端重新映射
#define IBL_DESCRIPTOR_SET 0
#define VIEW_DESCRIPTOR_SET 1
#define MATERIAL_DESCRIPTOR_SET 2

// ---- 数学常量 ----
const float PI = 3.14159265359;
const float RECIPROCAL_PI = 0.31830988618;   // 1/PI
const float RECIPROCAL_PI2 = 0.15915494;     // 1/(2*PI)
const float EPSILON = 1e-6;                  // 极小值, 防止除零
const float c_MinRoughness = 0.04;           // 最小粗糙度(非金属的 F0 基准值)

#define NUM_RINGS 10

#define EPS 1e-2  // 模型自阴影判断阈值, 过小会对效果有很大影响
#define PI 3.141592653589793
#define PI2 6.283185307179586

// ============================================================
// 材质纹理采样器 (set = MATERIAL_DESCRIPTOR_SET)
// 每个 binding 对应一种材质贴图类型, 由编译宏控制是否启用
// ============================================================

#ifdef VSG_DIFFUSE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D diffuseMap;      // 漫反射(反照率)贴图
#endif

#ifdef VSG_METALLROUGHNESS_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D mrMap;           // 金属度-粗糙度贴图 (R=金属度, G=粗糙度)
#endif

#ifdef VSG_NORMAL_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D normalMap;       // 法线贴图 (切线空间)
#endif

#ifdef VSG_LIGHTMAP_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D aoMap;           // 环境光遮蔽(AO)贴图 (烘焙光照贴图)
#endif

#ifdef VSG_EMISSIVE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 4) uniform sampler2D emissiveMap;     // 自发光贴图
#endif

#ifdef VSG_SPECULAR_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 5) uniform sampler2D specularMap;     // 高光-光泽度工作流贴图
#endif

// ---- PBR 材质参数结构体 ----
// 存储每个实例的材质属性因子, 与贴图采样值相乘得到最终材质参数
struct PbrMaterial {
    vec4 baseColorFactor;       // 基础颜色因子 (RGBA)
    vec4 emissiveFactor;        // 自发光颜色因子
    vec4 diffuseFactor;         // 漫反射因子 (Specular-Glossiness 工作流)
    vec4 specularFactor;        // 高光因子 (Specular-Glossiness 工作流)
    float metallicFactor;       // 金属度因子 (Metallic-Roughness 工作流)
    float roughnessFactor;      // 粗糙度因子 (Metallic-Roughness 工作流)
    float alphaMask;            // 是否启用 Alpha 测试 (1.0=启用)
    float alphaMaskCutoff;      // Alpha 测试阈值, 低于此值的片段被丢弃
};

// ---- 材质数组 (SSBO) ----
// 按实例索引存储的材质参数数组, 通过 materialIndex 索引访问
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 14) buffer MaterialArray {
    PbrMaterial materials[];
} materialArray;

// ---- 全局常量缓冲 (SSBO) ----
// 从 C++ 端传入的渲染全局参数
layout(std430, set = MATERIAL_DESCRIPTOR_SET, binding = 12) buffer ConstantBuffer {
    float z_far;            // 远平面距离
    int shader_type;        // 着色器类型选择
    int width;              // 视口宽度 (像素)
    int height;             // 视口高度 (像素)
}constantBuffer;

// ---- 阴影输入附件 (多重采样) ----
// 用于时域阴影滤波: 读取上一帧的阴影值进行时间累积
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 13) uniform sampler2DMS shadowInputAttachment;

// ============================================================
// 视图相关状态 (set = VIEW_DESCRIPTOR_SET)
// 包含灯光数据和阴影贴图, 随相机视角变化
// ============================================================

// ---- 灯光数据 (Uniform Buffer) ----
// 打包存储所有灯光信息, 通过 values 数组线性索引访问
// 数据布局: [0].xyzw = 灯光数量信息, 之后按灯光类型依次排列
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[2048];
} lightData;

// ---- 阴影贴图数组 ----
// shadowMaps: 带深度比较的阴影采样 (用于 PCF/PCSS 软阴影)
// shadowMapsSampler: 纯深度值采样 (用于 blocker search 阶段)
layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps;
layout(set = VIEW_DESCRIPTOR_SET, binding = 3) uniform sampler2DArray shadowMapsSampler;

// ============================================================
// IBL 纹理 (set = IBL_DESCRIPTOR_SET)
// 基于图像的光照: 预计算的环境光照数据
// ============================================================

layout(set = IBL_DESCRIPTOR_SET, binding = 0) uniform sampler2D samplerBRDFLUT;          // BRDF 积分查找表 (NdotV, roughness) -> (scale, bias)
layout(set = IBL_DESCRIPTOR_SET, binding = 1) uniform samplerCube samplerIrradiance;      // 漫反射辐照度立方体贴图 (低频环境光)
layout(set = IBL_DESCRIPTOR_SET, binding = 2) uniform samplerCube samplerPrefilteredEnv;  // 预滤波镜面反射环境贴图 (多级mipmap, 按粗糙度分级)
layout(set = IBL_DESCRIPTOR_SET, binding = 3) uniform EnvmapParams{                       // 环境贴图参数
    vec4 param;    // param.a = IBL 强度缩放因子
}envmapData;
// layout(set = IBL_DESCRIPTOR_SET, binding = 3) uniform EnvmapData
// {
//     vec4 params;
// } envmapData;

// ============================================================
// 顶点着色器传入的插值变量 (Varying)
// ============================================================
layout(location = 0) in vec3 eyePos;          // 视图空间中的位置
layout(location = 1) in vec3 normalDir;       // 视图空间中的法线方向
layout(location = 2) in vec4 vertexColor;     // 顶点颜色
layout(location = 3) in vec2 texCoord0;       // 第一组 UV 坐标
layout(location = 4) in float highlight;      // 高亮标记 (>0 表示该片段需要高亮显示)
layout(location = 5) in float InstanceID;     // 实例 ID (用于阴影时域滤波的实例匹配)
layout(location = 6) in vec3 worldNormal;     // 世界空间法线
layout(location = 7) in vec3 worldViewDir;    // 世界空间片段位置 (注意: 实际上是片段世界坐标, 不是视线方向)
layout(location = 8) in vec3 lastWorldPos;    // 上一帧的世界空间位置 (用于时域重投影)
layout(location = 9) in flat uint materialIndex;  // 材质索引 (flat = 不插值)

// ============================================================
// 多渲染目标输出 (MRT - Multiple Render Targets)
// ============================================================
layout(location = 0) out vec4 outColor;       // 附件0: 最终颜色 (RGB) + 透明度 (A)
layout(location = 1) out vec4 outNormal;      // 附件1: 世界空间法线 (RGB), A=1 表示不透明
layout(location = 2) out vec4 outWorldPos;    // 附件2: 世界空间位置 (RGB), A 标记透明度
layout(location = 3) out vec4 outShadow;      // 附件3: 阴影亮度 + InstanceID + 深度

// ============================================================
// Push Constants (推送常量)
// 通过 push constant 机制从 CPU 端直接传入, 避免使用 UBO
// 每帧绘制前更新, 包含相机矩阵和阴影/后处理参数
// ============================================================
layout(push_constant) uniform PushConstants {
    mat4 projection;            // 投影矩阵
    mat4 view;                  // 当前帧视图矩阵
    mat4 last_view;             // 上一帧视图矩阵 (用于时域重投影)
    vec3 camera_pos;            // 相机世界空间位置
    float softness;             // 阴影柔和度 (控制 PCF/PCSS 采样范围)
    float baseBrightness;       // 基础亮度 (无阴影区域的参考亮度)
    float ssao_radius;          // SSAO 采样半径
    float exposure;             // 曝光度 (色调映射用)
    float softness_falloff;     // 阴影柔和度衰减系数
    float shadow_bias;          // 阴影深度偏移 (消除阴影痤疮)
    int ssao_kernel_size;       // SSAO 采样核大小
    int denoise_size;           // 降噪核大小
    int blocker_sample_num;     // PCSS blocker search 采样数
    int pcf_sample_num;         // PCF/PCSS 滤波采样数
    int shadow_type;            // 阴影类型: 0=PCF, 1=PCSS
    uint frame_num;             // 当前帧号 (用于时域噪声抖动)
} pc;

// ============================================================
// 工具函数: 伪随机数生成 & 深度解包
// ============================================================

// 一维哈希随机数: 将标量 x 映射到 [-1, 1] 范围的伪随机值
highp float rand_1to1(highp float x ) {
  return fract(sin(x)*10000.0);
}

// 二维哈希随机数: 将二维向量 uv 映射到 [0, 1] 范围的伪随机值
highp float rand_2to1(vec2 uv ) {
	const highp float a = 12.9898, b = 78.233, c = 43758.5453;
	highp float dt = dot( uv.xy, vec2( a,b ) ), sn = mod( dt, PI );
	return fract(sin(sn) * c);
}

// 深度值解包: 将 RGBA 编码的深度值还原为 [0,1] 标量
// 与 shadowFragment.glsl 中的 pack() 函数互逆
float unpack(vec4 rgbaDepth) {
    const vec4 bitShift = vec4(1.0, 1.0/256.0, 1.0/(256.0*256.0), 1.0/(256.0*256.0*256.0));
    return dot(rgbaDepth, bitShift);
}

// ============================================================
// 阴影采样核: Poisson 圆盘采样点 & Fibonacci 螺旋方向
// ============================================================

// PoissonDisk: 64 个预计算的泊松圆盘采样点, 用于 PCF/PCSS 软阴影
// 这些点在单位圆内均匀分布, 避免规则网格带来的带状伪影
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

// Fibonacci 螺旋方向向量: 64 个单位向量, 用于面积光 PCSS 的圆盘采样
// 螺旋分布提供比泊松盘更均匀的采样覆盖, 特别适合大面积半影区域
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

// ============================================================
// 阴影采样函数
// ============================================================

// 二维旋转变换: 用于对采样点施加随机旋转, 打破采样模式的固定方向
vec2 Rotate(vec2 pos, vec2 rotationTrig)
{
	return vec2(pos.x * rotationTrig.x - pos.y * rotationTrig.y, pos.y * rotationTrig.x + pos.x * rotationTrig.y);
}

// PCF (Percentage-Closer Filtering / 百分比近邻滤波)
// 原理: 在阴影贴图上以泊松盘分布采样多个点, 取平均可见度, 实现软阴影效果
// coords: 阴影贴图空间坐标 (xy=UV, z=当前深度)
// shadowMapIndex: 阴影贴图数组索引
// area: 面积光面积参数, 影响采样范围
// random: 随机旋转角度, 用于消除采样模式的固定伪影
float PCF(vec4 coords,int shadowMapIndex, float area, float random) {
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
        float res  = texture(shadowMaps, vec4(coords.xy + Rotate(poissonDisk[i * 64 / pc.pcf_sample_num] * Stride / shadowmapSize, rotationTrig), shadowMapIndex, coords.z)).r;
        visibility += res;
    }

    return visibility / float(pc.pcf_sample_num);
}

// PCSS 第一步: Blocker Search (遮挡物搜索)
// 在搜索区域内查找所有比当前片段更深的遮挡物, 返回平均遮挡深度和数量
// 返回值: vec2(平均遮挡深度, 遮挡物数量)
vec2 findBlocker(vec4 coords, int shadowMapIndex, float search_size, vec2 rotationTrig) {
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

// PCSS (Percentage-Closer Soft Shadows / 百分比近邻软阴影)
// 三阶段算法: 1) 搜索遮挡物 2) 估算半影大小 3) 按半影半径进行 PCF 滤波
// 距离遮挡物越远, 半影越大, 阴影越柔和
float PCSS(vec4 coords,int shadowMapIndex, float area, float random){
    float d_Receiver = 1 - coords.z;
	float rotationAngle = random * 3.1415926;
	vec2 rotationTrig = vec2(cos(rotationAngle), sin(rotationAngle));

    float searchSize = pc.softness * clamp(d_Receiver - 0.02, 0.0, 1.0) / d_Receiver;
    vec2 blockerInfo = findBlocker(coords,shadowMapIndex, searchSize, rotationTrig);
	if (blockerInfo.y < 1)
	{
		//There are no occluders so early out (this saves filtering)
		return 0.0;
	}
    float d_Blocker = blockerInfo.x;
    float w_penumbra = d_Receiver - d_Blocker;

    w_penumbra = 1.0 - pow(1.0 - w_penumbra, sqrt(area) * pc.softness_falloff);
    float filterRadiusUV = w_penumbra * pc.softness;

    float Stride = 20.;
    float shadowmapSize = 2048.;
    float visibility = 0.;
    float cur_depth = coords.z;

    //float ctrl = 1.0;
    //float bias = getBias(ctrl);

    for(int i = 0; i < pc.pcf_sample_num; i++){
        float res  = texture(shadowMaps, vec4(coords.xy + Rotate(poissonDisk[i * 64 / pc.pcf_sample_num] * filterRadiusUV, rotationTrig), shadowMapIndex, coords.z)).r;
        visibility += res;
    }

    return visibility / float(pc.pcf_sample_num);
}

// Fibonacci 螺旋圆盘采样 - 聚拢模式 (用于 blocker search)
// 采样点偏向圆盘中心, 中心区域采样密度更高, 适合精确搜索遮挡物
vec2 ComputeFibonacciSpiralDiskSampleClumped(const in int sampleIndex, const in float sampleCountInverse, out float sampleDistNorm)
{
    // Samples not biased away from the center - sample 0 at (0, 0) is important for blocker search near shadow contact points.
    sampleDistNorm = sampleIndex * sampleCountInverse;

    // Third power chosen arbitrarily - center area is floatly that much more important
    sampleDistNorm = sampleDistNorm * sampleDistNorm * sampleDistNorm;

    return fibonacciSpiralDirection[sampleIndex] * sampleDistNorm;
}

// Fibonacci 螺旋圆盘采样 - 均匀模式 (用于 PCSS 滤波阶段)
// 采样点均匀分布在整个圆盘上, 避免中心聚集导致边缘采样不足
vec2 ComputeFibonacciSpiralDiskSampleUniform(const in int sampleIndex, const in float sampleCountInverse, const in float sampleBias, out float sampleDistNorm)
{
    // Samples biased away from the center, so that sample 0 doesn't fall at (0, 0), or it will not be affected by sample jitter and create a visible edge.
    sampleDistNorm = sampleIndex * sampleCountInverse + sampleBias;

    // sqrt results in uniform distribution
    sampleDistNorm = sqrt(sampleDistNorm);

    return fibonacciSpiralDirection[sampleIndex] * sampleDistNorm;
}

// 计算面积光阴影的采样缩放偏移量
// 根据 z 距离将圆锥形采样区域映射到阴影贴图的 UV 空间
void FilterScaleOffset(vec3 coord, float maxSampleZDistance, out vec2 filterScalePos, out vec2 filterScaleNeg, out vec2 filterOffset)
{
    float d = maxSampleZDistance / coord.z;
    vec2 target = (coord.xy + 0.5) * 0.5;

    filterScalePos = (1 - target) * d;
    filterScaleNeg = target * d;
    filterOffset = (target - coord.xy) * d;
}

// 面积光 Blocker Search (遮挡物搜索)
// 使用 Fibonacci 螺旋聚拢采样, 在锥形区域内搜索最近的遮挡物
// 返回 true 表示找到了遮挡物, closestBlocker 返回最近遮挡深度
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

// 面积光 PCSS 滤波阶段
// 使用 Fibonacci 螺旋均匀采样, 在锥形滤波区域内计算平均可见度
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

// 交错梯度噪声 (Interleaved Gradient Noise)
// 来自 [Jimenez 2014] "Next Generation Post Processing in Call of Duty: Advanced Warfare"
// 用于每帧产生不同的随机旋转角度, 实现时域采样点抖动, 消除空间上的固定噪声模式
float InterleavedGradientNoise(vec2 pixCoord, uint frameCount)
{
    const vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
    vec2 frameMagicScale = vec2(2.083f, 4.867f);
    pixCoord += frameCount * frameMagicScale;
    return fract(magic.z * fract(dot(pixCoord, magic.xy)));
}

// 点光源半影大小估算: 半影宽度 = |接收者深度 - 遮挡物深度| / 遮挡物深度
float PenumbraSizePunctual(float Reciever, float Blocker)
{
    return abs((Reciever - Blocker) / Blocker);
}

// 方向光半影大小估算: 半影宽度 = |接收者深度 - 遮挡物深度| * 范围缩放因子
float PenumbraSizeDirectional(float Reciever, float Blocker, float rangeScale)
{
    return abs(Reciever - Blocker) * rangeScale;
}

// 面积光 PCSS 阴影采样入口 (改良版 PCSS)
// 改良要点: 在 blocker search 和 filter 阶段, 采样点沿锥形(z方向)偏移而非平面圆盘
// 锥体顶点在着色点, 底面在光源近平面, 只有锥体内的遮挡物才贡献阴影
// posTCShadowmap: 阴影贴图空间坐标
// posSS: 屏幕空间像素坐标 (用于噪声生成)
// shadowSoftness: 阴影柔和度控制
// minFilterRadius: 最小滤波半径
float SampleShadow_PCSS_Area(vec3 posTCShadowmap, vec2 posSS, float shadowSoftness, float minFilterRadius, int blockerSampleCount, int filterSampleCount, float depthBias, int shadowMapIndex)
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
    float maxSampleZDistance = shadowSoftness * 0.01;

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

// 值噪声生成器: 基于片段位置和帧号生成伪随机数
// 用于为每帧的阴影采样提供不同的随机旋转角度, 实现时域去噪
float ValueNoise(vec3 pos)
{
	vec3 Noise_skew = pos + 0.2127 + pos.x * pos.y * pos.z * 0.3713;
	vec3 Noise_rnd = 4.789 * sin(489.123 * (Noise_skew));
	return fract(Noise_rnd.x * Noise_rnd.y * Noise_rnd.z * (1.0 + Noise_skew.x) * pc.frame_num);
}


// ============================================================
// PBR 着色所需的核心数据结构
// ============================================================

// PBRInfo: PBR 光照计算输入参数的集合体
// 封装了所有 BRDF 计算所需的中间量 (各种点积、粗糙度、反射率等)
// 方便在不同的漫反射模型 (Lambert / Oren-Nayar / Burley / Disney) 之间切换
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


// ============================================================
// 颜色空间转换 & 数学工具函数
// ============================================================

// sRGB -> 线性空间转换 (gamma 2.2 解码)
// PBR 计算需要在线性颜色空间中进行
vec4 SRGBtoLINEAR(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
    return vec4(linOut,srgbIn.w);
}

// 线性 -> sRGB 空间转换 (gamma 2.2 编码)
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

// ============================================================
// 法线处理
// ============================================================

// 获取世界空间法线:
// 如果有法线贴图, 则从切线空间法线贴图采样并转换到世界空间
// 使用屏幕空间导数 (dFdx/dFdy) 实时计算 TBN 矩阵, 无需预计算切线
// 同时处理双面渲染 (gl_FrontFacing 为 false 时翻转法线)
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
// ============================================================
// 漫反射 BRDF 模型 (共5种, 可按需切换)
// ============================================================

// Lambert 漫反射: 最简单的均匀漫反射模型
// 公式: diffuseColor / PI
vec3 BRDF_Diffuse_Lambert(PBRInfo pbrInputs)
{
    return pbrInputs.diffuseColor * RECIPROCAL_PI;
}

// 自定义 Lambert 漫反射: 在标准 Lambert 基础上加入 NdotV 和粗糙度的能量补偿
vec3 BRDF_Diffuse_Custom_Lambert(PBRInfo pbrInputs)
{
    return pbrInputs.diffuseColor * RECIPROCAL_PI * pow(pbrInputs.NdotV, 0.5 + 0.3 * pbrInputs.perceptualRoughness);
}

// Oren-Nayar 漫反射模型 [Gotanda 2012]
// 考虑粗糙表面对光线的多次散射, 比 Lambert 更真实地表现粗糙材质
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

// Gotanda 漫反射模型 [Gotanda 2014]
// 针对游戏主机优化的物理漫反射, 综合考虑菲涅耳和几何遮蔽对漫反射的影响
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

// Burley (Disney) 漫反射模型
// 能量守恒的漫反射, 根据粗糙度调整光线散射/视角衰减
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

// Disney 漫反射模型
// 基于 [Burley 2012] 的漫反射近似, 当前 BRDF() 函数中实际使用此模型
vec3 BRDF_Diffuse_Disney(PBRInfo pbrInputs)
{
	float Fd90 = 0.5 + 2.0 * pbrInputs.perceptualRoughness * pbrInputs.VdotH * pbrInputs.VdotH;
    vec3 f0 = vec3(0.1);
	vec3 invF0 = vec3(1.0, 1.0, 1.0) - f0;
	float dim = min(invF0.r, min(invF0.g, invF0.b));
	float result = ((1.0 + (Fd90 - 1.0) * pow(1.0 - pbrInputs.NdotL, 5.0 )) * (1.0 + (Fd90 - 1.0) * pow(1.0 - pbrInputs.NdotV, 5.0 ))) * dim;
	return pbrInputs.diffuseColor * result;
}

// ============================================================
// 镜面反射 BRDF 组件: F (菲涅耳) + G (几何遮蔽) + D (微面元分布)
// Cook-Torrance 镜面反射模型的三个核心项
// ============================================================

// F: 菲涅耳反射项 (Fresnel Reflectance)
// 描述光线在不同入射角下的反射比例: 掠射角反射强, 垂直入射反射弱
// 使用 Schlick 近似的优化版本 (exp2 代替 pow, 性能更好)
vec3 specularReflection(PBRInfo pbrInputs)
{
    //return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance90*pbrInputs.reflectance0) * exp2((-5.55473 * pbrInputs.VdotH - 6.98316) * pbrInputs.VdotH);
}

// G: 几何遮蔽项 (Geometric Occlusion)
// 描述微面元之间的自遮挡: 粗糙表面的微面元互相遮挡更多, 反射回观察者的光更少
// 使用 Smith-Schlick-GGX 模型
float geometricOcclusion(PBRInfo pbrInputs)
{
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r + (1.0 - r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r + (1.0 - r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// D: 微面元法线分布项 (Microfacet Distribution / GGX/Trowbridge-Reitz)
// 描述表面微面元朝向的统计分布: 粗糙表面的微面元朝向分散, 光泽表面朝向集中
float microfacetDistribution(PBRInfo pbrInputs)
{
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (PI * f * f);
}

// ============================================================
// 完整 BRDF 计算: 组合漫反射 + 镜面反射 + 自发光
// ============================================================

// BRDF: 双向反射分布函数主入口
// 计算单个光源对表面的光照贡献: Disney漫反射 + Cook-Torrance镜面反射
// u_LightColor: 光源颜色 (已考虑衰减)
// v: 视线方向, n: 法线方向, l: 光线方向, h: 半程向量
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

// Specular-Glossiness 工作流 -> Metallic-Roughness 工作流的转换
// 根据漫反射和高光值反推金属度
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

// 镜面菲涅耳近似 (Schlick 优化版): 根据视角计算 F0 到 F90 的过渡
vec3 specularFresnel(vec3 f0, vec3 f90, float NdotV)
{
    //return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
    return f0 + (f90 - f90 * f0) * exp2((-5.55473 * NdotV - 6.98316) * NdotV);
}

// 带粗糙度的 Schlick 菲涅耳近似: 用于 IBL 环境光照
// 粗糙度会降低掠射角的菲涅耳反射强度
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// 预滤波环境反射采样: 根据粗糙度选择不同的 mipmap 层级
// 越粗糙的表面采样越低分辨率的 mipmap (更模糊的反射)
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

// Uncharted 2 色调映射 (Tone Mapping)
// 来自 [Hable 2010] Filmic Tonemapping, 将 HDR 颜色压缩到 LDR 显示范围
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


// ============================================================
// IBL (Image-Based Lighting / 基于图像的光照)
// ============================================================

// IBL: 使用预计算的环境贴图进行全局光照
// 漫反射: 从辐照度立方体贴图采样低频环境光 (samplerIrradiance)
// 镜面反射: 从预滤波环境贴图采样, 粗糙度决定 mipmap 层级 (samplerPrefilteredEnv)
// 菲涅耳项: 通过 BRDF LUT 查表获取能量补偿系数 (samplerBRDFLUT)
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

// F0 基础值转换: 将 IOR (折射率) 转换为垂直入射的菲涅耳反射率 F0
// 用于 Specular-Glossiness 工作流中折射率到反射率的映射
float computeF0Base_Merged(float f0) {
    float sqrtF0 = sqrt(f0);
    float numerator = 1.0 - 5.0 * sqrtF0;
    float denominator = 5.0 - sqrtF0;
    return (numerator * numerator) / (denominator * denominator);
}

// ============================================================
// 主函数 (Fragment Shader Entry Point)
// 流程: 材质采样 -> 光照计算(IBL + 直接光) -> 阴影计算 -> 时域滤波 -> MRT 输出
// ============================================================
void main()
{
    // ---- 高亮标记: 直接输出白色, 跳过后续 PBR 计算 ----
    if(highlight > 0){
        outColor = vec4(1, 1, 1, 1);
        return;
    }

    float brightnessCutoff = 0.001;

    // ============================================================
    // 第一阶段: 材质参数采样
    // 从贴图和材质因子中获取 baseColor, roughness, metallic 等参数
    // ============================================================
    float perceptualRoughness = 0.0;
    float metallic;
    vec3 diffuseColor;
    vec4 baseColor;

    float ambientOcclusion = 1.0;

    vec3 f0 = vec3(0.04);  // 非金属材质的基准反射率 (4%)

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

    // ---- Alpha 测试: 透明度低于阈值的片段直接丢弃 (镂空效果) ----
    if (materialArray.materials[materialIndex].alphaMask == 1.0f)
    {
        if (baseColor.a < materialArray.materials[materialIndex].alphaMaskCutoff)
            discard;
    }

    // ---- Specular-Glossiness 工作流 (可选) ----
    // 使用漫反射贴图 + 高光贴图, 需要转换为 Metallic-Roughness 参数
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
    // ---- Metallic-Roughness 工作流 (默认) ----
    // 直接使用金属度和粗糙度因子, 配合金属度-粗糙度贴图
        perceptualRoughness = materialArray.materials[materialIndex].roughnessFactor;
        metallic = materialArray.materials[materialIndex].metallicFactor;

    #ifdef VSG_METALLROUGHNESS_MAP
        vec4 mrSample = texture(mrMap, texCoord0);
        perceptualRoughness = mrSample.g * perceptualRoughness;
        metallic = mrSample.r * metallic;
    #endif
#endif

    // ---- AO 贴图采样 ----
#ifdef VSG_LIGHTMAP_MAP
    ambientOcclusion = texture(aoMap, texCoord0).r;
#endif

    // ---- 从 baseColor 和 metallic 推导漫反射/镜面反射颜色 ----
    // 金属材质的漫反射为零, 全部贡献为镜面反射; 非金属则相反
    diffuseColor = baseColor.rgb * (vec3(1.0) - f0);
    diffuseColor *= 1.0 - metallic;

    float alphaRoughness = perceptualRoughness * perceptualRoughness;  // 感知粗糙度 -> 物理粗糙度 (平方映射)

    vec3 specularColor = mix(f0, baseColor.rgb, metallic);  // 金属: 使用 baseColor 作为反射色; 非金属: 使用 f0

    // ---- 菲涅耳反射率参数计算 ----
    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    // ============================================================
    // 第二阶段: 光照计算
    // IBL 环境光照 + 直接光源光照 + 阴影
    // ============================================================

    vec3 worldN = getWorldNormal();                                  // 世界空间法线
    vec3 worldCamPos = pc.camera_pos;
    vec3 worldV = normalize(worldCamPos - worldViewDir);             // 视线方向 (从片段指向相机)

    vec3 color = vec3(0.0, 0.0, 0.0);
    vec4 lightNums = lightData.values[0];
    int numDirectionalLights = int(lightNums[1]);                    // 方向光数量
    int index = 1;

    // ---- IBL 环境光照 ----
    vec3 iblColor = IBL(worldV, worldN, perceptualRoughness, metallic, specularEnvironmentR0, specularEnvironmentR90, diffuseColor);
    color += iblColor * envmapData.param.a;

    // ---- 直接光源循环 + 阴影计算 ----
    // 遍历所有方向光, 计算每盏光的阴影可见度, 加权得到整体场景亮度
    float scene_brightness = 1.0f;
    if (numDirectionalLights>0){
        int shadowMapIndex = 0;
        float totalBrigtness = pc.baseBrightness;
        float totalfloatBrightness = pc.baseBrightness;
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
                        // shadow_type=0: PCF 软阴影 (快速, 质量一般)
                        visibility = PCF(sm_tc,shadowMapIndex,area, random);
                    }else if(pc.shadow_type == 1){
                        // shadow_type=1: 面积光 PCSS 软阴影 (更真实, 半影随距离变化)
                        // visibility = 1 - PCSS(sm_tc,shadowMapIndex, area, random);
                        visibility = SampleShadow_PCSS_Area(sm_tc.xyz, vec2(gl_FragCoord.xy), pc.softness, pc.softness_falloff, pc.blocker_sample_num, pc.pcf_sample_num, pc.shadow_bias, shadowMapIndex);
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
    // ============================================================
    // 第三阶段: 时域阴影滤波 (Temporal Filtering)
    // 将当前帧的阴影值与上一帧的历史值混合, 减少阴影闪烁
    // ============================================================

    // 将上一帧的世界坐标投影到屏幕空间, 查找历史阴影值
    vec4 last_ndc = pc.projection * pc.last_view * vec4(lastWorldPos, 1);
    ivec2 last_coord = ivec2(((last_ndc.x / last_ndc.w) / 2 + 0.5) * constantBuffer.width, ((last_ndc.y / last_ndc.w) / 2 + 0.5) * constantBuffer.height);
    float old_shadow = 1;
    float oldInstanceID = -1;
    if(last_coord.x >= 0 && last_coord.y >= 0 && last_coord.x < constantBuffer.width && last_coord.y < constantBuffer.height){
        vec2 shadowdataold_shadow = texelFetch(shadowInputAttachment, last_coord, gl_SampleID).rg;
        oldInstanceID = shadowdataold_shadow.y;
        old_shadow = shadowdataold_shadow.x;
    }
    else{
        old_shadow = scene_brightness;
    }

    // ---- 时域混合 (Temporal Blend) ----
    // 仅在实例ID匹配 且 阴影值差异不大时进行混合, 避免鬼影
    // 使用自适应反馈: 差异越大, 越信任当前帧 (feedback 越小)
    float current_shadow_value = scene_brightness;
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

    // ============================================================
    // 第四阶段: MRT 输出
    // 写入四个渲染目标附件, 供后续 Pass (后处理、合成) 使用
    // ============================================================

    outColor = vec4(color * scene_brightness, baseColor.w);     // 最终颜色 = PBR颜色 * 阴影亮度

    // 透明度 > 0.8 视为不透明物体, 写入完整的法线和位置
    // 透明度 <= 0.8 视为半透明, alpha 通道设为标记值, 供合成阶段区分
    if(baseColor.w > 0.8){
        outNormal = vec4(worldN, 1);
        outWorldPos = vec4(worldViewDir, 1);
    }
    else{
        outNormal = vec4(worldN, 0);
        outWorldPos = vec4(worldViewDir, 0);
    }

    // outShadow: R=阴影亮度, G=实例ID(时域滤波用), B=线性深度(1-gl_FragCoord.z), A=1
    outShadow = vec4(scene_brightness, InstanceID, (1 - gl_FragCoord.z), 1);
}
