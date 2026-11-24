#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec3 frag_pos_world;
layout(location = 1) out vec3 frag_color;
layout(location = 2) out vec2 frag_uv;
layout(location = 3) out vec3 frag_normal_world;

layout(std140, set = 0, binding = 0) uniform CameraUBO {
    vec4 camera_position;
    mat4 view_proj;
} camera_data;

layout(push_constant) uniform Push {
    mat4 model_matrix;
    vec4 normal_columns[3];
    uint texture_slot;
} push;

void main() {
    vec4 pos_world = push.model_matrix * vec4(pos, 1.0);
    gl_Position = camera_data.view_proj * pos_world;

    frag_pos_world = pos_world.xyz;
    frag_color = color;
    frag_uv = uv;

    mat3 normal_matrix = mat3(push.normal_columns[0].xyz,
                          push.normal_columns[1].xyz,
                          push.normal_columns[2].xyz);
    frag_normal_world = normalize(normal_matrix * normal);

}
