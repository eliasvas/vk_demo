#version 450

layout (location = 0) in vec4 g_vVSColor;
layout (location = 1) in vec2 g_vVSUV;

layout (location =0) out vec4 FragColor;


layout(set = 0, binding = 1) uniform sampler2D tex_sampler;


void main()
{
	FragColor = g_vVSColor;
}
