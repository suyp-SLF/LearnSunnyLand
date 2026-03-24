#pragma once
#include "../../engine/scene/scene.h"
#include <SDL3/SDL.h>

namespace game::scene
{
    /**
     * @brief 飞船场景 —— 游戏的主基地/大厅
     *
     * 玩家始终从飞船着陆舱启动游戏。
     * 舱内有一台"星图仪"装置，按 F 键（interact）打开路线选择界面，
     * 出发探索星球；完成任务撤离后也会返回此处。
     */
    class ShipScene : public engine::scene::Scene
    {
    public:
        ShipScene(const std::string &name,
                  engine::core::Context &context,
                  engine::scene::SceneManager &sceneManager);

        void init()               override;
        void update(float dt)     override;
        void render()             override;
        void handleInput()        override;
        void clean()              override;

    private:
        SDL_GLContext m_glContext = nullptr;

        float m_time = 0.0f;          // 用于动画（脉冲/闪烁）
        bool  m_nearDevice = true;    // 玩家是否在装置旁（ShipScene 中始终为 true）
        bool  m_showPrompt = true;    // 显示交互提示

        void renderShipInterior();
        void renderPerformanceOverlay() const;
    };
} // namespace game::scene
