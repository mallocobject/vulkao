#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec3 v_color;

layout(push_constant) uniform Push {
    mat4 transform; // MVP: Projection * View * Model
    mat4 normal_matrix; // for normal Model Space -> World Space
} push;

const vec3 DIRECTION_TO_LIGHT = normalize(vec3(1.0, -3.0, -1.0));
const float AMBINET = 0.02;

void main() {
    gl_Position = push.transform * vec4(pos, 1.0);
    vec3 normal_world_space = normalize(mat3(push.normal_matrix) * normal);
    float intensity = AMBINET + max(dot(normal_world_space, DIRECTION_TO_LIGHT), 0);

    v_color = intensity * color;
}