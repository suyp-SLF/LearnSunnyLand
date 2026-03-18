#include "menu_scene.h"
#include "game_scene.h"
#include "../../engine/core/context.h"
#include "../../engine/scene/scene_manager.h"
#include "../../engine/input/input_manager.h"
#include "../../engine/render/renderer.h"
#include "../../engine/resource/resource_manager.h"
#include "../../engine/resource/font_manager.h"
#include "../../engine/ecs/ui_components.h"
#include "../../engine/actor/actor_manager.h"
#include "../../engine/object/game_object.h"
#include "../../engine/world/chunk_manager.h"
#include "../../engine/physics/physics_manager.h"
#include <spdlog/spdlog.h>

namespace game::scene
{
    MenuScene::MenuScene(const std::string &name, engine::core::Context &context, engine::scene::SceneManager &sceneManager)
        : Scene(name, context, sceneManager)
    {
    }

    void MenuScene::init()
    {
        auto& fontMgr = _context.getResourceManager().getFontManager();
        fontMgr.setDevice(_context.getRenderer().getDevice());
        text_renderer = fontMgr.loadFont("assets/fonts/VonwaonBitmap-16px.ttf", 32);

        auto entity = ecs_registry.create();
        ecs_registry.add<engine::ecs::Button>(entity,
            glm::vec2(220.0f, 150.0f),
            glm::vec2(200.0f, 50.0f),
            "Start Game",
            [this]() { startGame(); });
        ecs_registry.add<engine::ecs::Renderable>(entity, glm::vec4(0.2f, 0.6f, 0.8f, 1.0f));

        spdlog::info("MenuScene 初始化完成");
    }

    void MenuScene::update(float delta_time)
    {
    }

    void MenuScene::render()
    {
        if (!text_renderer)
            return;

        auto& renderer = _context.getRenderer();

        for (auto entity : ecs_registry.view<engine::ecs::Button>())
        {
            auto* btn = ecs_registry.get<engine::ecs::Button>(entity);
            if (btn)
            {
                auto glyphs = text_renderer->prepareText(btn->text, btn->pos.x, btn->pos.y + 30.0f);
                for (const auto& glyph : glyphs)
                {
                    renderer.drawTexture(glyph.texture, glyph.x, glyph.y, glyph.w, glyph.h);
                }
            }
        }
    }

    void MenuScene::handleInput()
    {
        auto& input = _context.getInputManager();

        if (input.isActionPressed("attack"))
        {
            glm::vec2 mousePos = input.getLogicalMousePosition();

            for (auto entity : ecs_registry.view<engine::ecs::Button>())
            {
                auto* btn = ecs_registry.get<engine::ecs::Button>(entity);
                if (btn && mousePos.x >= btn->pos.x && mousePos.x <= btn->pos.x + btn->size.x &&
                    mousePos.y >= btn->pos.y && mousePos.y <= btn->pos.y + btn->size.y)
                {
                    if (btn->onClick)
                        btn->onClick();
                }
            }
        }
    }

    void MenuScene::clean()
    {
    }

    void MenuScene::startGame()
    {
        spdlog::info("开始游戏按钮被点击");
        auto gameScene = std::make_unique<GameScene>("GameScene123", _context, _scene_manager);
        _scene_manager.requestReplaceScene(std::move(gameScene));
    }
}
