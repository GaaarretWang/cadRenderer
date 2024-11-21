#version 450

layout (location = 0) in vec3 inPos;

layout(push_constant) uniform VertPushConsts {
	mat4 proj;
	mat4 view;
} pc;

layout (location = 0) out vec3 outUVW;

out gl_PerVertex 
{
	vec4 gl_Position;
};

void main()
{
	mat4 _proj = pc.proj;
	outUVW = vec3(inPos.x, -inPos.z, inPos.y);

	mat4 proj = pc.proj;
	proj[2][2] = 0;
	proj[3][2] = 0;
	mat4 view = pc.view;
	view[3][0] = 0;
	view[3][1] = 0;
	view[3][2] = 0;
	gl_Position = pc.proj * view * vec4(inPos.xyz * 5, 1.0);
}