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
        // 增加一个小优化：如果值没变，就不增加版本号，避免下游系统重复计算
        if (_scale == scale) return;
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
    void TransformComponent::update(float delta_time)
    {
        // 在主循环或某个 System 中
        setRotation(delta_time * 50.0f + getRotation());                       // 旋转角度随时间变化
    }
}