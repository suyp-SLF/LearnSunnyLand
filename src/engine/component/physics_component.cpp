#include "physics_component.h"
#include "transform_component.h"
#include "../object/game_object.h"

namespace engine::component
{
    PhysicsComponent::PhysicsComponent(b2BodyId bodyId)
        : m_bodyId(bodyId)
    {
    }

    void PhysicsComponent::setVelocity(const glm::vec2& velocity)
    {
        if (B2_IS_NON_NULL(m_bodyId))
        {
            b2Body_SetLinearVelocity(m_bodyId, {velocity.x, velocity.y});
        }
    }

    void PhysicsComponent::applyForce(const glm::vec2& force)
    {
        if (B2_IS_NON_NULL(m_bodyId))
        {
            b2Body_ApplyForceToCenter(m_bodyId, {force.x, force.y}, true);
        }
    }

    void PhysicsComponent::applyImpulse(const glm::vec2& impulse)
    {
        if (B2_IS_NON_NULL(m_bodyId))
        {
            b2Body_ApplyLinearImpulseToCenter(m_bodyId, {impulse.x, impulse.y}, true);
        }
    }

    glm::vec2 PhysicsComponent::getVelocity() const
    {
        if (B2_IS_NON_NULL(m_bodyId))
        {
            b2Vec2 vel = b2Body_GetLinearVelocity(m_bodyId);
            return {vel.x, vel.y};
        }
        return {0.0f, 0.0f};
    }

    glm::vec2 PhysicsComponent::getPosition() const
    {
        if (B2_IS_NON_NULL(m_bodyId))
        {
            b2Vec2 pos = b2Body_GetPosition(m_bodyId);
            return {pos.x, pos.y};
        }
        return {0.0f, 0.0f};
    }

    void PhysicsComponent::update(float /*delta_time*/)
    {
        if (!_owner || B2_IS_NULL(m_bodyId))
            return;

        auto* transform = _owner->getComponent<TransformComponent>();
        if (transform)
        {
            constexpr float PIXELS_PER_METER = 32.0f;
            b2Vec2 pos = b2Body_GetPosition(m_bodyId);
            transform->setPosition({pos.x * PIXELS_PER_METER, pos.y * PIXELS_PER_METER});
        }
    }
}
