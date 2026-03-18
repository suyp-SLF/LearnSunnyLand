#include "font_manager.h"
#include <spdlog/spdlog.h>

namespace engine::resource
{
    FontManager::FontManager()
    {
        spdlog::trace("FontManager 初始化成功");
    }

    FontManager::~FontManager()
    {
        clearFonts();
        spdlog::trace("FontManager 析构成功");
    }

    void FontManager::setDevice(SDL_GPUDevice* device)
    {
        _device = device;
    }

    render::TextRenderer* FontManager::loadFont(const std::string &file, unsigned int fontSize)
    {
        std::string key = file + "_" + std::to_string(fontSize);
        auto it = _renderers.find(key);
        if (it != _renderers.end())
        {
            return it->second.get();
        }

        if (!_device)
        {
            spdlog::error("无法加载字体 '{}': GPU设备未设置", file);
            return nullptr;
        }

        spdlog::debug("加载字体 '{}' 大小 {}px", file, fontSize);
        auto renderer = std::make_unique<render::TextRenderer>();
        if (!renderer->init(_device, file, fontSize))
        {
            spdlog::error("无法加载字体 '{}'", file);
            return nullptr;
        }

        auto* ptr = renderer.get();
        _renderers[key] = std::move(renderer);
        spdlog::debug("加载字体成功: '{}' 大小 {}px", file, fontSize);
        return ptr;
    }

    render::TextRenderer* FontManager::getFont(const std::string &file)
    {
        auto it = _renderers.find(file);
        if (it != _renderers.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    void FontManager::clearFonts()
    {
        if (!_renderers.empty())
        {
            spdlog::debug("正在清理所有 {} 个字体", _renderers.size());
            _renderers.clear();
        }
    }
}
