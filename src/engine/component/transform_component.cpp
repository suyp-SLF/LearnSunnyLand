#include "transform_component.h"
#include "../object/game_object.h"

namespace engine::component
{
    void TransformComponent::setPosition(glm::vec2 &position)
    {
        _position = position; 
        _version++;
    }

    void TransformComponent::setScale(glm::vec2 &scale)
    {
        _scale = scale;
        _version++;
    }
    void TransformComponent::setRotation(float rotation)
    {
        _rotation = rotation; 
        _version++;
    }
    void TransformComponent::translate(glm::vec2 &translation)
    {
        _position += translation; 
        _version++;
    }
}