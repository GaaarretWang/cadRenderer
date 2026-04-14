// ============================================================================
// SSAO Denoise Vertex Shader（SSAO去噪顶点着色器）
// ============================================================================
// 作用：生成全屏四边形用于SSAO去噪和最终合成。
//       与ssao.vert功能类似，但深度值为1.0（在天空盒同一层）。
// 流水线角色：SSAO后处理的第二阶段。
//             第一阶段(ssao.frag)生成原始SSAO遮蔽值，
//             第二阶段(ssao_denoise.frag)进行去噪并与场景颜色合成。
// ============================================================================

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 vsg_Vertex;  // 顶点位置（来自VSG引擎的顶点缓冲）
layout(location = 0) out vec2 outUV;       // 输出UV坐标

out gl_PerVertex{ vec4 gl_Position; };

void main()
{
    gl_Position = vec4(vsg_Vertex.xy, 1, 1.0); // 深度1.0（与天空盒同层）
	outUV = vsg_Vertex.xy; // 顶点xy即为UV
}
