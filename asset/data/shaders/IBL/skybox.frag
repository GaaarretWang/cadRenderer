#version 450
#pragma import_defines (CAMERA_IMAGE)

layout (location = 0) in vec3 inUVW;
layout (set=0, binding = 0) uniform samplerCube samplerEnv;
layout (set=0, binding = 1) uniform Params {
	float exposure;
	float gamma;
	float width;
	float height;
} tonemapParams;

#ifdef CAMERA_IMAGE
layout (set=0, binding = 2) uniform sampler2D cameraImageSampler;
#endif

layout (location = 0) out vec4 outColor;

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
    vec3 dir = normalize(inUVW);
	vec3 color = textureLod(samplerEnv, dir, 0).rgb;
	// outColor = vec4(color, 1.0);
    // return;

	// Tone mapping
	color = Uncharted2Tonemap(color * tonemapParams.exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	// Gamma correction
	color = pow(color, vec3(1.0f / tonemapParams.gamma));
#ifdef CAMERA_IMAGE
	vec2 screen_uv = vec2(gl_FragCoord.x / tonemapParams.width, gl_FragCoord.y / tonemapParams.height);
	outColor = texture(cameraImageSampler, screen_uv);
#else
	outColor = vec4(color, 1.0);
#endif
}