#include "monster_ai_component.h"
#include "../../engine/component/controller_component.h"
#include "../../engine/component/physics_component.h"
#include "../../engine/component/sprite_component.h"
#include "../../engine/component/transform_component.h"
#include "../../engine/object/game_object.h"
#include "../../engine/world/chunk_manager.h"
#include <algorithm>
#include <cmath>

namespace game::monster
{
    namespace
    {
        float perceptionRangeForMonster(MonsterType type)
        {
            switch (type)
            {
            case MonsterType::WhiteApe: return 460.0f;
            case MonsterType::Wolf: return 380.0f;
            case MonsterType::Slime: return 300.0f;
            }
            return 320.0f;
        }

        float combatRangeForMonster(MonsterType type)
        {
            switch (type)
            {
            case MonsterType::WhiteApe: return 190.0f;
            case MonsterType::Wolf: return 165.0f;
            case MonsterType::Slime: return 140.0f;
            }
            return 160.0f;
        }

        float retreatDistanceForMonster(MonsterType type)
        {
            switch (type)
            {
            case MonsterType::WhiteApe: return 70.0f;
            case MonsterType::Wolf: return 84.0f;
            case MonsterType::Slime: return 56.0f;
            }
            return 64.0f;
        }

        float alertWindowForMonster(MonsterType type)
        {
            switch (type)
            {
            case MonsterType::WhiteApe: return 2.9f;
            case MonsterType::Wolf: return 2.4f;
            case MonsterType::Slime: return 2.0f;
            }
            return 2.2f;
        }

        int supportThresholdForMonster(MonsterType type)
        {
            switch (type)
            {
            case MonsterType::WhiteApe: return 1;
            case MonsterType::Wolf: return 1;
            case MonsterType::Slime: return 2;
            }
            return 1;
        }
    }

    MonsterAIComponent::MonsterAIComponent(MonsterType type,
                                           engine::object::GameObject *player,
                                           engine::world::ChunkManager *chunkManager,
                                           glm::vec2 spawnOrigin)
        : m_type(type)
        , m_target(player)
        , m_chunkManager(chunkManager)
        , m_spawnOrigin(spawnOrigin)
    {
    }

    glm::vec2 MonsterAIComponent::getTargetDelta() const
    {
        if (!_owner || !m_target)
            return {0.0f, 0.0f};

        auto *selfTransform = _owner->getComponent<engine::component::TransformComponent>();
        auto *targetTransform = m_target->getComponent<engine::component::TransformComponent>();
        if (!selfTransform || !targetTransform)
            return {0.0f, 0.0f};

        return targetTransform->getPosition() - selfTransform->getPosition();
    }

    bool MonsterAIComponent::isGrounded() const
    {
        if (!_owner) return false;
        auto *physics = _owner->getComponent<engine::component::PhysicsComponent>();
        if (!physics) return false;
        return std::abs(physics->getVelocity().y) < 0.15f;
    }

    void MonsterAIComponent::refreshAiState(float dt, float distanceToTarget)
    {
        if (!m_target)
        {
            m_alertTimer = 0.0f;
            m_aiState = AiState::Patrol;
            return;
        }

        const float perceptionRange = perceptionRangeForMonster(m_type);
        const float combatRange = combatRangeForMonster(m_type);
        const float retreatDistance = retreatDistanceForMonster(m_type);
        const bool sensesTarget = distanceToTarget > 0.0f && distanceToTarget <= perceptionRange;

        if (sensesTarget)
            m_alertTimer = alertWindowForMonster(m_type) + 0.18f * static_cast<float>(m_nearbyAllies);
        else
            m_alertTimer = std::max(0.0f, m_alertTimer - dt);

        if (m_alertTimer <= 0.0f)
        {
            m_aiState = AiState::Patrol;
            return;
        }

        const bool isolated = m_nearbyAllies <= 0;
        const bool supported = m_nearbyAllies >= supportThresholdForMonster(m_type);
        if (distanceToTarget <= retreatDistance && isolated)
        {
            m_aiState = AiState::Retreat;
            return;
        }

        if (sensesTarget && (distanceToTarget <= combatRange || supported))
        {
            m_aiState = AiState::Combat;
            return;
        }

        m_aiState = AiState::Alert;
    }

    void MonsterAIComponent::updateSlime(float dt)
    {
        auto *physics = _owner->getComponent<engine::component::PhysicsComponent>();
        auto *sprite = _owner->getComponent<engine::component::SpriteComponent>();
        if (!physics || !sprite) return;

        glm::vec2 delta = getTargetDelta();
        float distance = glm::length(delta);
        float dir = (delta.x == 0.0f) ? m_wanderDir : (delta.x >= 0.0f ? 1.0f : -1.0f);
        bool chasing = m_aiState == AiState::Combat;
        bool retreating = m_aiState == AiState::Retreat;
        bool alerting = m_aiState == AiState::Alert;

        if (m_aiState == AiState::Patrol || distance > 420.0f)
            dir = m_wanderDir;
        else if (retreating)
            dir *= -1.0f;

        sprite->setFlipped(dir < 0.0f);
        m_actionCooldown = std::max(0.0f, m_actionCooldown - dt);
        auto vel = physics->getVelocity();

        if (isGrounded() && m_actionCooldown <= 0.0f)
        {
            if (retreating)
            {
                vel.x = 3.9f * dir;
                vel.y = -3.7f;
                m_actionCooldown = 0.82f;
            }
            else if (chasing)
            {
                vel.x = 3.6f * dir;
                vel.y = -3.5f;
                m_actionCooldown = 0.78f;
            }
            else if (alerting)
            {
                vel.x = 2.6f * dir;
                vel.y = -3.0f;
                m_actionCooldown = 0.96f;
            }
            else
            {
                vel.x = 2.0f * dir;
                vel.y = -2.8f;
                m_actionCooldown = 1.24f;
            }
            physics->setVelocity(vel);
        }
    }

    void MonsterAIComponent::updateWolf(float dt)
    {
        auto *physics = _owner->getComponent<engine::component::PhysicsComponent>();
        auto *sprite = _owner->getComponent<engine::component::SpriteComponent>();
        if (!physics || !sprite) return;

        glm::vec2 delta = getTargetDelta();
        float distance = glm::length(delta);
        float dir = (delta.x == 0.0f) ? m_wanderDir : (delta.x >= 0.0f ? 1.0f : -1.0f);
        const bool chasing = m_aiState == AiState::Combat;
        const bool retreating = m_aiState == AiState::Retreat;
        const bool alerting = m_aiState == AiState::Alert;
        if (m_aiState == AiState::Patrol) dir = m_wanderDir;
        else if (retreating) dir *= -1.0f;

        sprite->setFlipped(dir < 0.0f);
        auto vel = physics->getVelocity();
        float targetX = (chasing ? 5.8f : alerting ? 4.0f : retreating ? 6.6f : 2.5f) * dir;
        float step = (chasing ? 11.0f : retreating ? 13.0f : alerting ? 8.5f : 6.0f) * dt;
        if (vel.x < targetX) vel.x = std::min(vel.x + step, targetX);
        else vel.x = std::max(vel.x - step, targetX);

        m_actionCooldown = std::max(0.0f, m_actionCooldown - dt);
        if (chasing && isGrounded() && m_actionCooldown <= 0.0f && distance < 150.0f)
        {
            vel.y = -3.9f;
            vel.x = dir * 7.2f;
            m_actionCooldown = 1.28f;
        }
        else if (retreating && isGrounded() && m_actionCooldown <= 0.0f && distance < 110.0f)
        {
            vel.y = -3.1f;
            vel.x = dir * 6.8f;
            m_actionCooldown = 1.05f;
        }

        physics->setVelocity(vel);
    }

    void MonsterAIComponent::updateWhiteApe(float dt)
    {
        auto *physics = _owner->getComponent<engine::component::PhysicsComponent>();
        auto *sprite = _owner->getComponent<engine::component::SpriteComponent>();
        if (!physics || !sprite) return;

        glm::vec2 delta = getTargetDelta();
        float dir = (delta.x == 0.0f) ? m_wanderDir : (delta.x >= 0.0f ? 1.0f : -1.0f);
        const bool chasing = m_aiState == AiState::Combat;
        const bool retreating = m_aiState == AiState::Retreat;
        const bool alerting = m_aiState == AiState::Alert;
        if (m_aiState == AiState::Patrol) dir = m_wanderDir;
        else if (retreating) dir *= -1.0f;

        sprite->setFlipped(dir < 0.0f);
        m_actionCooldown = std::max(0.0f, m_actionCooldown - dt);
        auto vel = physics->getVelocity();

        if (isGrounded())
        {
            if (chasing && m_actionCooldown <= 0.0f)
            {
                vel.x = dir * 8.4f;
                vel.y = -5.0f;
                m_actionCooldown = 1.45f;
            }
            else if (retreating && m_actionCooldown <= 0.0f)
            {
                vel.x = dir * 7.5f;
                vel.y = -4.4f;
                m_actionCooldown = 1.22f;
            }
            else if (alerting)
            {
                vel.x = dir * 3.2f;
            }
            else
            {
                vel.x = dir * 1.8f;
            }
        }

        physics->setVelocity(vel);
    }

    void MonsterAIComponent::updatePlayerControlled(float /*dt*/)
    {
        if (!_owner)
            return;

        auto *controller = _owner->getComponent<engine::component::ControllerComponent>();
        auto *sprite = _owner->getComponent<engine::component::SpriteComponent>();
        if (!controller || !sprite)
            return;

        sprite->setFlipped(
            controller->getFacingDirection() == engine::component::ControllerComponent::FacingDirection::Left);
    }

    void MonsterAIComponent::update(float delta_time)
    {
        if (!_owner) return;

        if (m_driveMode == DriveMode::PlayerControlled)
        {
            updatePlayerControlled(delta_time);
            return;
        }

        const glm::vec2 targetDelta = getTargetDelta();
        refreshAiState(delta_time, glm::length(targetDelta));

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