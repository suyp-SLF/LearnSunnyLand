#include "transform_component.h"
#include "../object/game_object.h"
#include <cmath> // 用于 std::abs 浮点数比对

namespace engine::component
{
    /**
     * @brief 设置位置
     * @note 仅在坐标发生变化时更新版本号，避免下游渲染系统无效重算偏移。
     */
    void TransformComponent::setPosition(const glm::vec2 &position)
    {
        if (_position == position)
            return;

        _position = position;
        _version++;
    }

    /**
     * @brief 设置缩放比例
     * @note 缩放的改变会直接影响 Sprite 的对齐偏移量（Offset）。
     */
    void TransformComponent::setScale(const glm::vec2 &scale)
    {
        if (_scale == scale)
            return;

        _scale = scale;
        _version++;
    }

    /**
     * @brief 设置旋转角度（弧度或角度，取决于引擎约定）
     */
    void TransformComponent::setRotation(float rotation)
    {
        // 浮点数建议增加微小差异判断，防止因精度误差导致的频繁版本跳变
        if (std::abs(_rotation - rotation) < 0.0001f)
            return;

        _rotation = rotation;
        _version++;
    }

    /**
     * @brief 位移增量操作
     * @param translation 移动的向量
     */
    void TransformComponent::translate(const glm::vec2 &translation)
    {
        // 位移操作通常意味着位置一定改变，直接递增版本
        _position += translation;
        _version++;
    }
} // namespace engine::component