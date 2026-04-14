// ============================================================================
// Irradiance Convolution Fragment Shader（辐照度卷积片元着色器）
// ============================================================================
// 作用：对环境贴图（Cubemap）进行辐照度卷积（Irradiance Convolution），
//       生成漫反射IBL（Image-Based Lighting）使用的Irradiance Cubemap。
//
// 原理：漫反射表面从半球所有方向接收光照并均匀散射。
//       对环境贴图的每个方向N，计算半球内所有方向L的光照加权平均值：
//       Irradiance(N) = (1/PI) * integral(Li * max(dot(N,L), 0) * dL)
//       其中权重cos(theta)*sin(theta)来自球面积分的立体角微分元素。
//
// 为什么需要卷积：
//   - 原始环境贴图包含高频率细节（强光、反射等）
//   - 漫反射表面会"模糊"这些细节（半球积分效果）
//   - 预计算卷积后，运行时只需一次cubemap采样即可获得漫反射光照
//
// 流水线角色：IBL预处理阶段。将环境cubemap卷积为低频的irradiance cubemap，
//             PBR渲染时直接采样此贴图作为漫反射环境光。
// ============================================================================

#version 450

layout (location = 0) in vec3 inPos;     // 立方体顶点位置（即采样方向）
layout (location = 0) out vec4 outColor;  // 输出卷积后的辐照度颜色
layout (set=0, binding = 0) uniform samplerCube samplerEnv; // 输入环境cubemap

const float PI = 3.1415926535897932384626433832795;
const float TWO_PI = PI * 2.0;
const float HALF_PI = PI * 0.5;

// 采样参数：在phi方向采180个点，theta方向采64个点
// 总共180*64=11520个采样点（精度和性能的平衡）
const uint samples_phi = 180;
const uint samples_theta = 64;
const uint sampleCount = samples_phi * samples_theta;
const float deltaPhi = TWO_PI / float(samples_phi);     // phi步长 = 2*PI/180
const float deltaTheta = HALF_PI / float(samples_theta); // theta步长 = PI/2/64

void main()
{
	// N: 当前像素对应的法线方向（即cubemap采样方向）
	vec3 N = normalize(inPos);

	// 构建以N为法线的局部坐标系（TBN空间）
	// right和up定义了与N垂直的切平面
	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, N));
	up = cross(N, right);

	vec3 color = vec3(0.0);
	// 双重循环遍历半球上的所有采样方向
	// iPhi: 方位角（绕法线旋转0-360度）
	// iTheta: 极角（从法线偏离0-90度）
	for (uint iPhi = 0; iPhi < samples_phi; iPhi++) {
		float phi = deltaPhi * iPhi; // 当前方位角
		for (uint iTheta = 0; iTheta < samples_theta; iTheta++) {
			float theta = deltaTheta * iTheta; // 当前极角

			// 在切平面上旋转得到tempVec方向
			vec3 tempVec = cos(phi) * right + sin(phi) * up;
			// 从法线N向tempVec方向倾斜theta角，得到最终采样方向
			vec3 sampleVector = cos(theta) * N + sin(theta) * tempVec;

			// 累加环境光照，权重 = cos(theta) * sin(theta)
			// cos(theta): Lambert余弦定律（斜射光强度降低）
			// sin(theta): 球坐标微分面积元素 dA = sin(theta) * dTheta * dPhi
			color += textureLod(samplerEnv, sampleVector, 0).rgb * cos(theta) * sin(theta);
		}
	}
	// 乘以PI除以采样数：近似积分值
	// Lambert BRDF = 1/PI，但这里与积分合并后简化为 PI/sampleCount
	outColor = vec4(PI * color / float(sampleCount), 1.0);
}
