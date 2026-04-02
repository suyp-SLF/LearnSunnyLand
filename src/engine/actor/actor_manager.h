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
        /** 将 from 索引的 actor 移动到 to 索引（渲染顺序） */
        void moveActor(size_t from, size_t to);
        size_t actorCount() const { return m_actors.size(); }
        const std::vector<std::unique_ptr<engine::object::GameObject>> &getActors() const { return m_actors; }

    private:
        engine::core::Context &m_context;
        std::vector<std::unique_ptr<engine::object::GameObject>> m_actors;
    };
}
