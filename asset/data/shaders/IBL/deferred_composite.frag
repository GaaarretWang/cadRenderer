#version 450

#define MATERIAL_DESCRIPTOR_SET 2

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D deferredSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D fallbackSampler;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 uv = inUV * 0.5 + 0.5;
    vec4 deferredColor = texture(deferredSampler, uv);
    vec4 fallbackColor = texture(fallbackSampler, uv);

    float coverage = clamp(deferredColor.a, 0.0, 1.0);
    vec3 color = mix(fallbackColor.rgb, deferredColor.rgb, coverage);
    float alpha = max(coverage, fallbackColor.a);

    outColor = vec4(color, alpha);
}
