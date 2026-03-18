#pragma once
#include "../../engine/scene/scene.h"
#include "../../engine/world/chunk_manager.h"
#include "../../engine/physics/physics_manager.h"
#include "../../engine/actor/actor_manager.h"
#include "../../engine/render/text_renderer.h"
#include "../../engine/ecs/registry.h"

namespace game::scene
{
    class GameScene : public engine::scene::Scene
    {
    public:
        GameScene(const std::string &name, engine::core::Context &context, engine::scene::SceneManager &sceneManager);

        void init() override;
        void update(float delta_time) override;
        void render() override;
        void handleInput() override;
        void clean() override;

    private:
        std::unique_ptr<engine::world::ChunkManager> chunk_manager;
        std::unique_ptr<engine::physics::PhysicsManager> physics_manager;
        std::unique_ptr<engine::actor::ActorManager> actor_manager;
        engine::render::TextRenderer* text_renderer = nullptr;
        engine::ecs::Registry ecs_registry;

        engine::object::GameObject* m_player = nullptr;
        b2BodyId m_testBoxId = b2_nullBodyId;

        SDL_GPUTexture* m_textTexture = nullptr;

        void createTestObject();
        void testCamera();
        void createPlayer();
        void renderUI();
        void updateTextTexture();
    };
} // namespace game::scene  