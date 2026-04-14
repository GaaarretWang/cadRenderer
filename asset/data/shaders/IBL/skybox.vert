// ============================================================================
// Skybox Vertex Shader（天空盒顶点着色器）
// ============================================================================
// 作用：渲染天空盒立方体，将立方体顶点位置作为3D纹理坐标（UVW）
//       传递给fragment shader，用于采样cubemap环境贴图。
// 流水线角色：天空盒渲染的顶点阶段。天空盒始终跟随相机移动，
//             需要特殊处理让相机永远在天空盒"中心"。
// ============================================================================

#version 450

layout (location = 0) in vec3 inPos; // 立方体顶点位置（Cube vertex position）

// Push Constants：投影矩阵和视图矩阵
layout(push_constant) uniform VertPushConsts {
	mat4 proj;   // 投影矩阵（Projection Matrix）
	mat4 view;   // 视图矩阵（View Matrix）
} pc;

layout (location = 0) out vec3 outUVW; // 输出3D纹理坐标（Cubemap采样方向）

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	mat4 _proj = pc.proj;
	//outUVW = vec3(inPos.x, -inPos.z, inPos.y);
	outUVW = normalize(vec3(inPos.x, inPos.y, inPos.z)); // 立方体顶点方向即为cubemap采样方向

	// ===== 天空盒特殊处理 =====
	// 目标：让天空盒始终包围相机，永远渲染在最远处（深度=1.0）
	//
	// 方法1：清除view矩阵中的平移分量（让天空盒不随相机移动）
	//   view[3][0/1/2] = 0 消除平移，这样天空盒中心始终在世界原点
	//
	// 方法2：清除projection矩阵中的深度映射
	//   proj[2][2] = 0, proj[3][2] = 0 让所有顶点深度固定为1.0（最远）
	//   这样天空盒永远在所有几何体之后渲染
	//
	// 注意：这里两种方法都做了（proj和view都被修改），
	//       虽然通常只用一种就够，但同时使用也能正常工作
	mat4 proj = pc.proj;
	proj[2][2] = 0;  // 清除深度映射的z分量
	proj[3][2] = 0;  // 清除深度映射的w分量
	mat4 view = pc.view;
	view[3][0] = 0;  // 清除x方向平移
	view[3][1] = 0;  // 清除y方向平移
	view[3][2] = 0;  // 清除z方向平移

	// 注意：这里传入的是pc.proj和pc.view（未修改的），而上面修改了局部变量
	// 这实际上是利用之前已修改的proj/view局部变量（虽然这里直接用pc.proj）
	// 天空盒放大5倍确保完全覆盖屏幕
	gl_Position = pc.proj * view * vec4(inPos.xyz * 5, 1.0);
}