#include "loading_scene.h"

#include "game_scene.h"
#include "../../engine/core/context.h"
#include "../../engine/render/renderer.h"
#include "../../engine/scene/scene_manager.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace game::scene
{
    LoadingScene::LoadingScene(const std::string& name,
                               engine::core::Context& context,
                               engine::scene::SceneManager& sceneManager,
                               game::route::RouteData routeData)
        : Scene(name, context, sceneManager)
        , m_routeData(std::move(routeData))
    {
    }

    void LoadingScene::init()
    {
        Scene::init();

        SDL_Window* window = _context.getRenderer().getWindow();
        if (window)
        {
            m_glContext = SDL_GL_GetCurrentContext();
            if (m_glContext)
            {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.Fonts->AddFontFromFileTTF(
                    "assets/fonts/VonwaonBitmap-16px.ttf",
                    16.0f,
                    nullptr,
                    io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
                ImGui_ImplSDL3_InitForOpenGL(window, m_glContext);
                ImGui_ImplOpenGL3_Init("#version 330");
            }
        }

        m_planetDataPath = planetDataPathFor(m_routeData.selectedPlanet);
        m_progress = 0.02f;
        m_status = "加载星球数据...";
    }

    void LoadingScene::update(float)
    {
        using json = nlohmann::json;

        if (m_step == LoadStep::ReadPlanetData)
        {
            m_status = "读取星球关卡配置...";
            if (fileExists(m_planetDataPath))
            {
                std::ifstream in(m_planetDataPath);
                json j;
                in >> j;
                m_mapFilePath = j.value("map_file", std::string{"assets/maps/level0.tmj"});
                m_tileCatalogPath = j.value("tile_catalog", std::string{"assets/ground_tiles/tile_kinds.json"});

                if (j.contains("characters") && j["characters"].is_array())
                {
                    for (const auto& item : j["characters"])
                    {
                        if (item.is_string())
                            m_characterFiles.push_back(item.get<std::string>());
                    }
                }
            }
            else
            {
                // 兜底：没有星球文件也可进入游戏
                m_mapFilePath = "assets/maps/level0.tmj";
                m_tileCatalogPath = "assets/ground_tiles/tile_kinds.json";
                m_characterFiles = {"assets/characters/gundom.character.json"};
            }

            m_totalCharacterCount = static_cast<int>(m_characterFiles.size());
            m_validCharacterFiles.clear();
            m_validCharacterCount = 0;
            m_invalidCharacterCount = 0;
            m_characterScanIndex = 0;

            m_progress = 0.25f;
            m_step = LoadStep::ValidateMapFile;
            return;
        }

        if (m_step == LoadStep::ValidateMapFile)
        {
            m_status = "校验地图文件...";
            if (!fileExists(m_mapFilePath))
                m_mapFilePath = "assets/maps/level0.tmj";

            std::ifstream mapIn(m_mapFilePath);
            if (mapIn.is_open())
            {
                json mapJ;
                mapIn >> mapJ;
                m_mapWidth = mapJ.value("width", 0);
                m_mapHeight = mapJ.value("height", 0);
                if (mapJ.contains("layers") && mapJ["layers"].is_array())
                    m_mapLayerCount = static_cast<int>(mapJ["layers"].size());
            }
            m_progress = 0.45f;
            m_step = LoadStep::ValidateTileCatalog;
            return;
        }

        if (m_step == LoadStep::ValidateTileCatalog)
        {
            m_status = "校验瓦片配置...";
            if (!fileExists(m_tileCatalogPath))
                m_tileCatalogPath = "assets/ground_tiles/tile_kinds.json";

            std::ifstream tileIn(m_tileCatalogPath);
            if (tileIn.is_open())
            {
                json tileJ;
                tileIn >> tileJ;
                if (tileJ.contains("kinds") && tileJ["kinds"].is_array())
                    m_tileKindCount = static_cast<int>(tileJ["kinds"].size());
                else if (tileJ.is_array())
                    m_tileKindCount = static_cast<int>(tileJ.size());
            }
            m_progress = 0.62f;
            m_step = LoadStep::LoadCharacterProfiles;
            return;
        }

        if (m_step == LoadStep::LoadCharacterProfiles)
        {
            m_status = "加载角色配置...";

            if (m_characterScanIndex < m_characterFiles.size())
            {
                const std::string& file = m_characterFiles[m_characterScanIndex];
                if (fileExists(file))
                {
                    m_validCharacterFiles.push_back(file);
                    ++m_validCharacterCount;
                }
                else
                {
                    ++m_invalidCharacterCount;
                }
                ++m_characterScanIndex;

                const float characterPhase = m_totalCharacterCount > 0
                    ? static_cast<float>(m_characterScanIndex) / static_cast<float>(m_totalCharacterCount)
                    : 1.0f;
                m_progress = 0.62f + characterPhase * 0.24f;
                return;
            }

            if (m_validCharacterFiles.empty())
                m_validCharacterFiles.push_back("assets/characters/gundom.character.json");

            // 约定：首个角色作为当前关卡玩家角色，写回 config 供 GameScene 初始化使用
            if (!m_validCharacterFiles.empty() && fileExists(m_validCharacterFiles.front()))
            {
                std::ifstream charIn(m_validCharacterFiles.front());
                if (charIn.is_open())
                {
                    json cj;
                    charIn >> cj;
                    const std::string frameJson = cj.value("frame_json", std::string{"assets/textures/Characters/gundom.frame.json"});
                    const std::string smJson = cj.value("state_machine_json", std::string{"assets/textures/Characters/gundom.sm.json"});

                    std::ifstream cfgIn("assets/config.json");
                    if (cfgIn.is_open())
                    {
                        json cfg;
                        cfgIn >> cfg;
                        if (!cfg.contains("gameplay") || !cfg["gameplay"].is_object())
                            cfg["gameplay"] = json::object();
                        cfg["gameplay"]["selected_character_profile_path"] = m_validCharacterFiles.front();
                        cfg["gameplay"]["player_frame_json_path"] = frameJson;
                        cfg["gameplay"]["player_sm_path"] = smJson;

                        if (cj.contains("collision") && cj["collision"].is_object())
                        {
                            const auto& c = cj["collision"];
                            cfg["gameplay"]["player_mech_height_px"] = c.value("mech_height_px", 0.0f);
                            cfg["gameplay"]["player_collision_half_w_px"] = c.value("half_w_px", 12.0f);
                            cfg["gameplay"]["player_collision_half_d_px"] = c.value("half_d_px", 3.5f);
                        }

                        std::ofstream cfgOut("assets/config.json");
                        if (cfgOut.is_open())
                            cfgOut << cfg.dump(4);
                    }
                }
            }

            m_progress = 0.88f;
            m_step = LoadStep::EnterGame;
            return;
        }

        if (m_step == LoadStep::EnterGame)
        {
            m_status = "完成，进入地图...";
            m_progress = 1.0f;
            auto scene = std::make_unique<GameScene>("GameScene", _context, _scene_manager, m_routeData);
            _scene_manager.requestReplaceScene(std::move(scene));
            m_step = LoadStep::Done;
            return;
        }
    }

    void LoadingScene::render()
    {
        if (!m_glContext)
            return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f - 220.0f, io.DisplaySize.y * 0.5f - 70.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({440.0f, 150.0f}, ImGuiCond_Always);
        ImGui::Begin("地图加载", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        ImGui::TextUnformatted("正在加载星球关卡资源...");
        ImGui::TextDisabled("%s", m_status.c_str());
        ImGui::ProgressBar(m_progress, {-1.0f, 24.0f});
        ImGui::TextDisabled("地图: %s", m_mapFilePath.c_str());
        ImGui::TextDisabled("地图尺寸: %d x %d  图层: %d", m_mapWidth, m_mapHeight, m_mapLayerCount);
        ImGui::TextDisabled("瓦片种类: %d", m_tileKindCount);
        ImGui::TextDisabled("角色配置: %d/%d 有效（无效 %d）",
                    m_validCharacterCount,
                    m_totalCharacterCount,
                    m_invalidCharacterCount);

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void LoadingScene::handleInput()
    {
    }

    void LoadingScene::clean()
    {
        if (m_glContext)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            m_glContext = nullptr;
        }
    }

    std::string LoadingScene::planetDataPathFor(game::route::PlanetType type) const
    {
        switch (type)
        {
        case game::route::PlanetType::Verdant: return "assets/planets/verdant.json";
        case game::route::PlanetType::Emberfall: return "assets/planets/emberfall.json";
        case game::route::PlanetType::Frostveil: return "assets/planets/frostveil.json";
        case game::route::PlanetType::Hollowreach: return "assets/planets/hollowreach.json";
        }
        return "assets/planets/verdant.json";
    }

    bool LoadingScene::fileExists(const std::string& path) const
    {
        return std::filesystem::exists(std::filesystem::path(path));
    }
} // namespace game::scene
