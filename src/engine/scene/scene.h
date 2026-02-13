#pragma once
#include <string>
#include <vector>
namespace engine::core
{
    class Context;
}
namespace engine::scene
{
    class SceneManager;
}
namespace engine::object
{
    class GameObject;
}
namespace engine::scene
{
    class SceneManager;

    class Scene
    {
    protected:
        std::string _name;
        engine::core::Context &_context;
        engine::scene::SceneManager &_scene_manager;
        bool _is_initialized = false;
        std::vector<std::unique_ptr<engine::object::GameObject>> _game_objects;
        std::vector<std::unique_ptr<engine::object::GameObject>> _pending_additions;

    public:
        Scene(std::string _name,
              engine::core::Context &context,
              engine::scene::SceneManager &scene_manager);
        virtual ~Scene();
        // 禁止拷贝和移动
        Scene(const Scene &) = delete;
        Scene &operator=(const Scene &) = delete;
        Scene(Scene &&) = delete;
        Scene &operator=(Scene &&) = delete;
        // 核心循环方法
        virtual void init();
        virtual void update(float delta_time);
        virtual void render();
        virtual void handleInput();
        virtual void clean();

        virtual void addGameObject(std::unique_ptr<engine::object::GameObject> &&game_object);
        virtual void safeAddGameObject(std::unique_ptr<engine::object::GameObject> &&game_object);
        virtual void removeGameObject(engine::object::GameObject *game_object_ptr);
        virtual void safeRemoveGameObject(engine::object::GameObject *game_object_ptr);
        engine::object::GameObject *findGameObjectByName(const std::string &name) const;

        // GETTER
        const std::string &getName() const { return _name; };
        bool isInitialized() const { return _is_initialized;};

        engine::core::Context &getContext() { return _context; };
        engine::scene::SceneManager &getSceneManager() { return _scene_manager; };

        // SETTER
        void setInitialized(bool is_initialized) { _is_initialized = is_initialized; };

    protected:
        void procesPendingAdditions();
    };
}