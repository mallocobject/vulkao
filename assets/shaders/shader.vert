#version 450

layout(location = 0) in vec2 postion;

void main() {
    gl_Position = vec4(postion, 0.0, 1.0);
}