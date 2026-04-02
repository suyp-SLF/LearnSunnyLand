#pragma once

#include "../../engine/scene/scene.h"
#include "../route/route_data.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace game::scene
{
    class LoadingScene : public engine::scene::Scene
    {
    public:
        LoadingScene(const std::string& name,
                     engine::core::Context& context,
                     engine::scene::SceneManager& sceneManager,
                     game::route::RouteData routeData);

        void init() override;
        void update(float delta_time) override;
        void render() override;
        void handleInput() override;
        void clean() override;

    private:
        enum class LoadStep
        {
            ReadPlanetData = 0,
            ValidateMapFile,
            ValidateTileCatalog,
            LoadCharacterProfiles,
            EnterGame,
            Done
        };

        SDL_GLContext m_glContext = nullptr;
        game::route::RouteData m_routeData;
        LoadStep m_step = LoadStep::ReadPlanetData;
        float m_progress = 0.0f;
        std::string m_status = "准备加载...";

        std::string m_planetDataPath;
        std::string m_mapFilePath;
        std::string m_tileCatalogPath;
        std::vector<std::string> m_characterFiles;
        std::vector<std::string> m_validCharacterFiles;

        int m_mapWidth = 0;
        int m_mapHeight = 0;
        int m_mapLayerCount = 0;
        int m_tileKindCount = 0;
        int m_totalCharacterCount = 0;
        int m_validCharacterCount = 0;
        int m_invalidCharacterCount = 0;
        size_t m_characterScanIndex = 0;

        std::string planetDataPathFor(game::route::PlanetType type) const;
        bool fileExists(const std::string& path) const;
    };
} // namespace game::scene
