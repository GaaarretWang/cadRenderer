// ============================================================================
// Skybox Background Fragment Shader（天空盒背景片元着色器）
// ============================================================================
// 作用：渲染天空盒背景，对cubemap环境贴图进行采样，
//       并应用Uncharted2色调映射和Gamma校正。
//
// 天空盒渲染流程：
//   1. vertex shader将立方体顶点方向作为3D纹理坐标传递过来
//   2. fragment shader用此方向采样cubemap
//   3. 对HDR颜色应用色调映射压缩到LDR范围
//   4. 应用Gamma校正输出到显示器
//
// 流水线角色：场景渲染的最后阶段（天空盒在所有不透明几何体之后渲染）。
//             使用cubemap而非equirectangular贴图，因为cubemap采样更高效。
// ============================================================================

#version 450

layout (location = 0) in vec3 inUVW; // 3D采样方向（来自vertex shader的立方体顶点方向）

// Cubemap环境贴图（存储预计算的HDR环境光照）
layout (set=0, binding = 0) uniform samplerCube samplerEnv;

// 色调映射参数（从CPU端传入）
layout (set=0, binding = 1) uniform Params {
	float exposure; // 曝光度（控制整体亮度）
	float gamma;    // Gamma值（控制亮度曲线，默认2.2）
} tonemapParams;

layout (location = 0) out vec4 outColor; // 输出最终颜色

// Uncharted2色调映射函数（Filmic Tonemapping）
// 模拟真实相机的曝光响应曲线：
//   - 低光区域保持线性
//   - 高光区域平滑压缩到白色
//   - W=11.2为白色参考点
// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

void main()
{
    vec3 dir = normalize(inUVW); // 归一化采样方向

	// 从cubemap采样环境颜色（mip level 0 = 最高分辨率）
	vec3 color = textureLod(samplerEnv, dir, 0).rgb;

	// ===== 色调映射（Tone Mapping） =====
	// 1. 先乘以曝光度调整亮度
	color = Uncharted2Tonemap(color * tonemapParams.exposure);
	// 2. 除以纯白色(11.2)的映射值进行归一化
	//    确保最亮的颜色映射到1.0（白色）
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

	// ===== Gamma校正（Gamma Correction） =====
	// 线性空间渲染的颜色需要转换到sRGB色彩空间
	// pow(color, 1/gamma) 应用gamma曲线（默认gamma=2.2）
	color = pow(color, vec3(1.0f / tonemapParams.gamma));

	outColor = vec4(color, 1.0);
}
