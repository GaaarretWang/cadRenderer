// ============================================================================
// BRDF LUT Vertex Shader（BRDF查找表顶点着色器）
// ============================================================================
// 作用：与fullscreenquad.vert类似，通过gl_VertexIndex生成全屏三角形，
//       用于离线预计算BRDF LUT（Split-Sum近似的积分查找表）。
// 流水线角色：IBL预处理阶段。BRDF LUT存储了不同粗糙度和视角下的
//             菲涅尔项积分结果，PBR渲染时通过查找此表实现高效的
//             镜面反射BRDF计算（避免实时积分）。
// ============================================================================

#version 450

layout (location = 0) out vec2 outUV; // 输出UV坐标（u=视角cosine, v=粗糙度）

void main()
{
	// 与fullscreenquad.vert相同的无顶点缓冲全屏三角形技术
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
}
