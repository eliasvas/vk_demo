#version 450

layout(location = 0) in vec2 vertex_pos;
layout(location = 1) in vec3 vertex_color;

layout(location = 0) out vec3 fragColor;

//uniform mat4 projection;
//uniform mat4 view;
//uniform mat4 model;

vec2 positions[6] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5),
	vec2(0.7, 0.7),
	vec2(-0.7, 0.7),
	vec2(0.7, -0.3)
);

vec3 colors[6] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
	vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    gl_Position = vec4(vertex_pos, 0.0, 1.0);
    fragColor = vertex_color;
}