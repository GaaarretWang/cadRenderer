#version 450

layout (location = 0) in vec3 inPos;

layout(push_constant) uniform PushConsts {
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
	vec3 _inPos = inPos;
	// vsg::perspective flips y, hack projection matrix here
	_proj[1][1] = -_proj[1][1];
	outUVW = _inPos.xyz;
	gl_Position = _proj * pc.view * vec4(_inPos.xyz, 1.0);
}