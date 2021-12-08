#version 450

layout(location = 0) in vec3 vertex_pos;
layout(location = 1) in vec3 vertex_normal;
layout(location = 2) in vec2 tex_coord;



layout(location = 0) out vec3 f_normal;
layout(location = 1) out vec2 f_tex_coord;


layout(set  = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
	float modifier;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(vertex_pos, 1.0);
    f_normal = mat3(transpose(inverse(ubo.model)))*vertex_normal;
	f_tex_coord = tex_coord;
}