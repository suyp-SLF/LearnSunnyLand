#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "../render/text_renderer.h"

namespace engine::resource
{
    class FontManager
    {
        friend class ResourceManager;

    private:
        std::unordered_map<std::string, std::unique_ptr<render::TextRenderer>> _renderers;
        SDL_GPUDevice* _device = nullptr;

    public:
        FontManager();
        ~FontManager();

        FontManager(const FontManager &) = delete;
        FontManager &operator=(const FontManager &) = delete;
        FontManager(FontManager &&) = delete;
        FontManager &operator=(FontManager &&) = delete;

        void setDevice(SDL_GPUDevice* device);
        render::TextRenderer* loadFont(const std::string &file, unsigned int fontSize);
        render::TextRenderer* getFont(const std::string &file);
        void clearFonts();
    };
}