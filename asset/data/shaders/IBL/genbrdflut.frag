// ============================================================================
// BRDF LUT Fragment Shader（BRDF查找表片元着色器）
// ============================================================================
// 作用：预计算PBR Split-Sum近似中的BRDF积分查找表。
//       Split-Sum近似将环境光IBL的镜面反射拆分为两部分：
//       1. 预滤波环境贴图（Prefiltered Environment Map）- 预模糊的cubemap
//       2. BRDF LUT - 存储不同(NoV, roughness)下的菲涅尔积分结果
//
//       本shader计算第2部分，生成一张2D LUT纹理。
//       输出的RG通道分别存储：(scale, bias)用于菲涅尔项近似。
//
// 算法核心（Epic Games Karis方法）：
//   - 使用Hammersley准随机序列生成半球采样点
//   - 使用GGX重要性采样（Importance Sampling）聚焦在主要贡献方向
//   - 积分 Schlick菲涅尔近似 * 几何遮蔽项 * V_dot_H / (N_H * N_V)
//
// 流水线角色：IBL预处理阶段，离线/启动时预计算一次，
//             结果存储为2D纹理供PBR fragment shader实时查找。
// ============================================================================

#version 450

layout(set = 0, binding = 0) uniform sampler2D debug;

layout (location = 0) in vec2 inUV;     // 输入UV坐标（u=NoV视角余弦，v=粗糙度）
layout (location = 0) out vec4 outColor; // 输出BRDF LUT值

// 采样数量（可通过specialization constant在编译时调整）
layout (constant_id = 0) const uint NUM_SAMPLES = 1024u;

const float PI = 3.1415926536;

// 伪随机数生成器（基于hash的快速随机数）
// 用于给Hammersley序列添加随机偏移，减少采样规律性
// Based on http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
float random(vec2 co)
{
	float a = 12.9898;
	float b = 78.233;
	float c = 43758.5453;
	float dt= dot(co.xy ,vec2(a,b));
	float sn= mod(dt,3.14);
	return fract(sin(sn) * c);
}

// Hammersley准随机序列（Hammersley Sequence）
// 生成[0,1]^2空间中分布均匀的2D采样点
// 第一个分量：均匀递增 (i/N)
// 第二个分量：Van der Corput序列的基数反转（Radical Inverse）
// 这种低差异序列（Low-discrepancy sequence）比纯随机采样收敛更快
// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
vec2 hammersley2d(uint i, uint N)
{
	uint bits = (i << 16u) | (i >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float rdi = float(bits) * 2.3283064365386963e-10;
	return vec2(float(i) /float(N), rdi);
}

// GGX重要性采样（GGX Importance Sampling）
// 根据GGX法线分布函数（NDF）的形状，在法线半球上生成采样方向
// roughness越大，采样点越分散（更粗糙的表面需要更广的采样范围）
// 返回的是微表面法线H（半角向量），不是光照方向L
//
// Xi: Hammersley序列生成的2D均匀随机数
// roughness: 表面粗糙度（0=完美镜面，1=完全粗糙）
// normal: 宏观表面法线（通常为(0,0,1)，即切线空间）
// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
vec3 importanceSample_GGX(vec2 Xi, float roughness, vec3 normal)
{
	// Maps a 2D point to a hemisphere with spread based on roughness
	// alpha = roughness^2（Epic Games的参数化方式）
	float alpha = roughness * roughness;
	float phi = 2.0 * PI * Xi.x + random(normal.xz) * 0.1;
	// GGX NDF的CDF逆函数，将均匀分布映射到GGX分布
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha*alpha - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

	// Tangent space到World space的变换
	// 构建切线空间(TBN)矩阵，将半球采样从切线空间转换到世界空间
	vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangentX = normalize(cross(up, normal));
	vec3 tangentY = normalize(cross(normal, tangentX));

	// Convert to world Space
	return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
}

// Schlick-Smith几何遮蔽函数（Geometric Shadowing Function）
// 计算微表面之间的自遮蔽效应
// 同时考虑光照方向(N·L)和视角方向(N·V)的遮蔽
// k = roughness^2 / 2（对于IBL的重映射）
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float k = (roughness * roughness) / 2.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// BRDF积分计算
// 对给定的视角余弦(NoV)和粗糙度，用重要性采样计算BRDF积分
// 返回值：x = (1-Fc) * G_Vis 用于缩放F0（基础反射率）
//         y = Fc * G_Vis       用于偏移（等效于菲涅尔项近似）
vec2 BRDF(float NoV, float roughness)
{
	// 切线空间下，法线固定为z轴(0,0,1)，视角在xz平面内
	// Normal always points along z-axis for the 2D lookup
	const vec3 N = vec3(0.0, 0.0, 1.0);
	vec3 V = vec3(sqrt(1.0 - NoV*NoV), 0.0, NoV);

	vec2 LUT = vec2(0.0);
	for(uint i = 0u; i < NUM_SAMPLES; i++) {
		vec2 Xi = hammersley2d(i, NUM_SAMPLES);
		vec3 H = importanceSample_GGX(Xi, roughness, N); // 生成微表面法线H
		vec3 L = 2.0 * dot(V, H) * H - V;               // 从V和H推导反射方向L

		float dotNL = max(dot(N, L), 0.0);
		float dotNV = max(dot(N, V), 0.0);
		float dotVH = max(dot(V, H), 0.0);
		float dotNH = max(dot(H, N), 0.0);

		if (dotNL > 0.0) {
			float G = G_SchlicksmithGGX(dotNL, dotNV, roughness); // 几何遮蔽
			float G_Vis = (G * dotVH) / (dotNH * dotNV);          // 可见性项
			float Fc = pow(1.0 - dotVH, 5.0);                     // Schlick菲涅尔近似的(1-cos)^5项
			LUT += vec2((1.0 - Fc) * G_Vis, Fc * G_Vis);
		}
	}
	return LUT / float(NUM_SAMPLES); // 取平均得到积分近似值
}

void main()
{
	// inUV.s = NoV（法线与视角的余弦值），inUV.t = roughness（粗糙度）
	outColor = vec4(BRDF(inUV.s, inUV.t), 0.0, 1.0);
}
