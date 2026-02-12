#include "transform_component.h"
#include "sprite_component.h"
#include "../object/game_object.h"

namespace engine::component
{
    void TransformComponent::setScale(glm::vec2 &scale)
    {
        _scale = scale;
        if (_owner)
        {
            auto sprite_comp = _owner->getComponent<engine::component::SpriteComponent>();
            if(sprite_comp){
                sprite_comp->updateOffset();
            }
        }
    }
}