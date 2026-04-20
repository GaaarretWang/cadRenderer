#version 450

layout(set = 0, binding = 1) uniform Params {
    float exposure;
    float gamma;
    float width;
    float height;
    float enableRealDepthOcclusion;
    float shadowMode;
} tonemapParams;

layout(set = 0, binding = 3) uniform sampler2D depthImageSampler;

layout(push_constant) uniform PushConstants {
    mat4 proj;
    mat4 view;
} pc;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outWorldPos;
layout(location = 3) out vec4 outShadow;

void main()
{
    vec2 screenUV = vec2(gl_FragCoord.x / tonemapParams.width, gl_FragCoord.y / tonemapParams.height);

    outColor = vec4(0.0);
    outNormal = vec4(0.0, 0.0, 0.0, 1.0);
    outWorldPos = vec4(0.0, 0.0, 0.0, 1.0);
    outShadow = vec4(0.0);
    gl_FragDepth = 0.0;

    float cameraDepthNorm = texture(depthImageSampler, screenUV).r;
    float linearZ = cameraDepthNorm * 65.535;
    if (linearZ <= 0.0 || linearZ >= 65.535 || tonemapParams.enableRealDepthOcclusion <= 0.5)
    {
        return;
    }

    float ndcX = screenUV.x * 2.0 - 1.0;
    float ndcY = screenUV.y * 2.0 - 1.0;
    vec4 ndcPoint = vec4(ndcX, ndcY, 0.0, 1.0);
    mat4 invProj = inverse(pc.proj);
    vec4 viewRay = invProj * ndcPoint;
    viewRay /= viewRay.w;
    vec3 rayDir = normalize(viewRay.xyz);
    float scale = -linearZ / rayDir.z;
    vec3 actualView = rayDir * scale;
    vec4 clipOut = pc.proj * vec4(actualView, 1.0);
    gl_FragDepth = clipOut.z / clipOut.w;
}
