#pragma once
#include "renderer.h"
#include <vector>

namespace engine::render
{
    class VulkanRenderer final : public Renderer
    {
    public:
        VulkanRenderer(struct SDL_Window* window);
        ~VulkanRenderer() override;

        // 实现基类接口
        void drawSprite(const Camera& camera, const Sprite& sprite, const glm::vec2& position,
                        const glm::vec2& scale, double angle) override;
        void present() override;
        void clearScreen() override;
        void setDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
    };
}