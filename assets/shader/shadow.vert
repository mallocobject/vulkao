#version 450

layout(location = 0) in vec3 pos;

layout(std140, set = 0, binding = 0) uniform ShadowUBO {
    mat4 light_view_proj;
} shadow_data;

layout(push_constant) uniform Push {
    mat4 model_matrix;
} push;

void main() {
    gl_Position = shadow_data.light_view_proj
                * push.model_matrix
                * vec4(pos, 1.0);
}