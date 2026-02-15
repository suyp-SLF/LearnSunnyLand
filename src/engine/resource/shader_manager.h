#pragma once
#include <SDL3/SDL_gpu.h>
#include <string>
#include <unordered_map>

namespace engine::resource {
    class ShaderManager {
    private:
        SDL_GPUDevice* _device;
        std::unordered_map<std::string, SDL_GPUShader*> _shaders;

    public:
        ShaderManager(SDL_GPUDevice* dev) : _device(dev) {}
        ~ShaderManager() { clear(); }

        // 加载预编译好的 Shader (SPIR-V 或 MSL)
        SDL_GPUShader* loadShader(const std::string& name, const std::string& path, 
                                 uint32_t sampler_count, uint32_t uniform_buffer_count,
                                 uint32_t storage_buffer_count, uint32_t storage_texture_count);
        
        void clear();
    };
} // namespace engine::resource