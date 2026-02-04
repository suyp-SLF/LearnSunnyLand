#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>

struct Mix_Music;
struct TTF_Font;
struct SDL_Texture;
struct MIX_Audio;
struct SDL_Renderer;

namespace engine::resource
{
    // 纹理管理器类的前向声明
    // 这个类用于管理和处理各种纹理资源
    class TextureManager;
    class AudioManager;
    class FontManager;
    class ResourceManager
    {
    private:
        std::unique_ptr<TextureManager> _texture_manager;
        std::unique_ptr<AudioManager> _audio_manager;
        std::unique_ptr<FontManager> _font_manager;

    public:
        explicit ResourceManager(SDL_Renderer *renderer);
        ~ResourceManager();
        void clear();

        // 删除拷贝移动
        ResourceManager(const ResourceManager &) = delete;
        ResourceManager &operator=(const ResourceManager &) = delete;
        ResourceManager(ResourceManager &&) = delete;
        ResourceManager &operator=(ResourceManager &&) = delete;

        SDL_Texture  *loadTexture(const std::string &path);
        glm::vec2 getTextureSize(const std::string &path);
        SDL_Texture *getTexture(const std::string &path);
        void unloadTexture(const std::string &path);
        void clearTextures();

        MIX_Audio *loadAudio(const std::string &path);
        MIX_Audio *getAudio(const std::string &path);
        void unloadAudio(const std::string &path);
        void clearAudios();

        TTF_Font *loadFont(const std::string &path, const int point_size);
        TTF_Font *getFont(const std::string &path, const int point_size);
        void unloadFont(const std::string &path, const int point_size);
        void clearFonts();
    };
};
