#include "./scene_manager.h"
#include "./scene.h"
#include <spdlog/spdlog.h>
#include "scene_manager.h"
namespace engine::scene
{
    SceneManager::SceneManager(engine::core::Context &context)
        : _context(context)
    {
        spdlog::trace("场景管理器初始化");
    }

    SceneManager::~SceneManager()
    {
        spdlog::trace("场景管理器析构");
        close();
    }
    void SceneManager::requestPushScene(std::unique_ptr<Scene> &&scene)
    {
        _pending_action = PendingAction::Push;
        _pending_scene = std::move(scene);
    }
    void SceneManager::requestPopScene()
    {
        _pending_action = PendingAction::Pop;
    }
    void SceneManager::requestReplaceScene(std::unique_ptr<Scene> &&scene)
    {
        _pending_action = PendingAction::Replace;
        _pending_scene = std::move(scene);
    }
    Scene *SceneManager::getCurrentScene() const
    {
        if (_scenes_stack.empty())
        {
            return nullptr;
        }
        return _scenes_stack.back().get();
    }
    void SceneManager::update(float delta_time)
    {
        // 只更新当前（栈顶）场景
        if (Scene *current_scene = getCurrentScene())
        {
            current_scene->update(delta_time);
        }
        // 执行可能的切换场景操作
        processPendingActions();
    }
    void SceneManager::render()
    {
        // 渲染时需要叠加渲染所有场景
        for (auto &scene : _scenes_stack)
        {
            scene->render();
        }
    }
    void SceneManager::handleInput()
    {
        // 只考虑栈顶
        Scene *current_scene = getCurrentScene();
        if (current_scene)
        {
            current_scene->handleInput();
        }
    }
    void SceneManager::close()
    {
        spdlog::debug("正在关闭并清理场景管理器");
        // 清理所有场景
        while (!_scenes_stack.empty())
        {
            if (_scenes_stack.back())
            {
                spdlog::debug("正在清理场景: {}", _scenes_stack.back()->getName());
                _scenes_stack.back()->clean();
            }
            _scenes_stack.pop_back();
        }
    }

    void SceneManager::processPendingActions()
    {
        if(_pending_action == PendingAction::None)
        {
            return;
        }
        switch (_pending_action)
        {
        case PendingAction::Pop:
            popScene();
            break;
        case PendingAction::Replace:
            replaceScene(std::move(_pending_scene));
            break;
        case PendingAction::Push:
            pushScene(std::move(_pending_scene));
            break;
        default:
            break;
        }
        _pending_action = PendingAction::None;
    }
    void SceneManager::pushScene(std::unique_ptr<Scene> &&scene)
    {
        if(!scene)
        {
            spdlog::error("尝试将空场景压入场景栈");
            return;
        }
        spdlog::debug("正在将场景: {} 压入栈", scene->getName());

        // 初始化新场景
        if (!scene->isInitialized())
        {
            scene->init();
        }
        // 将新场景压入栈
        _scenes_stack.push_back(std::move(scene));
    }
    void SceneManager::popScene()
    {
        if (_scenes_stack.empty())
        {
            spdlog::error("尝试弹出空场景栈");
            return;
        }
        spdlog::debug("正在弹出场景栈顶场景: {}", _scenes_stack.back()->getName());
        // 清理并移除栈顶场景
        if (_scenes_stack.back())
        {
            _scenes_stack.back()->clean();
        }
        _scenes_stack.pop_back();
    }
    void SceneManager::replaceScene(std::unique_ptr<Scene> &&scene)
    {
        if(!scene)
        {
            spdlog::error("尝试将空场景替换当前场景");
            return;
        }
        spdlog::debug("正在将场景: {} 替换场景 {}", scene->getName(), _scenes_stack.back()->getName());
        // 清理并移除场景栈中所有场景
        while (!_scenes_stack.empty())
        {
            if(_scenes_stack.back())
            {
                _scenes_stack.back()->clean();
            }
            _scenes_stack.pop_back();
        }
        // 初始化新场景
        if (!scene->isInitialized())
        {
            scene->init();
        }
        // 将新场景压入栈
        _scenes_stack.push_back(std::move(scene));
    }
}
