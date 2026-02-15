#include "scene.h"
#include "../object/game_object.h"
#include <spdlog/spdlog.h>

namespace engine::scene
{
    Scene::Scene(std::string name,
                 engine::core::Context &context,
                 engine::scene::SceneManager &scene_manager)
        : _name(name),
          _context(context),
          _scene_manager(scene_manager)
    {
        spdlog::trace("场景 {} ,构造完成", _name);
    }

    Scene::~Scene() = default;
    void Scene::init()
    {
        _is_initialized = true;
        spdlog::trace("场景 {} ,初始化完成", _name);
    }

    void Scene::update(float delta_time)
    {
        if (!_is_initialized)
            return;
        for (auto it = _game_objects.begin(); it != _game_objects.end();)
        {
            if (*it && !(*it)->isNeedRemove())
            {
                (*it)->update(delta_time);
                ++it;
            }
            else
            {
                if (*it)
                {
                    (*it)->clean(); // 清理游戏对象
                }
                it = _game_objects.erase(it); // 从场景中移除游戏对象
            }

            procesPendingAdditions(); // 处理待添加的游戏对象
        }
    }

    void Scene::render()
    {
        if (!_is_initialized)
            return;
        // 渲染所有的游戏对象
        for (const auto &game_object : _game_objects)
        {
            if (game_object)
            {
                game_object->render();
            }
        }
    }

    void Scene::handleInput()
    {
        if (!_is_initialized)
            return;
        for (auto it = _game_objects.begin(); it != _game_objects.end();)
        {
            if (*it && !(*it)->isNeedRemove())
            {
                (*it)->handleInput();
                ++it;
            }
            else
            {
                if (*it)
                    (*it)->clean();
                it = _game_objects.erase(it);
            }
        }
    }

    void Scene::clean()
    {
        if (!_is_initialized)
            return;
        for (const auto &game_object : _game_objects)
        {
            if (game_object)
                game_object->clean();
        }
        _game_objects.clear();

        _is_initialized = false; // 重置初始化标志
        spdlog::trace("场景 {} ,清理完成", _name);
    }

    void Scene::addGameObject(std::unique_ptr<engine::object::GameObject> &&game_object)
    {
        if (game_object)
        {
            _game_objects.push_back(std::move(game_object));
        }
        else
        {
            spdlog::warn("尝试向场景 {} 添加空的game_object", _name);
            return;
        }
    }

    void Scene::safeAddGameObject(std::unique_ptr<engine::object::GameObject> &&game_object)
    {
        if (game_object)
        {
            _pending_additions.push_back(std::move(game_object));
        }
        else
        {
            spdlog::warn("尝试向场景 {} 添加空的game_object", _name);
            return;
        }
    }
    void Scene::removeGameObject(engine::object::GameObject *game_object_ptr)
    {
        if (!game_object_ptr)
        {
            spdlog::warn("尝试从场景 {} 移除空的game_object", _name);
            return;
        }
        // erase_remove_if 是 C++17 中引入的算法，用于移除容器中的元素并返回新的末尾迭代器
        auto it = std::remove_if(_game_objects.begin(), _game_objects.end(),
                                 [game_object_ptr](const auto &p)
                                 {
                                     return p.get() == game_object_ptr;
                                 });
        if (it != _game_objects.end())
        {
            (*it)->clean();
            _game_objects.erase(it, _game_objects.end());
            spdlog::trace("从场景 {} 移除game_object", _name);
        }
        else
        {
            spdlog::warn("尝试从场景 {} 移除不存在的game_object", _name);
        }
    }

    void Scene::safeRemoveGameObject(engine::object::GameObject *game_object_ptr)
    {
        game_object_ptr->setNeedRemove(true);
    }

    engine::object::GameObject *Scene::findGameObjectByName(const std::string &name) const
    {
        for(const auto &obj : _game_objects)
        {
            if (obj && obj->getName() == name)
            {
                return obj.get();
            }
        }
        return nullptr;
    }

    void Scene::procesPendingAdditions()
    {
        // 处理待添加的游戏对象
        for (auto &game_object : _pending_additions)
        {
            addGameObject(std::move(game_object));
        }
        _pending_additions.clear();
    }
} // namespace engine::scene
