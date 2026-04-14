// ============================================================================
// PBR Standard Vertex Shader（PBR标准顶点着色器）
// ============================================================================
// 作用：处理PBR材质对象的顶点变换，输出世界空间/眼空间位置、法线、
//       纹理坐标等信息给PBR fragment shader使用。
// 流水线角色：这是PBR渲染管线的顶点阶段，接收CPU端的顶点属性和
//             实例化数据，完成MVP变换后将插值数据传递给片元着色器。
// ============================================================================

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#pragma import_defines (VSG_INSTANCE_POSITIONS, VSG_BILLBOARD, VSG_DISPLACEMENT_MAP)

// Push Constants（推送常量）：CPU端每帧更新的渲染参数
// 注意：push constant在Vulkan中大小有限（通常128-256字节），
//       适合传递每帧变化的少量数据
layout(push_constant) uniform PushConstants {
    mat4 projection;       // 投影矩阵（Projection Matrix）
    mat4 view;             // 视图矩阵（View Matrix，当前帧相机）
    mat4 last_view;        // 上一帧视图矩阵（用于Motion Vector / TAA）
    vec3 camera_pos;       // 相机世界坐标（World-space camera position）
    float softness;        // 软阴影柔度参数
    float baseBrightness;  // 基础亮度（Base brightness）
    float ssao_radius;     // SSAO采样半径
    float exposure;        // 曝光度（Exposure）
    float softness_falloff; // 软阴影衰减系数
    float shadow_bias;     // 阴影偏移（Shadow bias，防止阴影痤疮）
    int ssao_kernel_size;  // SSAO内核采样点数量
    int denoise_size;      // SSAO去噪核大小
    int blocker_sample_num; // PCSS阴影：blocker搜索采样数
    int pcf_sample_num;    // PCF阴影：滤波采样数
    int shadow_type;       // 阴影类型选择
    uint frame_num;        // 当前帧号（用于TAA抖动）
} pc;

// 置换贴图（Displacement Map）：可选，用于顶点位移
#ifdef VSG_DISPLACEMENT_MAP
layout(binding = 6) uniform sampler2D displacementMap;
#endif

// 实例化数据结构体（Instance Data）
// 每个绘制实例拥有独立的模型矩阵和上一帧模型矩阵
struct InstanceData {
    mat4 modelMatrix;       // 当前帧模型矩阵（Model Matrix）
    mat4 lastModelMatrix;   // 上一帧模型矩阵（用于Motion Vector计算）
    vec4 highlight[10];     // 高亮颜色（最多10个高亮通道）
};

// Material Descriptor Set（材质描述符集，set = 2）
// Vulkan的描述符集用于将GPU资源（缓冲、纹理等）绑定到shader
// set=2 通常专门用于材质相关的资源
#define MATERIAL_DESCRIPTOR_SET 2

// 实例化矩阵SSBO（Shader Storage Buffer Object）
// SSBO相比UBO可以存储更大的数据量，适合大量实例化绘制
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 11) buffer InstanceMatrices {
    InstanceData instanceModelMatrix[];
}instanceMatrices;

// ===== 顶点输入属性（Vertex Input Attributes）=====
// location对应Vulkan管线中VertexInputBinding的定义
layout(location = 0) in vec3 vsg_Vertex;    // 顶点位置（Object-space position）
layout(location = 1) in vec3 vsg_Normal;    // 顶点法线（Object-space normal）
layout(location = 2) in vec2 vsg_TexCoord0; // UV纹理坐标（Texture coordinate）
layout(location = 3) in vec4 vsg_Color;     // 顶点颜色（Vertex color）
layout(location = 4) in vec4 vsg_InstanceID;// 实例ID（x=实例ID, y=材质索引）

// Billboard和实例化位置的可选输入（条件编译）
#ifdef VSG_BILLBOARD
layout(location = 4) in vec4 vsg_position_scaleDistance;
#elif defined(VSG_INSTANCE_POSITIONS)
layout(location = 4) in vec3 vsg_position;
#endif

// ===== 顶点着色器输出（Varying outputs）=====
// 这些变量会被GPU自动插值后传给fragment shader
layout(location = 0) out vec3 eyePos;       // 眼空间位置（Eye/View-space position）
layout(location = 1) out vec3 normalDir;    // 眼空间法线（Eye-space normal）
layout(location = 2) out vec4 vertexColor;  // 顶点颜色
layout(location = 3) out vec2 texCoord0;    // UV坐标
layout(location = 4) out float highlight;   // 高亮系数
layout(location = 5) out float InstanceID;  // 实例ID（用于拾取等）
layout(location = 6) out vec3 worldNormal;  // 世界空间法线（World-space normal）
layout(location = 7) out vec3 worldViewDir; // 世界空间视线方向（World-space view direction）
layout(location = 8) out vec3 lastWorldPos; // 上一帧世界空间位置（用于Motion Vector）
layout(location = 9) out flat uint materialIndex; // 材质索引（flat表示不插值）

// View Descriptor Set（视图描述符集，set = 1）
// 存储光照数据等全局共享的渲染参数
#define VIEW_DESCRIPTOR_SET 1
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[2048]; // 光源数据数组（每盏灯4个vec4）
} lightData;

// 必须显式声明gl_PerVertex输出（Vulkan要求）
out gl_PerVertex{ vec4 gl_Position; };

// Billboard矩阵计算（可选编译）
// Billboard：始终面向相机的精灵/广告牌效果
// autoScaleDistance控制随距离自动缩放的阈值
#ifdef VSG_BILLBOARD
mat4 computeBillboadMatrix(vec4 center_eye, float autoScaleDistance)
{
    float distance = -center_eye.z;

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
    vec4 instanceIDVec = vsg_InstanceID;
    InstanceID = instanceIDVec.x;              // 保留给TAA等用途
    materialIndex = uint(instanceIDVec.y);      // 从.y分量提取材质索引

    // 从SSBO中获取当前实例的模型矩阵（gl_InstanceIndex由Vulkan实例化绘制自动设置）
    InstanceData instanceModelMatrixi = instanceMatrices.instanceModelMatrix[gl_InstanceIndex];
    mat4 model = instanceModelMatrixi.modelMatrix;
    mat4 lastModel = instanceModelMatrixi.lastModelMatrix;

    // 顶点变换流程：Object Space -> World Space -> View Space -> Clip Space
    // model * vertex：将顶点从模型空间变换到世界空间
    vec4 modelVertex = model * vertex;
    vec4 lastVertex = lastModel * vertex;

    // view * modelVertex：将顶点从世界空间变换到眼空间（View Space）
    vec4 viewVertex = pc.view * modelVertex;
    vec4 normal = vec4(vsg_Normal, 0.0);

    // projection * viewVertex：将顶点从眼空间变换到裁剪空间（Clip Space）
    // 这是最终输出给GPU光栅化的坐标
    gl_Position = pc.projection * viewVertex;

    // 传递世界空间数据给fragment shader
    worldViewDir = (modelVertex).xyz;          // 世界空间视线方向
    lastWorldPos = lastVertex.xyz;             // 上一帧世界位置（Motion Vector用）
    eyePos = viewVertex.xyz;                   // 眼空间位置

    // 法线变换：只用mat3（3x3），忽略平移分量
    // 世界空间法线 = model矩阵的3x3部分 * 对象空间法线
    worldNormal = mat3(model) * normal.xyz;
    // 眼空间法线 = view矩阵的3x3部分 * 世界空间法线
    normalDir = mat3(pc.view) * worldNormal;

    vertexColor = vsg_Color;
    InstanceID = vsg_InstanceID.x;
    texCoord0 = vsg_TexCoord0;
    highlight = instanceModelMatrixi.highlight[0].x;
}
