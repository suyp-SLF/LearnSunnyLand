#include "game_scene.h"
#include "menu_scene.h"
#include "../../engine/scene/scene_manager.h"
#include "../../engine/object/game_object.h"
#include "../../engine/component/transform_component.h"
#include "../../engine/component/sprite_component.h"
#include "../../engine/component/controller_component.h"
#include "../../engine/component/physics_component.h"
#include "../../engine/core/context.h"
#include "../../engine/render/sprite_render_system.h"
#include "../../engine/render/parallax_render_system.h"
#include "../../engine/render/tilelayer_render_system.h"
#include "../../engine/resource/resource_manager.h"
#include "../../engine/resource/font_manager.h"
#include "../../engine/input/input_manager.h"
#include "../../engine/render/camera.h"
#include "../../engine/world/world_config.h"
#include "../../engine/world/perlin_noise_generator.h"
#include "../../engine/world/chunk_manager.h"
#include "../../engine/render/renderer.h"
#include "../../engine/ecs/components.h"
#include "../locale/locale_manager.h"
#include "../../engine/utils/math.h"
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_set>

namespace game::scene
{
    GameScene::GameScene(const std::string &name,
                         engine::core::Context &context,
                         engine::scene::SceneManager &sceneManager,
                         game::route::RouteData routeData)
        : Scene(name, context, sceneManager)
        , m_routeData(std::move(routeData))
    {
        spdlog::debug("GameScene '{}' 构造完成（路线 {} 步）",
                      name, m_routeData.path.size());
    }

    void GameScene::init()
    {
        Scene::init(); // 设置 _is_initialized = true

        physics_manager = std::make_unique<engine::physics::PhysicsManager>();
        physics_manager->init({0.0f, 10.0f});

        engine::world::WorldConfig config;
        config.loadFromFile("assets/world_config.json");
        config.seed = 12345;
        m_worldConfig = config;  // 保存星球参数

        chunk_manager = std::make_unique<engine::world::ChunkManager>(
            "assets/textures/Tiles/tileset.svg",
            config.TILE_SIZE,
            &_context.getResourceManager(),
            physics_manager.get());

        auto generator = std::make_unique<engine::world::PerlinNoiseGenerator>(config);

        // 绑定生物群系查询：根据世界 X 坐标查询当前路线格子的地形类型
        if (m_routeData.isValid())
        {
            auto rd = m_routeData;  // 捕获副本，生成器生命周期内始终有效
            generator->setBiomeLookup([rd](int tileX) -> int {
                int zone = tileX / game::route::RouteData::TILES_PER_CELL;
                if (zone < 0 || zone >= static_cast<int>(rd.path.size()))
                    return 0;  // 路线外：草原
                auto cell = rd.path[zone];
                return static_cast<int>(rd.terrain[cell.y][cell.x]);
            });
        }

        chunk_manager->setTerrainGenerator(std::move(generator));

        actor_manager = std::make_unique<engine::actor::ActorManager>(_context);
        createPlayer();
        m_monsterManager = std::make_unique<game::monster::MonsterManager>(
            _context,
            *actor_manager,
            *physics_manager,
            *chunk_manager,
            m_player);

        // OpenGL模式下暂时不加载字体（TextRenderer需要SDL_GPUDevice）
        // auto& fontMgr = _context.getResourceManager().getFontManager();
        // fontMgr.setDevice(_context.getRenderer().getDevice());
        // text_renderer = fontMgr.loadFont("assets/fonts/VonwaonBitmap-16px.ttf", 16);

        // 初始化ImGui
        SDL_Window* window = _context.getRenderer().getWindow();
        spdlog::debug("GameScene::init() - window ptr: {}", (void*)window);
        if (window)
        {
            m_glContext = SDL_GL_GetCurrentContext();
            spdlog::debug("GameScene::init() - GL context ptr: {}", (void*)m_glContext);
            if (m_glContext)
            {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();

                // 加载支持中文的字体，解决中文显示为问号的问题
                ImGuiIO &io = ImGui::GetIO();
                io.Fonts->AddFontFromFileTTF(
                    "assets/fonts/VonwaonBitmap-16px.ttf",
                    16.0f, nullptr,
                    io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

                ImGui_ImplSDL3_InitForOpenGL(window, m_glContext);
                ImGui_ImplOpenGL3_Init("#version 330");
                spdlog::info("ImGui initialized with OpenGL3 + 中文字体");
            }
            else
            {
                spdlog::error("Failed to get OpenGL context for ImGui");
            }
        }
        else
        {
            spdlog::error("Failed to get window for ImGui");
        }

        // ECS测试
        for (int i = 0; i < 5; i++)
        {
            auto entity = ecs_registry.create();
            ecs_registry.add<engine::ecs::Position>(entity, glm::vec2(i * 50.0f, 100.0f));
            ecs_registry.add<engine::ecs::Velocity>(entity, glm::vec2(10.0f + i * 5.0f, 0.0f));
        }
        spdlog::info("ECS: 创建了5个测试实体");

        initTestItems();

        // 在初始可见区域生成树木（先 updateVisibleChunks 确保区块已加载）
        chunk_manager->updateVisibleChunks({0.0f, 0.0f}, 3);
        for (int cx = -3; cx <= 3; ++cx)
            for (int cy = -3; cy <= 3; ++cy)
                m_treeManager.generateTreesForChunk(cx, cy, *chunk_manager, m_worldConfig);
        spdlog::info("TreeManager: 初始区域树木生成完毕（min height={}, max height={}, spacing={})",
            m_worldConfig.treeMinTrunkHeight, m_worldConfig.treeMaxTrunkHeight, m_worldConfig.treeSpacing);

        // 在目标区域注入矿脉（需在区块加载后）
        injectOreVeins();
    }
    void GameScene::update(float delta_time)
    {
        Scene::update(delta_time);

        m_mechAttackCooldown = std::max(0.0f, m_mechAttackCooldown - delta_time);
        m_mechAttackFlashTimer = std::max(0.0f, m_mechAttackFlashTimer - delta_time);

        if (m_monsterManager)
            m_monsterManager->update(delta_time);

        // 更新相机
        _context.getCamera().update(delta_time);

        if (physics_manager)
        {
            physics_manager->update(delta_time, 4);
        }

        if (actor_manager)
        {
            actor_manager->update(delta_time);
        }

        syncPlayerPresentation();

        // 每秒输出一次玩家信息
        static float timer = 0.0f;
        timer += delta_time;
        if (timer >= 1.0f && m_player)
        {
            auto* transform = m_player->getComponent<engine::component::TransformComponent>();
            if (transform)
            {
                float height = transform->getPosition().y;
                spdlog::info("Player: {} | Height: {:.1f}", m_player->getName(), height);
            }
            timer = 0.0f;
        }

        // 更新掉落物（重力、拾取）
        {
            glm::vec2 ppos = getActorWorldPosition(getControlledActor());
            m_treeManager.updateDrops(delta_time, ppos, m_inventory, *chunk_manager);
        }

        // 更新天气
        {
            const auto &io = ImGui::GetIO();
            m_weatherSystem.update(delta_time, io.DisplaySize.x, io.DisplaySize.y);
        }

        // 更新星球任务规划 UI
        {
            glm::vec2 ppos = getActorWorldPosition(getControlledActor());
            m_missionUI.update(delta_time, ppos, *chunk_manager);
        }

        // 更新路线区域进度
        if (!m_routeData.path.empty())
        {
            // tile X = worldX / 16 ; zone = tileX / TILES_PER_CELL
            glm::vec2 ppos = getActorWorldPosition(getControlledActor());
            int tileX = static_cast<int>(ppos.x) / 16;
            int zone  = tileX / game::route::RouteData::TILES_PER_CELL;
            zone = std::max(0, std::min(zone, static_cast<int>(m_routeData.path.size()) - 1));
            if (zone != m_currentZone)
            {
                m_currentZone = zone;
                spdlog::info("路线进度: 第 {}/{} 步  ({})",
                    m_currentZone + 1,
                    static_cast<int>(m_routeData.path.size()),
                    game::route::RouteData::cellLabel(m_routeData.path[m_currentZone]));
            }
        }

        glm::vec2 playerPos = getActorWorldPosition(getControlledActor());

        // 只在玩家移动超过半个 chunk 时才更新可见区块
        constexpr float CHUNK_UPDATE_THRESHOLD = engine::world::Chunk::SIZE * 16.0f * 0.5f;
        if (glm::distance(playerPos, m_lastChunkUpdatePos) > CHUNK_UPDATE_THRESHOLD)
        {
            chunk_manager->updateVisibleChunks(playerPos, 3);
            m_lastChunkUpdatePos = playerPos;
        }
    }

    void GameScene::render()
    {
        Scene::render();
        chunk_manager->renderAll(_context);
        _context.getParallaxRenderSystem().renderAll(_context);
        _context.getSpriteRenderSystem().renderAll(_context);
        _context.getTilelayerRenderSystem().renderAll(_context);

        if (actor_manager)
        {
            actor_manager->render();
        }

        if (physics_manager && m_showPhysicsDebug)
        {
            physics_manager->debugDraw(_context.getRenderer(), _context.getCamera());
        }

        if (m_hasHoveredTile)
        {
            glm::vec2 tileWorldPos = chunk_manager->tileToWorld(m_hoveredTile);
            const auto &tileSize = chunk_manager->getTileSize();
            _context.getRenderer().drawRect(_context.getCamera(), tileWorldPos.x, tileWorldPos.y, tileSize.x, tileSize.y,
                                            glm::vec4(1.0f, 0.9f, 0.2f, 0.35f));
        }

        // 渲染UI文字
        // 渲染UI文字
        if (text_renderer && m_player)
        {
            auto* transform = m_player->getComponent<engine::component::TransformComponent>();
            if (transform)
            {
                glm::vec2 pos = transform->getPosition();
                std::string text = "X: " + std::to_string(static_cast<int>(pos.x)) +
                                   " Y: " + std::to_string(static_cast<int>(pos.y));

                auto glyphs = text_renderer->prepareText(text, 10.0f, 10.0f);
                auto& renderer = _context.getRenderer();

                for (const auto& glyph : glyphs)
                {
                    renderer.drawTexture(glyph.texture, glyph.x, glyph.y, glyph.w, glyph.h);
                }
            }
        }

        // ImGui滑块 + 武器显示
        if (m_glContext)
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            float displayW = ImGui::GetIO().DisplaySize.x;
            float displayH = ImGui::GetIO().DisplaySize.y;

            // ── 天气效果（背景层，位于所有ImGui窗口之下）──
            m_weatherSystem.render(displayW, displayH);

            // 相机缩放 + 天气控制
            ImGui::SetNextWindowPos(ImVec2(displayW - 260, 20), ImGuiCond_Always);
            ImGui::Begin("设置", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
            if (ImGui::SliderFloat(locale::T("game.zoom").c_str(), &m_zoomSliderValue, 0.5f, 3.0f))
                _context.getCamera().setZoom(m_zoomSliderValue);

            ImGui::Separator();
            ImGui::TextUnformatted("天气");
            // 当前天气显示
            ImGui::Text("当前: %s", m_weatherSystem.getCurrentWeatherName());

            // 天气切换按钮
            using game::weather::WeatherType;
            const struct { WeatherType type; const char* label; } kWeathers[] = {
                { WeatherType::Clear,        "晴天" },
                { WeatherType::LightRain,    "小雨" },
                { WeatherType::MediumRain,   "中雨" },
                { WeatherType::HeavyRain,    "大雨" },
                { WeatherType::Thunderstorm, "雷雨" },
            };
            for (auto &w : kWeathers)
            {
                bool selected = (m_weatherSystem.getCurrentWeather() == w.type);
                if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
                if (ImGui::Button(w.label))
                    m_weatherSystem.setWeather(w.type, 3.0f);
                if (selected) ImGui::PopStyleColor();
                ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::End();

            // 左侧武器栏（常显示）
            renderWeaponBar();

            // 玩家头顶状态名
            renderPlayerStateTag();

            // 背包界面（E 键开关）
            renderInventoryUI();

            // 掉落物标注（悬浮文字）
            renderDropItems();

            // 左下角路线 HUD
            renderRouteHUD();

            // 撤离结算界面
            renderSettlementUI();
            renderCommandTerminal();
            renderMechPrompt();

            // 星球任务规划 UI
            {
                glm::vec2 ppos = getActorWorldPosition(getControlledActor());
                m_missionUI.render(*chunk_manager, ppos);
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }

    void GameScene::handleInput()
    {
        Scene::handleInput();

        auto &input = _context.getInputManager();

        if (input.isActionPressed("command_mode"))
        {
            m_showCommandInput = !m_showCommandInput;
            m_focusCommandInput = m_showCommandInput;
            if (!m_showCommandInput)
                m_commandBuffer[0] = '\0';
        }

        if (m_showCommandInput)
            return;

        if (actor_manager)
            actor_manager->handleInput();

        glm::vec2 mousePos = input.getLogicalMousePosition();
        glm::vec2 worldPos = _context.getCamera().screenToWorld(mousePos);
        m_hoveredTile = chunk_manager->worldToTile(worldPos);
        m_hasHoveredTile = true;

        // 鼠标左键点击摧毁瓦片（树木会触发倒树逻辑）
        if (input.isActionPressed("attack"))
        {
            if (m_isPlayerInMech)
                performMechAttack();
            else
            {
                using engine::world::TileType;
                TileType t = chunk_manager->tileAt(m_hoveredTile.x, m_hoveredTile.y).type;
                if (t == TileType::Wood || t == TileType::Leaves)
                {
                    m_treeManager.digTile(m_hoveredTile.x, m_hoveredTile.y,
                                          *chunk_manager, m_treeManager.getDrops());
                }
                else if (t == TileType::Ore)
                {
                    chunk_manager->setTile(m_hoveredTile.x, m_hoveredTile.y,
                                           engine::world::TileData(TileType::Air));
                    using Cat = game::inventory::ItemCategory;
                    m_inventory.addItem({"ore", "矿石", 64, Cat::Material}, 1);
                }
                else
                {
                    chunk_manager->setTile(m_hoveredTile.x, m_hoveredTile.y,
                                           engine::world::TileData(TileType::Air));
                }
            }
        }

        // E键切换背包
        if (input.isActionPressed("open_inventory"))
            m_showInventory = !m_showInventory;

        // M键切换星球任务规划
        if (input.isActionPressed("open_map"))
            m_missionUI.showWindow = !m_missionUI.showWindow;

        // B键：在撤离区域触发结算
        if (input.isActionPressed("evacuate"))
        {
            if (m_routeData.isValid())
            {
                int lastZone = static_cast<int>(m_routeData.path.size()) - 1;
                if (m_currentZone >= lastZone)
                    m_showSettlement = true;
                else
                    spdlog::info("B键：还未到达撤离区 (当前区域 {}/{})", m_currentZone + 1, lastZone + 1);
            }
        }
        if (input.isActionPressed("interact"))
        {
            if (m_isPlayerInMech)
                exitMech();
            else
                tryEnterMech();
        }

        // 滚轮切换武器栏
        float wheel = input.getMouseWheelDelta();
        if (wheel > 0.5f)
            m_weaponBar.scroll(-1);   // 向上滚 → 上一个
        else if (wheel < -0.5f)
            m_weaponBar.scroll(1);    // 向下滚 → 下一个
    }

    void GameScene::clean()
    {
        Scene::clean();

        if (m_glContext)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            m_glContext = nullptr;
        }
    }
    void GameScene::createTestObject()
    {
        for (int i = 0; i < 50; i += 32)
        {
            for (int j = 0; j < 50; j += 32)
            {
                auto test_object = std::make_unique<engine::object::GameObject>(_context, "test_object");
                test_object->addComponent<engine::component::TransformComponent>(glm::vec2(i, j));
                test_object->addComponent<engine::component::SpriteComponent>("assets/textures/Props/bubble1.svg", engine::utils::Alignment::CENTER);
                addGameObject(std::move(test_object));
            }
        }
    }

    void GameScene::testCamera()
    {
        auto &camera = engine::core::Context::Current->getCamera();
        auto &input_manager = engine::core::Context::Current->getInputManager();
        if (input_manager.isActionDown("move_up"))
            camera.move(glm::vec2(0, -1));
        if (input_manager.isActionDown("move_down"))
            camera.move(glm::vec2(0, 1));
        if (input_manager.isActionDown("move_left"))
            camera.move(glm::vec2(-1, 0));
        if (input_manager.isActionDown("move_right"))
            camera.move(glm::vec2(1, 0));
    }

    // ------------------------------------------------------------------
    //  矿脉注入（目标区域在路线时，在对应世界列注入 Ore 瓦片）
    // ------------------------------------------------------------------
    void GameScene::injectOreVeins()
    {
        if (!m_routeData.isValid() || m_routeData.objectiveZone < 0) return;

        const int oz = m_routeData.objectiveZone;
        int startTileX = oz * game::route::RouteData::TILES_PER_CELL;
        int endTileX   = startTileX + game::route::RouteData::TILES_PER_CELL;
        int centerX    = (startTileX + endTileX) / 2;

        // 用世界种子生成矿脉位置
        uint64_t rngState = static_cast<uint64_t>(m_worldConfig.seed) ^ 0xBEEF1234CAFE5678ULL;
        auto rng = [&](int lo, int hi) -> int {
            rngState ^= rngState << 13;
            rngState ^= rngState >> 7;
            rngState ^= rngState << 17;
            return lo + static_cast<int>(rngState % static_cast<uint64_t>(hi - lo + 1));
        };

        using engine::world::TileType;
        using engine::world::TileData;

        int seaLevel = m_worldConfig.seaLevel;
        int clusters = rng(4, 7);
        for (int c = 0; c < clusters; ++c)
        {
            int cx = centerX + rng(-30, 30);
            int cy = seaLevel + rng(6, 18);  // 地表以下
            int clusterSize = rng(5, 10);
            for (int i = 0; i < clusterSize; ++i)
            {
                int tx = cx + rng(-3, 3);
                int ty = cy + rng(-2, 2);
                if (chunk_manager->tileAt(tx, ty).type == TileType::Stone)
                    chunk_manager->setTile(tx, ty, TileData(TileType::Ore));
            }
        }
        spdlog::info("矿脉注入完毕：目标区域 zone={}, X=[{},{}), {} 个矿团",
            oz, startTileX, endTileX, clusters);
    }

    // ------------------------------------------------------------------
    //  撤离结算界面
    // ------------------------------------------------------------------
    void GameScene::renderSettlementUI()
    {
        if (!m_showSettlement) return;

        ImGuiIO &io = ImGui::GetIO();
        float dw = io.DisplaySize.x;
        float dh = io.DisplaySize.y;

        // 半透明全屏遮罩
        ImDrawList *bg = ImGui::GetBackgroundDrawList();
        bg->AddRectFilled({0, 0}, {dw, dh}, IM_COL32(0, 0, 0, 160));

        float winW = 400.0f, winH = 360.0f;
        ImGui::SetNextWindowPos({(dw - winW) * 0.5f, (dh - winH) * 0.5f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({winW, winH}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.95f);
        ImGui::Begin("##settlement", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

        // 标题
        ImGui::SetCursorPosX((winW - ImGui::CalcTextSize("任务结算").x) * 0.5f);
        ImGui::TextColored({0.4f, 1.0f, 0.5f, 1.0f}, "任务结算");
        ImGui::Separator();
        ImGui::Spacing();

        // 路线信息
        if (m_routeData.isValid())
        {
            ImGui::TextColored({0.6f, 0.8f, 1.0f, 1.0f}, "路线概况");
            ImGui::Text("  出发点: %s", game::route::RouteData::cellLabel(m_routeData.startCell()).c_str());
            ImGui::Text("  撤离点: %s", game::route::RouteData::cellLabel(m_routeData.evacCell()).c_str());
            ImGui::Text("  路线步数: %d 格", static_cast<int>(m_routeData.path.size()));
            bool visitedObjective = (m_routeData.objectiveZone >= 0);
            ImGui::Text("  目标矿区: %s",
                visitedObjective ? "已经过 [矿山]" : "未经过（路线绕开了目标格）");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored({0.6f, 0.8f, 1.0f, 1.0f}, "收集物品");
        ImGui::Spacing();

        // 显示背包中各种材料数量
        const struct { const char* id; const char* name; } kItems[] = {
            {"ore",    "矿石"},
            {"wood",   "木材"},
            {"stone",  "石块"},
            {"leaves", "树叶"},
        };
        bool anyItem = false;
        for (auto &it : kItems)
        {
            int cnt = m_inventory.countItem(it.id);
            if (cnt > 0)
            {
                ImGui::Text("  %s  x%d", it.name, cnt);
                anyItem = true;
            }
        }
        if (!anyItem)
            ImGui::TextDisabled("  （背包为空）");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 返回主菜单按钮
        float btnW = winW - 32.0f;
        ImGui::SetCursorPosX(16.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        {0.12f, 0.50f, 0.15f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.20f, 0.70f, 0.22f, 1.0f});
        if (ImGui::Button("返回主菜单", {btnW, 40.0f}))
        {
            auto ms = std::make_unique<game::scene::MenuScene>("MenuScene", _context, _scene_manager);
            _scene_manager.requestReplaceScene(std::move(ms));
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing();
        ImGui::SetCursorPosX(16.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, {0.25f, 0.25f, 0.35f, 1.0f});
        if (ImGui::Button("继续游戏", {btnW, 30.0f}))
            m_showSettlement = false;
        ImGui::PopStyleColor();

        ImGui::End();
    }

    // ------------------------------------------------------------------
    //  玩家朝向 / 动画资源同步
    // ------------------------------------------------------------------
    void GameScene::syncPlayerPresentation()
    {
        if (!m_player) return;

        auto* controller = m_player->getComponent<engine::component::ControllerComponent>();
        auto* sprite = m_player->getComponent<engine::component::SpriteComponent>();
        if (!controller || !sprite) return;

        bool shouldFlip = controller->getFacingDirection() == engine::component::ControllerComponent::FacingDirection::Left;
        if (sprite->isFlipped() != shouldFlip)
            sprite->setFlipped(shouldFlip);

        constexpr float FRAME_W = 32.0f;
        constexpr float FRAME_H = 48.0f;
        const std::string animKey = controller->getAnimationStateKey();

        if (m_playerAnimationTexture != "assets/textures/Characters/player_sheet.svg")
        {
            sprite->setSpriteById("assets/textures/Characters/player_sheet.svg");
            m_playerAnimationTexture = "assets/textures/Characters/player_sheet.svg";
        }

        if (animKey != m_playerAnimationState)
        {
            m_playerAnimationState = animKey;
            m_playerAnimationTimer = 0.0f;
            m_playerAnimationFrame = 0;
        }

        int frameStart = 0;
        int frameCount = 1;
        float frameDuration = 0.12f;

        if (animKey == "idle")
        {
            frameStart = 0;
            frameCount = 2;
            frameDuration = 0.38f;
        }
        else if (animKey == "run")
        {
            frameStart = 2;
            frameCount = 4;
            frameDuration = 0.09f;
        }
        else if (animKey == "jump")
        {
            frameStart = 6;
            frameCount = 1;
        }
        else if (animKey == "fall")
        {
            frameStart = 7;
            frameCount = 1;
        }
        else if (animKey == "jetpack")
        {
            frameStart = 8;
            frameCount = 2;
            frameDuration = 0.08f;
        }

        if (frameCount > 1)
        {
            m_playerAnimationTimer += ImGui::GetIO().DeltaTime;
            while (m_playerAnimationTimer >= frameDuration)
            {
                m_playerAnimationTimer -= frameDuration;
                m_playerAnimationFrame = (m_playerAnimationFrame + 1) % frameCount;
            }
        }
        else
        {
            m_playerAnimationFrame = 0;
        }

        const float frameX = (frameStart + m_playerAnimationFrame) * FRAME_W;
        sprite->setSourceRect(engine::utils::FRect{{frameX, 0.0f}, {FRAME_W, FRAME_H}});

        if (m_mech)
        {
            auto* mechController = m_mech->getComponent<engine::component::ControllerComponent>();
            auto* mechSprite = m_mech->getComponent<engine::component::SpriteComponent>();
            if (mechController && mechSprite)
            {
                bool mechFlip = mechController->getFacingDirection() == engine::component::ControllerComponent::FacingDirection::Left;
                if (mechSprite->isFlipped() != mechFlip)
                    mechSprite->setFlipped(mechFlip);
            }
        }
    }

    void GameScene::renderCommandTerminal()
    {
        if (!m_showCommandInput)
            return;

        ImGuiIO &io = ImGui::GetIO();
        const float width = 420.0f;
        ImGui::SetNextWindowPos({(io.DisplaySize.x - width) * 0.5f, 40.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({width, 118.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.94f);
        ImGui::Begin("指令终端", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("输入 001 呼叫空投机甲。再次按 R 可退出指令模式。") ;
        ImGui::Spacing();
        if (m_focusCommandInput)
        {
            ImGui::SetKeyboardFocusHere();
            m_focusCommandInput = false;
        }

        bool submitted = ImGui::InputText("##command", m_commandBuffer.data(), m_commandBuffer.size(),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("执行") || submitted)
            executeCommand();

        ImGui::TextDisabled("当前可用指令: 001") ;
        ImGui::End();
    }

    void GameScene::renderMechPrompt()
    {
        if (!m_mech)
            return;

        if (m_isPlayerInMech)
        {
            ImGuiIO &io = ImGui::GetIO();
            const float width = 280.0f;
            ImGui::SetNextWindowPos({(io.DisplaySize.x - width) * 0.5f, io.DisplaySize.y - 110.0f}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({width, 78.0f}, ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.84f);
            ImGui::Begin("##mechhud", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted("F 下机   鼠标左键 震荡重击");
            if (m_mechAttackCooldown > 0.0f)
                ImGui::Text("重击冷却: %.2fs", m_mechAttackCooldown);
            else
                ImGui::TextColored({0.55f, 1.0f, 0.7f, 1.0f}, "重击就绪");
            if (m_mechLastAttackHits > 0)
                ImGui::Text("上次重击命中: %d", m_mechLastAttackHits);
            else
                ImGui::TextDisabled("上次重击未命中目标");
            ImGui::End();
            return;
        }

        if (!m_player)
            return;

        glm::vec2 playerPos = getActorWorldPosition(m_player);
        glm::vec2 mechPos = getActorWorldPosition(m_mech);
        if (glm::distance(playerPos, mechPos) > 220.0f)
            return;

        glm::vec2 screen = _context.getCamera().worldToScreen({mechPos.x, mechPos.y - 220.0f});
        const char* text = "按 F 进入机甲";
        ImDrawList *dl = ImGui::GetForegroundDrawList();
        ImVec2 textSize = ImGui::CalcTextSize(text);
        ImVec2 min{screen.x - textSize.x * 0.5f - 10.0f, screen.y - 6.0f};
        ImVec2 max{screen.x + textSize.x * 0.5f + 10.0f, screen.y + textSize.y + 6.0f};
        dl->AddRectFilled(min, max, IM_COL32(9, 17, 26, 220), 8.0f);
        dl->AddRect(min, max, IM_COL32(120, 220, 255, 220), 8.0f, 0, 1.2f);
        dl->AddText({screen.x - textSize.x * 0.5f, screen.y}, IM_COL32(240, 248, 255, 255), text);
    }

    // ------------------------------------------------------------------
    //  玩家状态标签（显示在角色头顶）
    // ------------------------------------------------------------------
    void GameScene::renderPlayerStateTag()
    {
        auto* actor = getControlledActor();
        if (!actor) return;

        auto* transform = actor->getComponent<engine::component::TransformComponent>();
        auto* controller = actor->getComponent<engine::component::ControllerComponent>();
        if (!transform || !controller) return;

        glm::vec2 screen = _context.getCamera().worldToScreen(transform->getPosition());
        std::string stateLabel = controller->getMovementStateName();
        if (m_isPlayerInMech)
            stateLabel = "机甲·" + stateLabel;
        float fuelRatio = controller->getJetpackFuelRatio();

        ImDrawList *dl = ImGui::GetForegroundDrawList();
        ImVec2 textSize = ImGui::CalcTextSize(stateLabel.c_str());
        ImVec2 textPos{screen.x - textSize.x * 0.5f, screen.y - 58.0f};
        ImVec2 boxMin{textPos.x - 8.0f, textPos.y - 4.0f};
        ImVec2 boxMax{textPos.x + textSize.x + 8.0f, textPos.y + textSize.y + 14.0f};

        dl->AddRectFilled(boxMin, boxMax, IM_COL32(12, 18, 30, 210), 6.0f);
        dl->AddRect(boxMin, boxMax, IM_COL32(100, 180, 255, 220), 6.0f, 0, 1.2f);
        dl->AddText(textPos, IM_COL32(240, 245, 255, 255), stateLabel.c_str());

        ImVec2 fuelBarMin{boxMin.x + 6.0f, boxMax.y - 8.0f};
        ImVec2 fuelBarMax{boxMax.x - 6.0f, boxMax.y - 3.0f};
        float fuelFillX = fuelBarMin.x + (fuelBarMax.x - fuelBarMin.x) * fuelRatio;
        dl->AddRectFilled(fuelBarMin, fuelBarMax, IM_COL32(30, 36, 46, 230), 2.0f);
        dl->AddRectFilled(fuelBarMin, {fuelFillX, fuelBarMax.y}, IM_COL32(255, 190, 60, 230), 2.0f);
    }

    // ------------------------------------------------------------------
    //  路线 HUD（左下角，显示 20×20 路线进度）
    // ------------------------------------------------------------------
    void GameScene::renderRouteHUD()
    {
        if (!m_routeData.isValid()) return;

        using namespace game::route;
        using R = RouteData;

        const float CELL_PX  = 6.0f;
        const float CELL_TOT = CELL_PX + 1.0f;      // 7px/格
        const float MAP_W    = R::MAP_SIZE * CELL_TOT;
        const float MAP_H    = R::MAP_SIZE * CELL_TOT;
        const float WIN_W    = MAP_W + 14.0f;
        const float WIN_H    = MAP_H + 46.0f;

        float dh = ImGui::GetIO().DisplaySize.y;
        ImGui::SetNextWindowPos({10.0f, dh - WIN_H - 10.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({WIN_W, WIN_H}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.82f);
        ImGui::Begin("##routehud", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoNav         | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        // 标题
        ImGui::TextColored({0.5f, 0.9f, 1.0f, 1.0f}, "路线进度");
        ImGui::SameLine(WIN_W - 40.0f);
        ImGui::TextDisabled("%d步", static_cast<int>(m_routeData.path.size()));

        ImDrawList *dl    = ImGui::GetWindowDrawList();
        ImVec2      origin = ImGui::GetCursorScreenPos();
        origin.x += 4.0f;

        int psize = static_cast<int>(m_routeData.path.size());

        // 绘制微型 20×20 地图
        for (int ry = 0; ry < R::MAP_SIZE; ++ry)
        {
            for (int cx = 0; cx < R::MAP_SIZE; ++cx)
            {
                float px = origin.x + cx * CELL_TOT;
                float py = origin.y + ry * CELL_TOT;
                ImVec2 p0{px, py}, p1{px + CELL_PX, py + CELL_PX};

                glm::ivec2 cell{cx, ry};
                int pidx = -1;
                for (int i = 0; i < psize; ++i)
                    if (m_routeData.path[i] == cell) { pidx = i; break; }

                bool isObjective = (cell == m_routeData.objectiveCell);

                ImU32 fill;
                if (pidx == 0)
                    fill = IM_COL32(40, 200, 80, 255);   // 出发
                else if (pidx == psize - 1)
                    fill = IM_COL32(220, 70, 70, 255);   // 撤离
                else if (pidx == m_currentZone)
                    fill = IM_COL32(255, 220, 50, 255);  // 当前区域
                else if (pidx >= 0)
                {
                    auto tc = game::route::RouteData::terrainColor(m_routeData.terrain[ry][cx]);
                    fill = IM_COL32(tc.r/3*2, tc.g/3*2+30, tc.b/3*2+60, 210);  // 路线（地形色调）
                }
                else
                {
                    auto tc = game::route::RouteData::terrainColor(m_routeData.terrain[ry][cx]);
                    fill = IM_COL32(tc.r/3, tc.g/3, tc.b/3, 160);  // 空格（暗淡地形）
                }

                dl->AddRectFilled(p0, p1, fill, 1.0f);

                // 目标格金色边框
                if (isObjective)
                    dl->AddRect(p0, p1, IM_COL32(255, 210, 50, 220), 1.0f, 0, 1.5f);

                // 当前区域加白色边框
                else if (pidx == m_currentZone)
                    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 220), 1.0f, 0, 1.5f);
            }
        }

        // 路线连线
        for (int i = 0; i + 1 < psize; ++i)
        {
            const auto &a = m_routeData.path[i];
            const auto &b = m_routeData.path[i + 1];
            ImVec2 pa{origin.x + a.x * CELL_TOT + CELL_PX * 0.5f,
                      origin.y + a.y * CELL_TOT + CELL_PX * 0.5f};
            ImVec2 pb{origin.x + b.x * CELL_TOT + CELL_PX * 0.5f,
                      origin.y + b.y * CELL_TOT + CELL_PX * 0.5f};
            ImU32 lcol = (i < m_currentZone)
                ? IM_COL32(100, 180, 120, 120)  // 已过
                : IM_COL32(255, 230, 80, 160);  // 未过
            dl->AddLine(pa, pb, lcol, 1.5f);
        }

        // 占位，让光标移到文字行
        ImGui::Dummy({MAP_W, MAP_H});

        // 当前位置文字
        if (m_currentZone < psize)
        {
            const auto &curCell = m_routeData.path[m_currentZone];
            bool isLast = (m_currentZone == psize - 1);
            ImGui::TextColored(
                isLast ? ImVec4{0.3f, 1.0f, 0.4f, 1.0f} : ImVec4{1.0f, 0.9f, 0.4f, 1.0f},
                "%s第%d/%d步 %s%s",
                isLast ? "[" : "",
                m_currentZone + 1,
                psize,
                RouteData::cellLabel(curCell).c_str(),
                isLast ? "] 撤离点" : "");
        }

        ImGui::End();
    }

    // ------------------------------------------------------------------
    //  掉落物渲染（屏幕空间悬浮标签）
    // ------------------------------------------------------------------
    void GameScene::renderDropItems()
    {
        const auto &camera = _context.getCamera();
        const auto &drops  = m_treeManager.getDrops();
        if (drops.empty()) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2.0f, 2.0f});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.0f, 0.0f, 0.0f, 0.55f});

        for (size_t i = 0; i < drops.size(); ++i)
        {
            const auto &drop = drops[i];
            // 世界坐标 → 屏幕坐标
            glm::vec2 screen = camera.worldToScreen(drop.worldPos);

            // 上下轻微飘动
            float bx = screen.x - 16.0f;
            float by = screen.y - 20.0f + std::sin(drop.bobTimer) * 3.0f;

            char wid[32];
            snprintf(wid, sizeof(wid), "##drop%zu", i);
            ImGui::SetNextWindowPos({bx, by}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({40.0f, 20.0f}, ImGuiCond_Always);
            ImGui::Begin(wid, nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

            // 根据物品类型选颜色
            ImVec4 col = (drop.item.id == "wood")
                ? ImVec4(0.85f, 0.65f, 0.30f, 1.0f)
                : ImVec4(0.40f, 0.90f, 0.40f, 1.0f);
            ImGui::TextColored(col, "%s x%d", drop.item.name.c_str(), drop.count);
            ImGui::End();
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    // ------------------------------------------------------------------
    //  初始化测试物品
    // ------------------------------------------------------------------
    void GameScene::initTestItems()
    {
        using namespace game::inventory;
        using Cat = ItemCategory;

        // 武器类物品（可拖入武器栏）
        m_inventory.addItem({"dig_gun",          "挖掘枪",     1, Cat::Weapon}, 1);
        m_inventory.addItem({"iron_sword",        "铁剑",       1, Cat::Weapon}, 1);
        m_inventory.addItem({"knife",             "匕首",       1, Cat::Weapon}, 1);
        m_inventory.addItem({"war_axe",           "战斧",       1, Cat::Weapon}, 1);
        m_inventory.addItem({"pistol",            "手枪",       1, Cat::Weapon}, 1);
        m_inventory.addItem({"shotgun",           "霰弹枪",     1, Cat::Weapon}, 1);
        m_inventory.addItem({"sniper",            "狙击枪",     1, Cat::Weapon}, 1);
        m_inventory.addItem({"rocket_launcher",   "火箭筒",     1, Cat::Weapon}, 1);
        m_inventory.addItem({"grenade_launcher",  "榴弹发射器", 1, Cat::Weapon}, 1);
        m_inventory.addItem({"laser_gun",         "激光枪",     1, Cat::Weapon}, 1);
        m_inventory.addItem({"flamethrower",      "火焰喷射器", 1, Cat::Weapon}, 1);

        // 普通物品
        m_inventory.addItem({"gold_coin", "金币",   99, Cat::Misc},     42);
        m_inventory.addItem({"apple",     "苹果",   20, Cat::Consumable}, 5);
        m_inventory.addItem({"wood",      "木材",   64, Cat::Material},  32);
        m_inventory.addItem({"stone",     "石头",   64, Cat::Material},  24);
    }

    // ------------------------------------------------------------------
    //  左侧武器栏（常驻显示）
    // ------------------------------------------------------------------
    void GameScene::renderWeaponBar()
    {
        constexpr float SLOT_H  = 58.0f;
        constexpr float SLOT_W  = 70.0f;
        constexpr float GAP     = 3.0f;
        constexpr int   SLOTS   = game::weapon::WeaponBar::SLOTS;

        const float WIN_H = SLOTS * SLOT_H + (SLOTS - 1) * GAP + 46.0f;
        ImGui::SetNextWindowPos({10.0f, 180.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({SLOT_W + 16.0f, WIN_H}, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {GAP, GAP});
        ImGui::Begin(locale::T("weapon_bar.title").c_str(), nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        for (int i = 0; i < SLOTS; ++i)
        {
            const auto &slot = m_weaponBar.getSlot(i);
            bool is_active   = (i == m_weaponBar.getActiveIndex());

            // 激活槽用黄色高亮边框
            if (is_active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,        {0.55f, 0.45f, 0.05f, 1.0f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.70f, 0.58f, 0.10f, 1.0f});
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button,        {0.18f, 0.18f, 0.28f, 1.0f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.30f, 0.30f, 0.50f, 1.0f});
            }

            char label[64];
            if (slot.isEmpty())
                snprintf(label, sizeof(label), "%d##wb%d", i + 1, i);
            else
            {
                const auto *def = game::weapon::getWeaponDef(slot.item->id);
                snprintf(label, sizeof(label), "%s\n%s##wb%d",
                    def ? def->icon_label.c_str() : "?",
                    slot.item->name.c_str(), i);
            }

            ImGui::Button(label, {SLOT_W, SLOT_H});

            // 武器栏格子作为拖放目标（接受背包物品）
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("INV_SLOT"))
                {
                    int src_idx = *static_cast<const int *>(payload->Data);
                    m_weaponBar.equipFromInventory(i, src_idx, m_inventory);
                }
                ImGui::EndDragDropTarget();
            }

            // 双击卸装回背包
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                m_weaponBar.unequipToInventory(i, m_inventory);

            // Tooltip
            if (!slot.isEmpty() && ImGui::IsItemHovered())
            {
                const auto *def = game::weapon::getWeaponDef(slot.item->id);
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(slot.item->name.c_str());
                if (def)
                {
                    ImGui::Separator();
                    ImGui::TextDisabled("%s: %d", locale::T("weapon.damage").c_str(), def->damage);
                    ImGui::TextDisabled("%s: %.1f/s", locale::T("weapon.speed").c_str(), def->attack_speed);
                    if (def->ammo_capacity > 0)
                        ImGui::TextDisabled("%s: %d", locale::T("weapon.ammo").c_str(), def->ammo_capacity);
                }
                ImGui::TextDisabled("%s", locale::T("weapon_bar.dblclick_unequip").c_str());
                ImGui::EndTooltip();
            }

            ImGui::PopStyleColor(2);
        }

        // 底部提示
        ImGui::Separator();
        ImGui::TextDisabled("%s", locale::T("weapon_bar.scroll_hint").c_str());

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void GameScene::renderInventoryUI()
    {
        if (!m_showInventory) return;

        constexpr int   COLS  = game::inventory::Inventory::COLS;
        constexpr int   ROWS  = game::inventory::Inventory::ROWS;
        constexpr float SLOT  = 54.0f;
        constexpr float GAP   = 4.0f;

        const float WIN_W = COLS * SLOT + (COLS - 1) * GAP + 16.0f;
        const float WIN_H = ROWS * SLOT + (ROWS - 1) * GAP + 50.0f;

        ImVec2 disp = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({(disp.x - WIN_W) * 0.5f, (disp.y - WIN_H) * 0.5f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({WIN_W, WIN_H}, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  {GAP, GAP});
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2.0f, 2.0f});

        ImGui::Begin(locale::T("inventory.title").c_str(), &m_showInventory,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);

        for (int row = 0; row < ROWS; ++row)
        {
            for (int col = 0; col < COLS; ++col)
            {
                if (col > 0) ImGui::SameLine();

                int idx = row * COLS + col;
                const auto &slot = m_inventory.getSlot(idx);

                // 武器物品用稍暖的底色
                bool is_weapon = !slot.isEmpty() &&
                    slot.item->category == game::inventory::ItemCategory::Weapon;

                if (is_weapon)
                    ImGui::PushStyleColor(ImGuiCol_Button, {0.30f, 0.18f, 0.10f, 1.0f});
                else
                    ImGui::PushStyleColor(ImGuiCol_Button, {0.18f, 0.18f, 0.28f, 1.0f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.38f, 0.35f, 0.55f, 1.0f});

                char label[48];
                if (slot.isEmpty())
                    snprintf(label, sizeof(label), "##s%d", idx);
                else if (is_weapon)
                {
                    const auto *def = game::weapon::getWeaponDef(slot.item->id);
                    snprintf(label, sizeof(label), "%s##s%d",
                        def ? def->icon_label.c_str() : "[W]", idx);
                }
                else
                    snprintf(label, sizeof(label), "x%d##s%d", slot.count, idx);

                ImGui::Button(label, {SLOT, SLOT});

                // 拖放源：只要格子不空就可拖
                if (!slot.isEmpty() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    ImGui::SetDragDropPayload("INV_SLOT", &idx, sizeof(int));
                    ImGui::TextUnformatted(slot.item->name.c_str());
                    if (is_weapon)
                        ImGui::TextDisabled("%s", locale::T("weapon_bar.drag_hint").c_str());
                    ImGui::EndDragDropSource();
                }

                // Tooltip
                if (!slot.isEmpty() && ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(slot.item->name.c_str());
                    if (is_weapon)
                    {
                        const auto *def = game::weapon::getWeaponDef(slot.item->id);
                        if (def)
                        {
                            ImGui::Separator();
                            ImGui::TextDisabled("%s: %d", locale::T("weapon.damage").c_str(), def->damage);
                            ImGui::TextDisabled("%s: %.1f/s", locale::T("weapon.speed").c_str(), def->attack_speed);
                        }
                        ImGui::TextDisabled("%s", locale::T("weapon_bar.drag_hint").c_str());
                    }
                    else
                        ImGui::TextDisabled("%s: %d / %d",
                            locale::T("inventory.quantity").c_str(), slot.count, slot.item->max_stack);
                    ImGui::EndTooltip();
                }

                ImGui::PopStyleColor(2);
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void GameScene::createPlayer()
    {
        m_player = actor_manager->createActor("player");

        // 玩家初始位置（从天空下落）
        glm::vec2 startPos = {0.0f, 0.0f};

        auto* transform = m_player->addComponent<engine::component::TransformComponent>(startPos);
        m_player->addComponent<engine::component::SpriteComponent>(
            "assets/textures/Characters/player_sheet.svg",
            engine::utils::Alignment::CENTER,
            engine::utils::FRect{{0.0f, 0.0f}, {32.0f, 48.0f}});
        m_player->addComponent<engine::component::ControllerComponent>(25.0f, 30.0f);

        // 创建物理体并添加 PhysicsComponent (32x48 像素 = 1x1.5 米)
        b2BodyId bodyId = physics_manager->createDynamicBody({startPos.x, startPos.y}, {0.5f, 0.75f}, m_player);
        m_player->addComponent<engine::component::PhysicsComponent>(bodyId, physics_manager.get());

        // 相机跟随玩家
        _context.getCamera().setFollowTarget(&transform->getPosition(), 5.0f);
    }

    void GameScene::executeCommand()
    {
        std::string command = m_commandBuffer.data();
        command.erase(std::remove_if(command.begin(), command.end(), [](unsigned char c) {
            return std::isspace(c) != 0;
        }), command.end());

        if (command == "001")
        {
            spawnMechDrop();
            m_showCommandInput = false;
            m_commandBuffer[0] = '\0';
            return;
        }

        spdlog::warn("未知指令: {}", command.empty() ? "<empty>" : command);
    }

    void GameScene::spawnMechDrop()
    {
        if (m_isPlayerInMech)
        {
            spdlog::info("机甲已连接驾驶员，忽略重复空投指令");
            return;
        }

        constexpr float kPixelsPerMeter = 32.0f;
        glm::vec2 playerPos = getActorWorldPosition(m_player);
        glm::vec2 dropTarget = {playerPos.x + 180.0f, playerPos.y - 32.0f};
        glm::vec2 spawnPos = {dropTarget.x, dropTarget.y - 640.0f};

        if (!m_mech)
        {
            m_mech = actor_manager->createActor("mech_drop");
            auto* mechTransform = m_mech->addComponent<engine::component::TransformComponent>(spawnPos, glm::vec2{5.0f, 5.0f});
            mechTransform->setScale({5.0f, 5.0f});
            m_mech->addComponent<engine::component::SpriteComponent>(
                "assets/textures/Characters/mech.svg",
                engine::utils::Alignment::CENTER);
            auto* mechController = m_mech->addComponent<engine::component::ControllerComponent>(18.0f, 8.0f);
            mechController->setGroundAcceleration(58.0f);
            mechController->setAirAcceleration(28.0f);
            mechController->setJumpSpeed(10.5f);
            mechController->setJumpCutFactor(0.78f);
            mechController->setCoyoteTime(0.16f);
            mechController->setGroundedThreshold(0.18f);
            mechController->setJetpackEnabled(false);
            mechController->setJetpackProfile(0.0f, 0.0f, 0.0f, 0.0f);
            mechController->setEnabled(false);
            b2BodyId bodyId = physics_manager->createDynamicBody(
                {spawnPos.x / kPixelsPerMeter, spawnPos.y / kPixelsPerMeter},
                {2.4f, 3.4f},
                m_mech);
            m_mech->addComponent<engine::component::PhysicsComponent>(bodyId, physics_manager.get());
        }
        else
        {
            if (auto* transform = m_mech->getComponent<engine::component::TransformComponent>())
                transform->setPosition(spawnPos);
            if (auto* sprite = m_mech->getComponent<engine::component::SpriteComponent>())
                sprite->setHidden(false);
            if (auto* physics = m_mech->getComponent<engine::component::PhysicsComponent>())
            {
                physics->setWorldPosition(spawnPos);
                physics->setVelocity({0.0f, 4.0f});
            }
            if (auto* controller = m_mech->getComponent<engine::component::ControllerComponent>())
            {
                controller->setGroundAcceleration(58.0f);
                controller->setAirAcceleration(28.0f);
                controller->setJumpSpeed(10.5f);
                controller->setJumpCutFactor(0.78f);
                controller->setCoyoteTime(0.16f);
                controller->setGroundedThreshold(0.18f);
                controller->setJetpackEnabled(false);
                controller->setEnabled(false);
            }
        }

        m_mechAttackCooldown = 0.0f;
        m_mechLastAttackHits = 0;

        spdlog::info("指令 001 已执行：机甲开始空投，目标坐标 ({:.1f}, {:.1f})", dropTarget.x, dropTarget.y);
    }

    void GameScene::tryEnterMech()
    {
        if (!m_mech || m_isPlayerInMech || !m_player)
            return;

        glm::vec2 playerPos = getActorWorldPosition(m_player);
        glm::vec2 mechPos = getActorWorldPosition(m_mech);
        if (glm::distance(playerPos, mechPos) > 220.0f)
            return;

        auto* mechController = m_mech->getComponent<engine::component::ControllerComponent>();
        auto* mechTransform = m_mech->getComponent<engine::component::TransformComponent>();
        auto* playerController = m_player->getComponent<engine::component::ControllerComponent>();
        auto* playerPhysics = m_player->getComponent<engine::component::PhysicsComponent>();
        auto* playerTransform = m_player->getComponent<engine::component::TransformComponent>();
        auto* playerSprite = m_player->getComponent<engine::component::SpriteComponent>();
        if (!mechController || !mechTransform || !playerController || !playerPhysics || !playerTransform || !playerSprite)
            return;

        playerController->setEnabled(false);
        playerPhysics->setVelocity({0.0f, 0.0f});
        playerPhysics->setWorldPosition({-4096.0f, -4096.0f});
        playerTransform->setPosition({-4096.0f, -4096.0f});
        playerSprite->setHidden(true);

        mechController->setEnabled(true);
        _context.getCamera().setFollowTarget(&mechTransform->getPosition(), 4.2f);
        m_isPlayerInMech = true;
        spdlog::info("驾驶员已进入机甲");
    }

    void GameScene::exitMech()
    {
        if (!m_isPlayerInMech || !m_mech || !m_player)
            return;

        auto* mechController = m_mech->getComponent<engine::component::ControllerComponent>();
        auto* playerController = m_player->getComponent<engine::component::ControllerComponent>();
        auto* playerPhysics = m_player->getComponent<engine::component::PhysicsComponent>();
        auto* playerTransform = m_player->getComponent<engine::component::TransformComponent>();
        auto* playerSprite = m_player->getComponent<engine::component::SpriteComponent>();
        if (!mechController || !playerController || !playerPhysics || !playerTransform || !playerSprite)
            return;

        glm::vec2 exitPos = findSafeDisembarkPosition();
        mechController->setEnabled(false);
        playerSprite->setHidden(false);
        playerTransform->setPosition(exitPos);
        playerPhysics->setWorldPosition(exitPos);
        playerPhysics->setVelocity({0.0f, 0.0f});
        playerController->setEnabled(true);
        _context.getCamera().setFollowTarget(&playerTransform->getPosition(), 5.0f);
        m_isPlayerInMech = false;
        spdlog::info("驾驶员已离开机甲");
    }

    void GameScene::performMechAttack()
    {
        if (!m_isPlayerInMech || !m_mech || m_mechAttackCooldown > 0.0f)
            return;

        auto* mechController = m_mech->getComponent<engine::component::ControllerComponent>();
        auto* mechTransform = m_mech->getComponent<engine::component::TransformComponent>();
        auto* mechPhysics = m_mech->getComponent<engine::component::PhysicsComponent>();
        if (!mechController || !mechTransform || !mechPhysics)
            return;

        const float facing = mechController->getFacingDirection() == engine::component::ControllerComponent::FacingDirection::Left ? -1.0f : 1.0f;
        glm::vec2 center = mechTransform->getPosition() + glm::vec2{facing * 128.0f, 28.0f};
        constexpr float radius = 92.0f;
        int destroyedTiles = 0;

        using engine::world::TileData;
        using engine::world::TileType;
        const glm::ivec2 tileMin = chunk_manager->worldToTile(center - glm::vec2{radius, radius});
        const glm::ivec2 tileMax = chunk_manager->worldToTile(center + glm::vec2{radius, radius});
        std::unordered_set<long long> processedTreeTiles;

        for (int ty = tileMin.y; ty <= tileMax.y; ++ty)
        {
            for (int tx = tileMin.x; tx <= tileMax.x; ++tx)
            {
                glm::vec2 tileCenter = chunk_manager->tileToWorld({tx, ty}) + glm::vec2{8.0f, 8.0f};
                if (glm::distance(tileCenter, center) > radius)
                    continue;

                TileType type = chunk_manager->tileAt(tx, ty).type;
                if (type == TileType::Air)
                    continue;

                if (type == TileType::Wood || type == TileType::Leaves)
                {
                    long long key = (static_cast<long long>(tx) << 32) ^ static_cast<unsigned int>(ty);
                    if (!processedTreeTiles.insert(key).second)
                        continue;
                    m_treeManager.digTile(tx, ty, *chunk_manager, m_treeManager.getDrops());
                    ++destroyedTiles;
                    continue;
                }

                if (type == TileType::Ore)
                {
                    using Cat = game::inventory::ItemCategory;
                    m_inventory.addItem({"ore", "矿石", 64, Cat::Material}, 1);
                }
                else if (type == TileType::Stone || type == TileType::Gravel)
                {
                    using Cat = game::inventory::ItemCategory;
                    m_inventory.addItem({"stone", "石块", 64, Cat::Material}, 1);
                }

                chunk_manager->setTile(tx, ty, TileData(TileType::Air));
                ++destroyedTiles;
            }
        }

        int crushedMonsters = m_monsterManager ? m_monsterManager->crushMonstersInRadius(center, radius + 32.0f) : 0;
        glm::vec2 vel = mechPhysics->getVelocity();
        vel.x += facing * 1.5f;
        mechPhysics->setVelocity(vel);

        m_mechAttackCooldown = 0.65f;
        m_mechAttackFlashTimer = 0.16f;
        m_mechLastAttackHits = destroyedTiles + crushedMonsters;
        spdlog::info("机甲重击：摧毁 {} 个瓦片，击退 {} 个怪物", destroyedTiles, crushedMonsters);
    }

    engine::object::GameObject* GameScene::getControlledActor() const
    {
        if (m_isPlayerInMech && m_mech)
            return m_mech;
        return m_player;
    }

    glm::vec2 GameScene::getActorWorldPosition(const engine::object::GameObject* actor) const
    {
        if (!actor)
            return {0.0f, 0.0f};

        auto* transform = actor->getComponent<engine::component::TransformComponent>();
        if (!transform)
            return {0.0f, 0.0f};

        return transform->getPosition();
    }

    glm::vec2 GameScene::findSafeDisembarkPosition() const
    {
        if (!m_mech)
            return {0.0f, 0.0f};

        auto* mechTransform = m_mech->getComponent<engine::component::TransformComponent>();
        auto* mechController = m_mech->getComponent<engine::component::ControllerComponent>();
        if (!mechTransform || !mechController)
            return {0.0f, 0.0f};

        glm::vec2 mechPos = mechTransform->getPosition();
        int primaryDir = mechController->getFacingDirection() == engine::component::ControllerComponent::FacingDirection::Left ? -1 : 1;
        const int directions[2] = {primaryDir, -primaryDir};

        for (int dir : directions)
        {
            for (int dx = 6; dx <= 12; ++dx)
            {
                int tileX = static_cast<int>(mechPos.x / 16.0f) + dir * dx;
                for (int tileY = 6; tileY < 140; ++tileY)
                {
                    auto below = chunk_manager->tileAt(tileX, tileY);
                    auto above = chunk_manager->tileAt(tileX, tileY - 1);
                    auto above2 = chunk_manager->tileAt(tileX, tileY - 2);
                    if (engine::world::isSolid(below.type) &&
                        above.type == engine::world::TileType::Air &&
                        above2.type == engine::world::TileType::Air)
                    {
                        return {tileX * 16.0f + 8.0f, (tileY - 1) * 16.0f};
                    }
                }
            }
        }

        return mechPos + glm::vec2{primaryDir * 120.0f, -48.0f};
    }
}