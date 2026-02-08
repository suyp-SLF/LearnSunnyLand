#include "font_manager.h"
#include <spdlog/spdlog.h>

namespace engine::resource
{
    FontManager::FontManager()
    {
        if (!TTF_WasInit() && !TTF_Init())
        {
            throw std::runtime_error("FontManager 错误：初始化失败" + std::string(SDL_GetError()));
        }
        spdlog::trace("FontManager 初始化成功");
    }

    FontManager::~FontManager()
    {
        if (!_fonts.empty())
        {
            spdlog::debug("FontManager 析构清理字体");
            clearFonts();
        }

        TTF_Quit();
        spdlog::trace("FontManager 析构成功");
    }
    TTF_Font *FontManager::loadFont(const std::string &file, const int point_size)
    {
        if (point_size <= 0)
        {
            spdlog::error("无法加载字体 '{}' 错误：字体大小必须大于0", file);
            return nullptr;
        }
        FontKey key{file, point_size};
        auto it = _fonts.find(key);
        if (it != _fonts.end())
        {
            return it->second.get();
        }

        spdlog::debug("加载字体 '{}' 大小 {}px", file, point_size);
        TTF_Font *raw_font = TTF_OpenFont(file.c_str(), point_size);
        if (!raw_font)
        {
            spdlog::error("无法加载字体 '{}' 错误：{}", file, SDL_GetError());
            return nullptr;
        }
        _fonts.emplace(key, std::unique_ptr<TTF_Font, SDLFontDeleter>(raw_font));
        spdlog::debug("加载字体成功:''{}' 大小 {}px", file, point_size);
        return raw_font;
    }

    TTF_Font *FontManager::getFont(const std::string &file, int point_size)
    {
        FontKey key{file, point_size};
        auto it = _fonts.find(key);
        if (it != _fonts.end())
        {
            return it->second.get();
        }
        spdlog::warn("字体 '{}' 大小 {}px ,尝试加载", file, point_size);
        return loadFont(file, point_size);
    }

    void FontManager::unloadFont(const std::string &file, int point_size)
    {
        FontKey key{file, point_size};
        auto it = _fonts.find(key);
        if (it != _fonts.end())
        {
            _fonts.erase(it);
            spdlog::debug("卸载字体 '{}' 大小 {}px", file, point_size);
        }
        else
        {
            spdlog::warn("尝试卸载不存在的字体 '{}' 大小 {}px", file, point_size);
        }
    }
    void FontManager::clearFonts()
    {
        if (!_fonts.empty())
        {
            spdlog::debug("正在清理所有 {} 个字体", _fonts.size());
            _fonts.clear();
        }
    }
}
