// Tonemapping utilities

vec3 Uncharted2Tonemap(vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec4 LINEARtoSRGB(vec4 srgbIn)
{
    return vec4(pow(srgbIn.xyz, vec3(1.0 / 2.2)), srgbIn.w);
}
