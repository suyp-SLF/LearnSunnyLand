#include "game_scene.h"
#include "../../engine/object/game_object.h"
#include "../../engine/component/transform_component.h"
#include "../../engine/component/sprite_component.h"
#include "../../engine/core/context.h"
#include "../../engine/render/sprite_render_system.h"
#include "../../engine/resource/resource_manager.h"
#include <spdlog/spdlog.h>

namespace game::scene
{
    GameScene::GameScene(std::string name,
                         engine::core::Context &context,
                         engine::scene::SceneManager &sceneManager)
        : Scene(name, context, sceneManager)
    {
        spdlog::trace("GameScene 构造完成");
    }

    void GameScene::init()
    {
        createTestObject();
        Scene::init();
        spdlog::trace("GameScene 初始化完成");
    }
    void GameScene::update(float delta_time)
    {
        Scene::update(delta_time);
    }
    void GameScene::render()
    {
        // 1. 调用基类逻辑（如果有必要）
        Scene::render();

        // 2. ⚡️ 核心：驱动渲染系统绘制所有已注册的 SpriteComponent
        // 这里才是真正去调 Renderer -> SDL_Render/SDL_GPU 的地方
        _context.getSpriteRenderSystem().renderAll(_context);
    }
    void GameScene::handleInput()
    {
        Scene::handleInput();
    }
    void GameScene::clean()
    {
        Scene::clean();
    }
    void GameScene::createTestObject()
    {
        spdlog::trace("GameScene 创建测试对象");
        auto test_object = std::make_unique<engine::object::GameObject>(_context, "test_object");
        // 添加组件
        test_object->addComponent<engine::component::TransformComponent>(glm::vec2(100, 100));
        test_object->addComponent<engine::component::SpriteComponent>("assets/textures/Props/bubble1.svg", engine::utils::Alignment::CENTER);
        // 添加到场景中
        addGameObject(std::move(test_object));
        spdlog::trace("GameScene 测试对象创建完成");
    }
}