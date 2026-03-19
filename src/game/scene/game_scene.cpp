#include "game_scene.h"
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
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <SDL3/SDL_opengl.h>

namespace game::scene
{
    GameScene::GameScene(const std::string &name,
                         engine::core::Context &context,
                         engine::scene::SceneManager &sceneManager)
        : Scene(name, context, sceneManager)
    {
        spdlog::debug("GameScene '{}' 构造完成", name);
    }

    void GameScene::init()
    {
        physics_manager = std::make_unique<engine::physics::PhysicsManager>();
        physics_manager->init({0.0f, 10.0f});

        engine::world::WorldConfig config;
        config.loadFromFile("assets/world_config.json");
        config.seed = 12345;

        chunk_manager = std::make_unique<engine::world::ChunkManager>(
            "assets/textures/Tiles/tileset.svg",
            config.TILE_SIZE,
            &_context.getResourceManager(),
            physics_manager.get());

        auto generator = std::make_unique<engine::world::PerlinNoiseGenerator>(config);
        chunk_manager->setTerrainGenerator(std::move(generator));

        actor_manager = std::make_unique<engine::actor::ActorManager>(_context);
        createPlayer();

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
                ImGui_ImplSDL3_InitForOpenGL(window, m_glContext);
                ImGui_ImplOpenGL3_Init("#version 330");
                spdlog::info("ImGui initialized with OpenGL3");
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
    }
    void GameScene::update(float delta_time)
    {
        Scene::update(delta_time);

        // 更新相机
        _context.getCamera().update(delta_time);

        const float TIME_STEP = 1.0f / 60.0f;
        if (physics_manager)
        {
            physics_manager->update(TIME_STEP, 4);
        }

        if (actor_manager)
        {
            actor_manager->update(delta_time);
        }

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

        glm::vec2 playerPos = {0.0, 0.0};
        if (m_player)
        {
            auto* transform = m_player->getComponent<engine::component::TransformComponent>();
            if (transform)
            {
                playerPos = transform->getPosition();
            }
        }

        // 区块加载距离（配置文件中设置为3）
        chunk_manager->updateVisibleChunks(playerPos, 3);
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

        if (physics_manager)
        {
            physics_manager->debugDraw(_context.getRenderer(), _context.getCamera());
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

        // ImGui滑块
        if (m_glContext)
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 250, 20), ImGuiCond_Always);
            ImGui::Begin("Camera", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            if (ImGui::SliderFloat("Zoom", &m_zoomSliderValue, 0.5f, 3.0f))
            {
                _context.getCamera().setZoom(m_zoomSliderValue);
            }
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }

    void GameScene::handleInput()
    {
        Scene::handleInput();

        if (actor_manager)
        {
            actor_manager->handleInput();
        }
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

    void GameScene::createPlayer()
    {
        m_player = actor_manager->createActor("player");

        // 玩家初始位置（从天空下落）
        glm::vec2 startPos = {0.0f, 0.0f};

        auto* transform = m_player->addComponent<engine::component::TransformComponent>(startPos);
        m_player->addComponent<engine::component::SpriteComponent>("assets/textures/Characters/player.svg", engine::utils::Alignment::CENTER);
        m_player->addComponent<engine::component::ControllerComponent>(25.0f, 30.0f);

        // 创建物理体并添加 PhysicsComponent (32x48 像素 = 1x1.5 米)
        b2BodyId bodyId = physics_manager->createDynamicBody({startPos.x, startPos.y}, {0.5f, 0.75f}, m_player);
        m_player->addComponent<engine::component::PhysicsComponent>(bodyId);

        // 相机跟随玩家
        _context.getCamera().setFollowTarget(&transform->getPosition(), 5.0f);
    }
}