#version 450

layout(location = 0) in vec3 frag_pos_world;
layout(location = 1) in vec3 frag_color;
layout(location = 2) in vec2 frag_uv;
layout(location = 3) in vec3 frag_normal_world;

layout(location = 0) out vec4 out_color;

layout(std140, set = 0, binding = 0) uniform CameraUBO {
    vec4 camera_position;
    mat4 view_proj;
} camera_data;

const uint MAX_LIGHTS = 10u;

struct Light {
    vec4 position;
    vec4 direction;
    vec4 falloff;
    vec4 color;
};

layout(std140, set = 0, binding = 1) uniform LightingUBO {
    vec4 ambient_light_color;
    Light lights[MAX_LIGHTS];
    uint light_count;
} light_data;


layout(set = 1, binding = 0)
uniform sampler2D base_color_texture;

void main() {
    vec3 normal_world = normalize(frag_normal_world);
    vec3 dir_eye = normalize(camera_data.camera_position.xyz - frag_pos_world);
    vec3 ambient = light_data.ambient_light_color.rgb *
                   light_data.ambient_light_color.a;
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (uint i = 0u; i < min(light_data.light_count, MAX_LIGHTS); ++i) {
        Light light = light_data.lights[i];
        vec3 to_light = light.position.xyz - frag_pos_world;
        float dist_squared = max(dot(to_light, to_light), 0.000001);
        float dist = sqrt(dist_squared);
        vec3 dir_light = to_light / dist;
        vec3 light_intensity = light.color.rgb * light.color.a;
        float attenuation = 1.0 / max(
            dot(light.falloff.xyz, vec3(1.0, dist, dist_squared)),
            0.000001);

        float n_dot_l = max(dot(normal_world, dir_light), 0.0);
        diffuse += light_intensity * n_dot_l * attenuation;

        if (n_dot_l > 0.0) {
            vec3 halfway = dir_eye + dir_light;
            vec3 h = halfway * inversesqrt(max(dot(halfway, halfway), 0.000001));
            specular += light_intensity *
                        pow(max(dot(normal_world, h), 0.0), 64.0) * attenuation;
        }
    }
    
    vec3 base_color = texture(base_color_texture, frag_uv).rgb * frag_color;

    out_color = vec4(base_color * (ambient + diffuse + specular), 1.0);
}
