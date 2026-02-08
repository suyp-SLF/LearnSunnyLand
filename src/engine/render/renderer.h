#pragma once
#include "sprite.h"
#include <optional>
#include <glm/glm.hpp>

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
    class Renderer final
    {
    private:
        SDL_Renderer *_renderer = nullptr;
        engine::resource::ResourceManager *_resource_manager = nullptr;

    public:
        Renderer(SDL_Renderer *renderer, engine::resource::ResourceManager *resource_manager);
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
        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;
        Renderer(Renderer &&) = delete;
        Renderer &operator=(Renderer &&) = delete;

    private:
        std::optional<SDL_FRect> getSpriteRect(const Sprite &sprite);
        bool isRectInViewport(const Camera &camera, const SDL_FRect &rect); // 判断矩形是否在视口内
    };
};