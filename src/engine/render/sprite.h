#pragma once
#include <string>
#include <optional>
#include <SDL3/SDL_rect.h>
namespace engine::render
{
    // class SDL_FRect;
    class Sprite final
    {
    private:
        std::string _texture_id;
        std::optional<SDL_FRect> _source_rect;
        bool _is_flipped = false;

    public:
        Sprite(const std::string &texture_id, const std::optional<SDL_FRect> &source_rect = std::nullopt, bool is_flipped = false)
            : _texture_id(texture_id),
              _source_rect(source_rect),
              _is_flipped(is_flipped)
        {}

        // GETTER
        const std::string &getTextureId() const { return _texture_id; }
        const std::optional<SDL_FRect> &getSourceRect() const { return _source_rect; }
        bool isFlipped() const { return _is_flipped; }
        // SETTER
        void setTextureId(const std::string &texture_id) { _texture_id = texture_id; }
        void setSourceRect(const std::optional<SDL_FRect> &source_rect) { _source_rect = source_rect; }
        void setFlipped(bool is_flipped) { _is_flipped = is_flipped; }
    };
};