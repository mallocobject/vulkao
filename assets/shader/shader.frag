#version 450

layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec3 frag_pos_world;
layout(location = 2) in vec3 frag_normal_world;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform GlobalUBO {
    vec4 camera_position;
    mat4 projection_view;
    vec4 ambient_light_color;
    vec4 light_position;
    vec4 light_color;
    vec4 falloff;
} ubo;

void main() {
    vec3 dir_light = normalize(ubo.light_position.xyz - frag_pos_world);

    vec3 ambient = ubo.ambient_light_color.rgb * ubo.ambient_light_color.a;

    vec3 light_intensity = ubo.light_color.rgb * ubo.light_color.a;
    float dist = distance(ubo.light_position.xyz, frag_pos_world);
    float falloff = dot(ubo.falloff.xyz, vec3(1.f, dist, dist * dist));
    vec3 diffuse = light_intensity * max(dot(frag_normal_world, dir_light), 0) / falloff;

    vec3 dir_eye = normalize(ubo.camera_position.xyz - frag_pos_world);
    vec3 h = normalize(dir_eye + dir_light);
    vec3 specular = light_intensity * pow(max(dot(frag_normal_world, h), 0), 64) / falloff;
    
    out_color = vec4(frag_color * (ambient + diffuse + specular), 1.0);
}