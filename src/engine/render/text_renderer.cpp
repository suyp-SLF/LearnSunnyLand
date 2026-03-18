#include "text_renderer.h"
#include <spdlog/spdlog.h>

namespace engine::render
{
    TextRenderer::TextRenderer()
    {
    }

    TextRenderer::~TextRenderer()
    {
        cleanup();
    }

    bool TextRenderer::init(SDL_GPUDevice* device, const std::string& fontPath, unsigned int fontSize)
    {
        m_device = device;

        if (FT_Init_FreeType(&m_ftLibrary))
        {
            spdlog::error("Failed to initialize FreeType");
            return false;
        }

        if (FT_New_Face(m_ftLibrary, fontPath.c_str(), 0, &m_ftFace))
        {
            spdlog::error("Failed to load font: {}", fontPath);
            return false;
        }

        FT_Set_Pixel_Sizes(m_ftFace, 0, fontSize);

        m_hbFont = hb_ft_font_create(m_ftFace, nullptr);
        m_hbBuffer = hb_buffer_create();

        createRenderResources();

        spdlog::info("TextRenderer initialized with font: {}", fontPath);
        return true;
    }

    void TextRenderer::createRenderResources()
    {
        SDL_GPUBufferCreateInfo bufferInfo = {};
        bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bufferInfo.size = sizeof(float) * 6 * 4 * 100;
        m_vertexBuffer = SDL_CreateGPUBuffer(m_device, &bufferInfo);

        SDL_GPUSamplerCreateInfo samplerInfo = {};
        samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        m_sampler = SDL_CreateGPUSampler(m_device, &samplerInfo);
    }

    bool TextRenderer::loadGlyph(uint32_t glyphIndex)
    {
        if (m_glyphs.find(glyphIndex) != m_glyphs.end())
            return true;

        if (FT_Load_Glyph(m_ftFace, glyphIndex, FT_LOAD_RENDER))
        {
            spdlog::warn("Failed to load glyph: {}", glyphIndex);
            return false;
        }

        FT_GlyphSlot g = m_ftFace->glyph;

        SDL_GPUTexture* texture = nullptr;

        if (g->bitmap.width > 0 && g->bitmap.rows > 0)
        {
            SDL_GPUTextureCreateInfo texInfo = {};
            texInfo.type = SDL_GPU_TEXTURETYPE_2D;
            texInfo.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
            texInfo.width = g->bitmap.width;
            texInfo.height = g->bitmap.rows;
            texInfo.layer_count_or_depth = 1;
            texInfo.num_levels = 1;
            texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

            texture = SDL_CreateGPUTexture(m_device, &texInfo);
            if (!texture)
                return false;

            if (g->bitmap.buffer)
            {
                SDL_GPUTransferBufferCreateInfo transferInfo = {};
                transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                transferInfo.size = g->bitmap.width * g->bitmap.rows;

                SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferInfo);
                if (transferBuffer)
                {
                    void* data = SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);
                    if (data)
                    {
                        memcpy(data, g->bitmap.buffer, g->bitmap.width * g->bitmap.rows);
                        SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

                        SDL_GPUCommandBuffer* uploadCmd = SDL_AcquireGPUCommandBuffer(m_device);
                        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmd);

                        SDL_GPUTextureTransferInfo texTransferInfo = {};
                        texTransferInfo.transfer_buffer = transferBuffer;
                        texTransferInfo.offset = 0;

                        SDL_GPUTextureRegion region = {};
                        region.texture = texture;
                        region.w = g->bitmap.width;
                        region.h = g->bitmap.rows;
                        region.d = 1;

                        SDL_UploadToGPUTexture(copyPass, &texTransferInfo, &region, false);

                        SDL_EndGPUCopyPass(copyPass);
                        SDL_SubmitGPUCommandBuffer(uploadCmd);
                        SDL_WaitForGPUIdle(m_device);
                        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
                    }
                }
            }
        }

        Glyph glyph;
        glyph.texture = texture;
        glyph.size = {g->bitmap.width, g->bitmap.rows};
        glyph.bearing = {g->bitmap_left, g->bitmap_top};
        glyph.advance = g->advance.x >> 6;

        m_glyphs[glyphIndex] = glyph;
        return true;
    }

    std::vector<GlyphRenderInfo> TextRenderer::prepareText(const std::string& text, float x, float y)
    {
        std::vector<GlyphRenderInfo> result;

        hb_buffer_clear_contents(m_hbBuffer);
        hb_buffer_add_utf8(m_hbBuffer, text.c_str(), -1, 0, -1);
        hb_buffer_set_direction(m_hbBuffer, HB_DIRECTION_LTR);
        hb_buffer_set_script(m_hbBuffer, HB_SCRIPT_LATIN);
        hb_buffer_set_language(m_hbBuffer, hb_language_from_string("en", -1));

        hb_shape(m_hbFont, m_hbBuffer, nullptr, 0);

        unsigned int glyphCount;
        hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(m_hbBuffer, &glyphCount);
        hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(m_hbBuffer, &glyphCount);

        float penX = x;
        float penY = y;

        for (unsigned int i = 0; i < glyphCount; i++)
        {
            uint32_t codepoint = glyphInfo[i].codepoint;
            loadGlyph(codepoint);

            auto it = m_glyphs.find(codepoint);
            if (it == m_glyphs.end())
                continue;

            Glyph& glyph = it->second;
            if (!glyph.texture || glyph.size.x == 0 || glyph.size.y == 0)
            {
                penX += glyphPos[i].x_advance / 64.0f;
                continue;
            }

            float xpos = penX + glyph.bearing.x + (glyphPos[i].x_offset / 64.0f);
            float ypos = penY - glyph.bearing.y + (glyphPos[i].y_offset / 64.0f);

            result.push_back({glyph.texture, xpos, ypos,
                            static_cast<float>(glyph.size.x),
                            static_cast<float>(glyph.size.y)});

            penX += glyphPos[i].x_advance / 64.0f;
            penY += glyphPos[i].y_advance / 64.0f;
        }

        return result;
    }

    void TextRenderer::cleanup()
    {
        for (auto& pair : m_glyphs)
        {
            if (pair.second.texture)
                SDL_ReleaseGPUTexture(m_device, pair.second.texture);
        }
        m_glyphs.clear();

        if (m_vertexBuffer)
        {
            SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
            m_vertexBuffer = nullptr;
        }

        if (m_sampler)
        {
            SDL_ReleaseGPUSampler(m_device, m_sampler);
            m_sampler = nullptr;
        }

        if (m_hbBuffer)
        {
            hb_buffer_destroy(m_hbBuffer);
            m_hbBuffer = nullptr;
        }

        if (m_hbFont)
        {
            hb_font_destroy(m_hbFont);
            m_hbFont = nullptr;
        }

        if (m_ftFace)
        {
            FT_Done_Face(m_ftFace);
            m_ftFace = nullptr;
        }

        if (m_ftLibrary)
        {
            FT_Done_FreeType(m_ftLibrary);
            m_ftLibrary = nullptr;
        }
    }
}
