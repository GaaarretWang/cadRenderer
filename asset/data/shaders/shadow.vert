// ============================================================
// shadow.vert — 阴影贴图(Shadow Map) 顶点着色器
// 渲染管线中的作用：从光源视角将场景几何体的深度信息渲染到阴影贴图中。
// 后续主渲染 pass 中，片段着色器会采样这张贴图来判断当前片段是否处于阴影中。
// ============================================================

#version 450
#extension GL_ARB_separate_shader_objects : enable

#pragma import_defines (VSG_INSTANCE_POSITIONS, VSG_BILLBOARD, VSG_DISPLACEMENT_MAP)

// Push Constants（推送常量）：每帧从 CPU 传入的少量高频更新数据
// Vulkan 限制总大小为 128 字节，速度比 uniform buffer 快
layout(push_constant) uniform PushConstants {
    mat4 projection;        // 投影矩阵（Projection Matrix）
    mat4 view;              // 视图矩阵（View Matrix），当前帧相机变换
    mat4 last_view;         // 上一帧的视图矩阵，用于运动模糊等时域处理
    vec3 camera_pos;        // 相机世界坐标位置
    float softness;         // 阴影柔和度参数
    float baseBrightness;   // 基础亮度
    float ssao_radius;      // SSAO（屏幕空间环境光遮蔽）采样半径
    float exposure;         // 曝光度
    float softness_falloff; // 阴影柔和度衰减
    float shadow_bias;      // 阴影偏移量，用于消除阴影痤疮(Shadow Acne)
    int ssao_kernel_size;   // SSAO 采样核大小
    int denoise_size;       // 降噪滤波大小
    int blocker_sample_num; // PCSS 遮挡物搜索的采样点数
    int pcf_sample_num;     // PCF（百分比渐近过滤）采样点数
    int shadow_type;        // 阴影算法类型（0=PCF, 1=PCSS）
    uint frame_num;         // 当前帧编号，用于时域抖动
} pc;

// 置换贴图（Displacement Map）：根据灰度值沿法线方向偏移顶点位置
#ifdef VSG_DISPLACEMENT_MAP
layout(binding = 6) uniform sampler2D displacementMap;
#endif

// 实例数据结构体（Instance Data）
// 每个绘制实例（如重复的零件）拥有独立的模型矩阵，实现 GPU 端实例化渲染
struct InstanceData {
    mat4 modelMatrix;       // 当前帧的模型矩阵（Model Matrix）
    mat4 lastModelMatrix;   // 上一帧的模型矩阵，用于计算运动向量
    vec4 highlight[10];     // 高亮颜色数组（最多 10 个高亮层）
};

// Material descriptor set (set=2)，binding 11：实例化矩阵的 SSBO（Shader Storage Buffer Object）
// SSBO 比 UBO 容量更大，支持动态数组长度，适合存储大量实例数据
#define MATERIAL_DESCRIPTOR_SET 2
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 11) buffer InstanceMatrices {
    InstanceData instanceModelMatrix[];
}instanceMatrices;

// 顶点属性输入（Vertex Attribute Inputs）
layout(location = 0) in vec3 vsg_Vertex;       // 顶点位置（模型空间）
layout(location = 1) in vec3 vsg_Normal;        // 顶点法线
layout(location = 2) in vec2 vsg_TexCoord0;     // 纹理坐标（UV）
layout(location = 3) in vec4 vsg_Color;         // 顶点颜色
layout(location = 4) in vec4 vsg_InstanceID;    // 实例 ID（x 分量使用）

// location = 4 根据编译宏选择不同输入模式
#ifdef VSG_BILLBOARD
layout(location = 4) in vec4 vsg_position_scaleDistance;  // Billboard: xyz=中心位置, w=自动缩放距离
#elif defined(VSG_INSTANCE_POSITIONS)
layout(location = 4) in vec3 vsg_position;                // 实例化: xyz=偏移位置
#endif

// 输出到片段着色器的变量（Varying）
layout(location = 0) out vec3 eyePos;        // 眼空间（View Space）位置
layout(location = 1) out vec3 normalDir;      // 眼空间法线方向
layout(location = 2) out vec4 vertexColor;    // 顶点颜色
layout(location = 3) out vec2 texCoord0;      // 纹理坐标
layout(location = 4) out vec3 worldViewDir;   // 世界空间位置（用于阴影贴图投影变换）

layout(location = 5) out vec3 viewDir;        // 视线方向（从顶点指向相机）
layout(location = 6) out float InstanceID;    // 实例 ID（用于阴影时域累积时匹配同一实例）
layout(location = 8) out vec3 lastWorldPos;   // 上一帧的世界空间位置（用于时域重投影）

out gl_PerVertex{ vec4 gl_Position; };

// Billboard 矩阵计算函数
// 原理：Billboard 始终面向相机。构建一个"先缩放再平移"的矩阵，
// 去除旋转分量，使面片永远朝向相机。
// 距离小于阈值时按比例缩放，超过阈值保持不变（实现近大远小效果）。
#ifdef VSG_BILLBOARD
mat4 computeBillboadMatrix(vec4 center_eye, float autoScaleDistance)
{
    float distance = -center_eye.z;  // 眼空间 z 为负值表示在相机前方

    // 距离小于自动缩放阈值时线性缩放
    float scale = (distance < autoScaleDistance) ? distance/autoScaleDistance : 1.0;
    mat4 S = mat4(scale, 0.0, 0.0, 0.0,
                  0.0, scale, 0.0, 0.0,
                  0.0, 0.0, scale, 0.0,
                  0.0, 0.0, 0.0, 1.0);

    mat4 T = mat4(1.0, 0.0, 0.0, 0.0,
                  0.0, 1.0, 0.0, 0.0,
                  0.0, 0.0, 1.0, 0.0,
                  center_eye.x, center_eye.y, center_eye.z, 1.0);
    return T*S;
}
#endif

void main()
{
    vec4 vertex = vec4(vsg_Vertex, 1.0);

    // 从 SSBO 中获取当前实例的模型矩阵（gl_InstanceIndex 是 Vulkan 内置的实例索引）
    InstanceData instanceModelMatrixi = instanceMatrices.instanceModelMatrix[gl_InstanceIndex];
    mat4 model = instanceModelMatrixi.modelMatrix;         // 当前帧模型矩阵
    mat4 lastModel = instanceModelMatrixi.lastModelMatrix; // 上一帧模型矩阵

    // 顶点变换：从模型空间 → 世界空间
    vertex = model * vertex;
    vec4 lastVertex = lastModel * vec4(vsg_Vertex, 1.0);   // 上一帧世界空间位置
    vec4 normal = vec4(vsg_Normal, 0.0);                    // 法线向量（w=0 表示方向向量，不受平移影响）
    mat4 mv = pc.view;                                      // 视图矩阵（从 push constant 获取）

    // 裁剪空间位置 = 投影矩阵 × 视图矩阵 × 世界空间顶点
    gl_Position = (pc.projection * mv) * vertex;
    eyePos = (mv * vertex).xyz;         // 眼空间坐标
    viewDir = - (mv * vertex).xyz;      // 视线方向
    normalDir = (mv * normal).xyz;      // 眼空间法线
    vertexColor = vsg_Color;
    InstanceID = vsg_InstanceID.x;      // 实例 ID
    texCoord0 = vsg_TexCoord0;
    worldViewDir = (vertex).xyz;        // 世界空间坐标（shadow map 采样时需要）
    lastWorldPos = lastVertex.xyz;      // 上一帧世界空间坐标
}
