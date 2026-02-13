#pragma once
#include <vector>

namespace engine::core
{
    class Context;
}

namespace engine::scene
{
    class Scene;
    class SceneManager final
    {
    private:
        engine::core::Context &_context;
        std::vector<std::unique_ptr<Scene>> _scenes_stack;

        enum class PendingAction
        {
            None,
            Push,
            Pop,
            Replace
        };
        PendingAction _pending_action = PendingAction::None;
        std::unique_ptr<Scene> _pending_scene;

    public:
        SceneManager(engine::core::Context &context);
        ~SceneManager();

        // 禁止拷贝和移动
        SceneManager(const SceneManager &) = delete;
        SceneManager &operator=(const SceneManager &) = delete;
        SceneManager(SceneManager &&) = delete;
        SceneManager &operator=(SceneManager &&) = delete;

        // 延时切换场景
        void requestPushScene(std::unique_ptr<Scene> &&scene);
        void requestPopScene();
        void requestReplaceScene(std::unique_ptr<Scene> &&scene);

        // GETTER
        Scene *getCurrentScene() const;
        engine::core::Context &getContext() const { return _context; };

        // 核心循环函数
        void update(float delta_time);
        void render();
        void handleInput();
        void close();

    private:
        void processPendingActions();   // 处理延时切换场景请求
        // 直接切换场景
        void pushScene(std::unique_ptr<Scene> &&scene);
        void popScene();
        void replaceScene(std::unique_ptr<Scene> &&scene);
    };
}