// PBR BRDF functions

struct PBRInfo
{
    float NdotL;
    float NdotV;
    float NdotH;
    float LdotH;
    float VdotH;
    float VdotL;
    float perceptualRoughness;
    float metalness;
    vec3 reflectance0;
    vec3 reflectance90;
    float alphaRoughness;
    vec3 diffuseColor;
    vec3 specularColor;
};

vec3 BRDF_Diffuse_Disney(PBRInfo pbrInputs)
{
    float Fd90 = 0.5 + 2.0 * pbrInputs.perceptualRoughness * pbrInputs.VdotH * pbrInputs.VdotH;
    vec3 f0 = vec3(0.1);
    vec3 invF0 = vec3(1.0) - f0;
    float dim = min(invF0.r, min(invF0.g, invF0.b));
    float result = ((1.0 + (Fd90 - 1.0) * pow(1.0 - pbrInputs.NdotL, 5.0)) * (1.0 + (Fd90 - 1.0) * pow(1.0 - pbrInputs.NdotV, 5.0))) * dim;
    return pbrInputs.diffuseColor * result;
}

vec3 specularReflection(PBRInfo pbrInputs)
{
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance90 * pbrInputs.reflectance0) * exp2((-5.55473 * pbrInputs.VdotH - 6.98316) * pbrInputs.VdotH);
}

float geometricOcclusion(PBRInfo pbrInputs)
{
    float r = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float attenuationL = 2.0 * pbrInputs.NdotL / (pbrInputs.NdotL + sqrt(r + (1.0 - r) * (pbrInputs.NdotL * pbrInputs.NdotL)));
    float attenuationV = 2.0 * pbrInputs.NdotV / (pbrInputs.NdotV + sqrt(r + (1.0 - r) * (pbrInputs.NdotV * pbrInputs.NdotV)));
    return attenuationL * attenuationV;
}

float microfacetDistribution(PBRInfo pbrInputs)
{
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (PI * f * f);
}

vec3 BRDF(vec3 lightColor, vec3 v, vec3 n, vec3 l, vec3 h, float perceptualRoughness, float metallic, vec3 specularEnvironmentR0, vec3 specularEnvironmentR90, float alphaRoughness, vec3 diffuseColor, vec3 specularColor)
{
    float NdotL = clamp(dot(n, l), 0.001, 1.0);
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);
    float VdotL = clamp(dot(v, l), 0.0, 1.0);

    PBRInfo pbrInputs = PBRInfo(NdotL, NdotV, NdotH, LdotH, VdotH, VdotL, perceptualRoughness, metallic, specularEnvironmentR0, specularEnvironmentR90, alphaRoughness, diffuseColor, specularColor);
    vec3 F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);
    vec3 diffuseContrib = (1.0 - F) * BRDF_Diffuse_Disney(pbrInputs);
    vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    return NdotL * lightColor * (diffuseContrib + specContrib);
}
