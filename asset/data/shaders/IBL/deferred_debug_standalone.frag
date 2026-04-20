#version 450

#define VIEW_DESCRIPTOR_SET 1
#define MATERIAL_DESCRIPTOR_SET 2

const float PI = 3.14159265359;

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

layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData {
    vec4 values[2048];
} lightData;
layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

float distributionGGX(vec3 normal, vec3 halfVector, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(normal, halfVector), 0.0);
    float nDotH2 = nDotH * nDotH;
    float denominator = nDotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denominator * denominator, 1e-4);
}

float geometrySchlickGGX(float nDotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 1e-4);
}

float geometrySmith(vec3 normal, vec3 viewDir, vec3 lightDir, float roughness)
{
    float nDotV = max(dot(normal, viewDir), 0.0);
    float nDotL = max(dot(normal, lightDir), 0.0);
    return geometrySchlickGGX(nDotV, roughness) * geometrySchlickGGX(nDotL, roughness);
}

vec3 tonemap(vec3 color)
{
    color = max(color, vec3(0.0));
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 2.2));
}

float shadowVisibility(vec3 worldPos, inout int lightIndex, inout int shadowMapIndex, vec4 shadowMapSettings)
{
    float visibility = 1.0;
    bool matched = false;
    int shadowMapCount = int(shadowMapSettings.r);
    for (int i = 0; i < shadowMapCount; ++i)
    {
        mat4 shadowMatrix = mat4(
            lightData.values[lightIndex++],
            lightData.values[lightIndex++],
            lightData.values[lightIndex++],
            lightData.values[lightIndex++]);

        vec4 shadowCoord = shadowMatrix * vec4(worldPos, 1.0);
        if (!matched &&
            shadowCoord.x >= 0.0 && shadowCoord.x <= 1.0 &&
            shadowCoord.y >= 0.0 && shadowCoord.y <= 1.0 &&
            shadowCoord.z >= 0.0 && shadowCoord.z <= 1.0)
        {
            matched = true;
            visibility = texture(
                shadowMaps,
                vec4(shadowCoord.xy, shadowMapIndex, shadowCoord.z + globalBuffer.shadow_bias));
        }

        ++shadowMapIndex;
    }

    return visibility;
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
    vec3 diffuseColor = baseColor * (1.0 - metallic);
    vec3 specularColor = mix(vec3(0.04), baseColor, metallic);
    vec3 ambient = diffuseColor * (0.12 * (0.5 + 0.5 * globalBuffer.baseBrightness)) * combinedAo;
    vec3 color = ambient;

    vec4 lightNums = lightData.values[0];
    int numDirectionalLights = int(lightNums[1]);
    int lightIndex = 1;
    int shadowMapIndex = 0;
    bool hasDirectionalLight = false;
    for (int i = 0; i < numDirectionalLights; ++i)
    {
        vec4 lightColorData = lightData.values[lightIndex++];
        vec4 lightDirectionData = lightData.values[lightIndex++];
        vec4 shadowMapSettings = lightData.values[lightIndex++];

        vec3 lightDir = normalize(-lightDirectionData.xyz);
        float nDotL = max(dot(normal, lightDir), 0.0);
        if (nDotL > 0.0)
        {
            hasDirectionalLight = true;
            float visibility = shadowVisibility(worldPos, lightIndex, shadowMapIndex, shadowMapSettings);
            vec3 radiance = max(lightColorData.rgb, vec3(0.0)) * max(lightColorData.a, 0.0);
            vec3 halfVector = normalize(viewDir + lightDir);
            float nDotV = max(dot(normal, viewDir), 0.0);
            vec3 fresnel = fresnelSchlick(max(dot(halfVector, viewDir), 0.0), specularColor);
            float distribution = distributionGGX(normal, halfVector, roughness);
            float geometry = geometrySmith(normal, viewDir, lightDir, roughness);
            vec3 numerator = distribution * geometry * fresnel;
            float denominator = max(4.0 * nDotV * nDotL, 1e-4);
            vec3 specular = numerator / denominator;
            vec3 kD = (vec3(1.0) - fresnel) * (1.0 - metallic);
            color += (kD * diffuseColor / PI + specular) * radiance * nDotL * visibility;
        }
        else
        {
            lightIndex += 4 * int(shadowMapSettings.r);
            shadowMapIndex += int(shadowMapSettings.r);
        }
    }

    if (!hasDirectionalLight)
    {
        vec3 fallbackLight = normalize(vec3(-0.45, -0.55, 0.70));
        vec3 fallbackHalf = normalize(viewDir + fallbackLight);
        float diffuse = max(dot(normal, fallbackLight), 0.0);
        float specular = pow(max(dot(normal, fallbackHalf), 0.0), mix(96.0, 8.0, roughness));
        color += diffuseColor * diffuse * 1.35;
        color += specularColor * specular * (0.30 + 0.70 * metallic);
    }

    color *= combinedAo;

    outColor = vec4(tonemap(color), 1.0);
}
