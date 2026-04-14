// ============================================================================
// Equirectangular to Cubemap Fragment Shader（等距矩形转Cubemap片元着色器）
// ============================================================================
// 作用：将等距矩形环境贴图（Equirectangular Map，即HDR全景图）
//       转换为Cubemap格式。
//
// 等距矩形贴图：用2D纹理存储360度全景，UV坐标对应球面经纬度
//   - u: 经度(phi)，范围[0,1]对应[-PI, PI]
//   - v: 纬度(theta)，范围[0,1]对应[0, PI]
//
// Cubemap：用6张正方形纹理存储立方体6个面的投影
//   更适合实时渲染中的环境采样（无需atan/acos计算）
//
// 流水线角色：IBL预处理阶段的第一步。
//             渲染6次（每个face一次），将全景图烘焙到cubemap。
// ============================================================================

#version 450

layout (location = 0) in vec2 inUV;      // 全屏四边形UV坐标
layout (location = 0) out vec4 outColor;  // 输出颜色

// 输入的等距矩形环境贴图（Equirectangular HDR Texture）
layout (set=0, binding = 0) uniform sampler2D samplerEnv;

// Push Constants：当前渲染的cubemap face索引（0-5）
layout(push_constant) uniform PushConstants {
    layout (offset = 128) uint faceIdx; // Cubemap face索引（0=x, 1=-x, 2=y, 3=-y, 4=z, 5=-z）
} pc;

#define PI 3.1415926535897932384626433832795

// 根据UV和cubemap face索引，计算3D采样方向
// 每个face对应立方体的一个面，需要将2D UV映射到对应的3D方向
vec3 getSamplingVector(vec2 st)
{
    // [0,1] to [-1,1]：将UV从[0,1]范围转换到[-1,1]范围
    vec2 uv = 2.0 * vec2(st.x, st.y) - 1.0;

    //1.把这个和skybox对齐
    //2.和python脚本中的旋转对齐（theta和phi设定旋转角度，之后再看）

    vec3 ret;
	// 根据face索引确定3D采样方向
    // 每个face的坐标轴映射不同，固定轴为+/-1，其余两个轴由uv控制
    // 注意：这里的轴顺序和方向需要与skybox的cubemap采样约定一致
    if(pc.faceIdx == 0)      ret = vec3(1.0, -uv.y,  -uv.x);   // +x面
    else if(pc.faceIdx == 1) ret = vec3(-1.0,  -uv.y, uv.x);   // -x面
    else if(pc.faceIdx == 2) ret = vec3(uv.x, 1.0, uv.y);      // +y面
    else if(pc.faceIdx == 3) ret = vec3(uv.x, -1.0, -uv.y);    // -y面
    else if(pc.faceIdx == 4) ret = vec3(uv.x, -uv.y, 1.0);     // +z面
    else if(pc.faceIdx == 5) ret = vec3(-uv.x, -uv.y, -1.0);   // -z面

    return normalize(ret);
}

void main()
{
	vec3 v = getSamplingVector(inUV); // 获取3D采样方向
	const float TWO_PI = PI * 2.0;

    // 将3D方向转换为球面坐标（Spherical Coordinates）
    // phi: 方位角（azimuth），atan(x,y)给出[-PI, PI]范围
    // theta: 极角（polar angle），acos(z)给出[0, PI]范围
    float phi   = atan(v.x, v.y);
	float theta = acos(v.z);

    // 将球面坐标映射到等距矩形贴图的UV坐标
    // -phi/TWO_PI + 0.5: 方位角归一化到[0,1]
    // theta/PI: 极角归一化到[0,1]
    vec2 equiRectUV = vec2(-phi/TWO_PI + 0.5, theta/PI);

    // 从等距矩形贴图中采样颜色
    vec3 color = texture(samplerEnv, equiRectUV).rgb;

    outColor = vec4(color, 1.0);
}
