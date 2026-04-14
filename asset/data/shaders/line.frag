// ============================================================
// line.frag — 线段渲染片段着色器
// 渲染管线中的作用：为线段图元输出颜色，支持虚实融合深度检测。
// 当虚拟线段被真实物体遮挡时，显示相机画面而非线段颜色。
// ============================================================

#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_METALLROUGHNESS_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, VSG_WORKFLOW_SPECGLOSS, SHADOWMAP_DEBUG)

// 描述符集编号
#define VIEW_DESCRIPTOR_SET 1        // 视图相关数据
#define MATERIAL_DESCRIPTOR_SET 2    // 材质相关数据

const float PI = 3.14159265359;
const float RECIPROCAL_PI = 0.31830988618;
const float RECIPROCAL_PI2 = 0.15915494;
const float EPSILON = 1e-6;
const float c_MinRoughness = 0.04;

// --- 材质贴图声明（在此线段着色器中通常不使用，由引擎统一布局绑定）---

#ifdef VSG_DIFFUSE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D diffuseMap;       // 漫反射贴图
#endif
#ifdef VSG_METALLROUGHNESS_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D mrMap;            // 金属粗糙度贴图
#endif
#ifdef VSG_NORMAL_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D normalMap;        // 法线贴图
#endif
#ifdef VSG_LIGHTMAP_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D aoMap;            // AO 贴图
#endif
#ifdef VSG_EMISSIVE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 4) uniform sampler2D emissiveMap;      // 自发光贴图
#endif
#ifdef VSG_SPECULAR_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 5) uniform sampler2D specularMap;      // 高光贴图
#endif

// 虚实融合相关纹理
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 7) uniform sampler2D cameraImage;  // 相机画面纹理
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 8) uniform sampler2D depthImage;   // 相机深度图
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


// PBR 材质参数（线段着色器中通常不使用）
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

// 常量缓冲区（SSBO）：运行时可变的渲染参数
layout(std430, set = MATERIAL_DESCRIPTOR_SET, binding = 12) buffer ConstantBuffer {
    float z_far;
    int shader_type;
    int width;
    int height;
}constantBuffer;


// ViewDependentState
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[2048];
} lightData;

// 阴影贴图数组
layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps;

// 从顶点着色器接收的插值变量
layout(location = 0) in vec3 eyePos;      // 眼空间位置
layout(location = 1) in vec3 normalDir;    // 眼空间法线
layout(location = 2) in vec4 vertexColor;  // 顶点颜色
layout(location = 3) in vec2 texCoord0;    // 纹理坐标
layout(location = 5) in vec3 viewDir;      // 视线方向

layout(location = 0) out vec4 outColor;    // 最终输出颜色

void main()
{
    // 计算屏幕 UV 坐标（归一化 [0,1]）
    vec2 screen_uv = vec2(gl_FragCoord.x / constantBuffer.width, gl_FragCoord.y / constantBuffer.height);

    // 默认输出顶点颜色（线段颜色）
    outColor = vertexColor;

    if(constantBuffer.shader_type == 0){
        // 纯虚拟渲染模式：不做深度比较，直接返回顶点颜色
        return;
    }else{
        // 虚实融合模式：比较虚拟线段与真实场景的深度
        float cadDepth = -eyePos.z / constantBuffer.z_far;           // 虚拟线段归一化深度
        float cameraDepth = texture(depthImage, screen_uv).r;        // 真实场景深度
        if(cadDepth > cameraDepth){
            // 虚拟线段在真实物体后面 → 被遮挡 → 显示相机画面
            outColor = texture(cameraImage, screen_uv);
        }
        return;
    }
}
