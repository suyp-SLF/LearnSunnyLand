#include "resource_manager.h"
#include "texture_manager.h"
#include "audio_manager.h"
#include "font_manager.h"
#include "shader_manager.h"
#include <spdlog/spdlog.h>

namespace engine::resource {
    ResourceManager::ResourceManager(SDL_Renderer* renderer, SDL_GPUDevice* device)
        : _renderer(renderer), _gpu_device(device) 
    {
        _texture_manager = std::make_unique<TextureManager>(renderer, device);
        _audio_manager   = std::make_unique<AudioManager>();
        _font_manager    = std::make_unique<FontManager>();
        
        if (_gpu_device) {
            _shader_manager = std::make_unique<ShaderManager>(_gpu_device);
        }
        
        spdlog::info("ResourceManager 初始化完成。后端: {}", device ? "SDL_GPU" : "SDL_Renderer");
    }

    ResourceManager::~ResourceManager() { clear(); }

    void ResourceManager::clear() {
        if (_texture_manager) _texture_manager->clearTextures();
        if (_audio_manager)   _audio_manager->clearAudios();
        if (_font_manager)    _font_manager->clearFonts();
        if (_shader_manager)  _shader_manager->clear();
        spdlog::trace("所有引擎资源已卸载");
    }

    // --- 纹理转发 ---
    SDL_Texture* ResourceManager::getTexture(const std::string& path) {
        return _texture_manager->getLegacyTexture(path);
    }
    SDL_GPUTexture* ResourceManager::getGPUTexture(const std::string& path) {
        return _texture_manager->getGPUTexture(path);
    }

    glm::vec2 ResourceManager::getTextureSize(const std::string &path)
    {
        return glm::vec2();
    }

    void ResourceManager::clearTextures()
    {
    }

    void ResourceManager::unloadTexture(const std::string &path)
    {
    }

    // --- 音频转发 ---
    MIX_Audio* ResourceManager::getAudio(const std::string& path) {
        return _audio_manager->getAudio(path);
    }
    void ResourceManager::unloadAudio(const std::string& path) {
        _audio_manager->unloadAudio(path);
    }

    // --- 字体转发 ---
    TTF_Font* ResourceManager::getFont(const std::string& path, int size) {
        return _font_manager->getFont(path, size);
    }

    // --- Shader 转发 (新增) ---
    SDL_GPUShader* ResourceManager::loadShader(const std::string& name, const std::string& path) {
        if (!_shader_manager) return nullptr;
        // 这里简化了参数，实际需要根据 Shader 需求传入 sampler 数量等
        return _shader_manager->loadShader(name, path, 1, 1, 0, 0);
    }
}