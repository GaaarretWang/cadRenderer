#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#pragma import_defines (VSG_INSTANCE_POSITIONS, VSG_BILLBOARD, VSG_DISPLACEMENT_MAP)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelView;
} pc;

#ifdef VSG_DISPLACEMENT_MAP
layout(binding = 6) uniform sampler2D displacementMap;
#endif

struct InstanceData {
    mat4 protoMatrix;
    mat4 modelMatrix;
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
layout(location = 5) out vec3 viewDir;

layout(location = 6) out vec3 worldNormal;
layout(location = 7) out vec3 worldViewDir;
layout(location = 8) out mat4 project;

#define VIEW_DESCRIPTOR_SET 1
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[2048];
} lightData;
layout(set = VIEW_DESCRIPTOR_SET, binding = 3) uniform ViewMatrixData{
    mat4 view;
    mat4 invView;
    mat4 unused[2];
} viewMatrixData;

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
    vertex = instanceMatrices.instanceModelMatrix[gl_InstanceIndex].modelMatrix * instanceMatrices.instanceModelMatrix[gl_InstanceIndex].protoMatrix * vertex;
    vec4 normal = vec4(vsg_Normal, 0.0);

#ifdef VSG_DISPLACEMENT_MAP
    // TODO need to pass as as uniform or per instance attributes
    vec3 scale = vec3(1.0, 1.0, 1.0);

    vertex.xyz = vertex.xyz + vsg_Normal * (texture(displacementMap, vsg_TexCoord0.st).s * scale.z);

    float s_delta = 0.01;
    float width = 0.0;

    float s_left = max(vsg_TexCoord0.s - s_delta, 0.0);
    float s_right = min(vsg_TexCoord0.s + s_delta, 1.0);
    float t_center = vsg_TexCoord0.t;
    float delta_left_right = (s_right - s_left) * scale.x;
    float dz_left_right = (texture(displacementMap, vec2(s_right, t_center)).s - texture(displacementMap, vec2(s_left, t_center)).s) * scale.z;

    // TODO need to handle different origins of displacementMap vs diffuseMap etc,
    float t_delta = s_delta;
    float t_bottom = max(vsg_TexCoord0.t - t_delta, 0.0);
    float t_top = min(vsg_TexCoord0.t + t_delta, 1.0);
    float s_center = vsg_TexCoord0.s;
    float delta_bottom_top = (t_top - t_bottom) * scale.y;
    float dz_bottom_top = (texture(displacementMap, vec2(s_center, t_top)).s - texture(displacementMap, vec2(s_center, t_bottom)).s) * scale.z;

    vec3 dx = normalize(vec3(delta_left_right, 0.0, dz_left_right));
    vec3 dy = normalize(vec3(0.0, delta_bottom_top, -dz_bottom_top));
    vec3 dz = normalize(cross(dx, dy));

    normal.xyz = normalize(dx * vsg_Normal.x + dy * vsg_Normal.y + dz * vsg_Normal.z);
#endif

#ifdef VSG_INSTANCE_POSITIONS
    vertex.xyz = vertex.xyz + vsg_position;
#endif

#ifdef VSG_BILLBOARD
    mat4 mv = computeBillboadMatrix(pc.modelView * vec4(vsg_position_scaleDistance.xyz, 1.0), vsg_position_scaleDistance.w);
#else
    mat4 mv = pc.modelView;
#endif

    gl_Position = (pc.projection * mv) * vertex;
    eyePos = (mv * vertex).xyz;
    viewDir = - (mv * vertex).xyz;
    normalDir = (mv * normal).xyz;

    // worldNormal = normal.xyz;
    // worldViewDir = -vertex.xyz;

    // mat3 modelRotateScale = mat3(viewMatrixData.invView  * mv);
    mat4 model = viewMatrixData.invView * mv;
    worldNormal = mat3(model) * normal.xyz;
    worldViewDir = (model * vertex).xyz;
    project = pc.projection;
    vertexColor = vsg_Color;
    texCoord0 = vsg_TexCoord0;
}
