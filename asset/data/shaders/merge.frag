#version 450
#extension GL_ARB_separate_shader_objects : enable

#define VIEW_DESCRIPTOR_SET 0
#define MATERIAL_DESCRIPTOR_SET 1

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D cadColor;

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D cadDepth;

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D planeColor;

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D planeDepth;

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 4) uniform sampler2D shadowColor;

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 5) uniform sampler2D shadowDepth;

layout(location = 4) in vec2 screen_uv;

layout(location = 0) out vec4 outColor;

void main()
{
    float intCadDepth = 0.1 / (texture(cadDepth, screen_uv).r * 65.435 + 0.1);
    //float intPlaneDepth = 0.1 / (texture(planeDepth, screen_uv).r * 65.435 + 0.1);
    float intPlaneDepth = texture(planeDepth, screen_uv).r;
    float intShadowDepth = 0.1 / (texture(shadowDepth, screen_uv).r * 65.435 + 0.1);
    float shadow = texture(shadowColor, screen_uv).r * 0.1 + 0.9;
    if(shadow > 1.0)
        shadow = 1.0;

    if(intCadDepth <= intPlaneDepth)
        outColor.rgb = texture(cadColor, screen_uv).rgb;
    else if(intShadowDepth <= intPlaneDepth)
        outColor.rgb = texture(planeColor, screen_uv).rgb * shadow;
    else
        outColor.rgb = texture(planeColor, screen_uv).rgb;

    //outColor.rgb = vec3(texture(shadowDepth, screen_uv).r * 100);
    outColor.a = 1.f;
}
