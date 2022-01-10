#version 450

layout (location = 0) in vec4 g_vVSColor;
layout (location = 1) in vec2 g_vVSUV;

layout (location =0) out vec4 FragColor;

layout(set = 0, binding = 1) uniform sampler2D tex_sampler;

layout(set  = 0, binding = 0) uniform UniformBufferObject {
	mat4 WorldViewProj;
	vec4 constColor;
	float ww;
	float wh;
	float textured;
};
void main()
{

	FragColor = g_vVSColor;
	FragColor = texture(tex_sampler, g_vVSUV) * textured * g_vVSColor + g_vVSColor * (1.0 - textured);
	if (texture(tex_sampler, g_vVSUV).a < 0.01)discard;
}