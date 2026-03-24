#pragma once
#include "../../engine/scene/scene.h"
#include "../route/route_data.h"
#include <SDL3/SDL.h>

namespace game::scene
{
    /**
     * @brief 路线选择场景（两阶段）
     *
     * Phase::PlanetSelect — 选择目标星球
     * Phase::RouteSelect  — 在 20×20 地图上规划路线
     */
    class RouteSelectScene : public engine::scene::Scene
    {
    public:
        RouteSelectScene(const std::string &name,
                         engine::core::Context &context,
                         engine::scene::SceneManager &sceneManager);

        void init()        override;
        void update(float) override;
        void render()      override;
        void handleInput() override;
        void clean()       override;

    private:
        enum class Phase { PlanetSelect, RouteSelect };

        SDL_GLContext          m_glContext = nullptr;
        game::route::RouteData m_route;
        int                    m_selectedPlanetIndex = 0;
        Phase                  m_phase = Phase::PlanetSelect;

        bool isAdjacent  (glm::ivec2 a, glm::ivec2 b) const;
        int  pathIndexOf (glm::ivec2 cell)             const;
        void handleCellClick(int cx, int cy, bool rightClick);
        void confirmAndStart();
        void renderPlanetSelect();
        void renderRouteSelect();
        void renderPerformanceOverlay() const;
    };

} // namespace game::scene
