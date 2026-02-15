#include "shader_manager.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <vector>

namespace engine::resource
{

    // 析构函数调用 clear
    void ShaderManager::clear()
    {
        if (!_device)
            return;

        for (auto &[name, shader] : _shaders)
        {
            if (shader)
            {
                SDL_ReleaseGPUShader(_device, shader);
            }
        }
        _shaders.clear();
        spdlog::info("ShaderManager: 所有着色器资源已释放");
    }

    SDL_GPUShader *ShaderManager::loadShader(const std::string &name, const std::string &path,
                                             uint32_t sampler_count, uint32_t uniform_buffer_count,
                                             uint32_t storage_buffer_count, uint32_t storage_texture_count)
    {
        if (_shaders.contains(name))
            return _shaders[name];

        // 1. 自动根据平台补全后缀
        std::string actual_path = path;
#ifdef __APPLE__
        if (!actual_path.ends_with(".msl"))
            actual_path += ".msl";
        SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_MSL;
#else
        if (!actual_path.ends_with(".spv"))
            actual_path += ".spv";
        SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_SPIRV;
#endif

        // 2. 二进制方式读取文件
        std::ifstream file(actual_path, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            spdlog::error("ShaderManager: 找不到文件 {}", actual_path);
            return nullptr;
        }

        size_t fileSize = (size_t)file.tellg();

        bool isMSL = (format == SDL_GPU_SHADERFORMAT_MSL);
        std::vector<Uint8> buffer(fileSize + (isMSL ? 1 : 0));
        file.seekg(0);
        file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
        file.close();

        // 3. 填充创建信息
        SDL_GPUShaderCreateInfo createInfo;
        SDL_zero(createInfo);

        createInfo.code = buffer.data();
        createInfo.code_size = buffer.size();
        createInfo.format = format;
        createInfo.entrypoint = "main0"; // 确保 GLSL 里的入口是 main

        // 4. 判断 Stage (根据原始路径或补全后的路径)
        if (actual_path.find(".vert") != std::string::npos)
        {
            createInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        }
        else if (actual_path.find(".frag") != std::string::npos)
        {
            createInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        }
        else
        {
            spdlog::error("ShaderManager: 无法识别着色器阶段: {}", actual_path);
            return nullptr;
        }

        // 5. 资源布局声明
        createInfo.num_samplers = sampler_count;
        createInfo.num_uniform_buffers = uniform_buffer_count;
        createInfo.num_storage_buffers = storage_buffer_count;
        createInfo.num_storage_textures = storage_texture_count;

        // 6. 创建 Shader 对象
        SDL_GPUShader *shader = SDL_CreateGPUShader(_device, &createInfo);
        if (!shader)
        {
            spdlog::error("ShaderManager: 创建失败 '{}' -> 实际路径: {} | SDL Error: {}",
                          name, actual_path, SDL_GetError());
            return nullptr;
        }

        _shaders[name] = shader;
        spdlog::info("ShaderManager: 成功加载 {} (Format: {})",
                     name, (format == SDL_GPU_SHADERFORMAT_MSL ? "MSL" : "SPIRV"));

        return shader;
    }

} // namespace engine::resource