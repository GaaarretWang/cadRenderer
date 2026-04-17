#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 vsg_Vertex;
layout(location = 0) out vec2 outUV;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    gl_Position = vec4(vsg_Vertex.xy, 1.0, 1.0);
    outUV = vsg_Vertex.xy;
}
