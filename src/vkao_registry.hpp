#pragma once

#include "entt.hpp"
#include "ref.hpp"

namespace vkao {
class Entity;

class Registry : public RefCounted {
  public:
    Registry() = default;
    ~Registry();

    Registry(const Registry &) = delete;

    Entity create_entity();

    void destroy_entity(Entity entity);

    entt::registry &registry() {
        return registry_;
    }
    const entt::registry &registry() const {
        return registry_;
    }

    template <typename... T>
    auto view() {
        return registry_.view<T...>();
    }

  private:
    uint32_t next_uid_{1};
    entt::registry registry_;
};
} // namespace vkao
