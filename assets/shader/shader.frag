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
    vec4 position;  // xyz = position,     w = type(0/1/2)
    vec4 direction; // xyz = direction,     w = cos(inner cone)
    vec4 falloff; // x = const, y = linear, z = quad, w = cos(outer cone)
    vec4 color; // rgb = color,     w = intensity
};


layout(std140, set = 0, binding = 1) uniform LightingUBO {
    vec4 ambient_light_color;
    Light lights[MAX_LIGHTS];
    uint light_count;
} light_data;


layout(set = 1, binding = 0) uniform sampler2D material_textures[32];

layout(std140, set = 2, binding = 0) uniform CascadedShadowUBO {
    mat4 light_view_proj[6];
    vec4 split_depths;  // 0|<--->|x|<--->|y|<--->|z|<--->|w
    mat4 camera_view;
} shadow_data;

layout(set = 2, binding = 1) uniform sampler2DShadow shadow_atlas;

layout(push_constant) uniform Push {
    mat4 model_matrix;
    vec4 normal_columns[3];
    uint texture_slot;
} push;

// PCF
float filtered_visibility(int cascade, vec2 local_uv, float receiver_depth) {
    vec2 origin = vec2(cascade % 2, cascade / 2) * 0.5;
    vec2 atlas_uv = origin + local_uv * 0.5;
    vec2 texel = 1.0 / vec2(textureSize(shadow_atlas, 0));
    float visible = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 tap = clamp(atlas_uv + vec2(x, y) * texel,
                            origin + 0.5 * texel,
                            origin + vec2(0.5) - 0.5 * texel);
            visible += textureLod(shadow_atlas, vec3(tap, receiver_depth), 0.0);
        }
    }
    return visible / 9.0;
}

float directional_visibility(vec3 world_position) {
    vec4 world = vec4(world_position, 1.0);
    float view_depth = (shadow_data.camera_view * world).z;
    int cascade = 0;
    while (cascade < 3 && view_depth > shadow_data.split_depths[cascade])
        ++cascade;
    if (view_depth > shadow_data.split_depths.w)
        return 1.0;

    vec4 clip = shadow_data.light_view_proj[cascade] * world;
    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) ||
        ndc.z < 0.0 || ndc.z > 1.0)
        return 1.0;

    return filtered_visibility(cascade, uv, ndc.z);
}

void main() {
    vec3 normal_world = normalize(frag_normal_world);
    vec3 dir_eye = normalize(camera_data.camera_position.xyz - frag_pos_world);
    vec3 ambient = light_data.ambient_light_color.rgb *
                   light_data.ambient_light_color.a;
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (uint i = 0u; i < min(light_data.light_count, MAX_LIGHTS); ++i) {
        Light light = light_data.lights[i];

        vec3 light_intensity = light.color.rgb * light.color.a;
        float type = light.position.w;
        vec3 dir_light = normalize(-light.direction.xyz);
        float attenuation = 1.0;

        if (type > 0.5) {
            vec3 to_light = light.position.xyz - frag_pos_world;
            float dist_squared = max(dot(to_light, to_light), 0.000001);
            float dist = sqrt(dist_squared);

            dir_light = to_light / dist;
            attenuation = 1.0 / max(
                dot(light.falloff.xyz, vec3(1.0, dist, dist_squared)),
                0.000001);

            if (type > 1.5) {
                float cos_angle = dot(-dir_light, normalize(light.direction.xyz));
                    attenuation *= smoothstep(
                        light.falloff.w, light.direction.w, cos_angle);
            }
        } else {
            attenuation = directional_visibility(frag_pos_world);
        }

        float n_dot_l = max(dot(normal_world, dir_light), 0.0);
        diffuse += light_intensity * n_dot_l * attenuation;

        if (n_dot_l > 0.0) {
            vec3 halfway = dir_eye + dir_light;
            vec3 h = halfway * inversesqrt(max(dot(halfway, halfway), 0.000001));
            specular += light_intensity *
                        pow(max(dot(normal_world, h), 0.0), 64.0) * attenuation;
        }
    }
    
    vec3 base_color = texture(material_textures[push.texture_slot], frag_uv).rgb * frag_color;

    out_color = vec4(base_color * (ambient + diffuse + specular), 1.0);
}
