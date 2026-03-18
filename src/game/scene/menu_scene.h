#pragma once
#include "../../engine/scene/scene.h"
#include "../../engine/ecs/registry.h"
#include "../../engine/render/text_renderer.h"

namespace game::scene
{
    class MenuScene : public engine::scene::Scene
    {
    public:
        MenuScene(const std::string &name, engine::core::Context &context, engine::scene::SceneManager &sceneManager);

        void init() override;
        void update(float delta_time) override;
        void render() override;
        void handleInput() override;
        void clean() override;

    private:
        engine::ecs::Registry ecs_registry;
        engine::render::TextRenderer* text_renderer = nullptr;
        void startGame();
    };
}
