#include "sdl3_gpu_renderer.h"
#include "../render/sprite.h"             // 否则无法调用 sprite.getTextureId()
#include "../core/context.h"              // 否则无法识别 Context
#include "../resource/resource_manager.h" //
namespace engine::render
{
    SDL3GPURenderer::SDL3GPURenderer(SDL_Window *window) : _window(window)
    {
        initGPU();
    }
    SDL3GPURenderer::~SDL3GPURenderer()
    {
    }
    void SDL3GPURenderer::initGPU()
    {
        // 1. 创建 GPU 设备（在 Mac 上会自动选择 Metal，Win 选择 DX12/Vulkan）
        _device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, true, nullptr);

        // 2. 声明窗口声明周期由该 GPU 设备管理
        SDL_ClaimWindowForGPUDevice(_device, _window);

        createPipeline();
    }
    void SDL3GPURenderer::setDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        // GPU 渲染通常在 BeginGPURenderPass 的 ClearColor 中处理背景色
        // 这里可以暂存颜色，用于后续的绘图填充
    }
    void SDL3GPURenderer::drawParallax(const Camera &camera, const Sprite &sprite, const glm::vec2 &position,
                                       const glm::vec2 &scroll_factor, const glm::bvec2 &repeat,
                                       const glm::vec2 &scale, double angle)
    {
        // 逻辑类似于 drawSprite，但需要应用滚动因子
        drawSprite(camera, sprite, position * scroll_factor, scale, angle);
    }
    void SDL3GPURenderer::clearScreen()
    {
    }
    void SDL3GPURenderer::present()
    {
    }
    void SDL3GPURenderer::drawSprite(const Camera &camera, const Sprite &sprite, const glm::vec2 &position, const glm::vec2 &scale, double angle)
    {
        if (!_current_cmd)
            return;

        // 1. ⚡️ 获取 Swapchain Texture (修复参数不兼容问题)
        SDL_GPUTexture *swapchain_tex = nullptr;
        uint32_t w, h;
        if (!SDL_AcquireGPUSwapchainTexture(_current_cmd, _window, &swapchain_tex, &w, &h))
        {
            return;
        }

        // 2. 配置渲染通道
        SDL_GPUColorTargetInfo color_target = {};
        color_target.texture = swapchain_tex;
        color_target.clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
        color_target.load_op = SDL_GPU_LOADOP_LOAD;
        color_target.store_op = SDL_GPU_STOREOP_STORE;

        // ⚡️ 注意：在实际引擎中，BeginRenderPass 应该在整个渲染流程的最开始调用一次
        SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(_current_cmd, &color_target, 1, nullptr);

        if (_sprite_pipeline)
        {
            SDL_BindGPUGraphicsPipeline(render_pass, _sprite_pipeline);

            // 3. 绑定纹理 (从 ResourceManager 获取 GPU 纹理)
            if (engine::core::Context::Current)
            {
                auto &rm = engine::core::Context::Current->getResourceManager();
                SDL_GPUTexture *gpu_tex = rm.getGPUTexture(sprite.getTextureId()); // 获取 GPU 纹理
                SDL_GPUTextureSamplerBinding texture_binding = {};
                texture_binding.texture = gpu_tex;
                // 注意：通常还需要绑定一个 Sampler
                // texture_binding.sampler = _default_sampler;
                SDL_BindGPUFragmentSamplers(render_pass, 0, &texture_binding, 1);
            }

            // 4. 提交绘制
            SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);
        }

        SDL_EndGPURenderPass(render_pass);
    }
    void SDL3GPURenderer::createPipeline()
    {
    }
}
