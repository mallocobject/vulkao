#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec3 v_color;

layout(set = 0, binding = 0) uniform GlobUBO {
    mat4 projection_view;
    vec3 light_direction;
} ubo;

layout(push_constant) uniform Push {
    mat4 model_matrix;
    mat4 normal_matrix; // for normal Model Space -> World Space
} push;

const float AMBINET = 0.02;

void main() {
    gl_Position = ubo.projection_view * push.model_matrix * vec4(pos, 1.0);
    vec3 normal_world_space = normalize(mat3(push.normal_matrix) * normal);
    float intensity = AMBINET + max(dot(normal_world_space, ubo.light_direction), 0);

    v_color = intensity * color;
}