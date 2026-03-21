#include "monster_ai_component.h"
#include "../../engine/component/physics_component.h"
#include "../../engine/component/sprite_component.h"
#include "../../engine/component/transform_component.h"
#include "../../engine/object/game_object.h"
#include "../../engine/world/chunk_manager.h"
#include <algorithm>
#include <cmath>

namespace game::monster
{
    MonsterAIComponent::MonsterAIComponent(MonsterType type,
                                           engine::object::GameObject *player,
                                           engine::world::ChunkManager *chunkManager,
                                           glm::vec2 spawnOrigin)
        : m_type(type)
        , m_player(player)
        , m_chunkManager(chunkManager)
        , m_spawnOrigin(spawnOrigin)
    {
    }

    glm::vec2 MonsterAIComponent::getPlayerDelta() const
    {
        if (!_owner || !m_player)
            return {0.0f, 0.0f};

        auto *selfTransform = _owner->getComponent<engine::component::TransformComponent>();
        auto *playerTransform = m_player->getComponent<engine::component::TransformComponent>();
        if (!selfTransform || !playerTransform)
            return {0.0f, 0.0f};

        return playerTransform->getPosition() - selfTransform->getPosition();
    }

    bool MonsterAIComponent::isGrounded() const
    {
        if (!_owner) return false;
        auto *physics = _owner->getComponent<engine::component::PhysicsComponent>();
        if (!physics) return false;
        return std::abs(physics->getVelocity().y) < 0.15f;
    }

    void MonsterAIComponent::updateSlime(float dt)
    {
        auto *physics = _owner->getComponent<engine::component::PhysicsComponent>();
        auto *sprite = _owner->getComponent<engine::component::SpriteComponent>();
        if (!physics || !sprite) return;

        glm::vec2 delta = getPlayerDelta();
        float distance = glm::length(delta);
        float dir = delta.x >= 0.0f ? 1.0f : -1.0f;
        bool chasing = distance < 320.0f;

        if (distance > 420.0f)
            dir = m_wanderDir;

        sprite->setFlipped(dir < 0.0f);
        m_actionCooldown = std::max(0.0f, m_actionCooldown - dt);
        auto vel = physics->getVelocity();

        if (isGrounded() && m_actionCooldown <= 0.0f)
        {
            vel.x = (chasing ? 4.5f : 2.5f) * dir;
            vel.y = -(chasing ? 4.1f : 3.2f);
            physics->setVelocity(vel);
            m_actionCooldown = chasing ? 0.72f : 1.1f;
        }
    }

    void MonsterAIComponent::updateWolf(float dt)
    {
        auto *physics = _owner->getComponent<engine::component::PhysicsComponent>();
        auto *sprite = _owner->getComponent<engine::component::SpriteComponent>();
        if (!physics || !sprite) return;

        glm::vec2 delta = getPlayerDelta();
        float distance = glm::length(delta);
        float dir = delta.x >= 0.0f ? 1.0f : -1.0f;
        bool chasing = distance < 440.0f;
        if (!chasing) dir = m_wanderDir;

        sprite->setFlipped(dir < 0.0f);
        auto vel = physics->getVelocity();
        float targetX = (chasing ? 8.5f : 3.0f) * dir;
        float step = (chasing ? 18.0f : 8.0f) * dt;
        if (vel.x < targetX) vel.x = std::min(vel.x + step, targetX);
        else vel.x = std::max(vel.x - step, targetX);

        m_actionCooldown = std::max(0.0f, m_actionCooldown - dt);
        if (chasing && isGrounded() && m_actionCooldown <= 0.0f && distance < 180.0f)
        {
            vel.y = -4.8f;
            vel.x = dir * 10.5f;
            m_actionCooldown = 1.05f;
        }

        physics->setVelocity(vel);
    }

    void MonsterAIComponent::updateWhiteApe(float dt)
    {
        auto *physics = _owner->getComponent<engine::component::PhysicsComponent>();
        auto *sprite = _owner->getComponent<engine::component::SpriteComponent>();
        if (!physics || !sprite) return;

        glm::vec2 delta = getPlayerDelta();
        float distance = glm::length(delta);
        float dir = delta.x >= 0.0f ? 1.0f : -1.0f;
        bool chasing = distance < 560.0f;
        if (!chasing) dir = m_wanderDir;

        sprite->setFlipped(dir < 0.0f);
        m_actionCooldown = std::max(0.0f, m_actionCooldown - dt);
        auto vel = physics->getVelocity();

        if (isGrounded())
        {
            if (chasing && m_actionCooldown <= 0.0f)
            {
                vel.x = dir * 12.0f;
                vel.y = -6.1f;
                m_actionCooldown = 1.2f;
            }
            else if (!chasing)
            {
                vel.x = dir * 2.4f;
            }
        }

        physics->setVelocity(vel);
    }

    void MonsterAIComponent::update(float delta_time)
    {
        if (!_owner) return;

        m_thinkTimer -= delta_time;
        if (m_thinkTimer <= 0.0f)
        {
            m_thinkTimer = 1.5f;
            m_wanderDir *= -1.0f;
        }

        switch (m_type)
        {
        case MonsterType::Slime:    updateSlime(delta_time); break;
        case MonsterType::Wolf:     updateWolf(delta_time); break;
        case MonsterType::WhiteApe: updateWhiteApe(delta_time); break;
        }
    }
}