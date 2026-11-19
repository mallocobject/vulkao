#pragma once

#include <cstddef>
#include <functional>
namespace vkao {
template <typename T>
void hash_combine(size_t &seed, const T &v, auto &&...rest) {
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
}
} // namespace vkao