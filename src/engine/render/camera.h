#pragma once
#include "../utils/math.h"

namespace engine::render
{
    class Camera final
    {
    private:
        glm::vec2 _viewport_size;
        glm::vec2 _position;                               // 左上角坐标
        std::optional<engine::utils::FRect> _limit_bounds; // 限制范围

    public:
        Camera(const glm::vec2 &viewport_size,
               const glm::vec2 &position = glm::vec2(0.0f, 0.0f),
               const std::optional<engine::utils::FRect> limit_bounds = std::nullopt);

        void update(float delta_timer);
        void move(const glm::vec2 &offset);

        glm::vec2 worldToScreen(const glm::vec2 &world_pos) const;
        glm::vec2 worldToScreenWithParallax(const glm::vec2 &world_pos, const glm::vec2 &parallax_factor) const; // 视差滚动背景
        glm::vec2 screenToWorld(const glm::vec2 &screen_pos) const;

        void setPosition(const glm::vec2 &position);
        void setLimitBounds(const std::optional<engine::utils::FRect> &limit_bounds);

        const glm::vec2 &getPosition() const;
        std::optional<engine::utils::FRect> getLimitBounds() const; // 获取限制范围
        const glm::vec2 &getViewportSize() const;

    private:
        void clampPosition(); // 限制位置
    };
}; // namespace engine::render
