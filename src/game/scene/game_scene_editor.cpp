#include "game_scene.h"

#include "../../engine/component/animation_component.h"
#include "../../engine/component/controller_component.h"
#include "../../engine/component/parallax_component.h"
#include "../../engine/component/physics_component.h"
#include "../../engine/component/sprite_component.h"
#include "../../engine/component/transform_component.h"
#include "../../engine/object/game_object.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace game::scene
{
namespace
{
    void saveBoolSetting(const char* key, bool enabled)
    {
        nlohmann::json json = nlohmann::json::object();

        std::ifstream input("assets/settings.json");
        if (input.is_open())
        {
            try
            {
                input >> json;
            }
            catch (const std::exception&)
            {
                json = nlohmann::json::object();
            }
        }

        json[key] = enabled;

        std::ofstream output("assets/settings.json");
        if (!output.is_open())
            return;
        output << json.dump(4);
    }
}

void GameScene::setGameplayRunning(bool running)
{
    if (m_gameplayRunning == running)
        return;

    if (running && m_enablePlayRollback)
        capturePlaySnapshot();

    m_gameplayRunning = running;
    m_gameplayPaused = false;
    m_stepOneFrame = false;
    if (!m_gameplayRunning)
    {
        const bool restored = m_enablePlayRollback && restorePlaySnapshot();

        auto stopActorMotion = [](engine::object::GameObject* actor) {
            if (!actor)
                return;
            if (auto* physics = actor->getComponent<engine::component::PhysicsComponent>())
                physics->setVelocity({0.0f, 0.0f});
            if (auto* ctrl = actor->getComponent<engine::component::ControllerComponent>())
                ctrl->setRunMode(false);
        };

        if (!restored)
        {
            stopActorMotion(m_player);
            stopActorMotion(m_mech);
            stopActorMotion(m_possessedMonster);
        }

        m_isDashing = false;
        m_dashTimer = 0.0f;
        m_dashCooldown = 0.0f;
        shutdownFlightAmbientSound();
    }

    spdlog::info("编辑器播放状态切换: {}", m_gameplayRunning ? "运行" : "编辑");
}

void GameScene::capturePlaySnapshot()
{
    m_hasPlaySnapshot = false;
    m_playActorSnapshots.clear();
    m_playTileSnapshots.clear();

    if (!actor_manager)
        return;

    const auto& actors = actor_manager->getActors();
    m_playActorSnapshots.reserve(actors.size());
    for (size_t index = 0; index < actors.size(); ++index)
    {
        const auto* actor = actors[index].get();
        if (!actor)
            continue;

        ActorRuntimeSnapshot snapshot;
        snapshot.name = actor->getName();
        snapshot.tag = actor->getTag();
        snapshot.needRemove = actor->isNeedRemove();

        if (auto* transform = actor->getComponent<engine::component::TransformComponent>())
        {
            snapshot.hasTransform = true;
            snapshot.position = transform->getPosition();
            snapshot.scale = transform->getScale();
            snapshot.rotation = transform->getRotation();
        }
        if (auto* controller = actor->getComponent<engine::component::ControllerComponent>())
        {
            snapshot.hasController = true;
            snapshot.controllerSpeed = controller->getSpeed();
            snapshot.controllerEnabled = controller->isEnabled();
            snapshot.controllerRunMode = controller->isRunMode();
        }
        if (auto* physics = actor->getComponent<engine::component::PhysicsComponent>())
        {
            snapshot.hasPhysics = true;
            snapshot.physicsVelocity = physics->getVelocity();
            snapshot.physicsPosition = physics->getPosition();
        }
        if (auto* sprite = actor->getComponent<engine::component::SpriteComponent>())
        {
            snapshot.hasSprite = true;
            snapshot.spriteHidden = sprite->isHidden();
            snapshot.spriteFlipped = sprite->isFlipped();
        }
        if (auto* parallax = actor->getComponent<engine::component::ParallaxComponent>())
        {
            snapshot.hasParallax = true;
            snapshot.parallaxFactor = parallax->getScrollFactor();
            snapshot.parallaxRepeat = parallax->getRepeat();
            snapshot.parallaxHidden = parallax->isHidden();
        }

        m_playActorSnapshots.push_back(std::move(snapshot));
    }

    if (chunk_manager)
    {
        const auto tileSize = chunk_manager->getTileSize();
        const auto loadedBounds = chunk_manager->getLoadedChunkBounds();
        for (const auto& [worldPos, worldSize] : loadedBounds)
        {
            const int startX = static_cast<int>(std::floor(worldPos.x / std::max(1, tileSize.x)));
            const int startY = static_cast<int>(std::floor(worldPos.y / std::max(1, tileSize.y)));
            const int widthTiles = std::max(1, static_cast<int>(std::round(worldSize.x / std::max(1, tileSize.x))));
            const int heightTiles = std::max(1, static_cast<int>(std::round(worldSize.y / std::max(1, tileSize.y))));
            for (int y = 0; y < heightTiles; ++y)
            {
                for (int x = 0; x < widthTiles; ++x)
                {
                    TileRuntimeSnapshot tile;
                    tile.x = startX + x;
                    tile.y = startY + y;
                    tile.tile = chunk_manager->tileAt(tile.x, tile.y);
                    m_playTileSnapshots.push_back(tile);
                }
            }
        }
    }

    auto findActorIndex = [&](engine::object::GameObject* target) {
        if (!target || !actor_manager)
            return -1;
        const auto& refs = actor_manager->getActors();
        for (int i = 0; i < static_cast<int>(refs.size()); ++i)
        {
            if (refs[static_cast<size_t>(i)].get() == target)
                return i;
        }
        return -1;
    };

    m_snapshotPlayerIndex = findActorIndex(m_player);
    m_snapshotMechIndex = findActorIndex(m_mech);
    m_snapshotPossessedIndex = findActorIndex(m_possessedMonster);
    m_snapshotIsPlayerInMech = m_isPlayerInMech;
    m_snapshotCurrentZone = m_currentZone;

    m_playUiSnapshot.showInventory = m_showInventory;
    m_playUiSnapshot.showSettings = m_showSettings;
    m_playUiSnapshot.showMapEditor = m_showMapEditor;
    m_playUiSnapshot.showCommandInput = m_showCommandInput;
    m_playUiSnapshot.missionWindow = m_missionUI.showWindow;
    m_playUiSnapshot.showSettlement = m_showSettlement;
    m_playUiSnapshot.showHierarchyPanel = m_showHierarchyPanel;
    m_playUiSnapshot.showInspectorPanel = m_showInspectorPanel;
    m_playUiSnapshot.showFpsOverlay = m_showFpsOverlay;
    m_playUiSnapshot.devMode = m_devMode;
    m_playUiSnapshot.showSkillDebug = m_showSkillDebugOverlay;
    m_playUiSnapshot.showChunkHighlight = m_showActiveChunkHighlights;
    m_playUiSnapshot.selectedActorIndex = m_selectedActorIndex;
    m_playUiSnapshot.weaponActiveIndex = m_weaponBar.getActiveIndex();
    m_playUiSnapshot.inventory = m_inventory;
    m_playUiSnapshot.mechInventory = m_mechInventory;
    m_playUiSnapshot.equipmentLoadout = m_equipmentLoadout;
    m_playUiSnapshot.starSockets = m_starSockets;
    m_playUiSnapshot.skillCooldowns = m_skillCooldowns;
    m_playUiSnapshot.weaponBar = m_weaponBar;
    m_playTimeSnapshot = m_timeOfDaySystem.captureRuntimeState();
    m_playWeatherSnapshot = m_weatherSystem.captureRuntimeState();
    m_hasPlaySnapshot = true;
}

bool GameScene::restorePlaySnapshot()
{
    if (!m_hasPlaySnapshot || !actor_manager)
        return false;

    auto& actors = actor_manager->getActors();
    const int restoreCount = std::min(static_cast<int>(actors.size()), static_cast<int>(m_playActorSnapshots.size()));
    for (int i = 0; i < restoreCount; ++i)
    {
        auto* actor = actors[static_cast<size_t>(i)].get();
        if (!actor)
            continue;

        const auto& snapshot = m_playActorSnapshots[static_cast<size_t>(i)];
        actor->setName(snapshot.name);
        actor->setTag(snapshot.tag);
        actor->setNeedRemove(snapshot.needRemove);

        if (snapshot.hasTransform)
        {
            if (auto* transform = actor->getComponent<engine::component::TransformComponent>())
            {
                transform->setPosition(snapshot.position);
                transform->setScale(snapshot.scale);
                transform->setRotation(snapshot.rotation);
            }
        }
        if (snapshot.hasController)
        {
            if (auto* controller = actor->getComponent<engine::component::ControllerComponent>())
            {
                controller->setSpeed(snapshot.controllerSpeed);
                controller->setEnabled(snapshot.controllerEnabled);
                controller->setRunMode(snapshot.controllerRunMode);
            }
        }
        if (snapshot.hasPhysics)
        {
            if (auto* physics = actor->getComponent<engine::component::PhysicsComponent>())
            {
                physics->setWorldPosition(snapshot.physicsPosition);
                physics->setVelocity(snapshot.physicsVelocity);
            }
        }
        if (snapshot.hasSprite)
        {
            if (auto* sprite = actor->getComponent<engine::component::SpriteComponent>())
            {
                sprite->setHidden(snapshot.spriteHidden);
                sprite->setFlipped(snapshot.spriteFlipped);
            }
        }
        if (snapshot.hasParallax)
        {
            if (auto* parallax = actor->getComponent<engine::component::ParallaxComponent>())
            {
                parallax->setScrollFactor(snapshot.parallaxFactor);
                parallax->setRepeat(snapshot.parallaxRepeat);
                parallax->setHidden(snapshot.parallaxHidden);
            }
        }
    }

    for (int i = restoreCount; i < static_cast<int>(actors.size()); ++i)
    {
        if (auto* actor = actors[static_cast<size_t>(i)].get())
            actor->setNeedRemove(true);
    }

    if (chunk_manager)
    {
        for (const auto& tile : m_playTileSnapshots)
            chunk_manager->setTileSilent(tile.x, tile.y, tile.tile);
        chunk_manager->rebuildDirtyChunks();
    }

    auto actorByIndex = [&](int index) -> engine::object::GameObject* {
        if (index < 0 || index >= static_cast<int>(actors.size()))
            return nullptr;
        return actors[static_cast<size_t>(index)].get();
    };

    m_player = actorByIndex(m_snapshotPlayerIndex);
    m_mech = actorByIndex(m_snapshotMechIndex);
    m_possessedMonster = actorByIndex(m_snapshotPossessedIndex);
    m_isPlayerInMech = m_snapshotIsPlayerInMech;
    m_currentZone = m_snapshotCurrentZone;

    m_showInventory = m_playUiSnapshot.showInventory;
    m_showSettings = m_playUiSnapshot.showSettings;
    m_showMapEditor = m_playUiSnapshot.showMapEditor;
    m_showCommandInput = m_playUiSnapshot.showCommandInput;
    m_missionUI.showWindow = m_playUiSnapshot.missionWindow;
    m_showSettlement = m_playUiSnapshot.showSettlement;
    m_showHierarchyPanel = m_playUiSnapshot.showHierarchyPanel;
    m_showInspectorPanel = m_playUiSnapshot.showInspectorPanel;
    m_showFpsOverlay = m_playUiSnapshot.showFpsOverlay;
    m_devMode = m_playUiSnapshot.devMode;
    m_showSkillDebugOverlay = m_playUiSnapshot.showSkillDebug;
    m_showActiveChunkHighlights = m_playUiSnapshot.showChunkHighlight;
    m_selectedActorIndex = m_playUiSnapshot.selectedActorIndex;
    m_inventory = m_playUiSnapshot.inventory;
    m_mechInventory = m_playUiSnapshot.mechInventory;
    m_equipmentLoadout = m_playUiSnapshot.equipmentLoadout;
    m_starSockets = m_playUiSnapshot.starSockets;
    m_skillCooldowns = m_playUiSnapshot.skillCooldowns;
    m_weaponBar = m_playUiSnapshot.weaponBar;
    m_weaponBar.setActiveIndex(m_playUiSnapshot.weaponActiveIndex);
    m_timeOfDaySystem.restoreRuntimeState(m_playTimeSnapshot);
    m_weatherSystem.restoreRuntimeState(m_playWeatherSnapshot);
    m_hasPlaySnapshot = false;

    spdlog::info("编辑器回滚完成: actors={}, tiles={}", m_playActorSnapshots.size(), m_playTileSnapshots.size());
    return true;
}

void GameScene::renderEditorToolbar()
{
    if (!m_showEditorToolbar)
        return;

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 8.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.92f);
    if (!ImGui::Begin("编辑器工具条", nullptr,
                      ImGuiWindowFlags_NoCollapse |
                      ImGuiWindowFlags_AlwaysAutoResize |
                      ImGuiWindowFlags_NoMove))
    {
        ImGui::End();
        return;
    }

    bool persistUiState = false;
    const bool running = m_gameplayRunning;

    if (m_toolbarShowPlayControls)
    {
        ImVec4 playCol = running ? ImVec4(0.16f, 0.60f, 0.30f, 1.0f) : ImVec4(0.16f, 0.40f, 0.68f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, playCol);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(playCol.x + 0.08f, playCol.y + 0.08f, playCol.z + 0.08f, 1.0f));
        if (ImGui::Button(running ? "运行中 [F5]" : "启动游戏 [F5]", ImVec2(150.0f, 0.0f)))
            setGameplayRunning(!running);
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        if (ImGui::Button("停止", ImVec2(72.0f, 0.0f)))
            setGameplayRunning(false);

        ImGui::SameLine();
        if (ImGui::Button(m_gameplayPaused ? "继续 [F6]" : "暂停 [F6]", ImVec2(94.0f, 0.0f)) && m_gameplayRunning)
            m_gameplayPaused = !m_gameplayPaused;

        ImGui::SameLine();
        if (ImGui::Button("单帧 [F10]", ImVec2(86.0f, 0.0f)) && m_gameplayRunning && m_gameplayPaused)
            m_stepOneFrame = true;
    }

    if (m_toolbarShowWindowControls)
    {
        if (m_toolbarShowPlayControls)
            ImGui::Separator();
        persistUiState |= ImGui::Checkbox("层级", &m_showHierarchyPanel);
        ImGui::SameLine();
        persistUiState |= ImGui::Checkbox("检视器", &m_showInspectorPanel);
        ImGui::SameLine();
        persistUiState |= ImGui::Checkbox("地图编辑", &m_showMapEditor);
        ImGui::SameLine();
        persistUiState |= ImGui::Checkbox("设置", &m_showSettings);
        ImGui::SameLine();
        persistUiState |= ImGui::Checkbox("开发覆盖", &m_devMode);
        ImGui::SameLine();
        persistUiState |= ImGui::Checkbox("性能", &m_showFpsOverlay);
    }

    if (m_toolbarShowDebugControls)
    {
        ImGui::Separator();
        ImGui::Text("选中Actor: %d", m_selectedActorIndex);
        ImGui::SameLine();
        ImGui::TextDisabled("总数: %zu", actor_manager ? actor_manager->actorCount() : 0);
    }

    ImGui::Separator();
    persistUiState |= ImGui::Checkbox("模块:播放", &m_toolbarShowPlayControls);
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("模块:窗口", &m_toolbarShowWindowControls);
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("模块:调试", &m_toolbarShowDebugControls);
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("停止回滚", &m_enablePlayRollback);

    ImGui::Separator();
    if (!running)
        ImGui::Text("当前: 编辑器态（等待启动）");
    else if (m_gameplayPaused)
        ImGui::Text("当前: 运行态（已暂停）");
    else
        ImGui::Text("当前: 运行态");

    ImGui::TextDisabled("F5 启动/停止  |  F6 暂停/继续  |  F10 单帧");

    if (persistUiState)
    {
        saveBoolSetting("show_hierarchy_panel", m_showHierarchyPanel);
        saveBoolSetting("show_inspector_panel", m_showInspectorPanel);
        saveBoolSetting("show_editor_toolbar", m_showEditorToolbar);
        saveBoolSetting("toolbar_show_play_controls", m_toolbarShowPlayControls);
        saveBoolSetting("toolbar_show_window_controls", m_toolbarShowWindowControls);
        saveBoolSetting("toolbar_show_debug_controls", m_toolbarShowDebugControls);
        saveBoolSetting("enable_play_rollback", m_enablePlayRollback);
    }

    ImGui::End();
}

void GameScene::renderHierarchyPanel()
{
    if (!m_showHierarchyPanel || !actor_manager)
        return;

    const auto& actors = actor_manager->getActors();
    if (m_selectedActorIndex >= static_cast<int>(actors.size()))
        m_selectedActorIndex = actors.empty() ? -1 : 0;

    ImGui::SetNextWindowPos({16.0f, 36.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({300.0f, 460.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hierarchy", &m_showHierarchyPanel, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Actors: %zu", actors.size());
    ImGui::SetNextItemWidth(-90.0f);
    ImGui::InputTextWithHint("##hier_filter", "搜索名称/标签", m_hierarchyFilterBuffer.data(), m_hierarchyFilterBuffer.size());
    ImGui::SameLine();
    if (ImGui::Checkbox("按标签", &m_hierarchyGroupByTag))
        saveBoolSetting("hierarchy_group_by_tag", m_hierarchyGroupByTag);
    ImGui::SameLine();
    if (ImGui::Checkbox("仅收藏", &m_hierarchyFavoritesOnly))
        saveBoolSetting("hierarchy_favorites_only", m_hierarchyFavoritesOnly);

    const std::string filterText(m_hierarchyFilterBuffer.data());
    auto containsInsensitive = [](const std::string& text, const std::string& key) {
        if (key.empty())
            return true;
        auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
        const auto it = std::search(text.begin(), text.end(), key.begin(), key.end(),
                                    [&](char a, char b) { return lower(static_cast<unsigned char>(a)) == lower(static_cast<unsigned char>(b)); });
        return it != text.end();
    };

    ImGui::Separator();
    auto drawActorItem = [&](int index) {
        const auto* actor = actors[static_cast<size_t>(index)].get();
        if (!actor)
            return;
        if (m_hierarchyFavoritesOnly && !m_hierarchyFavorites.contains(actor))
            return;

        const std::string name = actor->getName().empty() ? "<unnamed>" : actor->getName();
        const std::string tag = actor->getTag();
        if (!containsInsensitive(name, filterText) && !containsInsensitive(tag, filterText))
            return;

        const bool selected = (m_selectedActorIndex == index);
        std::string label = name;
        if (!tag.empty() && tag != "未定义的标签")
            label += " [" + tag + "]";
        if (actor->isNeedRemove())
            label += " [PendingDelete]";

        const std::string itemId = "##actor_" + std::to_string(index);
        if (ImGui::Selectable((label + itemId).c_str(), selected))
            m_selectedActorIndex = index;

        ImGui::SameLine();
        const bool favorite = m_hierarchyFavorites.contains(actor);
        const std::string buttonLabel = std::string(favorite ? "★" : "☆") + itemId;
        if (ImGui::SmallButton(buttonLabel.c_str()))
        {
            if (favorite)
                m_hierarchyFavorites.erase(actor);
            else
                m_hierarchyFavorites.insert(actor);
        }
    };

    if (!m_hierarchyGroupByTag)
    {
        for (int index = 0; index < static_cast<int>(actors.size()); ++index)
            drawActorItem(index);
    }
    else
    {
        std::vector<std::string> tagOrder;
        tagOrder.reserve(actors.size());
        for (const auto& holder : actors)
        {
            if (!holder)
                continue;
            std::string tag = holder->getTag();
            if (tag.empty() || tag == "未定义的标签")
                tag = "(untagged)";
            if (std::find(tagOrder.begin(), tagOrder.end(), tag) == tagOrder.end())
                tagOrder.push_back(tag);
        }

        for (const auto& tag : tagOrder)
        {
            if (!ImGui::CollapsingHeader(tag.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                continue;
            for (int index = 0; index < static_cast<int>(actors.size()); ++index)
            {
                const auto* actor = actors[static_cast<size_t>(index)].get();
                if (!actor)
                    continue;
                std::string actorTag = actor->getTag();
                if (actorTag.empty() || actorTag == "未定义的标签")
                    actorTag = "(untagged)";
                if (actorTag != tag)
                    continue;
                drawActorItem(index);
            }
        }
    }

    ImGui::End();
}

void GameScene::renderInspectorPanel()
{
    if (!m_showInspectorPanel || !actor_manager)
        return;

    const auto& actors = actor_manager->getActors();
    if (m_selectedActorIndex < 0 || m_selectedActorIndex >= static_cast<int>(actors.size()))
        return;

    auto* actor = actors[static_cast<size_t>(m_selectedActorIndex)].get();
    if (!actor)
        return;

    ImGui::SetNextWindowPos({288.0f, 36.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({320.0f, 420.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inspector", &m_showInspectorPanel, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    std::string actorName = actor->getName();
    if (actorName.empty())
        actorName = "<unnamed>";
    if (m_inspectorRenameBufferActorIndex != m_selectedActorIndex)
    {
        snprintf(m_inspectorRenameBuffer.data(), m_inspectorRenameBuffer.size(), "%s", actorName.c_str());
        snprintf(m_inspectorTagBuffer.data(), m_inspectorTagBuffer.size(), "%s", actor->getTag().c_str());
        m_inspectorRenameBufferActorIndex = m_selectedActorIndex;
    }

    ImGui::InputText("Name", m_inspectorRenameBuffer.data(), m_inspectorRenameBuffer.size());
    ImGui::InputText("Tag", m_inspectorTagBuffer.data(), m_inspectorTagBuffer.size());
    if (ImGui::Button("应用命名"))
    {
        actor->setName(std::string(m_inspectorRenameBuffer.data()));
        actor->setTag(std::string(m_inspectorTagBuffer.data()));
    }
    ImGui::SameLine();
    if (ImGui::Button("标记删除"))
        actor->setNeedRemove(true);

    ImGui::TextDisabled("Index: %d", m_selectedActorIndex);
    ImGui::Separator();

    ImGui::TextUnformatted("Component Controls");
    const bool isCoreControlled = (actor == m_player || actor == m_mech || actor == m_possessedMonster);
    if (!actor->hasComponent<engine::component::TransformComponent>())
    {
        if (ImGui::Button("+ Transform"))
            actor->addComponent<engine::component::TransformComponent>(glm::vec2{0.0f, 56.0f});
    }
    else
    {
        ImGui::BeginDisabled(isCoreControlled);
        if (ImGui::Button("- Transform"))
            actor->removeComponent<engine::component::TransformComponent>();
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!actor->hasComponent<engine::component::ControllerComponent>())
    {
        if (ImGui::Button("+ Controller"))
            actor->addComponent<engine::component::ControllerComponent>(15.0f, 20.0f);
    }
    else
    {
        ImGui::BeginDisabled(isCoreControlled);
        if (ImGui::Button("- Controller"))
            actor->removeComponent<engine::component::ControllerComponent>();
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!actor->hasComponent<engine::component::SpriteComponent>())
    {
        if (ImGui::Button("+ Sprite"))
            actor->addComponent<engine::component::SpriteComponent>("assets/textures/Props/bubble1.svg", engine::utils::Alignment::CENTER);
    }
    else
    {
        ImGui::BeginDisabled(isCoreControlled);
        if (ImGui::Button("- Sprite"))
            actor->removeComponent<engine::component::SpriteComponent>();
        ImGui::EndDisabled();
    }

    if (!actor->hasComponent<engine::component::AnimationComponent>())
    {
        if (ImGui::Button("+ Animation"))
            actor->addComponent<engine::component::AnimationComponent>(32.0f, 32.0f);
    }
    else
    {
        ImGui::BeginDisabled(isCoreControlled);
        if (ImGui::Button("- Animation"))
            actor->removeComponent<engine::component::AnimationComponent>();
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!actor->hasComponent<engine::component::ParallaxComponent>())
    {
        if (ImGui::Button("+ Parallax"))
            actor->addComponent<engine::component::ParallaxComponent>("assets/textures/Layers/back.png", glm::vec2{0.35f, 0.25f});
    }
    else
    {
        ImGui::BeginDisabled(isCoreControlled);
        if (ImGui::Button("- Parallax"))
            actor->removeComponent<engine::component::ParallaxComponent>();
        ImGui::EndDisabled();
    }
    ImGui::TextDisabled(isCoreControlled ? "核心角色已保护关键组件" : "可增删基础组件");
    ImGui::Separator();

    if (auto* transform = actor->getComponent<engine::component::TransformComponent>())
    {
        ImGui::TextUnformatted("Transform");
        glm::vec2 position = transform->getPosition();
        glm::vec2 scale = transform->getScale();
        float rotation = transform->getRotation();
        float positionArray[2] = {position.x, position.y};
        float scaleArray[2] = {scale.x, scale.y};
        if (ImGui::DragFloat2("Position", positionArray, 0.5f))
            transform->setPosition({positionArray[0], positionArray[1]});
        if (ImGui::DragFloat2("Scale", scaleArray, 0.01f, 0.01f, 20.0f))
            transform->setScale({scaleArray[0], scaleArray[1]});
        if (ImGui::DragFloat("Rotation", &rotation, 0.5f, -360.0f, 360.0f))
            transform->setRotation(rotation);
        ImGui::Separator();
    }

    if (auto* controller = actor->getComponent<engine::component::ControllerComponent>())
    {
        ImGui::TextUnformatted("Controller");
        float speed = controller->getSpeed();
        if (ImGui::SliderFloat("MoveSpeed", &speed, 1.0f, 80.0f))
            controller->setSpeed(speed);
        bool enabled = controller->isEnabled();
        if (ImGui::Checkbox("Enabled", &enabled))
            controller->setEnabled(enabled);
        ImGui::TextDisabled("State: %s", controller->getMovementStateName());
        ImGui::TextDisabled("FlyMode: %s", controller->isFlyModeActive() ? "true" : "false");
        ImGui::Separator();
    }

    if (auto* physics = actor->getComponent<engine::component::PhysicsComponent>())
    {
        ImGui::TextUnformatted("Physics");
        const glm::vec2 velocity = physics->getVelocity();
        ImGui::TextDisabled("Velocity: (%.2f, %.2f)", velocity.x, velocity.y);
        if (ImGui::Button("Velocity=0"))
            physics->setVelocity({0.0f, 0.0f});
        ImGui::Separator();
    }

    if (auto* sprite = actor->getComponent<engine::component::SpriteComponent>())
    {
        ImGui::TextUnformatted("Sprite");
        bool hidden = sprite->isHidden();
        bool flipped = sprite->isFlipped();
        if (ImGui::Checkbox("Hidden", &hidden))
            sprite->setHidden(hidden);
        ImGui::SameLine();
        if (ImGui::Checkbox("Flipped", &flipped))
            sprite->setFlipped(flipped);
        ImGui::TextDisabled("Texture: %s", sprite->getTextureId().c_str());
        ImGui::Separator();
    }

    if (auto* animation = actor->getComponent<engine::component::AnimationComponent>())
    {
        ImGui::TextUnformatted("Animation");
        ImGui::TextDisabled("Clip: %s", animation->currentClip().empty() ? "<none>" : animation->currentClip().c_str());
        ImGui::TextDisabled("Frame: %d  Timer: %.3f", animation->currentFrame(), animation->currentTimer());
        ImGui::Separator();
    }

    if (auto* parallax = actor->getComponent<engine::component::ParallaxComponent>())
    {
        ImGui::TextUnformatted("Parallax");
        glm::vec2 factor = parallax->getScrollFactor();
        float factorArray[2] = {factor.x, factor.y};
        if (ImGui::DragFloat2("Factor", factorArray, 0.01f, -4.0f, 4.0f))
            parallax->setScrollFactor({factorArray[0], factorArray[1]});

        glm::bvec2 repeat = parallax->getRepeat();
        bool repeatX = repeat.x;
        bool repeatY = repeat.y;
        const bool changedX = ImGui::Checkbox("Repeat X", &repeatX);
        const bool changedY = ImGui::Checkbox("Repeat Y", &repeatY);
        if (changedX || changedY)
            parallax->setRepeat({repeatX, repeatY});

        bool hidden = parallax->isHidden();
        if (ImGui::Checkbox("Parallax Hidden", &hidden))
            parallax->setHidden(hidden);
        ImGui::Separator();
    }

    if (actor == m_player)
        ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.55f, 1.0f), "[Player]");
    else if (actor == m_mech)
        ImGui::TextColored(ImVec4(0.50f, 0.80f, 1.0f, 1.0f), "[Mech]");

    ImGui::End();
}

void GameScene::renderMapEditor()
{
    if (!m_showMapEditor || !chunk_manager)
        return;

    const char* tileNames[] = {
        "Air", "Stone", "Dirt", "Grass", "Wood",
        "Leaves", "Ore", "Gravel", "GroundDecor", "WallDecor"
    };

    int tileIndex = static_cast<int>(m_mapEditorPaintTile);

    ImGui::SetNextWindowPos({16.0f, 140.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({280.0f, 260.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("地图编辑器 [F8]", &m_showMapEditor,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("左键绘制  右键擦除");
    ImGui::TextUnformatted("半径内为圆形笔刷");
    ImGui::Separator();

    if (ImGui::Combo("瓦片类型", &tileIndex, tileNames, IM_ARRAYSIZE(tileNames)))
        m_mapEditorPaintTile = static_cast<engine::world::TileType>(tileIndex);

    ImGui::SliderInt("笔刷半径", &m_mapEditorBrushRadius, 0, 4, "%d");

    if (m_hasHoveredTile)
    {
        const auto currentType = chunk_manager->tileAt(m_hoveredTile.x, m_hoveredTile.y).type;
        ImGui::Separator();
        ImGui::Text("悬停格: (%d, %d)", m_hoveredTile.x, m_hoveredTile.y);
        ImGui::Text("当前瓦片: %s", tileNames[static_cast<int>(currentType)]);
    }

    if (ImGui::Button("重建全部脏区块"))
        chunk_manager->rebuildDirtyChunks();

    ImGui::End();
}
} // namespace game::scene
