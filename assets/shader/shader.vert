#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec3 frag_pos_world;
layout(location = 1) out vec3 frag_color;
layout(location = 2) out vec2 frag_uv;
layout(location = 3) out vec3 frag_normal_world;

layout(std140, set = 0, binding = 0) uniform GlobalUBO {
    vec4 camera_position;
    mat4 view_proj;
    vec4 ambient_light_color;
    vec4 light_position;
    vec4 light_color;
    vec4 falloff;
} ubo;

layout(push_constant) uniform Push {
    mat4 model_matrix;
    mat4 normal_matrix; // for normal Model Space -> World Space
} push;

void main() {
    vec4 pos_world = push.model_matrix * vec4(pos, 1.0);
    gl_Position = ubo.view_proj * pos_world;

    frag_pos_world = pos_world.xyz;
    frag_color = color;
    frag_uv = uv;
    frag_normal_world = normalize(mat3(push.normal_matrix) * normal);

}