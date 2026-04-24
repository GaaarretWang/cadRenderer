// IBL (Image-Based Lighting) sampling functions
// Requires: samplerBRDFLUT, samplerIrradiance, samplerPrefilteredEnv

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 prefilteredReflection(vec3 R, float roughness)
{
    const float MAX_REFLECTION_LOD = 9.0;
    return textureLod(samplerPrefilteredEnv, R, roughness * MAX_REFLECTION_LOD).rgb;
}

vec3 IBL(vec3 v, vec3 n, float perceptualRoughness, float metallic, vec3 specularEnvironmentR0, vec3 diffuseColor)
{
    vec3 R = normalize(reflect(-v, n));
    float NdotV = clamp(dot(n, v), 0.001, 1.0);
    vec2 brdf = texture(samplerBRDFLUT, vec2(NdotV, perceptualRoughness)).rg;
    vec3 F = fresnelSchlickRoughness(NdotV, specularEnvironmentR0, perceptualRoughness);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 color = texture(samplerIrradiance, n).rgb * diffuseColor * pow(NdotV, 0.5 + 0.3 * perceptualRoughness) * kD;
    color += prefilteredReflection(R, perceptualRoughness) * (F * brdf.x + brdf.y);
    return color;
}
