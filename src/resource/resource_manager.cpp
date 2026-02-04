#include "resource_manager.h"
#include "texture_manager.h"
#include "audio_manager.h"
#include "font_manager.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <spdlog/spdlog.h>

namespace engine::resource
{
    ResourceManager::ResourceManager(SDL_Renderer *renderer)
    {
        _texture_manager = std::make_unique<TextureManager>(renderer);
        _audio_manager = std::make_unique<AudioManager>();
        _font_manager = std::make_unique<FontManager>();
        spdlog::trace("ResourceManager 被创建");
    }
    ResourceManager::~ResourceManager() = default;

    void ResourceManager::clear()
    {
        _texture_manager->clearTextures();
        _audio_manager->clearAudios();
        _font_manager->clearFonts();
        spdlog::trace("ResourceManager 中的资源通过clear()方法被清空");
    }
    SDL_Texture *ResourceManager::loadTexture(const std::string &path)
    {
        return _texture_manager->loadTexture(path);
    }
    SDL_Texture *ResourceManager::getTexture(const std::string &path)
    {
        return _texture_manager->getTexture(path);
    }
    glm::vec2 ResourceManager::getTextureSize(const std::string &path)
    {
        return _texture_manager->getTextureSize(path);
    }
    void ResourceManager::unloadTexture(const std::string &path)
    {
        _texture_manager->unloadTexture(path);
    }
    void ResourceManager::clearTextures()
    {
        _texture_manager->clearTextures();
    }
    MIX_Audio *ResourceManager::loadAudio(const std::string &path)
    {
        return _audio_manager->loadAudio(path);
    }
    MIX_Audio *ResourceManager::getAudio(const std::string &path)
    {
        return _audio_manager->getAudio(path);
    }
    void ResourceManager::unloadAudio(const std::string &path)
    {
        return _audio_manager->unloadAudio(path);
    }
    void ResourceManager::clearAudios()
    {
        return _audio_manager->clearAudios();
    }
    TTF_Font *ResourceManager::loadFont(const std::string &path, const int point_size)
    {
        return _font_manager->loadFont(path, point_size);
    }
    TTF_Font *ResourceManager::getFont(const std::string &path, const int point_size)
    {
        return _font_manager->getFont(path, point_size);
    }

    void ResourceManager::unloadFont(const std::string &path, const int point_size)
    {
        return _font_manager->unloadFont(path, point_size);
    }
    void ResourceManager::clearFonts()
    {
        return _font_manager->clearFonts();
    }
};