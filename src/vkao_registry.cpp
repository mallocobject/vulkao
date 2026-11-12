#include "vkao_registry.hpp"

#include "vkao_component.hpp"
#include "vkao_entity.hpp"

namespace vkao {
Registry::~Registry() {
    registry_.clear();
}

Entity Registry::create_entity() {
    Entity entity{registry_.create(), this};
    entity.add_component<IdComponent>(next_uid_++);
    entity.add_component<TransformComponent>();
    return entity;
}

void Registry::destroy_entity(Entity entity) {
    if (!entity || !registry_.valid(entity.handle())) {
        return;
    }
    registry_.destroy(entity.handle());
}
} // namespace vkao