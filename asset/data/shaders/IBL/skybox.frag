#version 450
#pragma import_defines (CAMERA_IMAGE, CAMERA_DEPTH)

layout (location = 0) in vec3 inUVW;
layout (set=0, binding = 0) uniform samplerCube samplerEnv;
layout (set=0, binding = 1) uniform Params {
	float exposure;
	float gamma;
	float width;
	float height;
	float enableRealDepthOcclusion;
	float shadowMode;
} tonemapParams;
#ifdef CAMERA_IMAGE
layout (set=0, binding = 2) uniform sampler2D cameraImageSampler;
#endif

#ifdef CAMERA_DEPTH
layout (set=0, binding = 3) uniform sampler2D depthImageSampler;

layout(push_constant) uniform PushConstants {
	mat4 proj;
	mat4 view;
	mat4 last_view;
	vec3 camera_pos;
	float softness;
	float baseBrightness;
	float ssao_radius;
	float exposure;
	float softness_falloff;
	float shadow_bias;
	int ssao_kernel_size;
	int denoise_size;
	int blocker_sample_num;
	int pcf_sample_num;
	int shadow_type;
	uint frame_num;
} pc;

#define VIEW_DESCRIPTOR_SET 1
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData {
	vec4 values[2048];
} lightData;
layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps;
layout(set = VIEW_DESCRIPTOR_SET, binding = 3) uniform sampler2DArray shadowMapsSampler;
#endif

layout (location = 0) out vec4 outColor;
#ifdef CAMERA_DEPTH
layout (location = 3) out vec4 outShadow;
#endif

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

#ifdef CAMERA_DEPTH
// Shadow helper functions (shared with shadow.frag)
#define NUM_RINGS 10
#define EPS 1e-2
#define PI2 6.283185307179586

vec2 poissonDisk[64] = {
	vec2(0.0617981, 0.07294159),
	vec2(0.6470215, 0.7474022),
	vec2(-0.5987766, -0.7512833),
	vec2(-0.693034, 0.6913887),
	vec2(0.6987045, -0.6843052),
	vec2(-0.9402866, 0.04474335),
	vec2(0.8934509, 0.07369385),
	vec2(0.1592735, -0.9686295),
	vec2(-0.05664673, 0.995282),
	vec2(-0.1203411, -0.1301079),
	vec2(0.1741608, -0.1682285),
	vec2(-0.09369049, 0.3196758),
	vec2(0.185363, 0.3213367),
	vec2(-0.1493771, -0.3147511),
	vec2(0.4452095, 0.2580113),
	vec2(-0.1080467, -0.5329178),
	vec2(0.1604507, 0.5460774),
	vec2(-0.4037193, -0.2611179),
	vec2(0.5947998, -0.2146744),
	vec2(0.3276062, 0.9244621),
	vec2(-0.6518704, -0.2503952),
	vec2(-0.3580975, 0.2806469),
	vec2(0.8587891, 0.4838005),
	vec2(-0.1596546, -0.8791054),
	vec2(-0.3096867, 0.5588146),
	vec2(-0.5128918, 0.1448544),
	vec2(0.8581337, -0.424046),
	vec2(0.1562584, -0.5610626),
	vec2(-0.7647934, 0.2709858),
	vec2(-0.3090832, 0.9020988),
	vec2(0.3935608, 0.4609676),
	vec2(0.3929337, -0.5010948),
	vec2(-0.8682281, -0.1990303),
	vec2(-0.01973724, 0.6478714),
	vec2(-0.3897587, -0.4665619),
	vec2(-0.7416366, -0.4377831),
	vec2(-0.5523247, 0.4272514),
	vec2(-0.5325066, 0.8410385),
	vec2(0.3085465, -0.7842533),
	vec2(0.8400612, -0.200119),
	vec2(0.6632416, 0.3067062),
	vec2(-0.4462856, -0.04265022),
	vec2(0.06892014, 0.812484),
	vec2(0.5149567, -0.7502338),
	vec2(0.6464897, -0.4666451),
	vec2(-0.159861, 0.1038342),
	vec2(0.6455986, 0.04419327),
	vec2(-0.7445076, 0.5035095),
	vec2(0.9430245, 0.3139912),
	vec2(0.0349884, -0.7968109),
	vec2(-0.9517487, 0.2963554),
	vec2(-0.7304786, -0.01006928),
	vec2(-0.5862702, -0.5531025),
	vec2(0.3029106, 0.09497032),
	vec2(0.09025345, -0.3503742),
	vec2(0.4356628, -0.0710125),
	vec2(0.4112572, 0.7500054),
	vec2(0.3401214, -0.3047142),
	vec2(-0.2192158, -0.6911137),
	vec2(-0.4676369, 0.6570358),
	vec2(0.6295372, 0.5629555),
	vec2(0.1253822, 0.9892166),
	vec2(-0.1154335, 0.8248222),
	vec2(-0.4230408, -0.7129914),
};

vec2 Rotate(vec2 pos, vec2 rotationTrig)
{
	return vec2(pos.x * rotationTrig.x - pos.y * rotationTrig.y, pos.y * rotationTrig.x + pos.x * rotationTrig.y);
}

float PCF_sky(vec4 coords, int shadowMapIndex, float area, float random) {
	float rotationAngle = random * 3.1415926;
	vec2 rotationTrig = vec2(cos(rotationAngle), sin(rotationAngle));

	float linearFrac = sqrt(max(area, 0.0));
	float baseStridePixels = 20.0;
	float Stride = baseStridePixels * linearFrac * pc.softness + 0.001;
	float shadowmapSize = 2048.;
	float visibility = 0.0;

	for(int i = 0; i < pc.pcf_sample_num; i++)
	{
		float res = texture(shadowMaps, vec4(coords.xy + Rotate(poissonDisk[i * 64 / pc.pcf_sample_num] * Stride / shadowmapSize, rotationTrig), shadowMapIndex, coords.z)).r;
		visibility += res;
	}
	return visibility / float(pc.pcf_sample_num);
}

vec2 fibonacciSpiralDirection[64] =
{
	vec2 (1, 0),
	vec2 (-0.7373688780783197, 0.6754902942615238),
	vec2 (0.08742572471695988, -0.9961710408648278),
	vec2 (0.6084388609788625, 0.793600751291696),
	vec2 (-0.9847134853154288, -0.174181950379311),
	vec2 (0.8437552948123969, -0.5367280526263233),
	vec2 (-0.25960430490148884, 0.9657150743757782),
	vec2 (-0.46090702471337114, -0.8874484292452536),
	vec2 (0.9393212963241182, 0.3430386308741014),
	vec2 (-0.924345556137805, 0.3815564084749356),
	vec2 (0.423845995047909, -0.9057342725556143),
	vec2 (0.29928386444487326, 0.9541641203078969),
	vec2 (-0.8652112097532296, -0.501407581232427),
	vec2 (0.9766757736281757, -0.21471942904125949),
	vec2 (-0.5751294291397363, 0.8180624302199686),
	vec2 (-0.12851068979899202, -0.9917081236973847),
	vec2 (0.764648995456044, 0.6444469828838233),
	vec2 (-0.9991460540072823, 0.04131782619737919),
	vec2 (0.7088294143034162, -0.7053799411794157),
	vec2 (-0.04619144594036213, 0.9989326054954552),
	vec2 (-0.6407091449636957, -0.7677836880006569),
	vec2 (0.9910694127331615, 0.1333469877603031),
	vec2 (-0.8208583369658855, 0.5711318504807807),
	vec2 (0.21948136924637865, -0.9756166914079191),
	vec2 (0.4971808749652937, 0.8676469198750981),
	vec2 (-0.952692777196691, -0.30393498034490235),
	vec2 (0.9077911335843911, -0.4194225289437443),
	vec2 (-0.38606108220444624, 0.9224732195609431),
	vec2 (-0.338452279474802, -0.9409835569861519),
	vec2 (0.8851894374032159, 0.4652307598491077),
	vec2 (-0.9669700052147743, 0.25489019011123065),
	vec2 (0.5408377383579945, -0.8411269468800827),
	vec2 (0.16937617250387435, 0.9855514761735877),
	vec2 (-0.7906231749427578, -0.6123030256690173),
	vec2 (0.9965856744766464, -0.08256508601054027),
	vec2 (-0.6790793464527829, 0.7340648753490806),
	vec2 (0.0048782771634473775, -0.9999881011351668),
	vec2 (0.6718851669348499, 0.7406553331023337),
	vec2 (-0.9957327006438772, -0.09228428288961682),
	vec2 (0.7965594417444921, -0.6045602168251754),
	vec2 (-0.17898358311978044, 0.9838520605119474),
	vec2 (-0.5326055939855515, -0.8463635632843003),
	vec2 (0.9644371617105072, 0.26431224169867934),
	vec2 (-0.8896863018294744, 0.4565723210368687),
	vec2 (0.34761681873279826, -0.9376366819478048),
	vec2 (0.3770426545691533, 0.9261958953890079),
	vec2 (-0.9036558571074695, -0.4282593745796637),
	vec2 (0.9556127564793071, -0.2946256262683552),
	vec2 (-0.50562235513749, 0.8627549095688868),
	vec2 (-0.2099523790012021, -0.9777116131824024),
	vec2 (0.8152470554454873, 0.5791133210240138),
	vec2 (-0.9923232342597708, 0.12367133357503751),
	vec2 (0.6481694844288681, -0.7614961060013474),
	vec2 (0.036443223183926, 0.9993357251114194),
	vec2 (-0.7019136816142636, -0.7122620188966349),
	vec2 (0.998695384655528, 0.05106396643179117),
	vec2 (-0.7709001090366207, 0.6369560596205411),
	vec2 (0.13818011236605823, -0.9904071165669719),
	vec2 (0.5671206801804437, 0.8236347091470047),
	vec2 (-0.9745343917253847, -0.22423808629319533),
	vec2 (0.8700619819701214, -0.49294233692210304),
	vec2 (-0.30857886328244405, 0.9511987621603146),
	vec2 (-0.4149890815356195, -0.9098263912451776),
	vec2 (0.9205789302157817, 0.3905565685566777)
};

vec2 ComputeFibonacciSpiralDiskSampleClumped(const in int sampleIndex, const in float sampleCountInverse, out float sampleDistNorm)
{
	sampleDistNorm = sampleIndex * sampleCountInverse;
	sampleDistNorm = sampleDistNorm * sampleDistNorm * sampleDistNorm;
	return fibonacciSpiralDirection[sampleIndex] * sampleDistNorm;
}

vec2 ComputeFibonacciSpiralDiskSampleUniform(const in int sampleIndex, const in float sampleCountInverse, const in float sampleBias, out float sampleDistNorm)
{
	sampleDistNorm = sampleIndex * sampleCountInverse + sampleBias;
	sampleDistNorm = sqrt(sampleDistNorm);
	return fibonacciSpiralDirection[sampleIndex] * sampleDistNorm;
}

void FilterScaleOffset(vec3 coord, float maxSampleZDistance, out vec2 filterScalePos, out vec2 filterScaleNeg, out vec2 filterOffset)
{
	float d = maxSampleZDistance / coord.z;
	vec2 target = (coord.xy + 0.5) * 0.5;
	filterScalePos = (1 - target) * d;
	filterScaleNeg = target * d;
	filterOffset = (target - coord.xy) * d;
}

bool BlockerSearch_Area_sky(inout float closestBlocker, float maxSampleZDistance, vec3 posTCShadowmap, vec2 minCoord, vec2 maxCoord, vec2 sampleJitter, int sampleCount, int shadowMapIndex)
{
	#define NEARPLANE 1
	maxSampleZDistance = min(1 - posTCShadowmap.z, maxSampleZDistance);
	float sampleCountInverse = 1.0f / sampleCount;

	vec2 filterScalePos, filterScaleNeg;
	vec2 filterOffset;
	FilterScaleOffset(posTCShadowmap, maxSampleZDistance, filterScalePos, filterScaleNeg, filterOffset);

	closestBlocker = NEARPLANE;
	for (int i = 0; i < sampleCount && i < 64; ++i)
	{
		float sampleDistNorm;
		vec2 offset = ComputeFibonacciSpiralDiskSampleClumped(i, sampleCountInverse, sampleDistNorm);
		offset = vec2(offset.x *  sampleJitter.y + offset.y * sampleJitter.x,
		              offset.x * -sampleJitter.x + offset.y * sampleJitter.y);
		offset = offset * vec2(offset.x > 0 ? filterScalePos.x : filterScaleNeg.x, offset.y > 0 ? filterScalePos.y : filterScaleNeg.y) + filterOffset * sampleDistNorm;
		float zoffset = maxSampleZDistance * sampleDistNorm;

		vec2 pos = posTCShadowmap.xy + offset;
		float blocker = texture(shadowMapsSampler, vec3(pos, shadowMapIndex)).r;
		if (!(pos.x < minCoord.x || pos.y < minCoord.y || pos.x > maxCoord.x || pos.y > maxCoord.y) &&
			(blocker > posTCShadowmap.z + zoffset) &&
			(closestBlocker > blocker))
		{
			closestBlocker = blocker;
		}
	}
	return NEARPLANE > closestBlocker;
}

float PCSS_Area_sky(vec3 posTCShadowmap, float maxSampleZDistance, vec2 minCoord, vec2 maxCoord, vec2 sampleJitter, int sampleCount, int shadowMapIndex)
{
	float biasFactor = 1;
	float sampleCountInverse = 1.0f / (sampleCount + biasFactor);
	float sampleBias = biasFactor * sampleCountInverse;

	vec2 filterScalePos, filterScaleNeg;
	vec2 filterOffset;
	FilterScaleOffset(posTCShadowmap, maxSampleZDistance, filterScalePos, filterScaleNeg, filterOffset);

	float sum = 0.0;
	for (int i = 0; i < sampleCount && i < 64; ++i)
	{
		float sampleDistNorm;
		vec2 offset = ComputeFibonacciSpiralDiskSampleUniform(i, sampleCountInverse, sampleBias, sampleDistNorm);
		offset = vec2(offset.x *  sampleJitter.y + offset.y * sampleJitter.x,
		              offset.x * -sampleJitter.x + offset.y * sampleJitter.y);
		offset = offset * vec2(offset.x > 0 ? filterScalePos.x : filterScaleNeg.x, offset.y > 0 ? filterScalePos.y : filterScaleNeg.y) + filterOffset * sampleDistNorm;
		float zoffset = maxSampleZDistance * sampleDistNorm;

		vec2 pos = posTCShadowmap.xy + offset;
		sum += (pos.x < minCoord.x || pos.y < minCoord.y || pos.x > maxCoord.x || pos.y > maxCoord.y) ?
				1.0 : texture(shadowMaps, vec4(pos, shadowMapIndex, posTCShadowmap.z + zoffset)).r;
	}
	return sum / sampleCount;
}

float InterleavedGradientNoise_sky(vec2 pixCoord, uint frameCount)
{
	const vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
	vec2 frameMagicScale = vec2(2.083f, 4.867f);
	pixCoord += frameCount * frameMagicScale;
	return fract(magic.z * fract(dot(pixCoord, magic.xy)));
}

float PenumbraSizePunctual_sky(float Reciever, float Blocker)
{
	return abs((Reciever - Blocker) / Blocker);
}

float SampleShadow_PCSS_Area_sky(vec3 posTCShadowmap, vec2 posSS, float shadowSoftness, float minFilterRadius, int blockerSampleCount, int filterSampleCount, float depthBias, int shadowMapIndex, float area)
{
	posTCShadowmap.z += depthBias;

	float maxSampleZDistance = shadowSoftness * 0.1 * sqrt(area);
	float sampleJitterAngle = InterleavedGradientNoise_sky(posSS.xy, pc.frame_num) * 2.0 * 3.14159265359;
	vec2 sampleJitter = vec2(sin(sampleJitterAngle), cos(sampleJitterAngle));
	vec2 minCoord = vec2(0);
	vec2 maxCoord = vec2(1);

	float blocker = 0.0;
	bool blockerFound = BlockerSearch_Area_sky(blocker, maxSampleZDistance, posTCShadowmap, minCoord, maxCoord, sampleJitter, blockerSampleCount, shadowMapIndex);

	maxSampleZDistance *= PenumbraSizePunctual_sky(posTCShadowmap.z, blocker);
	maxSampleZDistance = min(maxSampleZDistance, (blocker - posTCShadowmap.z) * 0.9);
	maxSampleZDistance = max(maxSampleZDistance, minFilterRadius / 100);

	bool withinShadowmap = posTCShadowmap.x > 0 && posTCShadowmap.y > 0 && posTCShadowmap.x < 1 && posTCShadowmap.y < 1;
	return blockerFound && withinShadowmap ? PCSS_Area_sky(posTCShadowmap, maxSampleZDistance, minCoord, maxCoord, sampleJitter, filterSampleCount, shadowMapIndex) : 1.0f;
}

float ValueNoise_sky(vec3 pos)
{
	vec3 Noise_skew = pos + 0.2127 + pos.x * pos.y * pos.z * 0.3713;
	vec3 Noise_rnd = 4.789 * sin(489.123 * (Noise_skew));
	return fract(Noise_rnd.x * Noise_rnd.y * Noise_rnd.z * (1.0 + Noise_skew.x) * pc.frame_num);
}

float computeShadowBrightness(vec3 worldPos)
{
	float brightnessCutoff = 0.001;

	vec4 lightNums = lightData.values[0];
	int numAmbientLights = int(lightNums[0]);
	int numDirectionalLights = int(lightNums[1]);
	int numPointLights = int(lightNums[2]);
	int numSpotLights = int(lightNums[3]);
	int index = 1;

	float scene_brightness = 1.0;
	int shadowMapIndex = 0;
	if (numDirectionalLights > 0)
	{
		float totalBrightness = pc.baseBrightness;
		float totalRealBrightness = pc.baseBrightness;

		for(int i = 0; i < numDirectionalLights; ++i)
		{
			vec4 lightColor = lightData.values[index++];
			float area = lightData.values[index].w;
			vec3 direction = -lightData.values[index++].xyz;
			vec4 shadowMapSettings = lightData.values[index++];

			float brightness = lightColor.a;
			totalBrightness += brightness;
			float visibility = 0.0;

			bool matched = false;
			while ((shadowMapSettings.r > 0.0 && brightness > brightnessCutoff) && !matched)
			{
				mat4 sm_matrix = mat4(lightData.values[index++],
				                      lightData.values[index++],
				                      lightData.values[index++],
				                      lightData.values[index++]);

				vec4 sm_tc = sm_matrix * vec4(worldPos, 1.0);

				if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0)
				{
					matched = true;
					float random = ValueNoise_sky(sm_tc.xyz);
					if(pc.shadow_type == 0){
						visibility = PCF_sky(sm_tc, shadowMapIndex, area, random);
					} else if(pc.shadow_type == 1){
						visibility = SampleShadow_PCSS_Area_sky(sm_tc.xyz, vec2(gl_FragCoord.xy), pc.softness, pc.softness_falloff, pc.blocker_sample_num, pc.pcf_sample_num, pc.shadow_bias, shadowMapIndex, area);
					}
				} else {
					visibility = 1.0;
				}

				++shadowMapIndex;
				shadowMapSettings.r -= 1.0;
			}

			if (shadowMapSettings.r > 0.0)
			{
				index += 4 * int(shadowMapSettings.r);
				shadowMapIndex += int(shadowMapSettings.r);
			}

			totalRealBrightness += brightness * visibility;
		}
		scene_brightness = totalRealBrightness / totalBrightness;
	}

	// 跳过ambient lights
	index = 1 + numAmbientLights;
	return scene_brightness;
}
#endif

void main()
{
#ifdef CAMERA_IMAGE
	vec2 screen_uv = vec2(gl_FragCoord.x / tonemapParams.width, gl_FragCoord.y / tonemapParams.height);	
	float scene_brightness = 1.0;
	#ifdef CAMERA_DEPTH
		vec2 depth_uv = vec2(gl_FragCoord.x / tonemapParams.width, gl_FragCoord.y / tonemapParams.height);
		gl_FragDepth = 0.0;
		float camera_depth_norm = texture(depthImageSampler, depth_uv).r;
		float linear_z = camera_depth_norm * 65.535;
		if(linear_z > 0 && linear_z < 65.535) {
			float ndc_x = depth_uv.x * 2.0 - 1.0;
			float ndc_y = depth_uv.y * 2.0 - 1.0;
			// Reversed-Z: far plane maps to ndc z = 0 in Vulkan clip space.
			vec4 ndc_point = vec4(ndc_x, ndc_y, 0.0, 1.0);
			mat4 inv_proj = inverse(pc.proj);
			vec4 view_ray = inv_proj * ndc_point;
			view_ray /= view_ray.w;
			vec3 ray_dir = normalize(view_ray.xyz);
			float scale = -linear_z / ray_dir.z;
			vec3 actual_view = ray_dir * scale;

			if (tonemapParams.enableRealDepthOcclusion > 0.5) {
				vec4 clip_out = pc.proj * vec4(actual_view, 1.0);
				gl_FragDepth = clip_out.z / clip_out.w;
			}
			if (tonemapParams.shadowMode > 0.5 && tonemapParams.shadowMode < 1.5) {
				mat4 inv_view = inverse(pc.view);
				vec3 worldPos = (inv_view * vec4(actual_view, 1.0)).xyz;
				scene_brightness = computeShadowBrightness(worldPos);
			}
		}
		outShadow = vec4(scene_brightness, -10000000.0, gl_FragDepth, 1.0);
	#endif
	outColor = texture(cameraImageSampler, screen_uv) * scene_brightness;
#else
    vec3 dir = inUVW;
	vec3 color = textureLod(samplerEnv, dir, 0).rgb;

	// Tone mapping
	color = Uncharted2Tonemap(color * tonemapParams.exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
	// Gamma correction
	color = pow(color, vec3(1.0f / tonemapParams.gamma));
	outColor = vec4(color, 1.0);
#endif
}
