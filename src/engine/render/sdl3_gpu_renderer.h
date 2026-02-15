#pragma once
#include "renderer.h"
#include <SDL3/SDL_gpu.h>
#include <vector>
namespace engine::resource
{
    class ResourceManager;
}
namespace engine::render
{
    class SDL3GPURenderer final : public Renderer
    {
    public:
        SDL3GPURenderer(SDL_Window *window);
        ~SDL3GPURenderer() override;

        void setResourceManager(engine::resource::ResourceManager* mgr);
        SDL_GPUDevice* getDevice() const { return _device; }

        // 实现基类接口
        void clearScreen() override;
        void present() override;

        // 高性能 GPU 绘制
        void drawSprite(const Camera &camera, const Sprite &sprite, const glm::vec2 &position,
                        const glm::vec2 &scale, double angle) override;
        void setDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
        void drawParallax(const Camera &camera, const Sprite &sprite, const glm::vec2 &position,
                          const glm::vec2 &scroll_factor, const glm::bvec2 &repeat,
                          const glm::vec2 &scale, double angle) override;

    private:
        // 本地缓存
        engine::resource::ResourceManager* _res_mgr = nullptr;
        SDL_GPUDevice *_device = nullptr;
        SDL_Window *_window = nullptr;

        // GPU 状态缓存
        SDL_GPURenderPass* _active_pass = nullptr;
        SDL_GPUGraphicsPipeline *_sprite_pipeline = nullptr;
        SDL_GPUCommandBuffer *_current_cmd = nullptr;
        SDL_GPUTexture* _current_swapchain_texture = nullptr; 

        void initGPU();
        void createPipeline();
    };
}