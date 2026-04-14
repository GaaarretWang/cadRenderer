// ============================================================================
// SSAO Vertex Shader（屏幕空间环境光遮蔽顶点着色器）
// ============================================================================
// 作用：生成全屏四边形用于SSAO（Screen-Space Ambient Occlusion）计算。
//       输入VSG引擎提供的顶点缓冲中的顶点位置（[-1,1]范围），
//       输出UV坐标给ssao.frag使用。
// 流水线角色：SSAO是后处理效果，通过分析深度/法线缓冲来计算
//             每个像素的环境光遮蔽程度，增强场景的立体感和接触阴影。
// ============================================================================

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 vsg_Vertex;  // 顶点位置（来自VSG引擎的顶点缓冲）
layout(location = 0) out vec2 outUV;       // 输出UV坐标（[-1,1]范围，fragment中转为[0,1]）

out gl_PerVertex{ vec4 gl_Position; };

void main()
{
    gl_Position = vec4(vsg_Vertex.xy, 0.99, 1.0); // 深度0.99，在天空盒(1.0)之前
	outUV = vsg_Vertex.xy; // 顶点xy即为UV（范围[-1,1]）
}
