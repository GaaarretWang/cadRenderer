// ============================================================================
// SSAO Fragment Shader（屏幕空间环境光遮蔽片元着色器）
// ============================================================================
// 作用：实现屏幕空间环境光遮蔽（Screen-Space Ambient Occlusion, SSAO）。
//       通过在每个像素的法线半球内随机采样，检测周围几何体的遮挡程度，
//       模拟间接光照中的接触阴影效果。
//
// SSAO原理简述：
//   1. 从G-Buffer获取当前像素的世界坐标和法线
//   2. 以法线为上方向，构建切线空间（TBN矩阵）
//   3. 在法线半球内均匀采样多个点（使用预计算的采样核）
//   4. 将采样点投影到屏幕空间，比较深度
//   5. 如果采样点被场景几何体遮挡，则增加遮蔽值
//   6. 最终遮蔽 = 1 - (被遮挡的采样数 / 总采样数)
//
// 流水线角色：后处理Pass，使用G-Buffer的法线和世界位置贴图。
//             输出遮蔽因子给ssao_denoise.frag进行去噪和最终合成。
// ============================================================================

#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_METALLROUGHNESS_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, VSG_WORKFLOW_SPECGLOSS, SHADOWMAP_DEBUG)

#define MATERIAL_DESCRIPTOR_SET 2

// G-Buffer输入：法线贴图（Multisampled，用于MSAA抗锯齿）
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2DMS normalInputAttachment;
// G-Buffer输入：世界位置贴图（Multisampled）
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2DMS worldPosInputAttachment;
// 随机噪声纹理（用于给TBN矩阵添加随机旋转，减少采样规律性）
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D samplerNoise;

// Push Constants（推送常量）：CPU端每帧更新的渲染参数
layout(push_constant) uniform PushConstants {
    mat4 projection;       // 投影矩阵（用于将采样点投影到屏幕空间）
    mat4 view;             // 视图矩阵
    mat4 last_view;        // 上一帧视图矩阵
    vec3 camera_pos;       // 相机世界坐标（用于计算深度/距离）
    float softness;        // 软阴影柔度
    float baseBrightness;  // 基础亮度
    float ssao_radius;     // SSAO采样半径（世界空间单位）
    float exposure;        // 曝光度
    float softness_falloff; // 软阴影衰减
    float shadow_bias;     // 阴影偏移
    int ssao_kernel_size;  // 实际使用的采样点数（从128中选取）
    int denoise_size;      // 去噪核大小
    int blocker_sample_num; // PCSS blocker采样数
    int pcf_sample_num;    // PCF采样数
    int shadow_type;       // 阴影类型
    uint frame_num;        // 帧号
} pc;

layout(location = 0) in vec2 inUV;     // 全屏四边形UV（[-1,1]范围）
layout(location = 0) out vec4 outColor; // 输出遮蔽因子（RGB=遮蔽值，A=0）

// ===== SSAO预计算采样核（128个半球采样点）=====
// 这些点在单位半球内随机分布，z分量（法线方向）大部分为正
// 采样核大小通过pc.ssao_kernel_size控制实际使用多少个点
#define SSAO_WHOLE_KERNEL_SIZE 128

const vec4 ssaoKernel[SSAO_WHOLE_KERNEL_SIZE] = vec4[](
    vec4(0.006057f, -0.020636f, 0.005974f, 0.0f),
    vec4(0.003845f, 0.002873f, 0.007254f, 0.0f),
    vec4(-0.008086f, -0.048698f, 0.011322f, 0.0f),
    vec4(-0.039987f, -0.025435f, 0.027443f, 0.0f),
    vec4(-0.000367f, 0.000117f, 0.000531f, 0.0f),
    vec4(0.011986f, 0.007766f, 0.006668f, 0.0f),
    vec4(0.009245f, -0.003304f, 0.000938f, 0.0f),
    vec4(0.047995f, 0.014326f, 0.055739f, 0.0f),
    vec4(0.004053f, 0.052923f, 0.021172f, 0.0f),
    vec4(0.035782f, 0.012874f, 0.046802f, 0.0f),
    vec4(0.012222f, -0.027134f, 0.006808f, 0.0f),
    vec4(-0.024888f, -0.015826f, 0.002991f, 0.0f),
    vec4(0.011517f, -0.011473f, 0.015711f, 0.0f),
    vec4(-0.026221f, 0.049136f, 0.036461f, 0.0f),
    vec4(-0.033791f, 0.023543f, 0.008395f, 0.0f),
    vec4(0.064887f, 0.018557f, 0.036912f, 0.0f),
    vec4(0.002760f, 0.002222f, 0.000922f, 0.0f),
    vec4(-0.064042f, -0.080599f, 0.036608f, 0.0f),
    vec4(0.032949f, -0.016224f, 0.028690f, 0.0f),
    vec4(0.028027f, -0.002782f, 0.008954f, 0.0f),
    vec4(0.017619f, -0.068117f, 0.083917f, 0.0f),
    vec4(-0.010958f, -0.030573f, 0.054328f, 0.0f),
    vec4(-0.053027f, -0.058704f, 0.007107f, 0.0f),
    vec4(0.047330f, -0.012614f, 0.005147f, 0.0f),
    vec4(0.080913f, 0.004748f, 0.079187f, 0.0f),
    vec4(-0.055471f, 0.025063f, 0.038704f, 0.0f),
    vec4(-0.049986f, 0.030218f, 0.011957f, 0.0f),
    vec4(-0.002699f, 0.026472f, 0.025545f, 0.0f),
    vec4(0.000131f, -0.071709f, 0.101826f, 0.0f),
    vec4(-0.011527f, 0.007947f, 0.017414f, 0.0f),
    vec4(0.044154f, 0.006624f, 0.065482f, 0.0f),
    vec4(-0.133877f, -0.047137f, 0.002610f, 0.0f),
    vec4(0.006512f, 0.005703f, 0.002644f, 0.0f),
    vec4(0.050024f, 0.059147f, 0.005667f, 0.0f),
    vec4(-0.014296f, 0.008649f, 0.012708f, 0.0f),
    vec4(-0.025101f, 0.050577f, 0.134586f, 0.0f),
    vec4(-0.023886f, -0.089564f, 0.083798f, 0.0f),
    vec4(-0.055764f, -0.035133f, 0.092845f, 0.0f),
    vec4(-0.028239f, 0.008018f, 0.027601f, 0.0f),
    vec4(-0.030108f, 0.016421f, 0.021395f, 0.0f),
    vec4(-0.157641f, 0.048175f, 0.042063f, 0.0f),
    vec4(0.080841f, -0.096465f, 0.026750f, 0.0f),
    vec4(-0.048680f, -0.062636f, 0.079683f, 0.0f),
    vec4(-0.002120f, 0.022080f, 0.031322f, 0.0f),
    vec4(-0.084348f, -0.014429f, 0.044320f, 0.0f),
    vec4(0.008358f, 0.006325f, 0.017954f, 0.0f),
    vec4(-0.011140f, -0.018383f, 0.049285f, 0.0f),
    vec4(-0.050490f, -0.008375f, 0.034379f, 0.0f),
    vec4(-0.090535f, 0.153162f, 0.080175f, 0.0f),
    vec4(0.014477f, -0.129286f, 0.143736f, 0.0f),
    vec4(0.024269f, 0.022063f, 0.021958f, 0.0f),
    vec4(-0.000585f, -0.011654f, 0.008164f, 0.0f),
    vec4(-0.045578f, 0.182765f, 0.049937f, 0.0f),
    vec4(-0.023391f, -0.040029f, 0.248857f, 0.0f),
    vec4(0.018064f, 0.070744f, 0.025070f, 0.0f),
    vec4(0.170523f, 0.028807f, 0.098629f, 0.0f),
    vec4(-0.199192f, 0.037864f, 0.113094f, 0.0f),
    vec4(-0.030799f, 0.041427f, 0.003601f, 0.0f),
    vec4(0.014020f, 0.025848f, 0.017349f, 0.0f),
    vec4(0.127451f, -0.082875f, 0.097072f, 0.0f),
    vec4(-0.078577f, 0.081396f, 0.254280f, 0.0f),
    vec4(-0.092477f, 0.067603f, 0.037318f, 0.0f),
    vec4(0.130682f, -0.152232f, 0.120329f, 0.0f),
    vec4(-0.205620f, -0.020066f, 0.240143f, 0.0f),
    vec4(-0.243751f, -0.163845f, 0.075741f, 0.0f),
    vec4(0.035104f, 0.034957f, 0.017029f, 0.0f),
    vec4(0.225226f, 0.137358f, 0.206393f, 0.0f),
    vec4(0.024284f, -0.077621f, 0.064433f, 0.0f),
    vec4(0.014111f, 0.037907f, 0.005799f, 0.0f),
    vec4(-0.204939f, 0.027757f, 0.071018f, 0.0f),
    vec4(0.043675f, -0.059488f, 0.063646f, 0.0f),
    vec4(-0.000681f, 0.024063f, 0.025115f, 0.0f),
    vec4(-0.096064f, -0.280709f, 0.002228f, 0.0f),
    vec4(0.064395f, -0.111798f, 0.174060f, 0.0f),
    vec4(-0.051494f, -0.349162f, 0.026790f, 0.0f),
    vec4(0.165189f, 0.018644f, 0.170656f, 0.0f),
    vec4(-0.246678f, -0.261152f, 0.108041f, 0.0f),
    vec4(0.040871f, 0.049784f, 0.062035f, 0.0f),
    vec4(-0.157513f, -0.249790f, 0.245295f, 0.0f),
    vec4(-0.225237f, 0.290287f, 0.185912f, 0.0f),
    vec4(0.200464f, 0.261823f, 0.222886f, 0.0f),
    vec4(-0.366142f, 0.182268f, 0.127971f, 0.0f),
    vec4(0.060745f, 0.073171f, 0.081474f, 0.0f),
    vec4(0.180788f, -0.246548f, 0.274341f, 0.0f),
    vec4(-0.086081f, 0.098183f, 0.071377f, 0.0f),
    vec4(0.070509f, -0.065033f, 0.002825f, 0.0f),
    vec4(-0.038568f, 0.081824f, 0.108568f, 0.0f),
    vec4(0.075201f, -0.053323f, 0.260752f, 0.0f),
    vec4(0.054282f, -0.047537f, 0.059962f, 0.0f),
    vec4(0.206278f, -0.104596f, 0.024172f, 0.0f),
    vec4(0.150625f, -0.122798f, 0.199764f, 0.0f),
    vec4(-0.240435f, 0.160398f, 0.266732f, 0.0f),
    vec4(-0.285876f, 0.244150f, 0.154453f, 0.0f),
    vec4(0.028012f, 0.019755f, 0.021084f, 0.0f),
    vec4(0.287455f, -0.297191f, 0.274714f, 0.0f),
    vec4(0.159708f, -0.145117f, 0.112387f, 0.0f),
    vec4(-0.105759f, -0.221429f, 0.068965f, 0.0f),
    vec4(0.255234f, 0.102773f, 0.261632f, 0.0f),
    vec4(-0.175052f, 0.042158f, 0.000178f, 0.0f),
    vec4(-0.060465f, 0.068980f, 0.282313f, 0.0f),
    vec4(-0.090042f, -0.445691f, 0.368313f, 0.0f),
    vec4(0.226141f, -0.252332f, 0.032389f, 0.0f),
    vec4(0.145534f, -0.180424f, 0.447976f, 0.0f),
    vec4(0.008476f, -0.013507f, 0.004884f, 0.0f),
    vec4(-0.025994f, -0.002533f, 0.043284f, 0.0f),
    vec4(-0.229313f, 0.347794f, 0.260560f, 0.0f),
    vec4(-0.000054f, -0.002447f, 0.003135f, 0.0f),
    vec4(0.208791f, 0.224666f, 0.044338f, 0.0f),
    vec4(-0.019510f, 0.027568f, 0.015590f, 0.0f),
    vec4(-0.311185f, 0.432203f, 0.283180f, 0.0f),
    vec4(0.204393f, 0.595077f, 0.363133f, 0.0f),
    vec4(0.282473f, 0.081296f, 0.259532f, 0.0f),
    vec4(0.346819f, 0.050225f, 0.470656f, 0.0f),
    vec4(-0.047605f, -0.452653f, 0.232371f, 0.0f),
    vec4(0.144376f, 0.011569f, 0.170208f, 0.0f),
    vec4(-0.226528f, -0.114880f, 0.072839f, 0.0f),
    vec4(0.061245f, -0.551594f, 0.176373f, 0.0f),
    vec4(0.182285f, -0.384821f, 0.179972f, 0.0f),
    vec4(-0.177966f, -0.619447f, 0.443873f, 0.0f),
    vec4(0.118142f, 0.274733f, 0.601905f, 0.0f),
    vec4(-0.149286f, -0.616648f, 0.219500f, 0.0f),
    vec4(0.390443f, 0.500890f, 0.231440f, 0.0f),
    vec4(0.058805f, 0.131617f, 0.140561f, 0.0f),
    vec4(-0.080475f, -0.590698f, 0.210787f, 0.0f),
    vec4(-0.027500f, -0.096268f, 0.067165f, 0.0f),
    vec4(0.125520f, -0.485657f, 0.202268f, 0.0f),
    vec4(-0.425669f, 0.128493f, 0.061074f, 0.0f),
    vec4(-0.303480f, -0.081584f, 0.071417f, 0.0f)
);

void main()
{
    vec3 worldCamPos = pc.camera_pos;
    ivec2 textureSize = textureSize(normalInputAttachment);

    // 将UV从[-1,1]范围转换到[0,1]范围（用于G-Buffer纹理采样）
    vec2 uv = inUV * 0.5 + 0.5;

    // 从G-Buffer读取当前像素的世界坐标和法线
    // texelFetch直接按整数纹素坐标读取，不做插值
    vec3 worldPosition = texelFetch(worldPosInputAttachment, ivec2(uv*textureSize), 0).rgb;
    vec3 normal = texelFetch(normalInputAttachment, ivec2(uv*textureSize), 0).rgb;

    // 法线长度接近0说明该像素没有几何体（背景区域），直接输出白色（无遮蔽）
    if(length(normal) < 0.001){
        outColor = vec4(1, 1, 1, 1);
        return;
    }

    float originDist = length(worldPosition - worldCamPos); // 当前像素到相机的距离

    // 从随机噪声纹理采样一个随机方向
    // 用于给TBN矩阵添加随机旋转，打破SSAO的规律性条纹
    vec3 randDir = texture(samplerNoise, uv).rgb * 2.0f - 1.0f;

    // 构建以法线为上方向的切线空间（TBN矩阵）
    // Gram-Schmidt正交化：从随机方向中去除法线分量得到切线
    vec3 tangent = normalize(randDir - normal * dot(randDir, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal); // 切线->世界空间变换矩阵

    // ===== SSAO遮蔽计算 =====
    // 遍历采样核中的每个采样点，检查是否被场景几何体遮挡
	float occlusion = 0.0f;
    for(uint i = 0; i < pc.ssao_kernel_size; i++) {
        // 从预计算采样核中选取采样点（均匀间隔选取，跳过部分点以支持不同核大小）
        vec3 samplePos = TBN * ssaoKernel[i * (SSAO_WHOLE_KERNEL_SIZE / pc.ssao_kernel_size)].xyz;

        // 将采样点从切线空间变换到世界空间
        // 缩放采样点到ssao_radius半径范围内，然后偏移到当前像素的世界位置
        samplePos = samplePos * pc.ssao_radius + worldPosition;

        // 计算采样点到相机的距离（用于深度比较）
        float sampleDepth = length(samplePos - worldCamPos);

        // 将采样点投影到屏幕空间（MVP变换 -> 透视除法 -> NDC）
        vec4 samplePosProj = pc.projection * pc.view * vec4(samplePos, 1.0f);
        samplePosProj /= samplePosProj.w;

        // NDC坐标[-1,1] -> UV坐标[0,1]
        vec2 sampleUV = vec2(samplePosProj.x, samplePosProj.y) * 0.5f + 0.5f;

        // 从G-Buffer读取该屏幕位置的实际世界坐标
        vec3 sceneWorldPos = texelFetch(worldPosInputAttachment, ivec2(sampleUV*textureSize), 0).rgb;
        if(length(sceneWorldPos) < 0.001) // 跳过没有几何体的位置
            continue;
        float sceneDepth = length(sceneWorldPos - worldCamPos); // 场景实际深度

        // 范围检查（Range Check）：只在ssao_radius范围内的遮挡才计入
        // 防止远处的几何体产生不合理的遮蔽
        float rangeCheck = step(abs(sampleDepth - sceneDepth), pc.ssao_radius);

        // 如果采样点比场景更远（被遮挡），则增加遮蔽值
        // step(a, b) = (b >= a) ? 1.0 : 0.0
        occlusion += step(sceneDepth, sampleDepth) * rangeCheck;
    }

    // 计算最终遮蔽因子：1 = 完全无遮蔽（白色），0 = 完全遮蔽（黑色）
    float factor = 1 - (occlusion / float(pc.ssao_kernel_size));
    outColor = vec4(factor, factor, factor, 0); // A=0标记为需要去噪处理

    return;
}
