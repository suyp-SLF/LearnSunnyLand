#pragma once

#include <memory>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

namespace engine::resource
{
    class TextureManager
    {
        friend class ResourceManager;

    private:
        struct SDLTextureDeleter
        {
            void operator()(SDL_Texture *texture) const
            {
                if (texture)
                {
                    SDL_DestroyTexture(texture);
                }
            }
        };
        std::unordered_map<std::string, std::unique_ptr<SDL_Texture, SDLTextureDeleter>> _textures;
        SDL_Renderer *_renderer = nullptr;

    public:
        explicit TextureManager(SDL_Renderer *renderer);
        ~TextureManager();
        TextureManager(const TextureManager &) = delete;
        TextureManager &operator=(const TextureManager &) = delete;
        TextureManager(TextureManager &&) = delete;
        TextureManager &operator=(TextureManager &&) = delete;

    private:
        SDL_Texture *loadTexture(const std::string &path);
        SDL_Texture *getTexture(const std::string &path);
        glm::vec2 getTextureSize(const std::string &path);
        void unloadTexture(const std::string &path);
        void clearTextures();
    };
}