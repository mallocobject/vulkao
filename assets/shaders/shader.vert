#version 450

layout(location = 0) in vec3 pos;

layout(location = 0) out vec3 v_color;

layout(push_constant) uniform Push {
    mat3 transform;
    vec3 offset;
    vec3 color;
} push;

void main() {
    gl_Position = vec4(pos + push.offset, 1.0);
}