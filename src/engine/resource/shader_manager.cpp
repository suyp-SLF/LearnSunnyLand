#include "shader_manager.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <vector>

namespace engine::resource {

    // 析构函数调用 clear
    void ShaderManager::clear() {
        if (!_device) return;

        for (auto& [name, shader] : _shaders) {
            if (shader) {
                SDL_ReleaseGPUShader(_device, shader);
            }
        }
        _shaders.clear();
        spdlog::info("ShaderManager: 所有着色器资源已释放");
    }

    SDL_GPUShader* ShaderManager::loadShader(const std::string& name, const std::string& path, 
                                            uint32_t sampler_count, uint32_t uniform_buffer_count,
                                            uint32_t storage_buffer_count, uint32_t storage_texture_count) {
        // 1. 检查是否已经加载过
        if (_shaders.contains(name)) {
            return _shaders[name];
        }

        // 2. 读取二进制着色器文件 (SPIR-V 或 MSL)
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("无法打开着色器文件: {}", path);
            return nullptr;
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        // 3. 配置 SDL_GPUShaderCreateInfo
        SDL_GPUShaderCreateInfo createInfo = {};
        createInfo.code = (const Uint8*)buffer.data();
        createInfo.code_size = fileSize;
        createInfo.entrypoint = "main"; // 通常默认为 main
        
        // 根据不同平台设置格式 (macOS 通常用 MSL, 其他用 SPIRV)
#ifdef __APPLE__
        createInfo.format = SDL_GPU_SHADERFORMAT_MSL;
#else
        createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
#endif
        
        // 设置着色器阶段 (这里简单演示，实际可能需要根据文件名判断是 Vertex 还是 Fragment)
        if (path.find(".vert") != std::string::npos) {
            createInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        } else {
            createInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        }

        createInfo.num_samplers = sampler_count;
        createInfo.num_uniform_buffers = uniform_buffer_count;
        createInfo.num_storage_buffers = storage_buffer_count;
        createInfo.num_storage_textures = storage_texture_count;

        // 4. 创建并存储
        SDL_GPUShader* shader = SDL_CreateGPUShader(_device, &createInfo);
        if (!shader) {
            spdlog::error("着色器创建失败: {} (Path: {})", SDL_GetError(), path);
            return nullptr;
        }

        _shaders[name] = shader;
        spdlog::info("成功加载着色器: {} [{}]", name, path);
        return shader;
    }

} // namespace engine::resource