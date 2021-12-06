#version 450

layout (location = 0) in vec4 g_vPosition;
layout (location = 1) in vec2 g_vUV;


layout (location = 0) out vec4 g_vVSColor;
layout (location = 1) out vec2 g_vVSUV;

layout(set  = 0, binding = 0) uniform UniformBufferObject {
	mat4 WorldViewProj;
	vec4 constColor;
}ubo;
void main()
{
	gl_Position = ubo.WorldViewProj * g_vPosition;
}
