// ============================================================================
// Cubemap Generation Vertex Shader（Cubemap生成顶点着色器）
// ============================================================================
// 作用：用于将等距矩形贴图（Equirectangular Map）转换为Cubemap时的
//       顶点处理。将立方体顶点位置直接作为纹理坐标传递给fragment shader，
//       由fragment shader负责将3D方向转换为等距矩形UV坐标。
// 流水线角色：IBL预处理阶段 - 环境贴图转换的第一步。
//             渲染6个cubemap face时，每个face使用不同的view矩阵。
// ============================================================================

#version 450

layout (location = 0) in vec3 inPos; // 立方体顶点位置（Cube vertex position）

// Push Constants：投影矩阵和视图矩阵
// 6个cubemap face各使用不同的view矩阵，分别朝向+x/-x/+y/-y/+z/-z
layout(push_constant) uniform PushConsts {
	mat4 proj;   // 投影矩阵（通常90度FOV的透视投影）
	mat4 view;   // 视图矩阵（朝向当前cubemap face的方向）
} pc;

layout (location = 0) out vec3 outUVW; // 输出3D方向（传给fragment shader采样等距矩形贴图）

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	mat4 _proj = pc.proj;
	vec3 _inPos = inPos;
	// vsg::perspective flips y, hack projection matrix here
	// VSG引擎的perspective函数翻转了y轴，这里取反proj[1][1]进行补偿
	_proj[1][1] = -_proj[1][1];
	outUVW = _inPos.xyz; // 立方体顶点位置即为采样方向
	gl_Position = _proj * pc.view * vec4(_inPos.xyz, 1.0);
}