#include "graphics/cascaded_shadow.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/quaternion_common.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "graphics/shadow_config.hpp"
#include <algorithm>
#include <cmath>

namespace vkao {
CascadeArray calculate_cascades(const glm::mat4 &camera_view,
                                const glm::mat4 &camera_projection,
                                float camera_near,
                                float camera_far,
                                const glm::vec3 &light_direction) {
    CascadeArray cascade_array{};

    float cascade_split_ratios[CASCADE_COUNT]{};

    float n = camera_near;
    float f = camera_far;
    float ratio = f / n;
    float range = f - n;
    float inv_N = 1.f / static_cast<float>(CASCADE_COUNT);

    for (int i = 0; i < CASCADE_COUNT; i++) {
        float p = (i + 1) * inv_N;
        float log_split = n * std::pow(ratio, p);
        float uniform_split = n + range * p;
        float d =
            CASCADE_SPLIT_LAMBDA * (log_split - uniform_split) + uniform_split;
        cascade_split_ratios[i] = (d - n) / range;
    }

    const glm::mat4 inverse_vp = glm::inverse(camera_projection * camera_view);

    glm::vec3 frustum_corners[8]{
        // near plane
        glm::vec3{-1.f, 1.f, 0.f},
        glm::vec3{1.f, 1.f, 0.f},
        glm::vec3{1.f, -1.f, 0.f},
        glm::vec3{-1.f, -1.f, 0.f},
        // far plane
        glm::vec3{-1.f, 1.f, 1.f},
        glm::vec3{1.f, 1.f, 1.f},
        glm::vec3{1.f, -1.f, 1.f},
        glm::vec3{-1.f, -1.f, 1.f},
    };

    for (int i = 0; i < 8; i++) {
        glm::vec4 ndc = glm::vec4{frustum_corners[i], 1.f}; // clip space
        glm::vec4 world = inverse_vp * ndc;
        frustum_corners[i] = glm::vec3{world / world.w};
    }

    float last_split = 0.f;
    for (int i = 0; i < CASCADE_COUNT; i++) {
        float split = cascade_split_ratios[i];

        glm::vec3 cascade_corners[8];
        for (int j = 0; j < 4; j++) {
            glm::vec3 ray_near = frustum_corners[j + 4] - frustum_corners[j];
            cascade_corners[j] = frustum_corners[j] + ray_near * last_split;
            cascade_corners[j + 4] = frustum_corners[j] + ray_near * split;
        }

        glm::vec3 center{0.f};
        for (const auto &corner : cascade_corners) {
            center += corner;
        }
        center /= 8.f;

        float radius = 0.f;
        for (const auto &corner : cascade_corners) {
            radius = std::max(radius, glm::distance(center, corner));
        }
        radius = std::ceil(radius * 16.f) / 16.f;

        glm::vec3 light_pos = center - glm::normalize(light_direction) * radius;
        glm::mat4 light_view =
            glm::lookAtRH(light_pos, center, glm::vec3{0.f, -1.f, 0.f});

        glm::mat4 light_proj = glm::orthoRH_ZO(
            -radius, radius, radius, -radius, 0.f, radius * 2.f);

        constexpr float tile_resolution = static_cast<float>(CASCADE_TILE_SIZE);
        glm::mat4 light_vp = light_proj * light_view;

        const glm::vec2 origin_ndc =
            glm::vec2(light_vp * glm::vec4(0.f, 0.f, 0.f, 1.f));
        const glm::vec2 origin_texel = origin_ndc * (tile_resolution * 0.5f);
        const glm::vec2 offset_ndc =
            (glm::round(origin_texel) - origin_texel) * (2.f / tile_resolution);

        light_proj[3][0] += offset_ndc.x;
        light_proj[3][1] += offset_ndc.y;

        cascade_array[i].light_view_proj = light_proj * light_view;
        cascade_array[i].split_depth = (n + split * range);

        last_split = split;
    }

    return cascade_array;
}
} // namespace vkao
