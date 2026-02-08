#pragma once

#include <memory>
#include <utility>
#include <unordered_map>
#include <functional>
#include <SDL3_ttf/SDL_ttf.h>

namespace engine::resource
{
    using FontKey = std::pair<std::string, int>;
    struct FontKeyHash
    {
        std::size_t operator()(const FontKey &key) const
        {
            std::hash<std::string> string_hasher;
            std::hash<int> int_hasher;
            return string_hasher(key.first) ^ int_hasher(key.second);
        }
    };
    class FontManager
    {
        friend class ResourceManager;

    private:
        struct SDLFontDeleter
        {
            void operator()(TTF_Font *font) const
            {
                if (font)
                {
                    TTF_CloseFont(font);
                }
            }
        };
        std::unordered_map<FontKey, std::unique_ptr<TTF_Font, SDLFontDeleter>, FontKeyHash> _fonts;

    public:
        FontManager();
        ~FontManager();

        FontManager(const FontManager &) = delete;
        FontManager &operator=(const FontManager &) = delete;
        FontManager(FontManager &&) = delete;
        FontManager &operator=(FontManager &&) = delete;

    private:
        TTF_Font *loadFont(const std::string &file, int point_size);
        TTF_Font *getFont(const std::string &file, int point_size);
        void unloadFont(const std::string &file, int point_size);
        void clearFonts();
    };
};