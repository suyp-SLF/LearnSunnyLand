#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

// ⚡️ 引入你存放结构体的头文件
#include "resource_types.h"

namespace engine::resource
{
    class TextureManager
    {
        friend class ResourceManager;

    public:
        explicit TextureManager(SDL_Renderer *renderer, SDL_GPUDevice *gpu_device);
        ~TextureManager();

        // 禁用拷贝
        TextureManager(const TextureManager &) = delete;
        TextureManager &operator=(const TextureManager &) = delete;

        // 公开接口
        SDL_Texture *getLegacyTexture(const std::string &path);
        SDL_GPUTexture *getGPUTexture(const std::string &path);
        glm::vec2 getTextureSize(const std::string &path);
        void unloadTexture(const std::string &path);
        void clearTextures();
        void setDevice(SDL_Renderer *renderer, SDL_GPUDevice *device)
        {
            _renderer = renderer;
            _gpu_device = device;
        }

    private:
        // 核心逻辑：获取内部包装资源
        TextureResource &getInternal(const std::string &path);

        // 强制从磁盘加载
        bool forceLoad(const std::string &path);

        // GPU 上传辅助逻辑
        SDL_GPUTexture *uploadToGPU(SDL_Surface *surface);

        // 保持简洁，不要重复定义成员变量
        SDL_Renderer *_renderer = nullptr;
        SDL_GPUDevice *_gpu_device = nullptr;

        // 使用 resource_types.h 中定义的 TextureResource
        std::unordered_map<std::string, TextureResource> _cache;
    };
} // namespace engine::resource