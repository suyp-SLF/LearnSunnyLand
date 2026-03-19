#include "actor_manager.h"
#include "../object/game_object.h"
#include "../core/context.h"

namespace engine::actor
{
    ActorManager::ActorManager(engine::core::Context &context)
        : m_context(context)
    {
    }

    ActorManager::~ActorManager() = default;

    engine::object::GameObject* ActorManager::createActor(const std::string &name)
    {
        auto actor = std::make_unique<engine::object::GameObject>(m_context, name);
        auto *ptr = actor.get();
        m_actors.push_back(std::move(actor));
        return ptr;
    }

    void ActorManager::update(float delta_time)
    {
        for (auto &actor : m_actors)
        {
            actor->update(delta_time);
        }
    }

    void ActorManager::render()
    {
        for (auto &actor : m_actors)
        {
            actor->render();
        }
    }

    void ActorManager::handleInput()
    {
        for (auto &actor : m_actors)
        {
            actor->handleInput();
        }
    }

    void ActorManager::clear()
    {
        m_actors.clear();
    }
}
