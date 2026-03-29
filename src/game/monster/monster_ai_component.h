#pragma once

#include "../../engine/component/component.h"
#include <glm/vec2.hpp>

namespace engine::object { class GameObject; }
namespace engine::world { class ChunkManager; }

namespace game::monster
{
    enum class MonsterType
    {
        WhiteApe,
        Slime,
        Wolf,
    };

    class MonsterAIComponent final : public engine::component::Component
    {
    public:
        enum class DriveMode
        {
            Autonomous,
            PlayerControlled,
        };

        enum class AiState
        {
            Patrol,
            Alert,
            Combat,
            Retreat,
        };

        MonsterAIComponent(MonsterType type,
                           engine::object::GameObject *player,
                           engine::world::ChunkManager *chunkManager,
                           glm::vec2 spawnOrigin);

        void setTarget(engine::object::GameObject *target) { m_target = target; }
        void setDriveMode(DriveMode mode) { m_driveMode = mode; }
        DriveMode getDriveMode() const { return m_driveMode; }
        MonsterType getMonsterType() const { return m_type; }
        void setNearbyAllies(int count) { m_nearbyAllies = count; }
        int getNearbyAllies() const { return m_nearbyAllies; }
        AiState getAiState() const { return m_aiState; }

    private:
        MonsterType m_type;
        engine::object::GameObject *m_target;
        engine::world::ChunkManager *m_chunkManager;
        glm::vec2 m_spawnOrigin;
        DriveMode m_driveMode = DriveMode::Autonomous;
        AiState m_aiState = AiState::Patrol;

        float m_thinkTimer = 0.0f;
        float m_actionCooldown = 0.0f;
        float m_wanderDir = 1.0f;
        float m_alertTimer = 0.0f;
        int m_nearbyAllies = 0;

        bool isGrounded() const;
        void updateSlime(float dt);
        void updateWolf(float dt);
        void updateWhiteApe(float dt);
        glm::vec2 getTargetDelta() const;
        void updatePlayerControlled(float dt);
        void refreshAiState(float dt, float distanceToTarget);

        void handleInput() override {}
        void update(float delta_time) override;
        void render() override {}
    };
}