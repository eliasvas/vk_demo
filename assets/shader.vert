#version 450

layout(location = 0) in vec2 vertex_pos;
layout(location = 1) in vec3 vertex_color;

layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;
void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(vertex_pos, 0.0, 1.0);
    fragColor = vertex_color;
}