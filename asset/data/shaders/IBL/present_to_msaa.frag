#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MATERIAL_DESCRIPTOR_SET 2

layout(location = 0) in vec2 outUV;

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D sourceSampler;

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 uv = outUV * 0.5 + 0.5;
    outColor = texture(sourceSampler, uv);
}
