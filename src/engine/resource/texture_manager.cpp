#include "texture_manager.h"
#include <spdlog/spdlog.h>
#include <memory>
#include <SDL3_image/SDL_image.h>

namespace engine::resource
{
    TextureManager::TextureManager(SDL_Renderer *renderer) : _renderer(renderer)
    {
        if (!_renderer)
        {
            throw std::runtime_error("TextureManager 构造函数错误，SDL_Renderer 为空");
        }
        spdlog::trace("TextureManager 构造函数调用");
    }
    TextureManager::~TextureManager() = default;
    SDL_Texture *TextureManager::loadTexture(const std::string &path)
    {
        auto it = _textures.find(path);
        if (it != _textures.end())
        {
            return it->second.get();
        }
        SDL_Texture *raw_texture = IMG_LoadTexture(_renderer, path.c_str());
        if (!raw_texture)
        {
            spdlog::error("加载纹理失败:'{}' {}", path, SDL_GetError());
            return nullptr;
        }
        _textures.emplace(path, std::unique_ptr<SDL_Texture, SDLTextureDeleter>(raw_texture));
        spdlog::debug("加载纹理成功:'{}'", path);
        return raw_texture;
    }

    SDL_Texture *TextureManager::getTexture(const std::string &path)
    {
        auto it = _textures.find(path);
        if (it != _textures.end())
        {
            spdlog::trace("纹理已存在:'{}'", path);
            return it->second.get();
        }
        spdlog::warn("纹理不存在:'{}',尝试加载", path);
        return loadTexture(path);
    }
    glm::vec2 TextureManager::getTextureSize(const std::string &path)
    {
        SDL_Texture *texture = getTexture(path);
        if (!texture)
        {
            spdlog::error("获取纹理大小失败:'{}'", path);
            return glm::vec2(0, 0);
        }
        glm::vec2 size;
        if (!SDL_GetTextureSize(texture, &size.x, &size.y))
        {
            spdlog::error("获取纹理大小失败:'{}'", path);
            return glm::vec2(0, 0);
        }
        return size;
    }
    void TextureManager::unloadTexture(const std::string &path)
    {
        auto it = _textures.find(path);
        if (it != _textures.end())
        {
            spdlog::debug("卸载纹理:'{}'", path);
            _textures.erase(it);
        }
        else
        {
            spdlog::warn("尝试卸载不存在的纹理:'{}'", path);
        }
    }
    void TextureManager::clearTextures()
    {
        if (!_textures.empty())
        {
            spdlog::debug("正在清除所有 {} 个缓存的纹理。", _textures.size());
            _textures.clear();
        }
    }
};
