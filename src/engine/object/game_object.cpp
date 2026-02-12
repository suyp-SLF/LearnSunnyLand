#include "game_object.h"
#include "../component/transform_component.h" 
#include "../component/sprite_component.h"

namespace engine::object
{
    GameObject::GameObject(const std::string &name, const std::string &tag)
        : _name(name), _tag(tag)
    {
        spdlog::trace("创建对象: {}:{}", tag, name);
    }

    void GameObject::update(float delta_time, engine::core::Context& context)
    {
        for (auto &pair : _components)
        {
            pair.second->update(delta_time, context);
        }
    }

    void GameObject::render(engine::core::Context& context)
    {
        for (auto &pair : _components)
        {
            pair.second->render(context);
        }
    }

    void GameObject::clean()
    {
        for (auto &pair : _components)
        {
            pair.second->clean();
        }
        _components.clear();
    }

    void GameObject::handleInput(engine::core::Context& context)
    {
        for (auto &pair : _components)
        {
            pair.second->handleInput(context);
        }
    }
} // namespace engine::object