#version 450

//layout(location = 0) in vec2 vertex_pos;
//layout(location = 1) in vec3 vertex_color;

layout (location = 0) out vec3 f_color;
layout (location = 1) out vec2 f_tex_coord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);
void main() {
    f_tex_coord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	f_color = colors[gl_VertexIndex];
    gl_Position = vec4(f_tex_coord * 2.0f + -1.0f, 0.0f, 1.0f);
}