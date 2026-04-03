#pragma once
#include "component.h"
#include <box2d/box2d.h>
#include <glm/vec2.hpp>

namespace engine::physics
{
    class PhysicsManager;
}

namespace engine::component
{
    class PhysicsComponent final : public Component
    {
    public:
        PhysicsComponent(b2BodyId bodyId, ::engine::physics::PhysicsManager *physicsManager = nullptr);
        ~PhysicsComponent() override = default;

        void setVelocity(const glm::vec2& velocity);
        void setWorldPosition(const glm::vec2& position);
        void applyForce(const glm::vec2& force);
        void applyImpulse(const glm::vec2& impulse);
        // 销毁旧碰撞形状并用新的半宽/半深(px)重建，可选局部偏移
        void reshapeBox(glm::vec2 halfExtentsPx, glm::vec2 localOffsetPx = {0.0f, 0.0f});

        glm::vec2 getVelocity() const;
        glm::vec2 getPosition() const;

        b2BodyId getBodyId() const { return m_bodyId; }
        /** 返回上次 reshapeBox 设置的碰撞半尺寸(px)，未调用时为 {0,0} */
        glm::vec2 getBodyHalfExtentsPx() const { return m_cachedHalfExtentsPx; }

    private:
        b2BodyId m_bodyId;
        ::engine::physics::PhysicsManager *m_physicsManager = nullptr;
        glm::vec2 m_cachedHalfExtentsPx{0.0f, 0.0f};

        void update(float delta_time) override;
        void render() override {}
        void clean() override;
    };
}
