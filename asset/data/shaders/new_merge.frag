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
    // ivec2 fragCoord = ivec2(gl_FragCoord.xy);
    // ivec2 screenSize = ivec2(720, 405);
    // vec2 texCoord = vec2(fragCoord * 1.f) / vec2(screenSize);
    vec2 texCoord = screenUV;

    float intCadDepth = texture(cadDepth, texCoord).r;
    float intPlaneDepth = texture(planeDepth, texCoord).r;
    float intShadowDepth = texture(shadowDepth, texCoord).r;

    // if(intCadDepth > intPlaneDepth)
    //     outColor.rgb = texture(cadColor, texCoord).rgb;
    // else
        outColor.rgb = texture(planeColor, texCoord).rgb * texture(shadowColor, texCoord).rrr;
    // outColor.rgb = texture(planeColor, texCoord).rgb;
    // outColor.rg = texCoord;
    outColor.a = 1.f;
}
