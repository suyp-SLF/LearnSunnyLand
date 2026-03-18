#pragma once
#include "component.h"
#include <glm/vec2.hpp>

namespace engine::component
{
    class ControllerComponent final : public Component
    {
    public:
        ControllerComponent(float speed = 15.0f, float jetpackForce = 20.0f);
        ~ControllerComponent() = default;

        void setSpeed(float speed) { m_speed = speed; }
        float getSpeed() const { return m_speed; }

    private:
        float m_speed;
        float m_jetpackForce;
        glm::vec2 m_inputDir{0.0f, 0.0f};

        void handleInput() override;
        void update(float delta_time) override;
        void render() override {}
    };
}
