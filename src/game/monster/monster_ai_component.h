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
        MonsterAIComponent(MonsterType type,
                           engine::object::GameObject *player,
                           engine::world::ChunkManager *chunkManager,
                           glm::vec2 spawnOrigin);

    private:
        MonsterType m_type;
        engine::object::GameObject *m_player;
        engine::world::ChunkManager *m_chunkManager;
        glm::vec2 m_spawnOrigin;

        float m_thinkTimer = 0.0f;
        float m_actionCooldown = 0.0f;
        float m_wanderDir = 1.0f;

        bool isGrounded() const;
        void updateSlime(float dt);
        void updateWolf(float dt);
        void updateWhiteApe(float dt);
        glm::vec2 getPlayerDelta() const;

        void handleInput() override {}
        void update(float delta_time) override;
        void render() override {}
    };
}