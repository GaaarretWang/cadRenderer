// Generates an irradiance cube from an environment map using convolution

#version 450

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;
layout (set=0, binding = 0) uniform sampler2D samplerEnv;

layout(push_constant) uniform PushConstants {
    layout (offset = 128) uint faceIdx;
} pc;

#define PI 3.1415926535897932384626433832795

vec3 getSamplingVector(vec2 st)
{
    // [0,1] to [-1,1]
    vec2 uv = 2.0 * vec2(st.x, st.y) - 1.0;
    // vec2 uv = st;

    vec3 ret;
	// Select vector based on cubemap face index.
    // Sadly 'switch' doesn't seem to work, at least on NVIDIA.
    // if(pc.faceIdx == 0)      ret = vec3(1.0,  uv.y, -uv.x); //+x
    // else if(pc.faceIdx == 1) ret = vec3(-1.0, uv.y,  uv.x); //-x
    // else if(pc.faceIdx == 2) ret = vec3(uv.x, 1.0, -uv.y);  //+y
    // else if(pc.faceIdx == 3) ret = vec3(uv.x, -1.0, uv.y);  //-y
    // else if(pc.faceIdx == 4) ret = vec3(uv.x, uv.y, 1.0);   //+z
    // else if(pc.faceIdx == 5) ret = vec3(-uv.x, uv.y, -1.0); //-z

    if(pc.faceIdx == 0)      ret = vec3(-uv.x, uv.y, -1.0); //+x
    else if(pc.faceIdx == 1) ret = vec3(uv.x, uv.y, 1.0); //-x
    else if(pc.faceIdx == 2) ret = vec3(uv.y, -1.0, -uv.x);  //+y
    else if(pc.faceIdx == 3) ret = vec3(-uv.y, 1.0, -uv.x);  //-y
    else if(pc.faceIdx == 4) ret = vec3(1.0,  uv.y, -uv.x);   //+z
    else if(pc.faceIdx == 5) ret = vec3(-1.0, uv.y,  uv.x); //-z
    return normalize(ret);
}

void main()
{
	vec3 v = getSamplingVector(inUV);
	// vec3 up = vec3(0.0, 1.0, 0.0);
	// vec3 right = normalize(cross(up, N));
	// up = cross(N, right);
	const float TWO_PI = PI * 2.0;

    float phi   = atan(v.z, v.x);
	float theta = acos(v.y);

    vec2 equiRectUV = vec2(phi/TWO_PI + 0.5, theta/PI);

    vec3 color = texture(samplerEnv, equiRectUV).rgb;

    // color = vec3(equiRectUV, 0.0);
    outColor = vec4(color, 1.0);
}
