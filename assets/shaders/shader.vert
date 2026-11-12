#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 v_color;

layout(push_constant) uniform Push {
    mat4 transform;
} push;

void main() {
    gl_Position = push.transform * vec4(pos, 1.0);
    v_color = normal;
}