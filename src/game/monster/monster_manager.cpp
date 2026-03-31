#include "monster_manager.h"
#include "../../engine/core/context.h"
#include "../../engine/actor/actor_manager.h"
#include "../../engine/render/camera.h"
#include "../../engine/render/renderer.h"
#include "../../engine/component/physics_component.h"
#include "../../engine/component/controller_component.h"
#include "../../engine/component/sprite_component.h"
#include "../../engine/component/transform_component.h"
#include "../../engine/object/game_object.h"
#include "../../engine/physics/physics_manager.h"
#include "../../engine/utils/alignment.h"
#include "../../engine/world/chunk_manager.h"
#include "../../engine/world/tile_info.h"
#include <imgui.h>
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
            case MonsterType::WhiteApe: return "assets/textures/Actors/eagle-attack.png"; // 40x41 per frame
            case MonsterType::Wolf:     return "assets/textures/Actors/opossum.png";       // 36x28 per frame
            case MonsterType::Slime:    return "assets/textures/Actors/frog.png";          // 35x64 per frame
            }
            return "assets/textures/Actors/frog.png";
        }

        // 每种怪物精灵第一帧的源矩形（精灵条）
        engine::utils::FRect frameRectForMonster(MonsterType type)
        {
            switch (type)
            {
            case MonsterType::WhiteApe: return engine::utils::FRect{{0.0f, 0.0f}, {40.0f, 41.0f}};
            case MonsterType::Wolf:     return engine::utils::FRect{{0.0f, 0.0f}, {36.0f, 28.0f}};
            case MonsterType::Slime:    return engine::utils::FRect{{0.0f, 0.0f}, {35.0f, 64.0f}};
            }
            return engine::utils::FRect{{0.0f, 0.0f}, {35.0f, 64.0f}};
        }

        // 2.5D 脚底矩形碰撞体：halfX=精灵宽度一半，halfY=深度薄片（shadow 高度一半）
        glm::vec2 bodyHalfSizeForMonster(MonsterType type)
        {
            constexpr float kPPM = 32.0f;
            switch (type)
            {
            case MonsterType::WhiteApe: return {13.0f / kPPM, 4.0f / kPPM};   // eagle  shadow 26×8px
            case MonsterType::Wolf:     return {11.0f / kPPM, 3.25f / kPPM};  // opossum shadow 22×6.5px
            case MonsterType::Slime:    return {9.0f  / kPPM, 2.75f / kPPM};  // frog   shadow 18×5.5px
            }
            return {10.0f / kPPM, 3.5f / kPPM};
        }

        glm::vec2 shadowSizeForMonster(MonsterType type)
        {
            switch (type)
            {
            case MonsterType::WhiteApe: return {26.0f, 8.0f};
            case MonsterType::Wolf: return {22.0f, 6.5f};
            case MonsterType::Slime: return {18.0f, 5.5f};
            }
            return {18.0f, 6.0f};
        }

        float speedForMonster(MonsterType type)
        {
            switch (type)
            {
            case MonsterType::WhiteApe: return 14.0f;
            case MonsterType::Wolf: return 16.0f;
            case MonsterType::Slime: return 12.0f;
            }
            return 12.0f;
        }

        void drawShadow(engine::core::Context &context, const glm::vec2 &center, const glm::vec2 &size, float alpha)
        {
            auto &renderer = context.getRenderer();
            const auto &camera = context.getCamera();

            renderer.drawRect(camera,
                              center.x - size.x * 0.5f,
                              center.y - size.y * 0.5f,
                              size.x,
                              size.y,
                              glm::vec4(0.0f, 0.0f, 0.0f, alpha));
            renderer.drawRect(camera,
                              center.x - size.x * 0.35f,
                              center.y - size.y * 0.38f,
                              size.x * 0.70f,
                              size.y * 0.76f,
                              glm::vec4(0.0f, 0.0f, 0.0f, alpha * 0.55f));
        }

        ImVec2 logicalToImGuiScreen(const engine::core::Context &context, const glm::vec2 &logicalPos)
        {
            glm::vec2 logicalSize = context.getRenderer().getLogicalSize();
            ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            if (logicalSize.x <= 0.0f || logicalSize.y <= 0.0f)
                return {logicalPos.x, logicalPos.y};

            return {
                logicalPos.x * (displaySize.x / logicalSize.x),
                logicalPos.y * (displaySize.y / logicalSize.y)
            };
        }

        float distance2p5d(const glm::vec2 &a, const glm::vec2 &b)
        {
            constexpr float kDepthWeight = 0.42f;
            const glm::vec2 d = a - b;
            return std::sqrt(d.x * d.x + d.y * d.y * kDepthWeight * kDepthWeight);
        }

        const char *labelForState(MonsterAIComponent::AiState state)
        {
            switch (state)
            {
            case MonsterAIComponent::AiState::Patrol: return "巡猎";
            case MonsterAIComponent::AiState::Alert: return "警觉";
            case MonsterAIComponent::AiState::Combat: return "围攻";
            case MonsterAIComponent::AiState::Retreat: return "退避";
            }
            return "巡猎";
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
        , m_anchorActor(player)
        , m_hostileTarget(player)
        , m_rng(1234567u)
    {
    }

    void MonsterManager::setHostileTarget(engine::object::GameObject *actor)
    {
        m_hostileTarget = actor ? actor : m_player;
        for (auto &entry : m_monsters)
        {
            if (!entry.actor)
                continue;
            if (auto *ai = entry.actor->getComponent<MonsterAIComponent>())
            {
                ai->setTarget(entry.actor == m_possessedMonster ? nullptr : m_hostileTarget);
            }
        }
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
        engine::object::GameObject *anchor = m_anchorActor ? m_anchorActor : m_player;
        if (!anchor) return false;
        auto *playerTransform = anchor->getComponent<engine::component::TransformComponent>();
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

        // DNF 模式回退：地面全为 GroundDecor（非固体），上方扫描无法找到实心地面。
        // 直接在玩家左右随机水平偏移处、保持相同 Y 深度生成敌人。
        {
            float sign = sideDist(m_rng) == 0 ? -1.0f : 1.0f;
            float spawnX = playerPos.x + sign * radiusDist(m_rng);
            outWorldPos = {spawnX, playerPos.y};
            return true;
        }
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
        // BOTTOM_CENTER：transform.y = 精灵底部（脚）= 世界深度位置；精灵向上渲染
        auto *monSprite = monster->addComponent<engine::component::SpriteComponent>(textureForMonster(type), engine::utils::Alignment::BOTTOM_CENTER);
        monSprite->setSourceRect(frameRectForMonster(type));
        auto *controller = monster->addComponent<engine::component::ControllerComponent>(speedForMonster(type), 0.0f);
        controller->setGroundAcceleration(56.0f);
        controller->setAirAcceleration(20.0f);
        controller->setJumpSpeed(8.0f);
        controller->setGroundBand(16.0f, 96.0f);
        controller->setJetpackEnabled(false);
        controller->setEnabled(false);
        glm::vec2 halfSize = bodyHalfSizeForMonster(type);
        b2BodyId bodyId = m_physicsManager.createDynamicBody(
            {spawnPos.x / kPixelsPerMeter, spawnPos.y / kPixelsPerMeter},
            {halfSize.x, halfSize.y},
            monster);
        monster->addComponent<engine::component::PhysicsComponent>(bodyId, &m_physicsManager);
        auto *ai = monster->addComponent<MonsterAIComponent>(type, m_hostileTarget ? m_hostileTarget : m_player, &m_chunkManager, spawnPos);
        ai->setTarget(m_hostileTarget ? m_hostileTarget : m_player);
        m_monsters.push_back({monster, type});
    }

    void MonsterManager::cleanupMonsters()
    {
        engine::object::GameObject *anchor = m_anchorActor ? m_anchorActor : m_player;
        if (!anchor) return;
        auto *playerTransform = anchor->getComponent<engine::component::TransformComponent>();
        if (!playerTransform) return;

        glm::vec2 playerPos = playerTransform->getPosition();
        m_monsters.erase(
            std::remove_if(m_monsters.begin(), m_monsters.end(), [&](MonsterEntry &entry)
            {
                if (!entry.actor || entry.actor->isNeedRemove())
                {
                    if (entry.actor == m_possessedMonster)
                        m_possessedMonster = nullptr;
                    return true;
                }

                auto *transform = entry.actor->getComponent<engine::component::TransformComponent>();
                if (!transform)
                {
                    entry.actor->setNeedRemove(true);
                    if (entry.actor == m_possessedMonster)
                        m_possessedMonster = nullptr;
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

        for (auto &entry : m_monsters)
        {
            if (!entry.actor || entry.actor->isNeedRemove() || entry.actor == m_possessedMonster)
                continue;

            auto *transform = entry.actor->getComponent<engine::component::TransformComponent>();
            auto *ai = entry.actor->getComponent<MonsterAIComponent>();
            if (!transform || !ai)
                continue;

            int nearbyAllies = 0;
            const glm::vec2 pos = transform->getPosition();
            for (const auto &other : m_monsters)
            {
                if (!other.actor || other.actor == entry.actor || other.actor->isNeedRemove() || other.actor == m_possessedMonster)
                    continue;

                auto *otherTransform = other.actor->getComponent<engine::component::TransformComponent>();
                if (!otherTransform)
                    continue;

                    if (distance2p5d(otherTransform->getPosition(), pos) <= 260.0f)
                    ++nearbyAllies;
            }

            ai->setNearbyAllies(nearbyAllies);
        }
    }

    MonsterType MonsterManager::getMonsterType(const engine::object::GameObject *monster) const
    {
        for (const auto &entry : m_monsters)
        {
            if (entry.actor == monster)
                return entry.type;
        }
        return MonsterType::Slime;
    }

    const MonsterAIComponent *MonsterManager::getMonsterAI(const engine::object::GameObject *monster) const
    {
        if (!monster)
            return nullptr;
        return monster->getComponent<MonsterAIComponent>();
    }

    engine::object::GameObject *MonsterManager::findNearestMonster(const glm::vec2 &origin, float maxDistance) const
    {
        engine::object::GameObject *best = nullptr;
        float bestDistanceSq = maxDistance * maxDistance;

        for (const auto &entry : m_monsters)
        {
            if (!entry.actor || entry.actor->isNeedRemove())
                continue;

            auto *transform = entry.actor->getComponent<engine::component::TransformComponent>();
            if (!transform)
                continue;

                const float dist2p5d = distance2p5d(transform->getPosition(), origin);
                const float distanceSq = dist2p5d * dist2p5d;
            if (distanceSq <= bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                best = entry.actor;
            }
        }

        return best;
    }

    bool MonsterManager::possessMonster(engine::object::GameObject *monster)
    {
        if (!monster || monster == m_possessedMonster)
            return false;

        auto *ai = monster->getComponent<MonsterAIComponent>();
        auto *controller = monster->getComponent<engine::component::ControllerComponent>();
        auto *physics = monster->getComponent<engine::component::PhysicsComponent>();
        if (!ai || !controller || !physics)
            return false;

        if (m_possessedMonster)
            releasePossessedMonster();

        ai->setDriveMode(MonsterAIComponent::DriveMode::PlayerControlled);
        ai->setTarget(nullptr);
        controller->setEnabled(true);
        physics->setVelocity({0.0f, 0.0f});
        m_possessedMonster = monster;
        setHostileTarget(monster);
        return true;
    }

    void MonsterManager::releasePossessedMonster()
    {
        if (!m_possessedMonster)
            return;

        auto *ai = m_possessedMonster->getComponent<MonsterAIComponent>();
        auto *controller = m_possessedMonster->getComponent<engine::component::ControllerComponent>();
        auto *physics = m_possessedMonster->getComponent<engine::component::PhysicsComponent>();
        if (controller)
            controller->setEnabled(false);
        if (physics)
            physics->setVelocity({0.0f, 0.0f});
        if (ai)
        {
            ai->setDriveMode(MonsterAIComponent::DriveMode::Autonomous);
            ai->setTarget(m_player);
        }

        m_possessedMonster = nullptr;
        setHostileTarget(m_player);
    }

    void MonsterManager::renderGroundShadows(engine::core::Context &context) const
    {
        for (const auto &entry : m_monsters)
        {
            if (!entry.actor || entry.actor->isNeedRemove())
                continue;

            auto *transform = entry.actor->getComponent<engine::component::TransformComponent>();
            auto *physics = entry.actor->getComponent<engine::component::PhysicsComponent>();
            if (!transform)
                continue;

            glm::vec2 size = shadowSizeForMonster(entry.type);
            float alpha = 0.16f;
            if (physics)
            {
                float airFactor = std::min(std::abs(physics->getVelocity().y) / 7.0f, 1.0f);
                alpha *= 1.0f - airFactor * 0.35f;
                size *= 1.0f - airFactor * 0.18f;
            }

            glm::vec2 shadowCenter = transform->getPosition() + glm::vec2(0.0f, 15.0f);
            drawShadow(context, shadowCenter, size, alpha);
        }
    }

    void MonsterManager::renderIFFMarkers(engine::object::GameObject *controlledActor, float pulse) const
    {
        ImDrawList *dl = ImGui::GetForegroundDrawList();
        const auto &camera = m_context.getCamera();

        for (const auto &entry : m_monsters)
        {
            if (!entry.actor || entry.actor->isNeedRemove())
                continue;

            auto *transform = entry.actor->getComponent<engine::component::TransformComponent>();
            auto *ai = entry.actor->getComponent<MonsterAIComponent>();
            if (!transform || !ai)
                continue;

            const bool isControlled = entry.actor == m_possessedMonster;
            const bool playerControllingMonster = controlledActor == m_possessedMonster;
            const glm::vec2 worldPos = transform->getPosition();
            const ImVec2 screen = logicalToImGuiScreen(m_context, camera.worldToScreen(worldPos + glm::vec2{0.0f, 14.0f}));

            ImU32 outerColor = IM_COL32(210, 92, 70, 160);
            ImU32 innerColor = IM_COL32(255, 140, 92, 110);
            ImU32 textColor = IM_COL32(255, 232, 220, 220);
            if (isControlled)
            {
                outerColor = IM_COL32(80, 235, 255, 220);
                innerColor = IM_COL32(80, 235, 255, 90);
                textColor = IM_COL32(210, 250, 255, 255);
            }
            else if (playerControllingMonster)
            {
                switch (ai->getAiState())
                {
                case MonsterAIComponent::AiState::Patrol:
                    outerColor = IM_COL32(214, 150, 74, 136);
                    innerColor = IM_COL32(214, 150, 74, 72);
                    break;
                case MonsterAIComponent::AiState::Alert:
                    outerColor = IM_COL32(255, 220, 92, 190);
                    innerColor = IM_COL32(255, 220, 92, 86);
                    break;
                case MonsterAIComponent::AiState::Combat:
                    outerColor = IM_COL32(255, 92, 92, 220);
                    innerColor = IM_COL32(255, 92, 92, 96);
                    break;
                case MonsterAIComponent::AiState::Retreat:
                    outerColor = IM_COL32(110, 190, 255, 210);
                    innerColor = IM_COL32(110, 190, 255, 92);
                    break;
                }
            }

            const float radius = isControlled ? 17.0f : 13.0f + 1.5f * pulse;
            dl->AddCircle(screen, radius + 4.0f, innerColor, 28, 2.2f);
            dl->AddCircle(screen, radius, outerColor, 28, isControlled ? 2.8f : 1.8f);

            if (isControlled || playerControllingMonster || ai->getAiState() != MonsterAIComponent::AiState::Patrol)
            {
                const char *label = isControlled ? "友军" : labelForState(ai->getAiState());
                ImVec2 textSize = ImGui::CalcTextSize(label);
                ImVec2 boxMin{screen.x - textSize.x * 0.5f - 6.0f, screen.y - 34.0f};
                ImVec2 boxMax{screen.x + textSize.x * 0.5f + 6.0f, screen.y - 16.0f};
                dl->AddRectFilled(boxMin, boxMax, IM_COL32(8, 14, 20, 170), 4.0f);
                dl->AddRect(boxMin, boxMax, outerColor, 4.0f, 0, 1.0f);
                dl->AddText({screen.x - textSize.x * 0.5f, screen.y - 31.0f}, textColor, label);
            }
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

    int MonsterManager::slashMonsters(const glm::vec2 &origin,
                                      float facing,
                                      float range,
                                      float halfHeight,
                                      std::vector<glm::vec2> *defeatPositions)
    {
        int slain = 0;
        const float rangeSq = range * range;

        for (auto &entry : m_monsters)
        {
            if (!entry.actor || entry.actor->isNeedRemove())
                continue;

            auto *transform = entry.actor->getComponent<engine::component::TransformComponent>();
            if (!transform)
                continue;

            glm::vec2 delta = transform->getPosition() - origin;
            if (glm::dot(delta, delta) > rangeSq)
                continue;
            if (delta.x * facing < -18.0f)
                continue;
            if (std::abs(delta.y) > halfHeight)
                continue;

            entry.actor->setNeedRemove(true);
            if (defeatPositions)
                defeatPositions->push_back(transform->getPosition());
            ++slain;
        }

        return slain;
    }

    int MonsterManager::strikeMonstersFrom(engine::object::GameObject *source,
                                           float facing,
                                           float range,
                                           float halfHeight,
                                           std::vector<glm::vec2> *defeatPositions)
    {
        if (!source)
            return 0;

        auto *transform = source->getComponent<engine::component::TransformComponent>();
        if (!transform)
            return 0;

        int slain = 0;
        const glm::vec2 origin = transform->getPosition();
        const float rangeSq = range * range;
        for (auto &entry : m_monsters)
        {
            if (!entry.actor || entry.actor == source || entry.actor->isNeedRemove())
                continue;

            auto *targetTransform = entry.actor->getComponent<engine::component::TransformComponent>();
            if (!targetTransform)
                continue;

            const glm::vec2 delta = targetTransform->getPosition() - origin;
            if (glm::dot(delta, delta) > rangeSq)
                continue;
            if (delta.x * facing < -18.0f)
                continue;
            if (std::abs(delta.y) > halfHeight)
                continue;

            entry.actor->setNeedRemove(true);
            if (defeatPositions)
                defeatPositions->push_back(targetTransform->getPosition());
            ++slain;
        }

        return slain;
    }

    int MonsterManager::blastMonstersFrom(engine::object::GameObject *source,
                                          const glm::vec2 &center,
                                          float radius,
                                          std::vector<glm::vec2> *defeatPositions)
    {
        int slain = 0;
        const float radiusSq = radius * radius;
        for (auto &entry : m_monsters)
        {
            if (!entry.actor || entry.actor == source || entry.actor->isNeedRemove())
                continue;

            auto *transform = entry.actor->getComponent<engine::component::TransformComponent>();
            if (!transform)
                continue;

            const glm::vec2 delta = transform->getPosition() - center;
            if (glm::dot(delta, delta) > radiusSq)
                continue;

            entry.actor->setNeedRemove(true);
            if (defeatPositions)
                defeatPositions->push_back(transform->getPosition());
            ++slain;
        }

        return slain;
    }
}