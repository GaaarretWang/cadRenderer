// ============================================================
// line.vert — 线段渲染顶点着色器
// 渲染管线中的作用：将线段图元的顶点变换到裁剪空间，输出虚实融合所需的中间变量。
// 支持置换贴图（Displacement Map）和 Billboard 模式。
// ============================================================

#version 450
#extension GL_ARB_separate_shader_objects : enable

#pragma import_defines (VSG_INSTANCE_POSITIONS, VSG_BILLBOARD, VSG_DISPLACEMENT_MAP)

// Push Constants（推送常量）：从 CPU 传入的高频渲染参数
layout(push_constant) uniform PushConstants {
    mat4 projection;        // 投影矩阵
    mat4 view;              // 当前帧视图矩阵
    mat4 last_view;         // 上一帧视图矩阵
    vec3 camera_pos;        // 相机世界坐标
    float softness;         // 阴影柔和度
    float baseBrightness;   // 基础亮度
    float ssao_radius;      // SSAO 采样半径
    float exposure;         // 曝光度
    float softness_falloff; // 阴影柔和度衰减
    float shadow_bias;      // 阴影偏移
    int ssao_kernel_size;   // SSAO 核大小
    int denoise_size;       // 降噪大小
    int blocker_sample_num; // PCSS 遮挡搜索采样数
    int pcf_sample_num;     // PCF 采样数
    int shadow_type;        // 阴影算法类型
    uint frame_num;         // 帧编号
} pc;

// 置换贴图：根据高度图偏移顶点，增加线段几何的表面细节
#ifdef VSG_DISPLACEMENT_MAP
layout(binding = 6) uniform sampler2D displacementMap;
#endif

// 顶点属性输入
layout(location = 0) in vec3 vsg_Vertex;       // 顶点位置（模型空间）
layout(location = 1) in vec3 vsg_Normal;        // 顶点法线
layout(location = 2) in vec2 vsg_TexCoord0;     // 纹理坐标
layout(location = 3) in vec4 vsg_Color;         // 顶点颜色

// 位置偏移输入（Billboard 或实例化模式，二选一）
#ifdef VSG_BILLBOARD
layout(location = 4) in vec4 vsg_position_scaleDistance;  // Billboard: xyz=中心, w=缩放距离
#elif defined(VSG_INSTANCE_POSITIONS)
layout(location = 4) in vec3 vsg_position;                // 实例化: xyz=偏移位置
#endif

// 输出到片段着色器
layout(location = 0) out vec3 eyePos;        // 眼空间位置
layout(location = 1) out vec3 normalDir;      // 眼空间法线
layout(location = 2) out vec4 vertexColor;    // 顶点颜色
layout(location = 3) out vec2 texCoord0;      // 纹理坐标

layout(location = 5) out vec3 viewDir;        // 视线方向

out gl_PerVertex {
    vec4 gl_Position;
};

// Billboard 矩阵计算：使面片始终面向相机
// 构建平移+缩放矩阵，去除旋转分量，实现"广告牌"效果
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
    vec4 normal = vec4(vsg_Normal, 0.0);  // w=0 表示方向向量，不受平移影响

    // --- 置换贴图处理 ---
    // 沿法线方向偏移顶点，并通过有限差分重新计算法线
#ifdef VSG_DISPLACEMENT_MAP
    // TODO need to pass as as uniform or per instance attributes
    vec3 scale = vec3(1.0, 1.0, 1.0);

    // 沿法线方向偏移顶点位置
    vertex.xyz = vertex.xyz + vsg_Normal * (texture(displacementMap, vsg_TexCoord0.st).s * scale.z);

    float s_delta = 0.01;
    float width = 0.0;

    // 水平方向（s轴）的有限差分，用于计算法线
    float s_left = max(vsg_TexCoord0.s - s_delta, 0.0);
    float s_right = min(vsg_TexCoord0.s + s_delta, 1.0);
    float t_center = vsg_TexCoord0.t;
    float delta_left_right = (s_right - s_left) * scale.x;
    // s 轴方向的高度差
    float dz_left_right = (texture(displacementMap, vec2(s_right, t_center)).s - texture(displacementMap, vec2(s_left, t_center)).s) * scale.z;

    // 垂直方向（t轴）的有限差分
    // TODO need to handle different origins of displacementMap vs diffuseMap etc,
    float t_delta = s_delta;
    float t_bottom = max(vsg_TexCoord0.t - t_delta, 0.0);
    float t_top = min(vsg_TexCoord0.t + t_delta, 1.0);
    float s_center = vsg_TexCoord0.s;
    float delta_bottom_top = (t_top - t_bottom) * scale.y;
    // t 轴方向的高度差
    float dz_bottom_top = (texture(displacementMap, vec2(s_center, t_top)).s - texture(displacementMap, vec2(s_center, t_bottom)).s) * scale.z;

    // 通过叉积重建法线：dx × dy = 新法线方向
    vec3 dx = normalize(vec3(delta_left_right, 0.0, dz_left_right));  // s 轴切线
    vec3 dy = normalize(vec3(0.0, delta_bottom_top, -dz_bottom_top)); // t 轴切线
    vec3 dz = normalize(cross(dx, dy));                               // 重建法线

    // 将新法线混合到原始法线方向
    normal.xyz = normalize(dx * vsg_Normal.x + dy * vsg_Normal.y + dz * vsg_Normal.z);
#endif

    // 实例化模式：将顶点偏移到实例位置
#ifdef VSG_INSTANCE_POSITIONS
    vertex.xyz = vertex.xyz + vsg_position;
#endif

    // 选择视图矩阵：Billboard 模式使用特殊矩阵，否则使用标准视图矩阵
#ifdef VSG_BILLBOARD
    mat4 mv = computeBillboadMatrix(pc.view * vec4(vsg_position_scaleDistance.xyz, 1.0), vsg_position_scaleDistance.w);
#else
    mat4 mv = pc.view;
#endif

    // 最终裁剪空间变换
    gl_Position = (pc.projection * mv) * vertex;
    eyePos = (mv * vertex).xyz;         // 眼空间坐标
    viewDir = - (mv * vertex).xyz;      // 视线方向
    normalDir = (mv * normal).xyz;      // 眼空间法线

    vertexColor = vsg_Color;
    texCoord0 = vsg_TexCoord0;
}
