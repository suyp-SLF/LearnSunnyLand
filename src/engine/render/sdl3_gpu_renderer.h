#pragma once
#include "renderer.h"
#include <SDL3/SDL_gpu.h>
#include <vector>

namespace engine::render
{
    class SDL3GPURenderer final : public Renderer
    {
    public:
        SDL3GPURenderer(SDL_Window *window);
        ~SDL3GPURenderer() override;

        // 实现基类接口
        void clearScreen() override;
        void present() override;
        void drawSprite(const Camera &camera, const Sprite &sprite, const glm::vec2 &position,
                        const glm::vec2 &scale, double angle) override;
        void setDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
        void drawParallax(const Camera &camera, const Sprite &sprite, const glm::vec2 &position,
                          const glm::vec2 &scroll_factor, const glm::bvec2 &repeat,
                          const glm::vec2 &scale, double angle) override;

    private:
        SDL_GPUDevice *_device = nullptr;
        SDL_Window *_window = nullptr;

        // 渲染管线：定义了 Sprite 该怎么画
        SDL_GPUGraphicsPipeline *_sprite_pipeline = nullptr;

        // 当前帧的命令缓冲
        SDL_GPUCommandBuffer *_current_cmd = nullptr;

        void initGPU();
        void createPipeline();
    };
}