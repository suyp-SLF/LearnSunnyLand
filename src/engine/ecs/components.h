#pragma once
#include <glm/glm.hpp>

namespace engine::ecs
{
    struct Position
    {
        glm::vec2 pos;
    };

    struct Velocity
    {
        glm::vec2 vel;
    };
}
