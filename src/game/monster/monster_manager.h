#pragma once

#include "monster_ai_component.h"
#include <glm/vec2.hpp>
#include <vector>
#include <random>

namespace engine::core { class Context; }
namespace engine::actor { class ActorManager; }
namespace engine::physics { class PhysicsManager; }
namespace engine::world { class ChunkManager; }
namespace engine::object { class GameObject; }

namespace game::monster
{
    class MonsterManager
    {
    public:
        MonsterManager(engine::core::Context &context,
                       engine::actor::ActorManager &actorManager,
                       engine::physics::PhysicsManager &physicsManager,
                       engine::world::ChunkManager &chunkManager,
                       engine::object::GameObject *player);

        void update(float delta_time);
        void setAnchorActor(engine::object::GameObject *actor) { m_anchorActor = actor; }
        void setHostileTarget(engine::object::GameObject *actor);
        engine::object::GameObject *findNearestMonster(const glm::vec2 &origin, float maxDistance) const;
        bool possessMonster(engine::object::GameObject *monster);
        void releasePossessedMonster();
        engine::object::GameObject *getPossessedMonster() const { return m_possessedMonster; }
        bool isMonsterControlled(engine::object::GameObject *actor) const { return actor && actor == m_possessedMonster; }
        MonsterType getMonsterType(const engine::object::GameObject *monster) const;
        const MonsterAIComponent *getMonsterAI(const engine::object::GameObject *monster) const;
        void renderGroundShadows(engine::core::Context &context) const;
        void renderIFFMarkers(engine::object::GameObject *controlledActor, float pulse) const;
        int crushMonstersInRadius(const glm::vec2 &center, float radius);
        int slashMonsters(const glm::vec2 &origin,
                  float facing,
                  float range,
                  float halfHeight,
                  std::vector<glm::vec2> *defeatPositions = nullptr);
        int strikeMonstersFrom(engine::object::GameObject *source,
                       float facing,
                       float range,
                       float halfHeight,
                       std::vector<glm::vec2> *defeatPositions = nullptr);
        int blastMonstersFrom(engine::object::GameObject *source,
                      const glm::vec2 &center,
                      float radius,
                      std::vector<glm::vec2> *defeatPositions = nullptr);
        size_t monsterCount() const { return m_monsters.size(); }

    private:
        struct MonsterEntry
        {
            engine::object::GameObject *actor = nullptr;
            MonsterType type = MonsterType::Slime;
        };

        engine::core::Context &m_context;
        engine::actor::ActorManager &m_actorManager;
        engine::physics::PhysicsManager &m_physicsManager;
        engine::world::ChunkManager &m_chunkManager;
        engine::object::GameObject *m_player;
        engine::object::GameObject *m_anchorActor = nullptr;
        engine::object::GameObject *m_hostileTarget = nullptr;
        engine::object::GameObject *m_possessedMonster = nullptr;
        std::vector<MonsterEntry> m_monsters;
        std::mt19937 m_rng;
        float m_spawnTimer = 0.0f;

        void spawnMonster();
        void cleanupMonsters();
        bool findSpawnPosition(glm::vec2 &outWorldPos);
        MonsterType pickMonsterType() const;
    };
}