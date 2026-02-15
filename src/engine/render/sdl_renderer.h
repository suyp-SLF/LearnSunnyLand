#pragma once
#include "renderer.h"
#include "sprite.h"
#include <optional>
#include <glm/glm.hpp>
#include <SDL3/SDL_stdinc.h>

struct SDL_Renderer;
struct SDL_FRect;
namespace engine::resource
{
    class ResourceManager;
}

namespace engine::render
{
    class Camera;
    class Sprite;
    class SDLRenderer final : public Renderer
    {
    private:
        SDL_Renderer *_renderer = nullptr;

    public:
        SDLRenderer(SDL_Renderer *renderer);
        void drawSprite(const Camera &camera, const Sprite &sprite, const glm::vec2 &position,
                        const glm::vec2 &scale, double angle = 0.0f);
        void drawParallax(const Camera &camera, const Sprite &sprite, const glm::vec2 &position,
                          const glm::vec2 &scroll_factor, const glm::bvec2 &repeat = {true, true}, const glm::vec2 &scale = {1.0f, 1.0f}, double angle = 0.0f);

        void drawUISprite(const Sprite &sprite, const glm::vec2 &position, const std::optional<glm::vec2> &size = std::nullopt);

        void present();
        void clearScreen();

        void setDrawColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255);
        void setDrawColorFloat(float r, float g, float b, float a = 1.0f);

        SDL_Renderer *getSDLRenderer() const;

        // 禁止拷贝和移动
        SDLRenderer(const SDLRenderer &) = delete;
        SDLRenderer &operator=(const SDLRenderer &) = delete;
        SDLRenderer(SDLRenderer &&) = delete;
        SDLRenderer &operator=(SDLRenderer &&) = delete;
        private:
        std::optional<SDL_FRect> getSpriteRect(const Sprite &sprite);
        bool isRectInViewport(const Camera &camera, const SDL_FRect &rect);
    };
} // namespace engine::render