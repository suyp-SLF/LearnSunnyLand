#pragma once
#include <glm/glm.hpp>
#include <string>
#include <functional>

namespace engine::ecs
{
    struct Button
    {
        glm::vec2 pos;
        glm::vec2 size;
        std::string text;
        std::function<void()> onClick;
    };

    struct Renderable
    {
        glm::vec4 color;
    };
}
