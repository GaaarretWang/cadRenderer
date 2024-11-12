// Generates an irradiance cube from an environment map using convolution

#version 450

layout (location = 0) in vec3 inPos;
layout (location = 0) out vec4 outColor;
layout (set=0, binding = 0) uniform samplerCube samplerEnv;

const float PI = 3.1415926535897932384626433832795;
const float TWO_PI = PI * 2.0;
const float HALF_PI = PI * 0.5;
const uint samples_phi = 180;
const uint samples_theta = 64;
const uint sampleCount = samples_phi * samples_theta;
const float deltaPhi = TWO_PI / float(samples_phi);
const float deltaTheta = HALF_PI / float(samples_theta); // HALF_PI / 64.0f;

void main()
{
	vec3 N = normalize(inPos);
	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, N));
	up = cross(N, right);

	vec3 color = vec3(0.0);
	for (uint iPhi = 0; iPhi < samples_phi; iPhi++) {
		float phi = deltaPhi * iPhi;
		for (uint iTheta = 0; iTheta < samples_theta; iTheta++) {
			float theta = deltaTheta * iTheta;
			vec3 tempVec = cos(phi) * right + sin(phi) * up;
			vec3 sampleVector = cos(theta) * N + sin(theta) * tempVec;
			color += textureLod(samplerEnv, sampleVector, 0).rgb * cos(theta) * sin(theta);
		}
	}
	outColor = vec4(PI * color / float(sampleCount), 1.0);
}