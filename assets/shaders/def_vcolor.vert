#version 450

layout (location = 0) in vec3 g_vPosition;
layout (location = 1) in vec3 g_vNormal;
layout (location = 2) in vec2 g_vUV;
layout (location = 3) in vec4 g_vColor;

layout (location = 0) out vec4 g_vVSColor;
layout (location = 1) out vec2 g_vVSUV;

layout(set  = 0, binding = 0) uniform UniformBufferObject {
	mat4 WorldViewProj;
	vec4 constColor;
	float ww;
	float wh;
	float textured;
};
void main()
{

	gl_Position = WorldViewProj * vec4(g_vPosition, 1.0);
	gl_Position.xy /= vec2(ww, wh);
	gl_Position.y *= -1.0;
	gl_Position.xy = gl_Position.xy * 2 - vec2(1,1);
	g_vVSColor = g_vColor;
	g_vVSUV = g_vUV;
	
}