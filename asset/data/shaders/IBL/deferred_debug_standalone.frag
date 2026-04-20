#version 450

#define MATERIAL_DESCRIPTOR_SET 2

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D normalSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D worldPosSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D materialSampler;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D ssaoSampler;

layout(std140, set = MATERIAL_DESCRIPTOR_SET, binding = 4) uniform GlobalBuffer {
    mat4 last_view;
    vec3 camera_pos;
    float softness;
    float baseBrightness;
    float ssao_radius;
    float exposure;
    float softness_falloff;
    float shadow_bias;
    float z_far;
    int width;
    int height;
    int ssao_kernel_size;
    int denoise_size;
    int blocker_sample_num;
    int pcf_sample_num;
    int shadow_type;
    uint frame_num;
    int enable_real_depth_occlusion;
    int shadow_mode;
} globalBuffer;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

vec3 tonemap(vec3 color)
{
    color = max(color, vec3(0.0));
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 2.2));
}

void main()
{
    vec2 uv = inUV * 0.5 + 0.5;
    ivec2 inputSize = textureSize(normalSampler, 0);
    ivec2 coord = clamp(ivec2(uv * vec2(inputSize)), ivec2(0), inputSize - 1);

    vec4 normalData = texelFetch(normalSampler, coord, 0);
    vec4 worldPosData = texelFetch(worldPosSampler, coord, 0);
    vec4 materialData = texelFetch(materialSampler, coord, 0);

    vec3 normal = normalData.xyz;
    vec3 worldPos = worldPosData.xyz;
    if (length(normal) < 0.001 || length(worldPos) < 0.001)
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    normal = normalize(normal);
    vec3 viewDir = normalize(globalBuffer.camera_pos - worldPos);
    vec3 baseColor = max(materialData.rgb, vec3(0.0));
    float ao = clamp(materialData.a, 0.0, 1.0);
    float roughness = clamp(normalData.w, 0.04, 1.0);
    float metallic = clamp(worldPosData.w, 0.0, 1.0);
    float ssao = clamp(texture(ssaoSampler, uv).r, 0.0, 1.0);
    float combinedAo = ao * ssao;

    vec3 keyLight = normalize(vec3(-0.45, -0.55, 0.70));
    vec3 fillLight = normalize(vec3(0.30, 0.20, 0.93));
    vec3 halfKey = normalize(viewDir + keyLight);
    vec3 halfFill = normalize(viewDir + fillLight);

    float keyDiffuse = max(dot(normal, keyLight), 0.0);
    float fillDiffuse = max(dot(normal, fillLight), 0.0);
    float keySpecular = pow(max(dot(normal, halfKey), 0.0), mix(96.0, 8.0, roughness));
    float fillSpecular = pow(max(dot(normal, halfFill), 0.0), mix(64.0, 4.0, roughness));

    vec3 diffuseColor = baseColor * (1.0 - metallic);
    vec3 specularColor = mix(vec3(0.04), baseColor, metallic);
    vec3 ambient = diffuseColor * (0.18 * combinedAo);
    vec3 color = ambient;
    color += diffuseColor * (1.35 * keyDiffuse + 0.35 * fillDiffuse);
    color += specularColor * ((0.30 + 0.70 * metallic) * keySpecular + 0.15 * fillSpecular);

    outColor = vec4(tonemap(color), 1.0);
}
