#pragma once
#include "component.h"
#include <box2d/box2d.h>
#include <glm/vec2.hpp>

namespace engine::component
{
    class PhysicsComponent final : public Component
    {
    public:
        PhysicsComponent(b2BodyId bodyId);
        ~PhysicsComponent() override = default;

        void setVelocity(const glm::vec2& velocity);
        void applyForce(const glm::vec2& force);
        void applyImpulse(const glm::vec2& impulse);

        glm::vec2 getVelocity() const;
        glm::vec2 getPosition() const;

        b2BodyId getBodyId() const { return m_bodyId; }

    private:
        b2BodyId m_bodyId;

        void update(float delta_time) override;
        void render() override {}
    };
}
