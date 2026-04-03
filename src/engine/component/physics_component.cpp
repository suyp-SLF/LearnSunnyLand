#include "physics_component.h"
#include "transform_component.h"
#include "../physics/physics_manager.h"
#include "../object/game_object.h"

namespace engine::component
{
    PhysicsComponent::PhysicsComponent(b2BodyId bodyId, ::engine::physics::PhysicsManager *physicsManager)
        : m_bodyId(bodyId)
        , m_physicsManager(physicsManager)
    {
    }

    void PhysicsComponent::setVelocity(const glm::vec2& velocity)
    {
        if (B2_IS_NON_NULL(m_bodyId))
        {
            b2Body_SetLinearVelocity(m_bodyId, {velocity.x, velocity.y});
        }
    }

    void PhysicsComponent::setWorldPosition(const glm::vec2& position)
    {
        if (B2_IS_NON_NULL(m_bodyId))
        {
            constexpr float PIXELS_PER_METER = 32.0f;
            b2Rot rotation = b2Body_GetRotation(m_bodyId);
            b2Body_SetTransform(m_bodyId,
                                {position.x / PIXELS_PER_METER, position.y / PIXELS_PER_METER},
                                rotation);
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

    void PhysicsComponent::reshapeBox(glm::vec2 halfExtentsPx, glm::vec2 localOffsetPx)
    {
        if (!B2_IS_NON_NULL(m_bodyId))
            return;
        constexpr float PPM = 32.0f;
        const int count = b2Body_GetShapeCount(m_bodyId);
        if (count > 0)
        {
            b2ShapeId shapes[16];
            const int actual = b2Body_GetShapes(m_bodyId, shapes, 16);
            for (int i = 0; i < actual; i++)
                b2DestroyShape(shapes[i], false);
        }
        m_cachedHalfExtentsPx = halfExtentsPx;
        const float halfW = std::max(0.01f, halfExtentsPx.x / PPM);
        const float halfH = std::max(0.01f, halfExtentsPx.y / PPM);
        const b2Vec2 localCenter = {localOffsetPx.x / PPM, localOffsetPx.y / PPM};
        b2Polygon box = b2MakeOffsetBox(halfW, halfH, localCenter, b2MakeRot(0.0f));
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.material.friction    = 0.0f;
        shapeDef.material.restitution = 0.0f;
        shapeDef.density              = 1.0f;
        b2CreatePolygonShape(m_bodyId, &shapeDef, &box);
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

    void PhysicsComponent::clean()
    {
        if (B2_IS_NULL(m_bodyId))
            return;

        if (m_physicsManager)
            m_physicsManager->destroyBody(m_bodyId);
        else if (b2Body_IsValid(m_bodyId))
            b2DestroyBody(m_bodyId);

        m_bodyId = b2_nullBodyId;
    }
}
