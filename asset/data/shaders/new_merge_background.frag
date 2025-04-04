#version 450
#extension GL_ARB_separate_shader_objects : enable

#define TEXTURE_DESCRIPTOR_SET 0

layout(set = TEXTURE_DESCRIPTOR_SET, binding = 0) uniform sampler2D cadColor;

layout(set = TEXTURE_DESCRIPTOR_SET, binding = 1) uniform sampler2D cadDepth;

layout(set = TEXTURE_DESCRIPTOR_SET, binding = 2) uniform sampler2D planeColor;

layout(set = TEXTURE_DESCRIPTOR_SET, binding = 3) uniform sampler2D planeDepth;

layout(set = TEXTURE_DESCRIPTOR_SET, binding = 4) uniform sampler2D shadowColor;

layout(set = TEXTURE_DESCRIPTOR_SET, binding = 5) uniform sampler2D shadowDepth;

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 screenUV;

void main()
{
    float intCadDepth = 0.1 / (texture(cadDepth, screenUV).r * 65.435 + 0.1);
    //float intPlaneDepth = 0.1 / (texture(planeDepth, screenUV).r * 65.435 + 0.1);
    float intPlaneDepth = texture(planeDepth, screenUV).r;
    float intShadowDepth = 0.1 / (texture(shadowDepth, screenUV).r * 65.435 + 0.1);
    float shadow = texture(shadowColor, screenUV).r * 0.5 + 0.5;
    if(shadow > 1.0)
        shadow = 1.0;

    //modified
    //if(intCadDepth <= intPlaneDepth)
    //    outColor.rgb = texture(cadColor, screenUV).rgb;
    //else if(intShadowDepth <= intPlaneDepth)
    //    outColor.rgb = texture(planeColor, screenUV).rgb * shadow;
    //else
    //    outColor.rgb = texture(planeColor, screenUV).rgb;

    if(texture(cadDepth, screenUV).r + 1e-6 >= texture(shadowDepth, screenUV).r)
        outColor.rgb = texture(cadColor, screenUV).rgb * shadow;
    else
        outColor.rgb = texture(planeColor, screenUV).rgb * shadow;


    //outColor.rgb = vec3(texture(shadowDepth, screenUV).r * 100);
    //outColor.rgb = vec3(screenUV.x, screenUV.y, 1.0);
    outColor.a = 1.f;
}
