float ComputeDenoisedOcclusion(sampler2D ssaoSource, vec2 uv, int denoiseSize)
{
    ivec2 ssaoSize = textureSize(ssaoSource, 0);
    vec2 texelSize = 1.0 / vec2(ssaoSize);
    float result = 0.0;
    int denoiseRadius = denoiseSize / 2;

    for (int x = -denoiseRadius; x <= denoiseRadius; ++x)
    {
        for (int y = -denoiseRadius; y <= denoiseRadius; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoSource, uv + offset).r;
        }
    }

    float kernelWidth = float(denoiseRadius * 2 + 1);
    return result / (kernelWidth * kernelWidth);
}

vec4 CompositeFallbackSample(vec4 colorData,
                             vec4 realSceneData,
                             float maskValue,
                             bool preferRealScene,
                             float occlusion,
                             float exposure)
{
    bool isOpaqueVirtual = abs(maskValue - 1.0) < 0.5;
    bool isShadowReceiver = maskValue > 1.5;
    bool isEmptySample = abs(colorData.a) < 1e-6 && length(colorData.rgb) < 1e-6;
    bool hasRealScene = realSceneData.a > 0.5;

    if (isOpaqueVirtual)
    {
        return vec4(0.0);
    }

    if (isShadowReceiver)
    {
        return vec4(colorData.rgb * occlusion, 1.0);
    }

    if (isEmptySample && hasRealScene)
    {
        return realSceneData;
    }

    if (preferRealScene)
    {
        return hasRealScene ? realSceneData : colorData;
    }

    vec3 color = Uncharted2Tonemap(colorData.rgb * occlusion * exposure);
    color = color * (vec3(1.0) / Uncharted2Tonemap(vec3(11.2)));
    return LINEARtoSRGB(vec4(color, colorData.a));
}
