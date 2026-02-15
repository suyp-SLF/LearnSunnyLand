#include "game_object.h"
#include "../component/transform_component.h"
#include "../component/sprite_component.h"

namespace engine::object
{
    GameObject::GameObject(engine::core::Context &context,
                           const std::string &name,
                           const std::string &tag)
        : _context(&context),
          _name(name),
          _tag(tag)
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