#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#pragma import_defines (VSG_INSTANCE_POSITIONS, VSG_BILLBOARD, VSG_DISPLACEMENT_MAP)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
    mat4 last_view;
    vec3 camera_pos;
    float softness;
    float baseBrightness;
    float ssao_radius;
    float exposure;
    float softness_falloff;
    float shadow_bias;
    int ssao_kernel_size;
    int denoise_size;
    int blocker_sample_num;
    int pcf_sample_num;
    int shadow_type;
    uint frame_num;
} pc;

#ifdef VSG_DISPLACEMENT_MAP
layout(binding = 6) uniform sampler2D displacementMap;
#endif

struct InstanceData {
    mat4 modelMatrix;
    mat4 lastModelMatrix;
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
layout(location = 4) in vec4 vsg_InstanceID;

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
layout(location = 5) out float InstanceID;
layout(location = 6) out vec3 worldNormal;
layout(location = 7) out vec3 worldViewDir;
layout(location = 8) out vec3 lastWorldPos;

#define VIEW_DESCRIPTOR_SET 1
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[2048];
} lightData;

out gl_PerVertex{ vec4 gl_Position; };

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
    InstanceData instanceModelMatrixi = instanceMatrices.instanceModelMatrix[gl_InstanceIndex];
    mat4 model = instanceModelMatrixi.modelMatrix;
    mat4 lastModel = instanceModelMatrixi.lastModelMatrix;
    vec4 modelVertex = model * vertex;
    vec4 lastVertex = lastModel * vertex;
    vec4 viewVertex = pc.view * modelVertex;
    vec4 normal = vec4(vsg_Normal, 0.0);

    gl_Position = pc.projection * viewVertex;

    worldViewDir = (modelVertex).xyz;
    lastWorldPos = lastVertex.xyz;
    eyePos = viewVertex.xyz;

    worldNormal = mat3(model) * normal.xyz;
    normalDir = mat3(pc.view) * worldNormal;

    vertexColor = vsg_Color;
    InstanceID = vsg_InstanceID.x;
    texCoord0 = vsg_TexCoord0;
    highlight = instanceModelMatrixi.highlight[0].x;
}
