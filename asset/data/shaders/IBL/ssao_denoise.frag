// ============================================================================
// SSAO Denoise Fragment Shader（SSAO去噪与最终合成片元着色器）
// ============================================================================
// 作用：对原始SSAO遮蔽值进行均值滤波去噪，并与场景颜色进行最终合成。
//       同时输出阴影贴图（Shadow Map）的结果。
//
// 本shader执行两个任务：
//   1. SSAO去噪 + 色调映射合成：
//      - 使用NxN窗口均值滤波平滑SSAO噪声
//      - 将遮蔽因子乘到场景颜色上
//      - 应用Uncharted2色调映射和Linear->sRGB转换
//   2. 阴影输出：
//      - 从子通道输入附件中读取阴影数据
//      - 输出给后续使用
//
// 流水线角色：SSAO后处理的第二阶段（最终合成阶段）。
//             使用Vulkan Subpass机制，直接从同一render pass中的
//             其他attachment读取数据，无需额外的纹理绑定。
// ============================================================================

#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_METALLROUGHNESS_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, VSG_WORKFLOW_SPECGLOSS, SHADOWMAP_DEBUG)

#define MATERIAL_DESCRIPTOR_SET 2

// 子通道输入附件（Subpass Input Attachment）
// input_attachment_index: 在render pass的input attachment列表中的索引
// 这些是同一render pass内其他subpass写入的attachment

// 颜色附件输入（法线/颜色数据，Multisampled用于MSAA）
layout(
    input_attachment_index = 1,  // 强制要求：子通道输入附件列表中的索引
    set = MATERIAL_DESCRIPTOR_SET,  // 和 CPU 侧一致（比如 2）
    binding = 0  // 和 CPU 侧一致（比如 0）
) uniform subpassInputMS colorInputAttachment;  // color attachment 1（法线）

// 阴影附件输入（阴影数据，Multisampled）
layout(
    input_attachment_index = 2,  // 强制要求：子通道输入附件列表中的索引
    set = MATERIAL_DESCRIPTOR_SET,  // 和 CPU 侧一致（比如 2）
    binding = 1  // 和 CPU 侧一致（比如 0）
) uniform subpassInputMS shadowInputAttachment;  // color attachment 1（法线）

// SSAO遮蔽值纹理（由ssao.frag生成）
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2DMS samplerSSAO;

layout(location = 0) in vec2 inUV; // 全屏四边形UV坐标

layout(location = 0) out vec4 outColor;  // 输出最终颜色（去噪+色调映射+sRGB）
layout(location = 1) out vec4 outShadow; // 输出阴影

// Push Constants（推送常量）
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
    mat4 last_view;
    vec3 camera_pos;
    float softness;
    float baseBrightness;
    float ssao_radius;
    float exposure;        // 曝光度（用于色调映射）
    float softness_falloff;
    float shadow_bias;
    int ssao_kernel_size;
    int denoise_size;      // 去噪滤波窗口大小（NxN）
    int blocker_sample_num;
    int pcf_sample_num;
    int shadow_type;
    uint frame_num;
} pc;

// Uncharted2色调映射函数（Filmic Tonemapping）
// 将HDR线性颜色压缩到[0,1]范围，模拟真实相机/人眼的响应曲线
// A-F为曲线控制参数，W为白色参考点
// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// Linear到sRGB颜色空间转换
// 显示器期望sRGB输入，但渲染在线性空间进行
// 需要应用gamma 1/2.2曲线进行转换
vec4 LINEARtoSRGB(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(1.0 / 2.2));
    return vec4(linOut, srgbIn.w);
}

void main()
{
    // 从子通道输入附件读取场景颜色（subpassLoad直接从同一render pass读取）
    vec4 colorAttachment = subpassLoad(colorInputAttachment, gl_SampleID);
    vec3 colorData = colorAttachment.xyz;
    vec2 uv = inUV * 0.5 + 0.5; // UV范围从[-1,1]转换到[0,1]

    // ===== SSAO去噪：均值滤波（Box Filter） =====
    // 使用NxN窗口对SSAO遮蔽值进行均值滤波，平滑噪声
    ivec2 textureSize = textureSize(samplerSSAO);
    vec2 texelSize = 1.0 / vec2(textureSize); // 单个纹素的UV大小
    float result = 0.0;
    int denoise_size = pc.denoise_size / 2; // 半径（窗口从-denoi到+denoi）
    for (int x = -denoise_size; x <= denoise_size; ++x)
    {
        for (int y = -denoise_size; y <= denoise_size; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            // texelFetch：按整数纹素坐标精确采样（不插值）
            result += texelFetch(samplerSSAO, ivec2((uv + offset)*textureSize), 0).r;
        }
    }

    // 根据SSAO的alpha通道判断是否需要应用SSAO
    // alpha > 0.5 表示该像素不应受SSAO影响（如天空/背景区域），直接输出原始颜色
    if(texelFetch(samplerSSAO, ivec2(uv*textureSize), 0).w > 0.5){
        outColor = vec4(colorData, 1);
    }else{
        // 应用SSAO遮蔽 + 色调映射 + sRGB转换
        float exposure = pc.exposure;
        // 遮蔽因子(均值滤波后) * 场景颜色 * 曝光度
        vec3 color = Uncharted2Tonemap(colorData * result / (denoise_size * 2 + 1) / (denoise_size * 2 + 1) * exposure);
        // 归一化：除以纯白色经过色调映射后的值，确保白色保持为白色
        color = color * (vec3(1.0f) / Uncharted2Tonemap(vec3(11.2f)));
        // Linear -> sRGB转换
        outColor = LINEARtoSRGB(vec4(color, 1));
    }

    // 输出阴影数据（从子通道输入附件读取）
    vec4 shadow = subpassLoad(shadowInputAttachment, gl_SampleID);
    outShadow = vec4(shadow.rgb, 1);
    return;
}
