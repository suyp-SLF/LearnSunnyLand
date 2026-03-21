#pragma once
#include <vector>
#include <memory>
#include <string>

namespace engine::object { class GameObject; }
namespace engine::core { class Context; }

namespace engine::actor
{
    class ActorManager
    {
    public:
        ActorManager(engine::core::Context &context);
        ~ActorManager();

        engine::object::GameObject* createActor(const std::string &name);
        void update(float delta_time);
        void render();
        void handleInput();
        void clear();
        size_t actorCount() const { return m_actors.size(); }

    private:
        engine::core::Context &m_context;
        std::vector<std::unique_ptr<engine::object::GameObject>> m_actors;
    };
}
