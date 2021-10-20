#version 450

layout(location = 0) in vec2 vertex_pos;
layout(location = 1) in vec3 vertex_color;

layout(location = 0) out vec3 fragColor;

//uniform mat4 projection;
//uniform mat4 view;
//uniform mat4 model;

void main() {
    gl_Position = vec4(vertex_pos, 0.0, 1.0);
    fragColor = vertex_color;
}