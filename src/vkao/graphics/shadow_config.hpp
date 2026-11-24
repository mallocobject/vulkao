#pragma once

#include <cstdint>

namespace vkao {
inline constexpr uint32_t CASCADE_COUNT = 4;
inline constexpr uint32_t CASCADE_TILE_SIZE = 2048;
inline constexpr uint32_t DIRECTIONAL_ATLAS_SIZE = 2 * CASCADE_TILE_SIZE;
inline constexpr float CASCADE_SPLIT_LAMBDA = 0.95f;
} // namespace vkao
