// ============================================================================
// Prefiltered Environment Map Fragment Shader（预滤波环境贴图片元着色器）
// ============================================================================
// 作用：预计算PBR Split-Sum近似中的镜面反射环境贴图。
//       对环境cubemap按不同粗糙度级别进行GGX重要性采样滤波，
//       生成mipmap链式的预滤波环境贴图（Prefiltered Env Map）。
//
// Split-Sum近似：将镜面反射IBL拆分为两部分
//   镜面IBL = integral(Li * D * G * F * cos / (4 * NoV * NoL))
//           ≈ PrefilteredEnvMap(roughness, R) * BRDF_LUT(NoV, roughness)
//
// 本shader计算第一部分：PrefilteredEnvMap
//   - 使用GGX重要性采样（Importance Sampling）聚焦在主要反射方向
//   - roughness越高，采样范围越广（模糊程度越高）
//   - 结果存储为cubemap的mipmap链（每个mip level对应一个roughness）
//
// 流水线角色：IBL预处理阶段，离线/启动时预计算。
//             结果为带mipmap的cubemap，PBR渲染时按roughness选mip level采样。
// ============================================================================

#version 450

layout (location = 0) in vec3 inPos;     // 立方体顶点位置（即法线/反射方向N）
layout (location = 0) out vec4 outColor;  // 输出预滤波后的颜色

layout (set=0, binding = 0) uniform samplerCube samplerEnv; // 输入环境cubemap

// Push Constants：控制当前渲染哪个mip level
layout(push_constant) uniform FragPushConsts {
	layout (offset = 128) uint numMips;   // 总mip level数
	layout (offset = 132) uint targetMip; // 当前目标mip level（对应roughness）
	layout (offset = 136) uint faceIdx;   // 当前cubemap face索引
} pcFrag;

const float PI = 3.1415926536;
const uint numSamples = 1024u; // 每个像素的采样数（更多采样=更高精度）

// 伪随机数生成器（与genbrdflut.frag相同）
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

// Hammersley准随机序列（与genbrdflut.frag相同）
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

// GGX重要性采样（与genbrdflut.frag相同）
// 在法线半球上按GGX分布生成微表面法线H
// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
vec3 importanceSample_GGX(vec2 Xi, float roughness, vec3 normal)
{
	// Maps a 2D point to a hemisphere with spread based on roughness
	float alpha = roughness * roughness;
	float phi = 2.0 * PI * Xi.x + random(normal.xz) * 0.1;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha*alpha - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

	// Tangent space
	vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangentX = normalize(cross(up, normal));
	vec3 tangentY = normalize(cross(normal, tangentX));

	// Convert to world Space
	return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
}

// GGX法线分布函数（Normal Distribution Function, NDF）
// 描述微表面法线在宏观法线周围的分布密度
// dotNH: 宏观法线N与微表面法线H的余弦值
// 返回值越大，表示该方向的微表面法线越集中
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom);
}

// 预滤波环境贴图的核心计算
// R: 反射方向（对光滑表面就是法线方向N）
// roughness: 当前mip level对应的粗糙度
vec3 prefilterEnvMap(vec3 R, float roughness)
{
	vec3 N = R; // 采样法线 = 反射方向（假设视角也在同一方向）
	vec3 V = R; // 视角方向
	vec3 color = vec3(0.0);
	float totalWeight = 0.0;
	float envMapDim = float(textureSize(samplerEnv, 0).s); // 环境贴图分辨率

	for(uint i = 0u; i < numSamples; i++) {
		vec2 Xi = hammersley2d(i, numSamples);        // 生成准随机采样点
		vec3 H = importanceSample_GGX(Xi, roughness, N); // GGX重要性采样得到微表面法线H
		vec3 L = 2.0 * dot(V, H) * H - V;            // 从V和H计算反射方向L（镜面反射公式）
		float dotNL = clamp(dot(N, L), 0.0, 1.0);

		if(dotNL > 0.0) {
			// Filtering based on https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/

			float dotNH = clamp(dot(N, H), 0.0, 1.0);
			float dotVH = clamp(dot(V, H), 0.0, 1.0);

			// 概率密度函数（PDF）：GGX分布 * 几何因子
			// PDF = D_GGX(N,H) * dot(N,H) / (4 * dot(V,H))
			float pdf = D_GGX(dotNH, roughness) * dotNH / (4.0 * dotVH) + 0.0001;

			// 当前采样对应的立体角（Solid Angle）
			float omegaS = 1.0 / (float(numSamples) * pdf);

			// 环境贴图一个像素对应的立体角
			// 4*PI / (6 * dim^2)：球面总立体角 / cubemap总像素数
			float omegaP = 4.0 * PI / (6.0 * envMapDim * envMapDim);

			// 根据立体角比例计算合适的mip level
			// roughness=0时直接采样mip0（最清晰）
			// 否则使用 log2(omegaS/omegaP)/2 + 1 作为mip偏移（+1.0是经验偏移）
			float mipLevel = roughness == 0.0 ? 0.0 : max(0.5 * log2(omegaS / omegaP) + 1.0, 0.0f);

			// 用计算出的mip level采样环境贴图
			color += textureLod(samplerEnv, L, mipLevel).rgb * dotNL;
			totalWeight += dotNL;
		}
	}
	// 加权平均：除以总权重得到最终颜色
	return (color / totalWeight);
}

void main()
{
	vec3 N = normalize(inPos); // 当前像素的法线方向

	// 根据targetMip和numMips计算粗糙度
	// mip0 = roughness 0（最光滑），最后一个mip = roughness 1（最粗糙）
	float roughness = float(pcFrag.targetMip) / float(pcFrag.numMips - 1);

	vec3 result = prefilterEnvMap(N, roughness);
	outColor = vec4(result, 1);
}
