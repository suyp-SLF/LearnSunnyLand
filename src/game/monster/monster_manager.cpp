#include "monster_manager.h"
#include "../../engine/actor/actor_manager.h"
#include "../../engine/component/physics_component.h"
#include "../../engine/component/sprite_component.h"
#include "../../engine/component/transform_component.h"
#include "../../engine/object/game_object.h"
#include "../../engine/physics/physics_manager.h"
#include "../../engine/utils/alignment.h"
#include "../../engine/world/chunk_manager.h"
#include "../../engine/world/tile_info.h"
#include <algorithm>

namespace game::monster
{
    namespace
    {
        constexpr float kSpawnInterval = 1.6f;
        constexpr size_t kMaxMonsters = 10;
        constexpr float kSpawnInnerRadius = 420.0f;
        constexpr float kSpawnOuterRadius = 960.0f;
        constexpr float kCleanupRadius = 1500.0f;
        constexpr float kPixelsPerMeter = 32.0f;

        const char* textureForMonster(MonsterType type)
        {
            switch (type)
            {
            case MonsterType::WhiteApe: return "assets/textures/Characters/white_ape.svg";
            case MonsterType::Wolf: return "assets/textures/Characters/wolf.svg";
            case MonsterType::Slime: return "assets/textures/Characters/slime.svg";
            }
            return "assets/textures/Characters/slime.svg";
        }

        glm::vec2 bodyHalfSizeForMonster(MonsterType type)
        {
            switch (type)
            {
            case MonsterType::WhiteApe: return {0.65f, 0.8f};
            case MonsterType::Wolf: return {0.60f, 0.45f};
            case MonsterType::Slime: return {0.48f, 0.38f};
            }
            return {0.5f, 0.5f};
        }
    }

    MonsterManager::MonsterManager(engine::core::Context &context,
                                   engine::actor::ActorManager &actorManager,
                                   engine::physics::PhysicsManager &physicsManager,
                                   engine::world::ChunkManager &chunkManager,
                                   engine::object::GameObject *player)
        : m_context(context)
        , m_actorManager(actorManager)
        , m_physicsManager(physicsManager)
        , m_chunkManager(chunkManager)
        , m_player(player)
        , m_rng(1234567u)
    {
    }

    MonsterType MonsterManager::pickMonsterType() const
    {
        int roll = const_cast<MonsterManager*>(this)->m_rng() % 3;
        switch (roll)
        {
        case 0: return MonsterType::Slime;
        case 1: return MonsterType::Wolf;
        default: return MonsterType::WhiteApe;
        }
    }

    bool MonsterManager::findSpawnPosition(glm::vec2 &outWorldPos)
    {
        if (!m_player) return false;
        auto *playerTransform = m_player->getComponent<engine::component::TransformComponent>();
        if (!playerTransform) return false;

        std::uniform_real_distribution<float> radiusDist(kSpawnInnerRadius, kSpawnOuterRadius);
        std::uniform_int_distribution<int> sideDist(0, 1);

        glm::vec2 playerPos = playerTransform->getPosition();
        for (int attempt = 0; attempt < 24; ++attempt)
        {
            float radius = radiusDist(m_rng);
            float sign = sideDist(m_rng) == 0 ? -1.0f : 1.0f;
            float worldX = playerPos.x + sign * radius;
            int tileX = static_cast<int>(worldX / 16.0f);

            for (int tileY = 8; tileY < 120; ++tileY)
            {
                auto below = m_chunkManager.tileAt(tileX, tileY);
                auto above = m_chunkManager.tileAt(tileX, tileY - 1);
                auto above2 = m_chunkManager.tileAt(tileX, tileY - 2);
                if (engine::world::isSolid(below.type) &&
                    above.type == engine::world::TileType::Air &&
                    above2.type == engine::world::TileType::Air)
                {
                    outWorldPos = {tileX * 16.0f + 8.0f, (tileY - 1) * 16.0f};
                    return true;
                }
            }
        }

        return false;
    }

    void MonsterManager::spawnMonster()
    {
        if (m_monsters.size() >= kMaxMonsters)
            return;

        glm::vec2 spawnPos{};
        if (!findSpawnPosition(spawnPos))
            return;

        MonsterType type = pickMonsterType();
        std::string name;
        switch (type)
        {
        case MonsterType::Slime: name = "slime"; break;
        case MonsterType::Wolf: name = "wolf"; break;
        case MonsterType::WhiteApe: name = "white_ape"; break;
        }

        auto *monster = m_actorManager.createActor(name);
        monster->setTag("monster");
        monster->addComponent<engine::component::TransformComponent>(spawnPos);
        monster->addComponent<engine::component::SpriteComponent>(textureForMonster(type), engine::utils::Alignment::CENTER);
        glm::vec2 halfSize = bodyHalfSizeForMonster(type);
        b2BodyId bodyId = m_physicsManager.createDynamicBody(
            {spawnPos.x / kPixelsPerMeter, spawnPos.y / kPixelsPerMeter},
            {halfSize.x, halfSize.y},
            monster);
        monster->addComponent<engine::component::PhysicsComponent>(bodyId, &m_physicsManager);
        monster->addComponent<MonsterAIComponent>(type, m_player, &m_chunkManager, spawnPos);
        m_monsters.push_back({monster, type});
    }

    void MonsterManager::cleanupMonsters()
    {
        if (!m_player) return;
        auto *playerTransform = m_player->getComponent<engine::component::TransformComponent>();
        if (!playerTransform) return;

        glm::vec2 playerPos = playerTransform->getPosition();
        m_monsters.erase(
            std::remove_if(m_monsters.begin(), m_monsters.end(), [&](MonsterEntry &entry)
            {
                if (!entry.actor || entry.actor->isNeedRemove())
                    return true;

                auto *transform = entry.actor->getComponent<engine::component::TransformComponent>();
                if (!transform)
                {
                    entry.actor->setNeedRemove(true);
                    return true;
                }

                if (glm::distance(transform->getPosition(), playerPos) > kCleanupRadius)
                {
                    entry.actor->setNeedRemove(true);
                    return true;
                }

                return false;
            }),
            m_monsters.end());
    }

    void MonsterManager::update(float delta_time)
    {
        cleanupMonsters();
        m_spawnTimer -= delta_time;
        if (m_spawnTimer <= 0.0f)
        {
            m_spawnTimer = kSpawnInterval;
            spawnMonster();
        }
    }

    int MonsterManager::crushMonstersInRadius(const glm::vec2 &center, float radius)
    {
        int crushed = 0;
        const float radiusSq = radius * radius;

        for (auto &entry : m_monsters)
        {
            if (!entry.actor || entry.actor->isNeedRemove())
                continue;

            auto *transform = entry.actor->getComponent<engine::component::TransformComponent>();
            if (!transform)
                continue;

            glm::vec2 delta = transform->getPosition() - center;
            if (glm::dot(delta, delta) > radiusSq)
                continue;

            entry.actor->setNeedRemove(true);
            ++crushed;
        }

        return crushed;
    }
}