#pragma once

#include "entt.hpp"
#include "vkao_component.hpp"
#include "vkao_registry.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace vkao {
class Entity {
  public:
    Entity() = default;
    Entity(entt::entity handle, Registry *registry)
        : handle_(handle), registry_(registry) {
    }

    template <typename T, typename... Args>
    T &add_component(Args &&...args) {
        return registry_->registry().emplace<T>(handle_,
                                                std::forward<Args>(args)...);
    }

    template <typename T>
    T &get_component() {
        return registry_->registry().get<T>(handle_);
    }

    template <typename T>
    const T &get_component() const {
        return registry_->registry().get<T>(handle_);
    }

    template <typename T>
    bool has_component() const {
        return registry_->registry().all_of<T>(handle_);
    }

    template <typename T>
    T *try_get_component() {
        return registry_->registry().try_get<T>(handle_);
    }

    template <typename T>
    const T *try_get_component() const {
        return registry_->registry().try_get<T>(handle_);
    }

    template <typename T>
    void remove_component() {
        registry_->registry().remove<T>(handle_);
    }

    entt::entity handle() const {
        return handle_;
    }

    Registry *registry() const {
        return registry_;
    }

    operator entt::entity() const {
        return handle_;
    }

    operator std::uintptr_t() const {
        return static_cast<std::uintptr_t>(handle_);
    }

    // True when the handle refers to a live entity. Unlike is_set(), this also
    // catches handles left behind by Scene::destroy_entity(), which is the
    // common way an editor panel ends up with a dangling entity.
    bool valid() const {
        return handle_ != entt::null && registry_ != nullptr &&
               registry_->registry().valid(handle_);
    }
    // Only checks that the handle was assigned, without touching the registry.
    bool is_set() const {
        return handle_ != entt::null && registry_ != nullptr;
    }

    explicit operator bool() const {
        return valid();
    }

    bool operator==(const Entity &other) const {
        return handle_ == other.handle_ && registry_ == other.registry_;
    }

  private:
    entt::entity handle_{entt::null};
    Registry *registry_{nullptr};
};
} // namespace vkao