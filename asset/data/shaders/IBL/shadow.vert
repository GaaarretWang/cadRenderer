#version 450
#extension GL_ARB_separate_shader_objects : enable

#pragma import_defines (VSG_INSTANCE_POSITIONS, VSG_BILLBOARD, VSG_DISPLACEMENT_MAP)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelView;
} pc;

#ifdef VSG_DISPLACEMENT_MAP
layout(binding = 6) uniform sampler2D displacementMap;
#endif

layout(location = 0) in vec3 vsg_Vertex;
layout(location = 1) in vec3 vsg_Normal;
layout(location = 2) in vec2 vsg_TexCoord0;
layout(location = 3) in vec4 vsg_Color;

layout(location = 0) out vec3 eyePos;
layout(location = 1) out vec3 normalDir;
layout(location = 2) out vec4 vertexColor;
layout(location = 3) out vec2 texCoord0;
layout(location = 5) out vec3 viewDir;

out gl_PerVertex{ vec4 gl_Position; };

void main()
{
    vec4 vertex = vec4(vsg_Vertex, 1.0);
    vec4 normal = vec4(vsg_Normal, 0.0);
    
    gl_Position = (pc.projection * pc.modelView) * vertex;
    eyePos = (pc.modelView * vertex).xyz;
    viewDir = - (pc.modelView * vertex).xyz;
    normalDir = (pc.modelView * normal).xyz;

    vertexColor = vsg_Color;
    texCoord0 = vsg_TexCoord0;
}
