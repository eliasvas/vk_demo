#version 450

layout (location = 0) in vec3 g_vPosition;
layout (location = 1) in vec3 g_vNormal;
layout (location = 2) in vec2 g_vUV;


layout (location = 0) out vec4 g_vVSColor;
layout (location = 1) out vec2 g_vVSUV;

layout(set  = 0, binding = 0) uniform UniformBufferObject {
	mat4 WorldViewProj;
	vec4 constColor;
	float ww;
	float wh;
	float textured;
}ubo;
void main()
{

	gl_Position = ubo.WorldViewProj * vec4(g_vPosition, 1.0);
	gl_Position.xy /= vec2(ubo.ww, ubo.wh);
	gl_Position.y *= -1.0;
	gl_Position.xy = gl_Position.xy * 2 - vec2(1,1);
	g_vVSColor = ubo.constColor;
	g_vVSUV = g_vUV;
	
}