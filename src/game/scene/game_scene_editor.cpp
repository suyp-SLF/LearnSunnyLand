#include "game_scene.h"

#include "../../engine/component/animation_component.h"
#include "../../engine/component/controller_component.h"
#include "../../engine/component/parallax_component.h"
#include "../../engine/component/physics_component.h"
#include "../../engine/component/sprite_component.h"
#include "../../engine/component/transform_component.h"
#include "../../engine/core/context.h"
#include "../../engine/object/game_object.h"
#include "../../engine/render/camera.h"
#include "../../engine/render/renderer.h"
#include "../../engine/resource/resource_manager.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace game::scene
{
namespace
{
    constexpr int kDevThemeVarCount = 6;
    constexpr int kDevThemeColorCount = 10;
    constexpr const char* kLayoutPresetDefault = "default";
    constexpr const char* kLayoutPresetAnimation = "animation";
    constexpr const char* kLayoutPresetDebug = "debug";
    constexpr const char* kResourceViewTree = "tree";
    constexpr const char* kResourceViewThumbnails = "thumbnails";

    const char* defaultTileTypeName(engine::world::TileType type)
    {
        using engine::world::TileType;
        switch (type)
        {
        case TileType::Air: return "Air";
        case TileType::Stone: return "Stone";
        case TileType::Dirt: return "Dirt";
        case TileType::Grass: return "Grass";
        case TileType::Wood: return "Wood";
        case TileType::Leaves: return "Leaves";
        case TileType::Ore: return "Ore";
        case TileType::Gravel: return "Gravel";
        case TileType::GroundDecor: return "GroundDecor";
        case TileType::WallDecor: return "WallDecor";
        }
        return "Unknown";
    }

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

    void saveStringSetting(const char* key, const std::string& value)
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

        json[key] = value;

        std::ofstream output("assets/settings.json");
        if (!output.is_open())
            return;
        output << json.dump(4);
    }

    std::string loadEditorLayoutIniFromConfig()
    {
        std::ifstream file("assets/config.json");
        if (!file.is_open())
            return {};

        try
        {
            nlohmann::json j;
            file >> j;
            if (!j.contains("editor") || !j["editor"].is_object())
                return {};
            const auto& editor = j["editor"];
            if (!editor.contains("imgui_ini") || !editor["imgui_ini"].is_string())
                return {};
            return editor["imgui_ini"].get<std::string>();
        }
        catch (const std::exception&)
        {
            return {};
        }
    }

    void saveEditorLayoutIniToConfig(const std::string& iniText)
    {
        nlohmann::json j = nlohmann::json::object();
        {
            std::ifstream input("assets/config.json");
            if (input.is_open())
            {
                try
                {
                    input >> j;
                }
                catch (const std::exception&)
                {
                    j = nlohmann::json::object();
                }
            }
        }

        if (!j.contains("editor") || !j["editor"].is_object())
            j["editor"] = nlohmann::json::object();
        j["editor"]["imgui_ini"] = iniText;

        std::ofstream output("assets/config.json");
        if (!output.is_open())
            return;
        output << j.dump(4);
    }

    std::string toLowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    std::string normalizeLayoutPresetKey(const std::string& value)
    {
        const std::string lowered = toLowerAscii(value);
        if (lowered == kLayoutPresetAnimation)
            return kLayoutPresetAnimation;
        if (lowered == kLayoutPresetDebug)
            return kLayoutPresetDebug;
        return kLayoutPresetDefault;
    }

    const char* layoutPresetLabel(const std::string& presetKey)
    {
        if (presetKey == kLayoutPresetAnimation)
            return "动画布局";
        if (presetKey == kLayoutPresetDebug)
            return "调试布局";
        return "默认布局";
    }

    std::string normalizeResourceViewMode(const std::string& value)
    {
        return toLowerAscii(value) == kResourceViewThumbnails ? kResourceViewThumbnails : kResourceViewTree;
    }

    bool containsInsensitive(const std::string& text, const std::string& filter)
    {
        if (filter.empty())
            return true;
        return toLowerAscii(text).find(filter) != std::string::npos;
    }

    bool isImageResource(const std::filesystem::path& path)
    {
        const std::string ext = toLowerAscii(path.extension().string());
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".webp";
    }

    std::string formatConsoleTimestamp(double seconds)
    {
        const int clamped = std::max(0, static_cast<int>(seconds));
        const int hour = (clamped / 3600) % 24;
        const int minute = (clamped / 60) % 60;
        const int sec = clamped % 60;
        char buffer[24];
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hour, minute, sec);
        return buffer;
    }

    const char* resourceTypeLabel(const std::filesystem::path& path)
    {
        if (std::filesystem::is_directory(path))
            return "Folder";
        const std::string ext = toLowerAscii(path.extension().string());
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".webp")
            return "Texture";
        if (ext == ".json" || ext == ".tmj" || ext == ".tsj")
            return "Data";
        if (ext == ".glb" || ext == ".gltf" || ext == ".fbx" || ext == ".obj")
            return "Model";
        if (ext == ".ogg" || ext == ".wav" || ext == ".mp3")
            return "Audio";
        if (ext == ".spv" || ext == ".msl" || ext == ".vert" || ext == ".frag")
            return "Shader";
        if (ext == ".ttf" || ext == ".otf")
            return "Font";
        return "Asset";
    }

    ImU32 resourceAccentColor(const std::filesystem::path& path)
    {
        if (std::filesystem::is_directory(path))
            return IM_COL32(111, 182, 255, 255);
        const std::string ext = toLowerAscii(path.extension().string());
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".webp")
            return IM_COL32(84, 198, 132, 255);
        if (ext == ".json" || ext == ".tmj" || ext == ".tsj")
            return IM_COL32(255, 189, 89, 255);
        if (ext == ".glb" || ext == ".gltf" || ext == ".fbx" || ext == ".obj")
            return IM_COL32(255, 128, 102, 255);
        if (ext == ".ogg" || ext == ".wav" || ext == ".mp3")
            return IM_COL32(202, 132, 255, 255);
        if (ext == ".spv" || ext == ".msl" || ext == ".vert" || ext == ".frag")
            return IM_COL32(255, 102, 102, 255);
        return IM_COL32(138, 154, 173, 255);
    }

    bool directoryHasFilterMatch(const std::filesystem::path& directory, const std::string& filter)
    {
        if (filter.empty())
            return true;
        if (containsInsensitive(directory.filename().string(), filter))
            return true;

        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec))
        {
            if (ec)
                break;
            const auto relative = it->path().generic_string();
            if (containsInsensitive(relative, filter) || containsInsensitive(it->path().filename().string(), filter))
                return true;
        }
        return false;
    }

    ImVec2 logicalToImGuiScreen(const engine::core::Context& context, const glm::vec2& logicalPos)
    {
        const glm::vec2 logicalSize = context.getRenderer().getLogicalSize();
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

        if (logicalSize.x <= 0.0f || logicalSize.y <= 0.0f)
            return {logicalPos.x, logicalPos.y};

        return {
            logicalPos.x * (displaySize.x / logicalSize.x),
            logicalPos.y * (displaySize.y / logicalSize.y)
        };
    }

    void pushDevEditorTheme()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.0f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.09f, 0.12f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.12f, 0.16f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.12f, 0.16f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.16f, 0.24f, 0.36f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.13f, 0.16f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.22f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.30f, 0.44f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.40f, 0.58f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.27f, 0.38f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.37f, 0.52f, 1.0f));
    }

    void popDevEditorTheme()
    {
        ImGui::PopStyleColor(kDevThemeColorCount);
        ImGui::PopStyleVar(kDevThemeVarCount);
    }

    void drawEditorSectionTitle(const char* title)
    {
        ImGui::SeparatorText(title);
    }

    void drawEditorKeyValue(const char* label, const char* value)
    {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine();
        ImGui::TextUnformatted(value);
    }

    void drawEditorStatusChip(const char* text, const ImVec4& color)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
        ImGui::Button(text, ImVec2(88.0f, 0.0f));
        ImGui::PopStyleColor(3);
    }
}

std::string GameScene::editorLayoutIniPath(const std::string& presetKey) const
{
    const std::string normalized = normalizeLayoutPresetKey(presetKey);
    return "imgui_layout_" + normalized + ".ini";
}

void GameScene::persistEditorUiSettings() const
{
    saveStringSetting("editor_layout_preset", normalizeLayoutPresetKey(m_editorLayoutPreset));
    saveBoolSetting("show_editor_toolbar", m_showEditorToolbar);
    saveBoolSetting("show_main_toolbar", m_showMainToolbar);
    saveBoolSetting("show_resource_explorer_panel", m_showResourceExplorerPanel);
    saveBoolSetting("show_scene_viewport_panel", m_showSceneViewportPanel);
    saveBoolSetting("show_console_panel", m_showConsolePanel);
    saveBoolSetting("show_animation_editor_panel", m_showAnimationEditorPanel);
    saveBoolSetting("show_shader_editor_panel", m_showShaderEditorPanel);
    saveBoolSetting("show_profiler_panel", m_showProfilerPanel);
    saveBoolSetting("show_hierarchy_panel", m_showHierarchyPanel);
    saveBoolSetting("show_inspector_panel", m_showInspectorPanel);
    saveBoolSetting("toolbar_show_play_controls", m_toolbarShowPlayControls);
    saveBoolSetting("toolbar_show_window_controls", m_toolbarShowWindowControls);
    saveBoolSetting("toolbar_show_debug_controls", m_toolbarShowDebugControls);
    saveBoolSetting("dev_overlay_show_editor_tools", m_devOverlayShowEditorTools);
    saveBoolSetting("show_editor_collider_boxes", m_showEditorColliderBoxes);
    saveBoolSetting("show_foot_collision_debug", m_showFootCollisionDebug);
    saveBoolSetting("hierarchy_group_by_tag", m_hierarchyGroupByTag);
    saveBoolSetting("hierarchy_favorites_only", m_hierarchyFavoritesOnly);
    saveBoolSetting("enable_play_rollback", m_enablePlayRollback);
    saveBoolSetting("scene_viewport_show_grid", m_sceneViewportShowGrid);
    saveBoolSetting("scene_viewport_show_axes", m_sceneViewportShowAxes);
    saveBoolSetting("scene_viewport_show_camera_info", m_sceneViewportShowCameraInfo);
    saveBoolSetting("scene_viewport_show_lighting", m_sceneViewportShowLighting);
    saveBoolSetting("scene_viewport_show_gizmo", m_sceneViewportShowGizmo);
    saveStringSetting("resource_explorer_view", normalizeResourceViewMode(m_resourceExplorerViewMode));
}

void GameScene::applyEditorLayoutPreset(const std::string& presetKey, bool loadImGuiLayout)
{
    const std::string nextPreset = normalizeLayoutPresetKey(presetKey);
    const std::string currentPreset = normalizeLayoutPresetKey(m_editorLayoutPreset);

    if (loadImGuiLayout && m_glContext && ImGui::GetCurrentContext() && !currentPreset.empty())
    {
        const std::string currentLayoutPath = editorLayoutIniPath(currentPreset);
        ImGui::SaveIniSettingsToDisk(currentLayoutPath.c_str());
    }

    m_editorLayoutPreset = nextPreset;
    m_showMainToolbar = true;
    m_showSceneViewportPanel = true;
    m_showHierarchyPanel = true;
    m_showInspectorPanel = true;
    m_showResourceExplorerPanel = true;

    if (m_editorLayoutPreset == kLayoutPresetAnimation)
    {
        m_showAnimationEditorPanel = true;
        m_showConsolePanel = false;
        m_showShaderEditorPanel = false;
        m_showProfilerPanel = false;
    }
    else if (m_editorLayoutPreset == kLayoutPresetDebug)
    {
        m_showAnimationEditorPanel = false;
        m_showConsolePanel = true;
        m_showShaderEditorPanel = false;
        m_showProfilerPanel = true;
        m_showResourceExplorerPanel = false;
    }
    else
    {
        m_showAnimationEditorPanel = false;
        m_showConsolePanel = true;
        m_showShaderEditorPanel = false;
        m_showProfilerPanel = false;
    }

    if (loadImGuiLayout && m_glContext && ImGui::GetCurrentContext())
    {
        const std::string layoutPath = editorLayoutIniPath(m_editorLayoutPreset);
        if (std::filesystem::exists(layoutPath))
            ImGui::LoadIniSettingsFromDisk(layoutPath.c_str());
        else if (std::filesystem::exists("imgui.ini"))
            ImGui::LoadIniSettingsFromDisk("imgui.ini");
    }

    appendEditorConsole(EditorConsoleLevel::Log, "Layout", std::string("切换布局预设: ") + layoutPresetLabel(m_editorLayoutPreset));
    persistEditorUiSettings();
}

void GameScene::appendEditorConsole(EditorConsoleLevel level, const std::string& source, const std::string& message)
{
    constexpr size_t kMaxEntries = 2000;
    EditorConsoleEntry entry;
    entry.level = level;
    entry.source = source;
    entry.message = message;
    entry.timeSeconds = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;

    m_consoleEntries.push_back(std::move(entry));
    if (m_consoleEntries.size() > kMaxEntries)
        m_consoleEntries.erase(m_consoleEntries.begin(), m_consoleEntries.begin() + static_cast<long>(m_consoleEntries.size() - kMaxEntries));
    m_consoleScrollToBottom = true;
}

void GameScene::ensureEditorConsoleSeeded()
{
    if (!m_consoleEntries.empty())
        return;
    appendEditorConsole(EditorConsoleLevel::Log, "Editor", "控制台已就绪");
    appendEditorConsole(EditorConsoleLevel::Log, "Editor", "提示: 双击资源可在面板中选中并用于拖拽引用");
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
    appendEditorConsole(EditorConsoleLevel::Log,
                        "Runtime",
                        std::string("播放状态切换: ") + (m_gameplayRunning ? "运行" : "编辑"));
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

    pushDevEditorTheme();

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 8.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.92f);
    if (!ImGui::Begin("编辑器工具条", nullptr,
                      ImGuiWindowFlags_NoCollapse |
                      ImGuiWindowFlags_AlwaysAutoResize |
                      ImGuiWindowFlags_NoMove))
    {
        ImGui::End();
        popDevEditorTheme();
        return;
    }

    bool persistUiState = false;
    const bool running = m_gameplayRunning;
    const bool paused = running && m_gameplayPaused;

    if (!running)
        drawEditorStatusChip("编辑模式", ImVec4(0.26f, 0.36f, 0.54f, 1.0f));
    else if (paused)
        drawEditorStatusChip("已暂停", ImVec4(0.64f, 0.48f, 0.16f, 1.0f));
    else
        drawEditorStatusChip("运行中", ImVec4(0.20f, 0.56f, 0.30f, 1.0f));

    ImGui::SameLine();
    ImGui::TextDisabled("F5 启动/停止  F6 暂停/继续  F10 单帧");

    if (m_toolbarShowPlayControls)
    {
        drawEditorSectionTitle("运行控制");
        ImVec4 playCol = running ? ImVec4(0.16f, 0.60f, 0.30f, 1.0f) : ImVec4(0.16f, 0.40f, 0.68f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, playCol);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(playCol.x + 0.08f, playCol.y + 0.08f, playCol.z + 0.08f, 1.0f));
        if (ImGui::Button(running ? "停止运行 [F5]" : "启动游戏 [F5]", ImVec2(132.0f, 0.0f)))
            setGameplayRunning(!running);
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::BeginDisabled(!running);
        if (ImGui::Button(m_gameplayPaused ? "继续 [F6]" : "暂停 [F6]", ImVec2(96.0f, 0.0f)))
            m_gameplayPaused = !m_gameplayPaused;
        ImGui::SameLine();
        if (ImGui::Button("单步 [F10]", ImVec2(88.0f, 0.0f)) && m_gameplayPaused)
            m_stepOneFrame = true;
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("强制停止", ImVec2(88.0f, 0.0f)))
            setGameplayRunning(false);
    }

    if (m_toolbarShowWindowControls)
    {
        drawEditorSectionTitle("窗口开关");
        persistUiState |= ImGui::Checkbox("层级面板", &m_showHierarchyPanel);
        ImGui::SameLine();
        persistUiState |= ImGui::Checkbox("检视器", &m_showInspectorPanel);
        ImGui::SameLine();
        {
            bool _ueOpen = m_universeEditor.isOpen();
            if (ImGui::Checkbox("宇宙编辑器", &_ueOpen)) m_universeEditor.setOpen(_ueOpen);
        }
        ImGui::SameLine();
        persistUiState |= ImGui::Checkbox("设置", &m_showSettings);
        persistUiState |= ImGui::Checkbox("开发覆盖", &m_devMode);
        ImGui::SameLine();
        persistUiState |= ImGui::Checkbox("性能浮层", &m_showFpsOverlay);
    }

    if (m_toolbarShowDebugControls)
    {
        drawEditorSectionTitle("调试摘要");
        persistUiState |= ImGui::Checkbox("显示编辑器碰撞箱", &m_showEditorColliderBoxes);
        ImGui::SameLine();
        persistUiState |= ImGui::Checkbox("显示脚底碰撞框", &m_showFootCollisionDebug);
        if (ImGui::BeginTable("##editor_toolbar_debug", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ImGui::Text("选中对象");
            ImGui::TextDisabled("%d", m_selectedActorIndex);
            ImGui::TableNextColumn();
            ImGui::Text("对象总数");
            ImGui::TextDisabled("%zu", actor_manager ? actor_manager->actorCount() : 0);
            ImGui::TableNextColumn();
            ImGui::Text("回滚模式");
            ImGui::TextDisabled("%s", m_enablePlayRollback ? "退出运行时恢复" : "关闭");
            ImGui::EndTable();
        }
    }

    drawEditorSectionTitle("工具条模块");
    persistUiState |= ImGui::Checkbox("显示运行控制", &m_toolbarShowPlayControls);
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("显示窗口开关", &m_toolbarShowWindowControls);
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("显示调试摘要", &m_toolbarShowDebugControls);
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("退出运行时回滚", &m_enablePlayRollback);

    if (persistUiState)
    {
        saveBoolSetting("show_hierarchy_panel", m_showHierarchyPanel);
        saveBoolSetting("show_inspector_panel", m_showInspectorPanel);
        saveBoolSetting("show_editor_toolbar", m_showEditorToolbar);
        saveBoolSetting("toolbar_show_play_controls", m_toolbarShowPlayControls);
        saveBoolSetting("toolbar_show_window_controls", m_toolbarShowWindowControls);
        saveBoolSetting("toolbar_show_debug_controls", m_toolbarShowDebugControls);
        saveBoolSetting("show_editor_collider_boxes", m_showEditorColliderBoxes);
        saveBoolSetting("show_foot_collision_debug", m_showFootCollisionDebug);
        saveBoolSetting("enable_play_rollback", m_enablePlayRollback);
    }

    ImGui::End();
    popDevEditorTheme();
}

void GameScene::renderEditorWorkbenchShell()
{
    if (!m_editorLayoutLoadedFromConfig)
    {
        if (ImGui::GetCurrentContext())
        {
            const std::string iniText = loadEditorLayoutIniFromConfig();
            if (!iniText.empty())
                ImGui::LoadIniSettingsFromMemory(iniText.c_str(), iniText.size());
            m_editorLayoutLoadedFromConfig = true;
        }
    }

    if (ImGui::GetCurrentContext())
    {
        m_editorLayoutSaveAccumulator += ImGui::GetIO().DeltaTime;
        if (m_editorLayoutSaveAccumulator >= 1.2f)
        {
            size_t iniSize = 0;
            const char* iniData = ImGui::SaveIniSettingsToMemory(&iniSize);
            if (iniData && iniSize > 0)
                saveEditorLayoutIniToConfig(std::string(iniData, iniSize));
            m_editorLayoutSaveAccumulator = 0.0f;
        }
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                                 ImGuiWindowFlags_NoNavFocus |
                                 ImGuiWindowFlags_NoBackground |
                                 ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("##editor_workbench_host", nullptr, hostFlags))
    {
        renderEditorMainMenuBar();
        renderEditorMainToolbar();

        ImGuiID dockspaceId = ImGui::GetID("EditorWorkbenchDockspace");
        m_editorDockspaceId = dockspaceId;
        ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, -24.0f), dockFlags);

        renderEditorStatusBar();
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void GameScene::renderEditorMainMenuBar()
{
    if (!ImGui::BeginMenuBar())
        return;

    bool persistUiState = false;

    if (ImGui::BeginMenu("文件"))
    {
        ImGui::MenuItem("新建场景", "Ctrl+N", false, false);
        if (ImGui::MenuItem("保存全部", "Ctrl+S"))
        {
            saveGroundActorsToConfig();
            appendEditorConsole(EditorConsoleLevel::Log, "Scene", "执行保存全部");
        }
        ImGui::MenuItem("版本控制提交", "Ctrl+Shift+K", false, false);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("编辑"))
    {
        ImGui::MenuItem("撤销", "Ctrl+Z", false, false);
        ImGui::MenuItem("重做", "Ctrl+Y", false, false);
        ImGui::MenuItem("复制", "Ctrl+C", false, false);
        ImGui::MenuItem("粘贴", "Ctrl+V", false, false);
        ImGui::MenuItem("删除", "Delete", false, false);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("视图"))
    {
        persistUiState |= ImGui::MenuItem("场景视图", nullptr, &m_showSceneViewportPanel);
        persistUiState |= ImGui::MenuItem("层级面板", nullptr, &m_showHierarchyPanel);
        persistUiState |= ImGui::MenuItem("属性面板", nullptr, &m_showInspectorPanel);
        persistUiState |= ImGui::MenuItem("控制台", nullptr, &m_showConsolePanel);
        if (ImGui::MenuItem("宇宙编辑器", nullptr, m_universeEditor.isOpen()))
            m_universeEditor.setOpen(!m_universeEditor.isOpen());
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("工具"))
    {
        persistUiState |= ImGui::MenuItem("动画编辑器", nullptr, &m_showAnimationEditorPanel);
        persistUiState |= ImGui::MenuItem("着色器编辑器", nullptr, &m_showShaderEditorPanel);
        persistUiState |= ImGui::MenuItem("性能分析器", nullptr, &m_showProfilerPanel);
        ImGui::MenuItem("帧编辑器", nullptr, nullptr, false);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("窗口"))
    {
        persistUiState |= ImGui::MenuItem("设置", nullptr, &m_showSettings);
        persistUiState |= ImGui::MenuItem("主工具栏", nullptr, &m_showMainToolbar);
        persistUiState |= ImGui::MenuItem("性能浮层", nullptr, &m_showFpsOverlay);
        if (ImGui::BeginMenu("布局预设"))
        {
            const std::string currentPreset = normalizeLayoutPresetKey(m_editorLayoutPreset);
            if (ImGui::MenuItem("默认布局", nullptr, currentPreset == kLayoutPresetDefault))
                applyEditorLayoutPreset(kLayoutPresetDefault, true);
            if (ImGui::MenuItem("动画布局", nullptr, currentPreset == kLayoutPresetAnimation))
                applyEditorLayoutPreset(kLayoutPresetAnimation, true);
            if (ImGui::MenuItem("调试布局", nullptr, currentPreset == kLayoutPresetDebug))
                applyEditorLayoutPreset(kLayoutPresetDebug, true);
            ImGui::Separator();
            if (ImGui::MenuItem("保存当前布局到预设"))
            {
                const std::string layoutPath = editorLayoutIniPath(currentPreset);
                ImGui::SaveIniSettingsToDisk(layoutPath.c_str());
                ImGui::SaveIniSettingsToDisk("imgui.ini");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("帮助"))
    {
        ImGui::MenuItem("快捷键", nullptr, false, false);
        ImGui::MenuItem("文档", nullptr, false, false);
        ImGui::EndMenu();
    }

    const float fps = ImGui::GetIO().Framerate;
    const ImVec4 fpsColor = fps >= 55.0f ? ImVec4(0.35f, 0.95f, 0.40f, 1.0f)
                          : (fps >= 35.0f ? ImVec4(0.95f, 0.85f, 0.25f, 1.0f)
                                          : ImVec4(0.95f, 0.35f, 0.30f, 1.0f));
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 260.0f);
    ImGui::TextDisabled("LearnSunnyLand");
    ImGui::SameLine();
    ImGui::TextColored(fpsColor, "FPS %.1f", fps);

    if (persistUiState)
        persistEditorUiSettings();

    ImGui::EndMenuBar();
}

void GameScene::renderEditorMainToolbar()
{
    if (!m_showMainToolbar)
        return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + 24.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 36.0f), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.13f, 0.16f, 0.95f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##editor_main_toolbar", nullptr, flags))
    {
        bool persistUiState = false;

        if (ImGui::Button("新建场景")) {}
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("新建场景（待接入）\n快捷键: Ctrl+N");
        ImGui::SameLine();
        if (ImGui::Button("保存全部"))
        {
            saveGroundActorsToConfig();
            appendEditorConsole(EditorConsoleLevel::Log, "Scene", "执行保存全部");
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("保存地面配置与编辑器布局\n快捷键: Ctrl+S");
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::Button("版本提交");
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("版本控制提交流程待接入");
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::BeginDisabled();
        if (ImGui::Button("撤销")) {}
        ImGui::SameLine();
        if (ImGui::Button("重做")) {}
        ImGui::SameLine();
        if (ImGui::Button("复制")) {}
        ImGui::SameLine();
        if (ImGui::Button("粘贴")) {}
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        const bool running = m_gameplayRunning;
        if (running)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.74f, 0.22f, 0.22f, 1.0f));
        if (ImGui::Button(running ? "停止" : "播放"))
            setGameplayRunning(!running);
        if (running)
            ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::BeginDisabled(!running);
        if (ImGui::Button(m_gameplayPaused ? "继续" : "暂停"))
            m_gameplayPaused = !m_gameplayPaused;
        ImGui::SameLine();
        if (ImGui::Button("逐帧"))
            m_stepOneFrame = true;
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (m_toolbarShowDebugControls)
        {
            persistUiState |= ImGui::Checkbox("帧调试", &m_showSkillDebugOverlay);
            ImGui::SameLine();
            persistUiState |= ImGui::Checkbox("性能", &m_showProfilerPanel);
            ImGui::SameLine();
            persistUiState |= ImGui::Checkbox("碰撞盒", &m_showEditorColliderBoxes);
            ImGui::SameLine();
        }

        if (m_toolbarShowWindowControls)
        {
            persistUiState |= ImGui::Checkbox("层级", &m_showHierarchyPanel);
            ImGui::SameLine();
            persistUiState |= ImGui::Checkbox("属性", &m_showInspectorPanel);
            ImGui::SameLine();
            persistUiState |= ImGui::Checkbox("控制台", &m_showConsolePanel);
            ImGui::SameLine();
        }

        ImGui::SetNextItemWidth(118.0f);
        const std::string currentPreset = normalizeLayoutPresetKey(m_editorLayoutPreset);
        if (ImGui::BeginCombo("##layout_preset", layoutPresetLabel(currentPreset)))
        {
            if (ImGui::Selectable("默认布局", currentPreset == kLayoutPresetDefault))
                applyEditorLayoutPreset(kLayoutPresetDefault, true);
            if (ImGui::Selectable("动画布局", currentPreset == kLayoutPresetAnimation))
                applyEditorLayoutPreset(kLayoutPresetAnimation, true);
            if (ImGui::Selectable("调试布局", currentPreset == kLayoutPresetDebug))
                applyEditorLayoutPreset(kLayoutPresetDebug, true);
            ImGui::EndCombo();
        }
        if (persistUiState)
            persistEditorUiSettings();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void GameScene::renderEditorStatusBar()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - 24.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 24.0f), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.12f, 0.15f, 0.95f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##editor_status_bar", nullptr, flags))
    {
        ImGui::Text("对象 %zu", actor_manager ? actor_manager->actorCount() : 0);
        ImGui::SameLine();
        ImGui::TextDisabled("| 区块 %zu", chunk_manager ? chunk_manager->loadedChunkCount() : 0);
        ImGui::SameLine();
        ImGui::TextDisabled("| 状态 %s", m_gameplayRunning ? (m_gameplayPaused ? "暂停" : "运行") : "编辑");
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void GameScene::renderResourceExplorerPanel()
{
    if (!m_showResourceExplorerPanel)
        return;

    if (m_editorDockspaceId != 0)
        ImGui::SetNextWindowDockID(static_cast<ImGuiID>(m_editorDockspaceId), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("资源管理器", &m_showResourceExplorerPanel))
    {
        ImGui::End();
        return;
    }

    bool persistUiState = false;
    const std::string filter = toLowerAscii(m_resourceExplorerFilterBuffer.data());

    ImGui::SetNextItemWidth(-232.0f);
    ImGui::InputTextWithHint("##res_filter", "搜索资源、路径、后缀...", m_resourceExplorerFilterBuffer.data(), m_resourceExplorerFilterBuffer.size());
    ImGui::SameLine();
    if (ImGui::Button("清空"))
        m_resourceExplorerFilterBuffer[0] = '\0';
    ImGui::SameLine();
    if (ImGui::Button("树状"))
    {
        m_resourceExplorerViewMode = kResourceViewTree;
        persistUiState = true;
        appendEditorConsole(EditorConsoleLevel::Log, "Resource", "资源视图切换为树状");
    }
    ImGui::SameLine();
    if (ImGui::Button("缩略图"))
    {
        m_resourceExplorerViewMode = kResourceViewThumbnails;
        persistUiState = true;
        appendEditorConsole(EditorConsoleLevel::Log, "Resource", "资源视图切换为缩略图");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("模式: %s", m_resourceExplorerViewMode == kResourceViewThumbnails ? "缩略图" : "树状");
    ImGui::Separator();

    const std::filesystem::path root = "assets";
    if (persistUiState)
        persistEditorUiSettings();

    if (!std::filesystem::exists(root))
    {
        ImGui::TextDisabled("assets 目录不存在。");
        ImGui::End();
        return;
    }

    auto selectResource = [&](const std::filesystem::path& path) {
        m_selectedResourcePath = path.generic_string();
    };

    auto emitDragPayload = [&](const std::filesystem::path& path) {
        const std::string fullPath = path.generic_string();
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("ASSET_PATH", fullPath.c_str(), fullPath.size() + 1);
            ImGui::TextUnformatted(path.filename().string().c_str());
            ImGui::TextDisabled("%s", fullPath.c_str());
            ImGui::EndDragDropSource();
        }
    };

    auto resourceContextMenu = [&](const std::filesystem::path& path) {
        const std::string fullPath = path.generic_string();
        if (ImGui::BeginPopupContextItem(fullPath.c_str()))
        {
            if (ImGui::MenuItem("复制路径"))
                ImGui::SetClipboardText(fullPath.c_str());
            if (ImGui::MenuItem("复制文件名"))
                ImGui::SetClipboardText(path.filename().string().c_str());
            ImGui::Separator();
            ImGui::TextDisabled("%s", resourceTypeLabel(path));
            ImGui::EndPopup();
        }
    };

    const std::string viewMode = normalizeResourceViewMode(m_resourceExplorerViewMode);
    if (viewMode == kResourceViewTree)
    {
        std::function<void(const std::filesystem::path&)> drawTree;
        drawTree = [&](const std::filesystem::path& directory) {
            std::error_code ec;
            for (std::filesystem::directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, ec), end;
                 it != end; it.increment(ec))
            {
                if (ec)
                    break;

                const auto& entry = *it;
                const std::filesystem::path path = entry.path();
                const std::string fullPath = path.generic_string();
                if (entry.is_directory())
                {
                    if (!directoryHasFilterMatch(path, filter))
                        continue;

                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
                    if (m_selectedResourcePath == fullPath)
                        flags |= ImGuiTreeNodeFlags_Selected;
                    const bool open = ImGui::TreeNodeEx(fullPath.c_str(), flags, "%s %s", "[DIR]", path.filename().string().c_str());
                    if (ImGui::IsItemClicked())
                        selectResource(path);
                    emitDragPayload(path);
                    resourceContextMenu(path);
                    if (open)
                    {
                        drawTree(path);
                        ImGui::TreePop();
                    }
                    continue;
                }

                if (!containsInsensitive(fullPath, filter) && !containsInsensitive(path.filename().string(), filter))
                    continue;

                const bool selected = m_selectedResourcePath == fullPath;
                std::string label = std::string(resourceTypeLabel(path)) + "  " + path.filename().string();
                if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
                    selectResource(path);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    selectResource(path);
                emitDragPayload(path);
                resourceContextMenu(path);
            }
        };

        ImGui::BeginChild("##resource_tree", ImVec2(0.0f, -110.0f), false);
        drawTree(root);
        ImGui::EndChild();
    }
    else
    {
        std::vector<std::filesystem::path> assets;
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec))
        {
            if (ec)
                break;
            if (it->is_directory())
                continue;
            const std::string fullPath = it->path().generic_string();
            if (!containsInsensitive(fullPath, filter) && !containsInsensitive(it->path().filename().string(), filter))
                continue;
            assets.push_back(it->path());
        }

        const float cardWidth = 132.0f;
        const float availWidth = std::max(ImGui::GetContentRegionAvail().x, cardWidth);
        const int columns = std::max(1, static_cast<int>(availWidth / cardWidth));
        if (ImGui::BeginTable("##resource_grid", columns, ImGuiTableFlags_SizingStretchSame))
        {
            for (size_t index = 0; index < assets.size(); ++index)
            {
                ImGui::TableNextColumn();
                const auto& path = assets[index];
                const std::string fullPath = path.generic_string();
                const bool selected = m_selectedResourcePath == fullPath;

                ImGui::PushID(fullPath.c_str());
                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.17f, 0.24f, 0.34f, 0.95f));
                ImGui::BeginChild("##asset_card", ImVec2(0.0f, 150.0f), true, ImGuiWindowFlags_NoScrollbar);

                if (isImageResource(path) && m_glContext)
                {
                    const unsigned int textureId = _context.getResourceManager().getGLTexture(fullPath);
                    if (textureId != 0)
                    {
                        ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(textureId)), ImVec2(96.0f, 72.0f));
                    }
                    else
                    {
                        ImGui::Dummy(ImVec2(96.0f, 72.0f));
                    }
                }
                else
                {
                    const ImVec2 pos = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + 96.0f, pos.y + 72.0f), resourceAccentColor(path), 10.0f);
                    ImGui::Dummy(ImVec2(96.0f, 72.0f));
                }

                if (ImGui::Selectable(path.filename().string().c_str(), selected, 0, ImVec2(0.0f, 0.0f)))
                    selectResource(path);
                resourceContextMenu(path);
                ImGui::TextDisabled("%s", resourceTypeLabel(path));
                ImGui::TextWrapped("%s", fullPath.c_str());
                emitDragPayload(path);
                ImGui::EndChild();
                if (selected)
                    ImGui::PopStyleColor();
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    ImGui::Separator();
    if (!m_selectedResourcePath.empty())
    {
        const std::filesystem::path selectedPath = m_selectedResourcePath;
        ImGui::Text("当前资源");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", selectedPath.filename().string().c_str());
        drawEditorKeyValue("路径", m_selectedResourcePath.c_str());
        drawEditorKeyValue("类型", resourceTypeLabel(selectedPath));
        if (std::filesystem::exists(selectedPath) && !std::filesystem::is_directory(selectedPath) && isImageResource(selectedPath) && m_glContext)
        {
            const glm::vec2 textureSize = _context.getResourceManager().getTextureSize(m_selectedResourcePath);
            ImGui::TextDisabled("尺寸 %.0f x %.0f", textureSize.x, textureSize.y);
        }
    }

    ImGui::End();
}

void GameScene::renderSceneViewportPanel()
{
    if (!m_showSceneViewportPanel)
        return;

    if (m_editorDockspaceId != 0)
        ImGui::SetNextWindowDockID(static_cast<ImGuiID>(m_editorDockspaceId), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(780.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("场景视图", &m_showSceneViewportPanel))
    {
        ImGui::End();
        return;
    }

    const auto& cam = _context.getCamera();
    const glm::vec2 c = cam.getPosition();
    bool persistUiState = false;
    if (ImGui::Button(cam.isPseudo3DEnabled() ? "伪3D" : "正交"))
        _context.getCamera().setPseudo3DEnabled(!cam.isPseudo3DEnabled());
    ImGui::SameLine();
    if (ImGui::Button("聚焦选中") && actor_manager && m_selectedActorIndex >= 0 && m_selectedActorIndex < static_cast<int>(actor_manager->getActors().size()))
    {
        if (auto* actor = actor_manager->getActors()[static_cast<size_t>(m_selectedActorIndex)].get())
        {
            if (auto* transform = actor->getComponent<engine::component::TransformComponent>())
                _context.getCamera().setPosition(transform->getPosition() - _context.getCamera().getViewportSize() * 0.5f / std::max(_context.getCamera().getZoom(), 0.01f));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("重置相机"))
    {
        _context.getCamera().setZoom(1.0f);
        _context.getCamera().setPosition({0.0f, 0.0f});
    }
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("网格", &m_sceneViewportShowGrid);
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("Gizmo", &m_sceneViewportShowGizmo);
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("坐标轴", &m_sceneViewportShowAxes);
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("光照", &m_sceneViewportShowLighting);
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("相机信息", &m_sceneViewportShowCameraInfo);
    if (persistUiState)
        persistEditorUiSettings();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasMax = ImVec2(canvasMin.x + avail.x, canvasMin.y + avail.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilledMultiColor(canvasMin, canvasMax,
                                      IM_COL32(20, 27, 35, 235),
                                      IM_COL32(25, 34, 44, 235),
                                      IM_COL32(13, 18, 24, 235),
                                      IM_COL32(18, 23, 30, 235));
    drawList->AddRect(canvasMin, canvasMax, IM_COL32(92, 122, 153, 180), 6.0f, 0, 1.0f);

    ImGui::InvisibleButton("##scene_viewport_canvas", avail, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        _context.getCamera().move(glm::vec2(-delta.x / std::max(_context.getCamera().getZoom(), 0.01f), -delta.y / std::max(_context.getCamera().getZoom(), 0.01f)));
    }
    if (hovered && std::abs(ImGui::GetIO().MouseWheel) > 0.001f)
    {
        const float nextZoom = std::clamp(_context.getCamera().getZoom() + ImGui::GetIO().MouseWheel * 0.08f, 0.35f, 4.0f);
        _context.getCamera().setZoom(nextZoom);
    }

    if (m_sceneViewportShowGrid)
    {
        const auto& camera = _context.getCamera();
        const glm::vec2 viewMin = camera.screenToWorld({0.0f, 0.0f});
        const glm::vec2 viewMax = camera.screenToWorld(_context.getRenderer().getLogicalSize());

        const float minX = std::min(viewMin.x, viewMax.x);
        const float maxX = std::max(viewMin.x, viewMax.x);

        // 瓦片网格与地图分区网格对齐：上半使用背景格，下半使用地形格
        const glm::vec2 bgGrid    = backgroundGridCellSizeWorld();
        const glm::vec2 gndGrid   = groundGridCellSizeWorld();
        const float bgTopY        = backgroundZoneTopWorldY();
        const float bgBottomY     = backgroundZoneBottomWorldY();
        const float gndTopY       = groundZoneTopWorldY();
        const float gndBottomY    = groundZoneBottomWorldY();

        // helper: screen-Y clipped between two canvas-Y bounds
        auto addZoneVLine = [&](float worldX, float clipTop, float clipBot, ImU32 col) {
            const ImVec2 sA = logicalToImGuiScreen(_context, camera.worldToScreen({worldX, 0.0f}));
            if (sA.x < canvasMin.x || sA.x > canvasMax.x) return;
            const float yA = std::max(canvasMin.y, clipTop);
            const float yB = std::min(canvasMax.y, clipBot);
            if (yA < yB) drawList->AddLine(ImVec2(sA.x, yA), ImVec2(sA.x, yB), col, 1.0f);
        };
        auto addZoneHLine = [&](float worldY, float clipLeft, float clipRight, ImU32 col) {
            const ImVec2 sA = logicalToImGuiScreen(_context, camera.worldToScreen({0.0f, worldY}));
            if (sA.y < canvasMin.y || sA.y > canvasMax.y) return;
            const float xA = std::max(canvasMin.x, clipLeft);
            const float xB = std::min(canvasMax.x, clipRight);
            if (xA < xB) drawList->AddLine(ImVec2(xA, sA.y), ImVec2(xB, sA.y), col, 1.0f);
        };

        const ImVec2 bgTopScreen  = logicalToImGuiScreen(_context, camera.worldToScreen({0.0f, bgTopY}));
        const ImVec2 bgBotScreen  = logicalToImGuiScreen(_context, camera.worldToScreen({0.0f, bgBottomY}));
        const ImVec2 gndTopScreen = logicalToImGuiScreen(_context, camera.worldToScreen({0.0f, gndTopY}));
        const ImVec2 gndBotScreen = logicalToImGuiScreen(_context, camera.worldToScreen({0.0f, gndBottomY}));

        // ── 上半背景区竖线 ──────────────────────────────────────────
        if (bgGrid.x > 0.001f)
        {
            for (float x = std::floor(minX / bgGrid.x) * bgGrid.x; x <= maxX + bgGrid.x; x += bgGrid.x)
                addZoneVLine(x, bgTopScreen.y, bgBotScreen.y, IM_COL32(120, 160, 255, 22));
        }
        // ── 上半背景区横线 ──────────────────────────────────────────
        if (bgGrid.y > 0.001f)
        {
            for (int row = 0; row <= m_backgroundGridRows; ++row)
            {
                const float wy = bgTopY + static_cast<float>(row) * bgGrid.y;
                if (wy > bgBottomY + bgGrid.y * 0.05f) break;
                addZoneHLine(wy, canvasMin.x, canvasMax.x, IM_COL32(120, 160, 255, 20));
            }
        }
        // ── 下半地形区竖线 ──────────────────────────────────────────
        if (gndGrid.x > 0.001f)
        {
            for (float x = std::floor(minX / gndGrid.x) * gndGrid.x; x <= maxX + gndGrid.x; x += gndGrid.x)
                addZoneVLine(x, gndTopScreen.y, gndBotScreen.y, IM_COL32(120, 220, 120, 20));
        }
        // ── 下半地形区横线 ──────────────────────────────────────────
        if (gndGrid.y > 0.001f)
        {
            for (int row = 0; row <= m_groundGridRows; ++row)
            {
                const float wy = gndTopY + static_cast<float>(row) * gndGrid.y;
                if (wy > gndBottomY + gndGrid.y * 0.05f) break;
                addZoneHLine(wy, canvasMin.x, canvasMax.x, IM_COL32(120, 220, 120, 18));
            }
        }
    }

    if (m_sceneViewportShowAxes)
    {
        const ImVec2 origin(canvasMin.x + 20.0f, canvasMax.y - 24.0f);
        drawList->AddLine(origin, ImVec2(origin.x + 28.0f, origin.y), IM_COL32(235, 81, 81, 255), 2.0f);
        drawList->AddLine(origin, ImVec2(origin.x, origin.y - 28.0f), IM_COL32(94, 214, 110, 255), 2.0f);
        drawList->AddLine(origin, ImVec2(origin.x - 18.0f, origin.y - 18.0f), IM_COL32(97, 163, 255, 255), 2.0f);
        drawList->AddText(ImVec2(origin.x + 32.0f, origin.y - 10.0f), IM_COL32(235, 81, 81, 255), "X");
        drawList->AddText(ImVec2(origin.x - 8.0f, origin.y - 42.0f), IM_COL32(94, 214, 110, 255), "Y");
        drawList->AddText(ImVec2(origin.x - 30.0f, origin.y - 30.0f), IM_COL32(97, 163, 255, 255), "Z");
    }

    if (m_sceneViewportShowCameraInfo)
    {
        const ImVec2 infoMin(canvasMin.x + 12.0f, canvasMin.y + 12.0f);
        const ImVec2 infoMax(infoMin.x + 256.0f, infoMin.y + 72.0f);
        drawList->AddRectFilled(infoMin, infoMax, IM_COL32(7, 10, 15, 210), 8.0f);
        drawList->AddText(ImVec2(infoMin.x + 10.0f, infoMin.y + 10.0f), IM_COL32(226, 230, 236, 255),
                          (std::string("Camera ") + (cam.isPseudo3DEnabled() ? "Pseudo3D" : "Flat2D")).c_str());
        char buffer[160];
        std::snprintf(buffer, sizeof(buffer), "Pos %.1f, %.1f  Zoom %.2f", c.x, c.y, cam.getZoom());
        drawList->AddText(ImVec2(infoMin.x + 10.0f, infoMin.y + 30.0f), IM_COL32(161, 172, 186, 255), buffer);
        std::snprintf(buffer, sizeof(buffer), "Viewport %.0f x %.0f  Selected %d", avail.x, avail.y, m_selectedActorIndex);
        drawList->AddText(ImVec2(infoMin.x + 10.0f, infoMin.y + 48.0f), IM_COL32(161, 172, 186, 255), buffer);
    }

    if (actor_manager && m_selectedActorIndex >= 0 && m_selectedActorIndex < static_cast<int>(actor_manager->getActors().size()))
    {
        if (auto* actor = actor_manager->getActors()[static_cast<size_t>(m_selectedActorIndex)].get())
        {
            if (auto* transform = actor->getComponent<engine::component::TransformComponent>())
            {
                const ImVec2 screenPos = logicalToImGuiScreen(_context, cam.worldToScreen(transform->getPosition()));
                if (screenPos.x >= canvasMin.x && screenPos.x <= canvasMax.x && screenPos.y >= canvasMin.y && screenPos.y <= canvasMax.y)
                {
                    drawList->AddCircle(ImVec2(screenPos.x, screenPos.y), 18.0f, IM_COL32(109, 196, 255, 255), 32, 2.0f);
                    drawList->AddText(ImVec2(screenPos.x + 22.0f, screenPos.y - 8.0f), IM_COL32(109, 196, 255, 255), actor->getName().c_str());
                }
            }
        }
    }

    ImGui::End();
}

void GameScene::renderConsolePanel()
{
    if (!m_showConsolePanel)
        return;

    if (m_editorDockspaceId != 0)
        ImGui::SetNextWindowDockID(static_cast<ImGuiID>(m_editorDockspaceId), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(900.0f, 220.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("控制台", &m_showConsolePanel))
    {
        ImGui::End();
        return;
    }

    ensureEditorConsoleSeeded();

    if (ImGui::Button("清空"))
        m_consoleEntries.clear();
    ImGui::SameLine();
    if (ImGui::Button("导出"))
    {
        std::ofstream output("assets/editor_console.log");
        if (output.is_open())
        {
            for (const auto& entry : m_consoleEntries)
            {
                const char* levelText = "LOG";
                if (entry.level == EditorConsoleLevel::Warning)
                    levelText = "WARN";
                else if (entry.level == EditorConsoleLevel::Error)
                    levelText = "ERROR";
                output << "[" << formatConsoleTimestamp(entry.timeSeconds) << "]"
                       << "[" << levelText << "]"
                       << "[" << entry.source << "] "
                       << entry.message << "\n";
            }
            appendEditorConsole(EditorConsoleLevel::Log, "Console", "日志导出到 assets/editor_console.log");
        }
        else
        {
            appendEditorConsole(EditorConsoleLevel::Error, "Console", "导出失败: 无法写入 assets/editor_console.log");
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("自动滚动", &m_consoleAutoScroll);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##console_search", "搜索日志内容...", m_consoleSearchBuffer.data(), m_consoleSearchBuffer.size());

    int logCount = 0;
    int warningCount = 0;
    int errorCount = 0;
    for (const auto& entry : m_consoleEntries)
    {
        if (entry.level == EditorConsoleLevel::Warning)
            ++warningCount;
        else if (entry.level == EditorConsoleLevel::Error)
            ++errorCount;
        else
            ++logCount;
    }

    ImGui::Separator();
    ImGui::Checkbox((std::string("日志 ") + std::to_string(logCount)).c_str(), &m_consoleFilterLog);
    ImGui::SameLine();
    ImGui::Checkbox((std::string("警告 ") + std::to_string(warningCount)).c_str(), &m_consoleFilterWarning);
    ImGui::SameLine();
    ImGui::Checkbox((std::string("错误 ") + std::to_string(errorCount)).c_str(), &m_consoleFilterError);
    ImGui::Separator();

    const std::string keyword = toLowerAscii(m_consoleSearchBuffer.data());
    auto visibleByFilter = [&](const EditorConsoleEntry& entry) {
        const bool allowLevel = (entry.level == EditorConsoleLevel::Log && m_consoleFilterLog)
                             || (entry.level == EditorConsoleLevel::Warning && m_consoleFilterWarning)
                             || (entry.level == EditorConsoleLevel::Error && m_consoleFilterError);
        if (!allowLevel)
            return false;
        if (keyword.empty())
            return true;
        const std::string haystack = toLowerAscii(entry.source + " " + entry.message);
        return haystack.find(keyword) != std::string::npos;
    };

    if (ImGui::BeginChild("##console_logs", ImVec2(0.0f, 0.0f), true))
    {
        for (const auto& entry : m_consoleEntries)
        {
            if (!visibleByFilter(entry))
                continue;

            ImVec4 color = ImVec4(0.82f, 0.84f, 0.88f, 1.0f);
            const char* levelText = "LOG";
            if (entry.level == EditorConsoleLevel::Warning)
            {
                color = ImVec4(0.98f, 0.85f, 0.39f, 1.0f);
                levelText = "WARN";
            }
            else if (entry.level == EditorConsoleLevel::Error)
            {
                color = ImVec4(0.95f, 0.43f, 0.41f, 1.0f);
                levelText = "ERROR";
            }

            const std::string line = "[" + formatConsoleTimestamp(entry.timeSeconds) + "] [" + levelText + "] [" + entry.source + "] " + entry.message;
            ImGui::TextColored(color, "%s", line.c_str());
        }

        if (m_consoleAutoScroll && m_consoleScrollToBottom)
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    m_consoleScrollToBottom = false;

    ImGui::End();
}

void GameScene::renderAnimationEditorPanel()
{
    if (!m_showAnimationEditorPanel)
        return;

    ImGui::SetNextWindowSize(ImVec2(900.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("动画编辑器", &m_showAnimationEditorPanel))
    {
        ImGui::End();
        return;
    }

    static float timelineCursor = 0.0f;
    ImGui::Text("时间轴 | 帧率 60 | 当前帧 0");
    ImGui::SliderFloat("播放头", &timelineCursor, 0.0f, 1.0f, "%.2f");
    ImGui::Separator();
    ImGui::TextDisabled("轨道编辑与曲线编辑将逐步接入。当前为布局骨架。");

    ImGui::End();
}

void GameScene::renderShaderEditorPanel()
{
    if (!m_showShaderEditorPanel)
        return;

    ImGui::SetNextWindowSize(ImVec2(760.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("着色器编辑器", &m_showShaderEditorPanel))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("节点图画布（规划中）");
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::GetWindowDrawList()->AddRect(ImGui::GetCursorScreenPos(),
                                        ImVec2(ImGui::GetCursorScreenPos().x + avail.x,
                                               ImGui::GetCursorScreenPos().y + avail.y),
                                        IM_COL32(120, 150, 210, 120), 4.0f, 0, 1.2f);
    ImGui::Dummy(avail);

    ImGui::End();
}

void GameScene::renderProfilerPanel()
{
    if (!m_showProfilerPanel)
        return;

    ImGui::SetNextWindowSize(ImVec2(500.0f, 300.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("性能分析器", &m_showProfilerPanel))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("帧耗时 %.2f ms", m_frameProfiler.frameDeltaMs);
    ImGui::Text("Update %.2f ms  Render %.2f ms", m_frameProfiler.updateTotal.lastMs, m_frameProfiler.renderTotal.lastMs);
    ImGui::Separator();
    ImGui::TextDisabled("详细采样表和折线图后续接入。当前显示实时核心指标。");

    ImGui::End();
}

void GameScene::renderHierarchyPanel()
{
    if (!m_showHierarchyPanel || !actor_manager)
        return;

    pushDevEditorTheme();
    pruneGroundSelection();

    const auto& actors = actor_manager->getActors();
    if (m_selectedActorIndex >= static_cast<int>(actors.size()))
        m_selectedActorIndex = actors.empty() ? -1 : 0;

    if (m_editorDockspaceId != 0)
        ImGui::SetNextWindowDockID(static_cast<ImGuiID>(m_editorDockspaceId), ImGuiCond_Always);
    ImGui::SetNextWindowPos({16.0f, 36.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({332.0f, 520.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("对象层级", &m_showHierarchyPanel, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        popDevEditorTheme();
        return;
    }

    drawEditorSectionTitle("筛选");
    ImGui::Text("对象总数: %zu", actors.size());
    ImGui::SameLine();
    ImGui::TextDisabled("当前选中: %d", m_selectedActorIndex);

    ImGui::SetNextItemWidth(-112.0f);
    ImGui::InputTextWithHint("##hier_filter", "搜索名称/标签", m_hierarchyFilterBuffer.data(), m_hierarchyFilterBuffer.size());
    ImGui::SameLine();
    if (ImGui::Button("清空"))
        m_hierarchyFilterBuffer[0] = '\0';

    bool persistUiState = false;
    persistUiState |= ImGui::Checkbox("按标签分组", &m_hierarchyGroupByTag);
    ImGui::SameLine();
    persistUiState |= ImGui::Checkbox("仅看收藏", &m_hierarchyFavoritesOnly);
    if (persistUiState)
    {
        saveBoolSetting("hierarchy_group_by_tag", m_hierarchyGroupByTag);
        saveBoolSetting("hierarchy_favorites_only", m_hierarchyFavoritesOnly);
    }

    const std::string filterText(m_hierarchyFilterBuffer.data());
    auto containsInsensitive = [](const std::string& text, const std::string& key) {
        if (key.empty())
            return true;
        auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
        const auto it = std::search(text.begin(), text.end(), key.begin(), key.end(),
                                    [&](char a, char b) { return lower(static_cast<unsigned char>(a)) == lower(static_cast<unsigned char>(b)); });
        return it != text.end();
    };

    drawEditorSectionTitle("地面制作");
    static const char* kGroundTextureOptions[] = {
        "assets/textures/Props/platform-long.png",
        "assets/textures/Props/small-platform.png",
        "assets/textures/Props/block-big.png",
        "assets/textures/Props/block.png"
    };
    ImGui::TextDisabled("基础");
    ImGui::InputText("名称##ground_name", m_groundMakerNameBuffer.data(), m_groundMakerNameBuffer.size());
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("地面素材", &m_groundMakerTextureIndex, kGroundTextureOptions, IM_ARRAYSIZE(kGroundTextureOptions));

    const char* groundTexturePath = kGroundTextureOptions[std::clamp(m_groundMakerTextureIndex, 0, static_cast<int>(IM_ARRAYSIZE(kGroundTextureOptions)) - 1)];
    const unsigned int groundTextureId = _context.getResourceManager().getGLTexture(groundTexturePath);
    const glm::vec2 groundTextureSize = _context.getResourceManager().getTextureSize(groundTexturePath);
    if (groundTextureId != 0 && groundTextureSize.x > 0.0f && groundTextureSize.y > 0.0f)
    {
        drawEditorSectionTitle("材质预览");
        const float previewMaxW = 240.0f;
        const float previewMaxH = 88.0f;
        const float ratio = std::min(previewMaxW / groundTextureSize.x, previewMaxH / groundTextureSize.y);
        const ImVec2 previewSize(groundTextureSize.x * ratio, groundTextureSize.y * ratio);
        ImGui::Image((ImTextureID)(intptr_t)groundTextureId, previewSize);
        ImGui::TextDisabled("%.0f x %.0f px", groundTextureSize.x, groundTextureSize.y);
    }
    else
    {
        ImGui::TextDisabled("材质预览不可用: %s", groundTexturePath);
    }

    ImGui::Separator();
    ImGui::TextDisabled("网格规划");
    ImGui::Checkbox("显示场景网格", &m_groundMakerShowGrid);
    ImGui::SameLine();
    ImGui::Checkbox("启用网格吸附", &m_groundMakerUseGridSnap);

    bool gridLayoutChanged = false;
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("背景竖排格子", &m_backgroundGridRows, 1, 120, "%d"))
        gridLayoutChanged = true;
    m_backgroundGridRows = std::max(1, m_backgroundGridRows);
    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::SliderFloat("背景起始位置(0=顶)", &m_backgroundGridStart, 0.0f, 0.98f, "%.2f"))
    {
        m_backgroundGridStart = std::clamp(m_backgroundGridStart, 0.0f, 0.98f);
        m_backgroundGridEnd   = std::max(m_backgroundGridEnd, m_backgroundGridStart + 0.01f);
        gridLayoutChanged = true;
    }
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("背景结束位置(1=底)", &m_backgroundGridEnd, 0.01f, 1.0f, "%.2f"))
    {
        m_backgroundGridEnd   = std::clamp(m_backgroundGridEnd, 0.01f, 1.0f);
        m_backgroundGridStart = std::min(m_backgroundGridStart, m_backgroundGridEnd - 0.01f);
        gridLayoutChanged = true;
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("地形竖排格子", &m_groundGridRows, 1, 120, "%d"))
        gridLayoutChanged = true;
    m_groundGridRows = std::max(1, m_groundGridRows);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("地形起始位置(0=顶)", &m_groundGridStart, 0.0f, 0.98f, "%.2f"))
    {
        m_groundGridStart = std::clamp(m_groundGridStart, 0.0f, 0.98f);
        m_groundGridEnd   = std::max(m_groundGridEnd, m_groundGridStart + 0.01f);
        gridLayoutChanged = true;
    }
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("地形结束位置(1=底)", &m_groundGridEnd, 0.01f, 1.0f, "%.2f"))
    {
        m_groundGridEnd   = std::clamp(m_groundGridEnd, 0.01f, 1.0f);
        m_groundGridStart = std::min(m_groundGridStart, m_groundGridEnd - 0.01f);
        gridLayoutChanged = true;
    }
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("地形网格长宽比", &m_groundGridAspect, 0.5f, 8.0f, "%.2f"))
    {
        m_groundGridAspect = std::clamp(m_groundGridAspect, 0.5f, 8.0f);
        gridLayoutChanged = true;
    }
    if (ImGui::Button("地形长宽比 2.5(推荐)"))
    {
        m_groundGridAspect = 2.5f;
        gridLayoutChanged = true;
    }

    float groundSpawnPos[2] = {m_groundMakerSpawnPos.x, m_groundMakerSpawnPos.y};
    if (ImGui::DragFloat2("生成位置", groundSpawnPos, 1.0f))
        m_groundMakerSpawnPos = snapGroundMakerPosition({groundSpawnPos[0], groundSpawnPos[1]});

    const bool spawnInGroundZone = isGroundZoneAt(m_groundMakerSpawnPos);
    const glm::vec2 bgGrid = backgroundGridCellSizeWorld();
    const glm::vec2 groundGrid = groundGridCellSizeWorld();
    ImGui::TextDisabled("当前放置分区: %s", spawnInGroundZone ? "下半地形区" : "上半背景区");
    ImGui::TextDisabled("背景网格(世界单位): %.1f x %.1f", bgGrid.x, bgGrid.y);
    ImGui::TextDisabled("地形网格(世界单位): %.1f x %.1f", groundGrid.x, groundGrid.y);
    {
        const float gap = m_groundGridStart - m_backgroundGridEnd;
        if (gap > 0.01f)
            ImGui::TextDisabled("背景[%.2f-%.2f] 地形[%.2f-%.2f] 间距 %.2f",
                m_backgroundGridStart, m_backgroundGridEnd, m_groundGridStart, m_groundGridEnd, gap);
        else if (gap < -0.01f)
            ImGui::TextColored(ImVec4(1.0f,0.5f,0.3f,1.0f), "警告: 背景结束%.2f > 地形起始%.2f，区域重叠",
                m_backgroundGridEnd, m_groundGridStart);
        else
            ImGui::TextDisabled("背景[%.2f-%.2f] 地形[%.2f-%.2f] 相邻",
                m_backgroundGridStart, m_backgroundGridEnd, m_groundGridStart, m_groundGridEnd);
    }

    ImGui::Separator();
    ImGui::TextDisabled("吸附设置");
    ImGui::BeginDisabled(!m_groundMakerUseGridSnap);
    ImGui::Checkbox("吸附 X", &m_groundMakerSnapX);
    ImGui::SameLine();
    ImGui::Checkbox("吸附 Y", &m_groundMakerSnapY);

    if (chunk_manager)
    {
        if (ImGui::Button("背景按瓦片高度重算行数"))
        {
            const float displayH = std::max(100.0f, ImGui::GetIO().DisplaySize.y);
            const float bgH = std::max(1.0f, displayH * std::max(0.0f, m_backgroundGridEnd - m_backgroundGridStart));
            const int tileH = std::max(1, chunk_manager->getTileSize().y);
            m_backgroundGridRows = std::max(1, static_cast<int>(std::round(bgH / static_cast<float>(tileH))));
            m_groundMakerSpawnPos = snapGroundMakerPosition(m_groundMakerSpawnPos);
            gridLayoutChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("地形按瓦片高度重算行数"))
        {
            const float displayH = std::max(100.0f, ImGui::GetIO().DisplaySize.y);
            const float groundH = std::max(1.0f, displayH * std::max(0.0f, m_groundGridEnd - m_groundGridStart));
            const int tileH = std::max(1, chunk_manager->getTileSize().y);
            m_groundGridRows = std::max(1, static_cast<int>(std::round(groundH / static_cast<float>(tileH))));
            m_groundMakerSpawnPos = snapGroundMakerPosition(m_groundMakerSpawnPos);
            gridLayoutChanged = true;
        }
    }
    ImGui::EndDisabled();

    if (gridLayoutChanged)
    {
        m_groundMakerSpawnPos = snapGroundMakerPosition(m_groundMakerSpawnPos);
        m_groundConfigDirty = true;
        saveGroundActorsToConfig();
    }

    ImGui::Separator();
    ImGui::TextDisabled("形态与放置");
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("长度格数", &m_groundMakerLengthCells))
        m_groundMakerLengthCells = std::max(1, m_groundMakerLengthCells);
    ImGui::SameLine();
    ImGui::TextDisabled("宽度 %.0f px", groundMakerWidthFromCells());

    float scaleY = m_groundMakerScale.y;
    if (ImGui::DragFloat("垂直缩放", &scaleY, 0.05f, 0.1f, 20.0f))
        m_groundMakerScale.y = std::max(0.1f, scaleY);
    ImGui::DragFloat("旋转角度", &m_groundMakerRotation, 1.0f, -180.0f, 180.0f, "%.0f deg");
    ImGui::TextDisabled("旋转作用于精灵预览与对象变换，碰撞仍保持轴对齐");

    ImGui::Checkbox("创建默认碰撞体(仅地面区生效)", &m_groundMakerUsePhysics);
    if (m_groundMakerUsePhysics && spawnInGroundZone)
    {
        float bodyHalfPx[2] = {m_groundMakerBodyHalfPx.x, m_groundMakerBodyHalfPx.y};
        if (ImGui::DragFloat2("碰撞半尺寸", bodyHalfPx, 1.0f, 1.0f, 512.0f))
        {
            m_groundMakerBodyHalfPx.x = std::max(1.0f, bodyHalfPx[0]);
            m_groundMakerBodyHalfPx.y = std::max(1.0f, bodyHalfPx[1]);
        }
    }
    else if (m_groundMakerUsePhysics)
    {
        ImGui::TextDisabled("当前在背景区，创建对象将不附加碰撞体。");
    }

    ImGui::Checkbox("右键拖拽放置", &m_groundMakerPlaceMode);
    if (m_groundMakerPlaceMode)
        ImGui::TextDisabled("在场景里按住右键拖拽，可直接落点并按拖拽长度扩展宽度");

    if (m_hasHoveredTile)
    {
        ImGui::SameLine();
        if (ImGui::Button("使用悬停瓦片"))
            m_groundMakerSpawnPos = snapGroundMakerPosition(m_lastHoveredTileCenter);
    }

    if (ImGui::Button("创建地面对象", ImVec2(-1.0f, 0.0f)))
        createGroundActor();

    drawEditorSectionTitle("地面批量操作");
    ImGui::TextDisabled("已框选地面: %zu  |  Shift + 左键框选", m_groundSelection.size());
    if (m_groundConfigDirty)
        drawEditorStatusChip("待保存", ImVec4(0.68f, 0.48f, 0.16f, 1.0f));
    else
        drawEditorStatusChip("已同步", ImVec4(0.20f, 0.52f, 0.28f, 1.0f));

    if (ImGui::Button("对齐框选到网格", ImVec2(-1.0f, 0.0f)))
        snapSelectedGroundActorsToGrid();
    if (ImGui::Button("保存地面到关卡", ImVec2(-1.0f, 0.0f)))
        saveGroundActorsToConfig();
    if (ImGui::Button("重载已保存地面", ImVec2(-1.0f, 0.0f)))
        loadGroundActorsFromConfig(true);
    if (ImGui::Button("清空框选", ImVec2(-1.0f, 0.0f)))
    {
        m_groundSelection.clear();
        m_selectedActorIndex = -1;
    }

    drawEditorSectionTitle("对象列表");
    ImGui::BeginChild("##hierarchy_actor_list", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    auto drawActorItem = [&](int index) {
        auto* actor = actors[static_cast<size_t>(index)].get();
        if (!actor)
            return;
        if (m_hierarchyFavoritesOnly && !m_hierarchyFavorites.contains(actor))
            return;

        const std::string name = actor->getName().empty() ? "<unnamed>" : actor->getName();
        const std::string tag = actor->getTag();
        if (!containsInsensitive(name, filterText) && !containsInsensitive(tag, filterText))
            return;

        const bool selected = (m_selectedActorIndex == index) || m_groundSelection.contains(actor);
        std::string label = name;
        if (!tag.empty() && tag != "未定义的标签")
            label += " [" + tag + "]";
        if (actor->isNeedRemove())
            label += " [PendingDelete]";

        const bool favorite = m_hierarchyFavorites.contains(actor);
        const std::string rowId = "##actor_row_" + std::to_string(index);
        if (!ImGui::BeginTable(rowId.c_str(), 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
            return;

        ImGui::TableSetupColumn("fav",  ImGuiTableColumnFlags_WidthFixed, 22.0f);
        ImGui::TableSetupColumn("vis",  ImGuiTableColumnFlags_WidthFixed, 20.0f);
        ImGui::TableSetupColumn("enb",  ImGuiTableColumnFlags_WidthFixed, 20.0f);
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        const std::string buttonLabel = std::string(favorite ? "★" : "☆") + "##fav_" + std::to_string(index);
        if (ImGui::SmallButton(buttonLabel.c_str()))
        {
            if (favorite)
                m_hierarchyFavorites.erase(actor);
            else
                m_hierarchyFavorites.insert(actor);
        }

        ImGui::TableSetColumnIndex(1);
        {
            bool vis = actor->isVisible();
            ImGui::PushStyleColor(ImGuiCol_Text, vis ? ImVec4(1.0f,1.0f,1.0f,1.0f) : ImVec4(0.4f,0.4f,0.4f,1.0f));
            const std::string visLabel = std::string(vis ? "O" : "-") + "##vis_" + std::to_string(index);
            if (ImGui::SmallButton(visLabel.c_str()))
            {
                actor->setVisible(!vis);
                appendEditorConsole(EditorConsoleLevel::Log, "Hierarchy",
                    std::string(actor->isVisible() ? "显示对象: " : "隐藏对象: ") + actor->getName());
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("可见性 (O=可见 / -=隐藏)");
        }

        ImGui::TableSetColumnIndex(2);
        {
            bool enb = actor->isEnabled();
            ImGui::PushStyleColor(ImGuiCol_Text, enb ? ImVec4(0.5f,1.0f,0.5f,1.0f) : ImVec4(0.4f,0.4f,0.4f,1.0f));
            const std::string enbLabel = std::string(enb ? "E" : "D") + "##enb_" + std::to_string(index);
            if (ImGui::SmallButton(enbLabel.c_str()))
            {
                actor->setEnabled(!enb);
                appendEditorConsole(EditorConsoleLevel::Log, "Hierarchy",
                    std::string(actor->isEnabled() ? "启用对象: " : "禁用对象: ") + actor->getName());
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("逻辑启用 (E=启用 / D=禁用)");
        }

        ImGui::TableSetColumnIndex(3);
        if (ImGui::Selectable((label + "##actor_" + std::to_string(index)).c_str(), selected))
        {
            m_selectedActorIndex = index;
            m_groundSelection.clear();
            if (isGroundActor(actor))
                m_groundSelection.insert(actor);
        }

        // 拖拽重排：source
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::SetDragDropPayload("HIERARCHY_REORDER", &index, sizeof(int));
            ImGui::TextUnformatted(label.c_str());
            ImGui::TextDisabled("拖拽到目标位置重排");
            ImGui::EndDragDropSource();
        }
        // 拖拽重排：target
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_REORDER"))
            {
                const int fromIndex = *static_cast<const int*>(payload->Data);
                if (fromIndex != index && actor_manager)
                {
                    actor_manager->moveActor(static_cast<size_t>(fromIndex), static_cast<size_t>(index));
                    m_selectedActorIndex = index;
                    appendEditorConsole(EditorConsoleLevel::Log, "Hierarchy",
                        std::string("重排对象顺序: ") + std::to_string(fromIndex) + " → " + std::to_string(index));
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("设为当前选择"))
            {
                m_selectedActorIndex = index;
                m_groundSelection.clear();
                if (isGroundActor(actor))
                    m_groundSelection.insert(actor);
            }
            if (ImGui::MenuItem(m_hierarchyFavorites.contains(actor) ? "取消收藏" : "加入收藏"))
            {
                if (m_hierarchyFavorites.contains(actor))
                    m_hierarchyFavorites.erase(actor);
                else
                    m_hierarchyFavorites.insert(actor);
            }
            if (ImGui::MenuItem("同步到检视器重命名"))
            {
                m_selectedActorIndex = index;
                m_inspectorRenameBufferActorIndex = -1;
            }
            if (ImGui::MenuItem("聚焦对象"))
            {
                if (auto* transform = actor->getComponent<engine::component::TransformComponent>())
                    _context.getCamera().setPosition(transform->getPosition() - _context.getCamera().getViewportSize() * 0.5f / std::max(_context.getCamera().getZoom(), 0.01f));
            }
            ImGui::Separator();
            if (ImGui::MenuItem(actor->isNeedRemove() ? "取消删除标记" : "标记删除"))
            {
                const bool nextRemove = !actor->isNeedRemove();
                actor->setNeedRemove(nextRemove);
                appendEditorConsole(nextRemove ? EditorConsoleLevel::Warning : EditorConsoleLevel::Log,
                                    "Hierarchy",
                                    std::string(nextRemove ? "标记删除对象: " : "取消删除标记: ") + actor->getName());
                if (isGroundActor(actor))
                    m_groundConfigDirty = true;
            }
            ImGui::EndPopup();
        }

        ImGui::EndTable();
    };

    int visibleActorCount = 0;
    int visibleGroundCount = 0;
    for (int index = 0; index < static_cast<int>(actors.size()); ++index)
    {
        const auto* actor = actors[static_cast<size_t>(index)].get();
        if (!actor)
            continue;
        if (m_hierarchyFavoritesOnly && !m_hierarchyFavorites.contains(actor))
            continue;
        const std::string actorName = actor->getName().empty() ? "<unnamed>" : actor->getName();
        const std::string actorTag = actor->getTag();
        if (!containsInsensitive(actorName, filterText) && !containsInsensitive(actorTag, filterText))
            continue;
        ++visibleActorCount;
        if (isGroundActor(actor))
            ++visibleGroundCount;
    }

    ImGui::TextDisabled("可见对象 %d | 可见地面 %d", visibleActorCount, visibleGroundCount);
    if (ImGui::Button("选中全部地面", ImVec2(-1.0f, 0.0f)))
    {
        m_groundSelection.clear();
        for (const auto& holder : actors)
        {
            const auto* actor = holder.get();
            if (!actor || actor->isNeedRemove() || !isGroundActor(actor))
                continue;
            const std::string actorName = actor->getName().empty() ? "<unnamed>" : actor->getName();
            const std::string actorTag = actor->getTag();
            if (!containsInsensitive(actorName, filterText) && !containsInsensitive(actorTag, filterText))
                continue;
            m_groundSelection.insert(actor);
        }
        appendEditorConsole(EditorConsoleLevel::Log, "Hierarchy", "已选中当前过滤结果中的全部地面对象");
    }
    if (ImGui::Button("收藏当前选择", ImVec2(-1.0f, 0.0f)))
    {
        if (m_selectedActorIndex >= 0 && m_selectedActorIndex < static_cast<int>(actors.size()))
            m_hierarchyFavorites.insert(actors[static_cast<size_t>(m_selectedActorIndex)].get());
        for (const auto* actor : m_groundSelection)
            m_hierarchyFavorites.insert(actor);
    }
    if (ImGui::Button("标记删除当前选择", ImVec2(-1.0f, 0.0f)))
    {
        if (m_selectedActorIndex >= 0 && m_selectedActorIndex < static_cast<int>(actors.size()))
        {
            if (auto* actor = actors[static_cast<size_t>(m_selectedActorIndex)].get())
            {
                actor->setNeedRemove(true);
                if (isGroundActor(actor))
                    m_groundConfigDirty = true;
            }
        }
        for (const auto* actor : m_groundSelection)
        {
            const_cast<engine::object::GameObject*>(actor)->setNeedRemove(true);
            m_groundConfigDirty = true;
        }
        appendEditorConsole(EditorConsoleLevel::Warning, "Hierarchy", "已标记当前选择对象为删除");
    }

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

    ImGui::EndChild();

    ImGui::End();
    popDevEditorTheme();
}

void GameScene::createGroundActor()
{
    if (!actor_manager)
        return;

    static const char* kGroundTextureOptions[] = {
        "assets/textures/Props/platform-long.png",
        "assets/textures/Props/small-platform.png",
        "assets/textures/Props/block-big.png",
        "assets/textures/Props/block.png"
    };

    m_groundMakerSpawnPos = snapGroundMakerPosition(m_groundMakerSpawnPos);
    const bool placeInGroundZone = isGroundZoneAt(m_groundMakerSpawnPos);
    const int textureIndex = std::clamp(m_groundMakerTextureIndex, 0, static_cast<int>(IM_ARRAYSIZE(kGroundTextureOptions)) - 1);
    const std::string actorName = m_groundMakerNameBuffer[0] ? m_groundMakerNameBuffer.data() : "ground_platform";
    const float targetWidthPx = groundMakerWidthFromCells();
    m_groundMakerScale.x = std::max(0.1f, targetWidthPx / 96.0f);

    auto* ground = actor_manager->createActor(actorName);
    if (!ground)
        return;

    ground->setTag(placeInGroundZone ? "Ground" : "Background");
    auto* transform = ground->addComponent<engine::component::TransformComponent>(m_groundMakerSpawnPos, m_groundMakerScale, m_groundMakerRotation);
    transform->setRotation(m_groundMakerRotation);
    ground->addComponent<engine::component::SpriteComponent>(kGroundTextureOptions[textureIndex], engine::utils::Alignment::CENTER);

    const bool createPhysics = m_groundMakerUsePhysics && placeInGroundZone;
    if (createPhysics && physics_manager)
    {
        constexpr float kPixelsPerMeter = 32.0f;
        const glm::vec2 bodyHalfPx = {
            std::max(1.0f, m_groundMakerBodyHalfPx.x * m_groundMakerScale.x),
            std::max(1.0f, m_groundMakerBodyHalfPx.y * m_groundMakerScale.y)
        };
        const glm::vec2 bodyHalfMeters = bodyHalfPx / kPixelsPerMeter;
        b2BodyId bodyId = physics_manager->createStaticBody(
            {m_groundMakerSpawnPos.x / kPixelsPerMeter, m_groundMakerSpawnPos.y / kPixelsPerMeter},
            {bodyHalfMeters.x, bodyHalfMeters.y},
            ground);
        ground->addComponent<engine::component::PhysicsComponent>(bodyId, physics_manager.get());
        m_groundColliderHalfByActor[ground] = bodyHalfPx;
    }
    else
    {
        m_groundColliderHalfByActor[ground] = {
            std::max(8.0f, m_groundMakerBodyHalfPx.x * m_groundMakerScale.x),
            std::max(8.0f, m_groundMakerBodyHalfPx.y * m_groundMakerScale.y)
        };
    }

    m_selectedActorIndex = static_cast<int>(actor_manager->actorCount()) - 1;
    m_inspectorRenameBufferActorIndex = -1;
    m_groundSelection.clear();
    if (placeInGroundZone)
        m_groundSelection.insert(ground);
    m_groundConfigDirty = true;
    spdlog::info("编辑器创建网格对象: zone='{}' name='{}' texture='{}' pos=({}, {}) rot={}",
                 placeInGroundZone ? "ground" : "background",
                 actorName,
                 kGroundTextureOptions[textureIndex],
                 m_groundMakerSpawnPos.x,
                 m_groundMakerSpawnPos.y,
                 m_groundMakerRotation);
    appendEditorConsole(EditorConsoleLevel::Log,
                        "Hierarchy",
                        std::string("创建对象 ") + actorName + " (" + (placeInGroundZone ? "ground" : "background") + ")");
}

float GameScene::backgroundZoneBottomWorldY() const
{
    const float displayH = std::max(100.0f, ImGui::GetIO().DisplaySize.y);
    return _context.getCamera().screenToWorld({0.0f, displayH * std::clamp(m_backgroundGridEnd, 0.0f, 1.0f)}).y;
}

float GameScene::backgroundZoneTopWorldY() const
{
    const float displayH = std::max(100.0f, ImGui::GetIO().DisplaySize.y);
    return _context.getCamera().screenToWorld({0.0f, displayH * std::clamp(m_backgroundGridStart, 0.0f, 1.0f)}).y;
}

float GameScene::groundZoneTopWorldY() const
{
    const float displayH = std::max(100.0f, ImGui::GetIO().DisplaySize.y);
    return _context.getCamera().screenToWorld({0.0f, displayH * std::clamp(m_groundGridStart, 0.0f, 1.0f)}).y;
}

float GameScene::groundZoneBottomWorldY() const
{
    const float displayH = std::max(100.0f, ImGui::GetIO().DisplaySize.y);
    return _context.getCamera().screenToWorld({0.0f, displayH * std::clamp(m_groundGridEnd, 0.0f, 1.0f)}).y;
}

glm::vec2 GameScene::backgroundGridCellSizeWorld() const
{
    const auto& cam = _context.getCamera();
    const glm::vec2 world0 = cam.screenToWorld({0.0f, 0.0f});
    const glm::vec2 world1 = cam.screenToWorld({1.0f, 1.0f});
    const float worldPerPixelY = std::max(0.001f, std::abs(world1.y - world0.y));

    const float displayH = std::max(100.0f, ImGui::GetIO().DisplaySize.y);
    const float zoneH = std::max(1.0f, displayH * std::max(0.0f, m_backgroundGridEnd - m_backgroundGridStart));
    const float cellH = std::max(1.0f, zoneH / static_cast<float>(std::max(1, m_backgroundGridRows)));
    const float worldCellH = cellH * worldPerPixelY;
    return {worldCellH, worldCellH};
}

glm::vec2 GameScene::groundGridCellSizeWorld() const
{
    const auto& cam = _context.getCamera();
    const glm::vec2 world0 = cam.screenToWorld({0.0f, 0.0f});
    const glm::vec2 world1 = cam.screenToWorld({1.0f, 1.0f});
    const float worldPerPixelY = std::max(0.001f, std::abs(world1.y - world0.y));

    const float displayH = std::max(100.0f, ImGui::GetIO().DisplaySize.y);
    const float zoneH = std::max(1.0f, displayH * std::max(0.0f, m_groundGridEnd - m_groundGridStart));
    const float cellH = std::max(1.0f, zoneH / static_cast<float>(std::max(1, m_groundGridRows)));
    const float worldCellH = cellH * worldPerPixelY;
    const float aspect = std::clamp(m_groundGridAspect, 0.5f, 8.0f);
    return {worldCellH * aspect, worldCellH};
}

bool GameScene::isGroundZoneAt(glm::vec2 worldPos) const
{
    return worldPos.y >= groundZoneTopWorldY();
}

glm::vec2 GameScene::groundMakerGridSizeFor(glm::vec2 worldPos) const
{
    if (isGroundZoneAt(worldPos))
        return groundGridCellSizeWorld();
    return backgroundGridCellSizeWorld();
}

glm::vec2 GameScene::snapGroundMakerPosition(glm::vec2 worldPos) const
{
    if (!m_groundMakerUseGridSnap)
        return worldPos;

    const glm::vec2 grid = groundMakerGridSizeFor(worldPos);
    return {
        m_groundMakerSnapX ? std::round(worldPos.x / grid.x) * grid.x : worldPos.x,
        m_groundMakerSnapY ? std::round(worldPos.y / grid.y) * grid.y : worldPos.y
    };
}

float GameScene::snapGroundMakerWidth(float widthPx) const
{
    if (!m_groundMakerUseGridSnap)
        return widthPx;

    const float gridX = groundMakerGridSizeFor(m_groundMakerSpawnPos).x;
    return std::max(gridX, std::round(widthPx / gridX) * gridX);
}

float GameScene::groundMakerWidthFromCells() const
{
    const float gridX = groundMakerGridSizeFor(m_groundMakerSpawnPos).x;
    return std::max(gridX, static_cast<float>(std::max(1, m_groundMakerLengthCells)) * gridX);
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

    pushDevEditorTheme();

    if (m_editorDockspaceId != 0)
        ImGui::SetNextWindowDockID(static_cast<ImGuiID>(m_editorDockspaceId), ImGuiCond_Always);
    ImGui::SetNextWindowPos({360.0f, 36.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({360.0f, 520.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("对象检视器", &m_showInspectorPanel, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        popDevEditorTheme();
        return;
    }

    std::string actorName = actor->getName();
    if (actorName.empty())
        actorName = "<unnamed>";
    const bool isGround = isGroundActor(actor);
    if (m_inspectorRenameBufferActorIndex != m_selectedActorIndex)
    {
        snprintf(m_inspectorRenameBuffer.data(), m_inspectorRenameBuffer.size(), "%s", actorName.c_str());
        snprintf(m_inspectorTagBuffer.data(), m_inspectorTagBuffer.size(), "%s", actor->getTag().c_str());
        m_inspectorRenameBufferActorIndex = m_selectedActorIndex;
    }

    drawEditorSectionTitle("基本信息");
    drawEditorKeyValue("对象名称:", actorName.c_str());
    ImGui::TextDisabled("对象索引: %d", m_selectedActorIndex);
    ImGui::InputText("名称", m_inspectorRenameBuffer.data(), m_inspectorRenameBuffer.size());
    ImGui::InputText("标签", m_inspectorTagBuffer.data(), m_inspectorTagBuffer.size());
    if (ImGui::Button("应用名称与标签", ImVec2(-120.0f, 0.0f)))
    {
        actor->setName(std::string(m_inspectorRenameBuffer.data()));
        actor->setTag(std::string(m_inspectorTagBuffer.data()));
        if (isGround)
            m_groundConfigDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("标记删除", ImVec2(-1.0f, 0.0f)))
    {
        actor->setNeedRemove(true);
        m_groundSelection.erase(actor);
        if (isGround)
            m_groundConfigDirty = true;
    }

    if (isGround)
    {
        ImGui::TextDisabled("Ground 已接入关卡保存，可在层级面板执行保存/重载。");
    }

    if (ImGui::BeginPopupContextWindow("##inspector_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem("聚焦当前对象"))
        {
            if (auto* transform = actor->getComponent<engine::component::TransformComponent>())
                _context.getCamera().setPosition(transform->getPosition() - _context.getCamera().getViewportSize() * 0.5f / std::max(_context.getCamera().getZoom(), 0.01f));
        }
        if (ImGui::MenuItem("加入收藏"))
            m_hierarchyFavorites.insert(actor);
        ImGui::EndPopup();
    }

    drawEditorSectionTitle("组件管理");
    const bool isCoreControlled = (actor == m_player || actor == m_mech || actor == m_possessedMonster);
    if (ImGui::BeginTable("##component_controls", 2, ImGuiTableFlags_SizingStretchSame))
    {
        auto drawComponentToggle = [&](const char* addLabel,
                                       const char* removeLabel,
                                       bool hasComponent,
                                       auto addFn,
                                       auto removeFn) {
            ImGui::TableNextColumn();
            if (!hasComponent)
            {
                if (ImGui::Button(addLabel, ImVec2(-1.0f, 0.0f)))
                    addFn();
            }
            else
            {
                ImGui::BeginDisabled(isCoreControlled);
                if (ImGui::Button(removeLabel, ImVec2(-1.0f, 0.0f)))
                    removeFn();
                ImGui::EndDisabled();
            }
        };

        drawComponentToggle("添加 Transform", "移除 Transform",
                            actor->hasComponent<engine::component::TransformComponent>(),
                            [&] { actor->addComponent<engine::component::TransformComponent>(glm::vec2{0.0f, 56.0f}); },
                            [&] { actor->removeComponent<engine::component::TransformComponent>(); });
        drawComponentToggle("添加 Controller", "移除 Controller",
                            actor->hasComponent<engine::component::ControllerComponent>(),
                            [&] { actor->addComponent<engine::component::ControllerComponent>(15.0f, 20.0f); },
                            [&] { actor->removeComponent<engine::component::ControllerComponent>(); });
        drawComponentToggle("添加 Sprite", "移除 Sprite",
                            actor->hasComponent<engine::component::SpriteComponent>(),
                            [&] { actor->addComponent<engine::component::SpriteComponent>("assets/textures/Props/bubble1.svg", engine::utils::Alignment::CENTER); },
                            [&] { actor->removeComponent<engine::component::SpriteComponent>(); });
        drawComponentToggle("添加 Animation", "移除 Animation",
                            actor->hasComponent<engine::component::AnimationComponent>(),
                            [&] { actor->addComponent<engine::component::AnimationComponent>(32.0f, 32.0f); },
                            [&] { actor->removeComponent<engine::component::AnimationComponent>(); });
        drawComponentToggle("添加 Parallax", "移除 Parallax",
                            actor->hasComponent<engine::component::ParallaxComponent>(),
                            [&] { actor->addComponent<engine::component::ParallaxComponent>("assets/textures/Layers/back.png", glm::vec2{0.35f, 0.25f}); },
                            [&] { actor->removeComponent<engine::component::ParallaxComponent>(); });
        ImGui::EndTable();
    }
    ImGui::TextDisabled(isCoreControlled ? "核心角色已保护关键组件" : "可增删基础组件");

    if (auto* transform = actor->getComponent<engine::component::TransformComponent>())
    {
        if (ImGui::CollapsingHeader("Transform##insp", ImGuiTreeNodeFlags_DefaultOpen))
        {
            glm::vec2 position = transform->getPosition();
            glm::vec2 scale = transform->getScale();
            float rotation = transform->getRotation();
            float positionArray[2] = {position.x, position.y};
            float scaleArray[2] = {scale.x, scale.y};
            auto* physics = actor->getComponent<engine::component::PhysicsComponent>();
            if (ImGui::DragFloat2("Position", positionArray, 0.5f))
            {
                transform->setPosition({positionArray[0], positionArray[1]});
                if (physics)
                    physics->setWorldPosition({positionArray[0], positionArray[1]});
                if (isGround)
                    m_groundConfigDirty = true;
            }
            if (ImGui::DragFloat2("Scale", scaleArray, 0.01f, 0.01f, 20.0f))
            {
                transform->setScale({scaleArray[0], scaleArray[1]});
                if (isGround)
                    m_groundConfigDirty = true;
            }
            if (ImGui::DragFloat("Rotation", &rotation, 0.5f, -360.0f, 360.0f))
            {
                transform->setRotation(rotation);
                if (isGround)
                    m_groundConfigDirty = true;
            }
        }
    }

    if (auto* controller = actor->getComponent<engine::component::ControllerComponent>())
    {
        if (ImGui::CollapsingHeader("Controller##insp", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool ctrlEnabled = controller->isEnabled();
            if (ImGui::Checkbox("Enabled##ctrl", &ctrlEnabled))
                controller->setEnabled(ctrlEnabled);
            ImGui::SameLine();
            ImGui::TextDisabled("State: %s", controller->getMovementStateName());
            if (controller->isFlyModeActive())
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[FlyMode]");
            }

            if (ImGui::TreeNodeEx("运动参数##ctrl_motion", ImGuiTreeNodeFlags_DefaultOpen))
            {
                float speed = controller->getSpeed();
                if (ImGui::SliderFloat("MoveSpeed##ctrl", &speed, 1.0f, 80.0f))
                    controller->setSpeed(speed);
                float jumpSpeed = controller->getJumpSpeed();
                if (ImGui::SliderFloat("JumpSpeed##ctrl", &jumpSpeed, 1.0f, 30.0f))
                    controller->setJumpSpeed(jumpSpeed);
                float gAccel = controller->getGroundAccel();
                if (ImGui::SliderFloat("GroundAccel##ctrl", &gAccel, 5.0f, 200.0f))
                    controller->setGroundAcceleration(gAccel);
                float aAccel = controller->getAirAccel();
                if (ImGui::SliderFloat("AirAccel##ctrl", &aAccel, 1.0f, 100.0f))
                    controller->setAirAcceleration(aAccel);
                float coyote = controller->getCoyoteTime();
                if (ImGui::SliderFloat("CoyoteTime##ctrl", &coyote, 0.0f, 0.5f, "%.2fs"))
                    controller->setCoyoteTime(coyote);
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("喷气背包##ctrl_jet"))
            {
                bool jetEnabled = controller->isJetpackEnabled();
                if (ImGui::Checkbox("Jetpack Enabled##jet", &jetEnabled))
                    controller->setJetpackEnabled(jetEnabled);
                const float fuelRatio = controller->getJetpackFuelRatio();
                ImGui::ProgressBar(fuelRatio, ImVec2(-1.0f, 0.0f), "Fuel");
                float jetForce = controller->getJetpackForce();
                float fuelMax = controller->getJetpackFuelMax();
                float jAccel = 20.0f; float jRise = 5.5f;
                ImGui::TextDisabled("Force=%.1f  FuelMax=%.2fs", jetForce, fuelMax);
                ImGui::TreePop();
            }
        }
    }

    if (auto* physics = actor->getComponent<engine::component::PhysicsComponent>())
    {
        if (ImGui::CollapsingHeader("Physics##insp", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const glm::vec2 velocity = physics->getVelocity();
            const glm::vec2 physPos  = physics->getPosition();
            ImGui::TextDisabled("Velocity: (%.2f, %.2f)", velocity.x, velocity.y);
            ImGui::TextDisabled("PhysPos:  (%.1f, %.1f)", physPos.x * 32.0f, physPos.y * 32.0f);

            if (ImGui::Button("Velocity=0##phys"))
                physics->setVelocity({0.0f, 0.0f});

            ImGui::SameLine();
            // 一次性施加向上冲量按钮
            if (ImGui::Button("Impulse Up##phys"))
                physics->applyImpulse({0.0f, 8.0f});

            // 自定义冲量输入
            static float s_impulse[2] = {0.0f, 0.0f};
            ImGui::SetNextItemWidth(120.0f);
            ImGui::DragFloat2("##impulse_vec", s_impulse, 0.1f, -50.0f, 50.0f);
            ImGui::SameLine();
            if (ImGui::SmallButton("Apply Impulse"))
                physics->applyImpulse({s_impulse[0], s_impulse[1]});
        }
    }

    if (auto* sprite = actor->getComponent<engine::component::SpriteComponent>())
    {
        if (ImGui::CollapsingHeader("Sprite##insp", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool hidden = sprite->isHidden();
            bool flipped = sprite->isFlipped();
            if (ImGui::Checkbox("Hidden##spr", &hidden))
                sprite->setHidden(hidden);
            ImGui::SameLine();
            if (ImGui::Checkbox("Flipped##spr", &flipped))
                sprite->setFlipped(flipped);

            // 贴图路径
            const std::string texId = sprite->getTextureId();
            ImGui::TextDisabled("Texture: %s", texId.empty() ? "<none>" : texId.c_str());

            // 内联缩略图预览
            if (!texId.empty() && m_glContext)
            {
                const unsigned int texGlId = _context.getResourceManager().getGLTexture(texId);
                const glm::vec2 texSize    = _context.getResourceManager().getTextureSize(texId);
                if (texGlId != 0 && texSize.x > 0.0f && texSize.y > 0.0f)
                {
                    constexpr float kPreviewMaxW = 120.0f;
                    constexpr float kPreviewMaxH = 80.0f;
                    const float ratio = std::min(kPreviewMaxW / texSize.x, kPreviewMaxH / texSize.y);
                    const ImVec2 previewSz(texSize.x * ratio, texSize.y * ratio);
                    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(texGlId)), previewSz);
                    ImGui::SameLine();
                    ImGui::TextDisabled("%.0fx%.0f px", texSize.x, texSize.y);
                }
            }

            ImGui::Button("拖拽资源到此以替换贴图", ImVec2(-1.0f, 0.0f));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
                {
                    const char* path = static_cast<const char*>(payload->Data);
                    if (path && path[0] != '\0')
                    {
                        sprite->setSpriteById(path);
                        appendEditorConsole(EditorConsoleLevel::Log,
                                            "Inspector",
                                            std::string("Sprite 贴图已更新: ") + path);
                        if (isGround)
                            m_groundConfigDirty = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
    }

    if (auto* animation = actor->getComponent<engine::component::AnimationComponent>())
    {
        if (ImGui::CollapsingHeader("Animation##insp", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // 帧尺寸编辑
            float fsize[2] = { animation->getFrameWidth(), animation->getFrameHeight() };
            if (ImGui::DragFloat2("帧尺寸 (px)", fsize, 1.0f, 1.0f, 2048.0f, "%.0f"))
                animation->setFrameSize(std::max(1.0f, fsize[0]), std::max(1.0f, fsize[1]));

            // 当前状态
            const std::string curClip = animation->currentClip();
            if (curClip.empty())
                ImGui::TextDisabled("当前片段: <未播放>");
            else
                ImGui::TextDisabled("当前片段: %s  Frame:%d  t=%.3fs",
                    curClip.c_str(), animation->currentFrame(), animation->currentTimer());

            ImGui::Separator();

            // 片段列表
            const auto& clips = animation->getClips();
            if (clips.empty())
            {
                ImGui::TextDisabled("无已注册片段");
            }
            else
            {
                std::vector<std::string> clipNames;
                clipNames.reserve(clips.size());
                for (const auto& kv : clips) clipNames.push_back(kv.first);
                std::sort(clipNames.begin(), clipNames.end());

                ImGui::TextDisabled("片段列表 (%zu):", clipNames.size());
                if (ImGui::BeginTable("##anim_clips", 6,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp |
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoSavedSettings,
                    ImVec2(0.0f, std::min(static_cast<float>(clips.size()) * 22.0f + 4.0f, 156.0f))))
                {
                    ImGui::TableSetupColumn("片段名",  ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("行",   ImGuiTableColumnFlags_WidthFixed, 28.0f);
                    ImGui::TableSetupColumn("起始列", ImGuiTableColumnFlags_WidthFixed, 36.0f);
                    ImGui::TableSetupColumn("帧数",  ImGuiTableColumnFlags_WidthFixed, 36.0f);
                    ImGui::TableSetupColumn("时长",  ImGuiTableColumnFlags_WidthFixed, 44.0f);
                    ImGui::TableSetupColumn("操作",  ImGuiTableColumnFlags_WidthFixed, 36.0f);
                    ImGui::TableHeadersRow();

                    for (const auto& clipName : clipNames)
                    {
                        const auto& clip = clips.at(clipName);
                        const bool isCurrent = (clipName == curClip);
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);
                        if (isCurrent)
                            ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.55f, 1.0f), "%s", clipName.c_str());
                        else
                            ImGui::TextUnformatted(clipName.c_str());

                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextDisabled("%d", clip.row);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextDisabled("%d", clip.col_start);
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextDisabled("%d", clip.frame_count);
                        ImGui::TableSetColumnIndex(4);
                        ImGui::TextDisabled("%.2fs", clip.frame_duration);
                        ImGui::TableSetColumnIndex(5);
                        const std::string playLabel = std::string("▶##cp_") + clipName;
                        if (ImGui::SmallButton(playLabel.c_str()))
                        {
                            animation->forcePlay(clipName);
                            appendEditorConsole(EditorConsoleLevel::Log, "Inspector",
                                std::string("播放动画片段: ") + clipName);
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("强制播放 %s", clipName.c_str());
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::Button("Animation 更多操作", ImVec2(-1.0f, 0.0f));
            if (ImGui::BeginPopupContextItem("##anim_ctx"))
            {
                if (ImGui::MenuItem("聚焦对象"))
                {
                    if (auto* transform = actor->getComponent<engine::component::TransformComponent>())
                        _context.getCamera().setPosition(transform->getPosition() -
                            _context.getCamera().getViewportSize() * 0.5f /
                            std::max(_context.getCamera().getZoom(), 0.01f));
                }
                ImGui::EndPopup();
            }
        }
    }

    if (auto* parallax = actor->getComponent<engine::component::ParallaxComponent>())
    {
        if (ImGui::CollapsingHeader("Parallax##insp", ImGuiTreeNodeFlags_DefaultOpen))
        {
            glm::vec2 factor = parallax->getScrollFactor();
            float factorArray[2] = {factor.x, factor.y};
            if (ImGui::DragFloat2("Factor##px", factorArray, 0.01f, -4.0f, 4.0f))
                parallax->setScrollFactor({factorArray[0], factorArray[1]});

            glm::bvec2 repeat = parallax->getRepeat();
            bool repeatX = repeat.x;
            bool repeatY = repeat.y;
            const bool changedX = ImGui::Checkbox("Repeat X##px", &repeatX);
            const bool changedY = ImGui::Checkbox("Repeat Y##px", &repeatY);
            if (changedX || changedY)
                parallax->setRepeat({repeatX, repeatY});

            bool hidden = parallax->isHidden();
            if (ImGui::Checkbox("Parallax Hidden##px", &hidden))
                parallax->setHidden(hidden);
            ImGui::TextDisabled("Texture: %s", parallax->getSprite().getTextureId().c_str());
            ImGui::Button("拖拽资源到此以替换背景", ImVec2(-1.0f, 0.0f));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
                {
                    const char* path = static_cast<const char*>(payload->Data);
                    if (path && path[0] != '\0')
                    {
                        parallax->setTexture(path);
                        appendEditorConsole(EditorConsoleLevel::Log,
                                            "Inspector",
                                            std::string("Parallax 贴图已更新: ") + path);
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
    }

    if (ImGui::CollapsingHeader("对象身份##insp", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (actor == m_player)
            ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.55f, 1.0f), "[Player]");
        else if (actor == m_mech)
            ImGui::TextColored(ImVec4(0.50f, 0.80f, 1.0f, 1.0f), "[Mech]");
        else if (actor == m_possessedMonster)
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.35f, 1.0f), "[PossessedMonster]");
        else
            ImGui::TextDisabled("普通对象");

        ImGui::Separator();
        bool actorVisible = actor->isVisible();
        bool actorEnabled = actor->isEnabled();
        if (ImGui::Checkbox("Visible##ident", &actorVisible))
        {
            actor->setVisible(actorVisible);
            appendEditorConsole(EditorConsoleLevel::Log, "Inspector",
                std::string(actorVisible ? "显示对象: " : "隐藏对象: ") + actor->getName());
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Enabled##ident", &actorEnabled))
        {
            actor->setEnabled(actorEnabled);
            appendEditorConsole(EditorConsoleLevel::Log, "Inspector",
                std::string(actorEnabled ? "启用对象: " : "禁用对象: ") + actor->getName());
        }
    }

    ImGui::End();
    popDevEditorTheme();
}

void GameScene::renderMapEditor()
{
    if (!m_showMapEditor || !chunk_manager)
        return;

    pushDevEditorTheme();

    if (m_editorDockspaceId != 0)
        ImGui::SetNextWindowDockID(static_cast<ImGuiID>(m_editorDockspaceId), ImGuiCond_Always);
    ImGui::SetNextWindowPos({16.0f, 140.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({308.0f, 300.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("宇宙编辑器", &m_showMapEditor,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        popDevEditorTheme();
        return;
    }

    drawEditorSectionTitle("笔刷");
    ImGui::TextUnformatted("左键绘制，右键擦除，笔刷范围为圆形。");

    std::string selectedTileLabel = defaultTileTypeName(m_mapEditorPaintTile);
    if (const auto* selectedKind = m_groundTileCatalog.kindForKey(m_mapEditorPaintTileKey))
        selectedTileLabel = selectedKind->displayName + " [" + selectedKind->key + "]";
    else if (const auto* selectedKindByType = m_groundTileCatalog.kindForType(m_mapEditorPaintTile))
    {
        selectedTileLabel = selectedKindByType->displayName + " [" + selectedKindByType->key + "]";
        m_mapEditorPaintTileKey = selectedKindByType->key;
    }

    if (ImGui::BeginCombo("瓦片类型", selectedTileLabel.c_str()))
    {
        if (!m_groundTileCatalog.kinds().empty())
        {
            ImGui::TextDisabled("自定义目录");
            for (const auto& kind : m_groundTileCatalog.kinds())
            {
                const bool selected = (m_mapEditorPaintTileKey == kind.key);
                const std::string label = kind.displayName + " [" + kind.key + "]";
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    m_mapEditorPaintTile = kind.tileType;
                    m_mapEditorPaintTileKey = kind.key;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::Separator();
        }

        ImGui::TextDisabled("原生枚举");
        for (int tileIndex = 0; tileIndex <= static_cast<int>(engine::world::TileType::WallDecor); ++tileIndex)
        {
            const auto type = static_cast<engine::world::TileType>(tileIndex);
            const bool selected = (m_mapEditorPaintTile == type) && (m_groundTileCatalog.kindForType(type) == nullptr);
            if (ImGui::Selectable(defaultTileTypeName(type), selected))
            {
                m_mapEditorPaintTile = type;
                m_mapEditorPaintTileKey.clear();
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("重载地形 catalog"))
    {
        if (m_groundTileCatalog.loadFromFile("assets/ground_tiles/tile_kinds.json"))
        {
            if (const auto* selectedKindByType = m_groundTileCatalog.kindForType(m_mapEditorPaintTile))
                m_mapEditorPaintTileKey = selectedKindByType->key;
            else if (!m_groundTileCatalog.kinds().empty())
            {
                const auto& firstKind = m_groundTileCatalog.kinds().front();
                m_mapEditorPaintTile = firstKind.tileType;
                m_mapEditorPaintTileKey = firstKind.key;
            }
        }
    }
    if (const auto* selectedKind = m_groundTileCatalog.kindForKey(m_mapEditorPaintTileKey))
        ImGui::TextDisabled("当前 key: %s", selectedKind->key.c_str());

    ImGui::SliderInt("笔刷半径", &m_mapEditorBrushRadius, 0, 4, "%d");

    if (m_hasHoveredTile)
    {
        const auto currentType = chunk_manager->tileAt(m_hoveredTile.x, m_hoveredTile.y).type;
        std::string hoveredTileLabel = defaultTileTypeName(currentType);
        if (const auto* hoveredKind = m_groundTileCatalog.kindForType(currentType))
            hoveredTileLabel = hoveredKind->displayName + " [" + hoveredKind->key + "]";
        drawEditorSectionTitle("悬停信息");
        ImGui::Text("悬停格: (%d, %d)", m_hoveredTile.x, m_hoveredTile.y);
        ImGui::Text("当前瓦片: %s", hoveredTileLabel.c_str());
    }

    drawEditorSectionTitle("操作");
    if (ImGui::Button("重建全部脏区块"))
        chunk_manager->rebuildDirtyChunks();

    ImGui::End();
    popDevEditorTheme();
}
} // namespace game::scene
