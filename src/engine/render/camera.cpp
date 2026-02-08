#include "camera.h"
#include <spdlog/spdlog.h>

namespace engine::render
{
    Camera::Camera(const glm::vec2 &viewport_size, const glm::vec2 &position, const std::optional<engine::utils::FRect> limit_bounds)
        : _viewport_size(viewport_size),
          _position(position),
          _limit_bounds(limit_bounds)
    {
        spdlog::trace("Camera 初始化成功，位置: {}, 限制边界: {}， 大小: {}", _position, limit_bounds, viewport_size);
    }

    void Camera::update(float delta_timer)
    {
        // to do
    }

    void Camera::move(const glm::vec2 &offset)
    {
        _position += offset;
        clampPosition();
    }

    glm::vec2 Camera::worldToScreen(const glm::vec2 &world_pos) const
    {
        return world_pos - _position;
    }

    glm::vec2 Camera::worldToScreenWithParallax(const glm::vec2 &world_pos, const glm::vec2 &parallax_factor) const
    {
        return world_pos - _position * parallax_factor;
    }

    glm::vec2 Camera::screenToWorld(const glm::vec2 &screen_pos) const
    {
        return screen_pos + _position;
    }

    void Camera::setPosition(const glm::vec2 &position)
    {
        _position = position;
    }

    void Camera::setLimitBounds(const std::optional<engine::utils::FRect> &limit_bounds)
    {
        _limit_bounds = limit_bounds;
        clampPosition();
    }

    const glm::vec2 &Camera::getPosition() const
    {
        return _position;
    }

    std::optional<engine::utils::FRect> Camera::getLimitBounds() const
    {
        return std::optional<engine::utils::FRect>();
    }

    const glm::vec2 &Camera::getViewportSize() const
    {
        return _viewport_size;
    }

    void Camera::clampPosition()
    {
        // 检查限制边界是否有效
        if (_limit_bounds.has_value() && _limit_bounds->size.x > 0 && _limit_bounds->size.y > 0)
        {
            // 计算相机位置在限制边界内的最大和最小值
            glm::vec2 min_pos = _limit_bounds->position;
            glm::vec2 max_pos = _limit_bounds->position + _limit_bounds->size - _viewport_size;

            // 将相机位置限制在最大和最小值之间
            max_pos.x = std::max(max_pos.x, min_pos.x);
            max_pos.y = std::max(max_pos.y, min_pos.y);

            _position.x = std::clamp(_position.x, min_pos.x, max_pos.x);
        }
    }
}