#version 450
#extension GL_ARB_shader_viewport_layer_array : require

layout(location = 0) in vec3 pos;

layout(std140, set = 0, binding = 0) uniform ShadowUBO {
    mat4 light_view_proj[4];
} shadow_data;

layout(push_constant) uniform Push {
    mat4 model_matrix;
} push;

void main() {
    int cascade = gl_InstanceIndex;

    gl_ViewportIndex = cascade;
    gl_Position = shadow_data.light_view_proj[cascade]
                * push.model_matrix
                * vec4(pos, 1.0);
}
