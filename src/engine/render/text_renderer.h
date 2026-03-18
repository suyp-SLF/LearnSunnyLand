#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::render
{
    struct Glyph
    {
        SDL_GPUTexture* texture = nullptr;
        glm::ivec2 size;
        glm::ivec2 bearing;
        unsigned int advance;
    };

    struct GlyphRenderInfo
    {
        SDL_GPUTexture* texture;
        float x, y, w, h;
    };

    class TextRenderer
    {
    public:
        TextRenderer();
        ~TextRenderer();

        bool init(SDL_GPUDevice* device, const std::string& fontPath, unsigned int fontSize);
        std::vector<GlyphRenderInfo> prepareText(const std::string& text, float x, float y);
        void cleanup();

    private:
        FT_Library m_ftLibrary = nullptr;
        FT_Face m_ftFace = nullptr;
        hb_font_t* m_hbFont = nullptr;
        hb_buffer_t* m_hbBuffer = nullptr;
        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUBuffer* m_vertexBuffer = nullptr;
        SDL_GPUSampler* m_sampler = nullptr;

        std::unordered_map<uint32_t, Glyph> m_glyphs;

        bool loadGlyph(uint32_t codepoint);
        void createRenderResources();
    };
}
