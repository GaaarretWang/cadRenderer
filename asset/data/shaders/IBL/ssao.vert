#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#pragma import_defines (VSG_INSTANCE_POSITIONS, VSG_BILLBOARD, VSG_DISPLACEMENT_MAP)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
    mat4 invView;
    mat4 cameraData;
} pc;

#ifdef VSG_DISPLACEMENT_MAP
layout(binding = 6) uniform sampler2D displacementMap;
#endif

struct InstanceData {
    mat4 modelMatrix;
    vec4 highlight[10];
};


#define MATERIAL_DESCRIPTOR_SET 2
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 11) buffer InstanceMatrices {
    InstanceData instanceModelMatrix[];
}instanceMatrices;

layout(location = 0) in vec3 vsg_Vertex;
layout(location = 1) in vec3 vsg_Normal;
layout(location = 2) in vec2 vsg_TexCoord0;
layout(location = 3) in vec4 vsg_Color;

#ifdef VSG_BILLBOARD
layout(location = 4) in vec4 vsg_position_scaleDistance;
#elif defined(VSG_INSTANCE_POSITIONS)
layout(location = 4) in vec3 vsg_position;
#endif

layout(location = 0) out vec3 eyePos;
layout(location = 1) out vec3 normalDir;
layout(location = 2) out vec4 vertexColor;
layout(location = 3) out vec2 texCoord0;
layout(location = 4) out float highlight;

layout(location = 6) out vec3 worldNormal;
layout(location = 7) out vec3 worldViewDir;

#define VIEW_DESCRIPTOR_SET 1
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[2048];
} lightData;

out gl_PerVertex{ vec4 gl_Position; };

void main()
{
    gl_Position = vec4(vsg_Vertex.xy, 1, 1.0);
}
