#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <string>

namespace engine::resource
{
    // 纹理资源包
    struct TextureResource
    {
        SDL_Texture *sdl_tex = nullptr;    // 对应之前的 legacy
        SDL_GPUTexture *gpu_tex = nullptr; // 对应之前的 gpu
        unsigned int gl_tex = 0;           // OpenGL 纹理 ID
        glm::vec2 size = {0.0f, 0.0f};     // 统一使用 glm::vec2 方便计算

        void release(SDL_Renderer *ren, SDL_GPUDevice *dev);
    };

    // GPU 管线资源包
    struct PipelineResource
    {
        SDL_GPUGraphicsPipeline *pipeline = nullptr;
    };
}