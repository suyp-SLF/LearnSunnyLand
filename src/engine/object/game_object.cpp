#include "game_object.h"
#include "../component/component.h"

namespace engine::object
{
    GameObject::GameObject(const std::string &name, const std::string &tag)
        : _name(name), _tag(tag)
    {
        spdlog::trace("创建对象: {}:{}", tag, name);
    }

    void GameObject::update(float delta_time)
    {
        for (auto &pair : _components)
        {
            pair.second->update(delta_time);
        }
    }

    void GameObject::render()
    {
        for (auto &pair : _components)
        {
            pair.second->render();
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

    void GameObject::handleInput()
    {
        for (auto &pair : _components)
        {
            pair.second->handleInput();
        }
    }
} // namespace engine::object