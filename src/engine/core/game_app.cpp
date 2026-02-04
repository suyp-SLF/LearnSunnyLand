#include "game_app.h"

#include "time.h"
#include "../../resource/resource_manager.h"

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
namespace engine::core
{
    GameApp::GameApp()
    {
        _time = std::make_unique<Time>();
    }
    GameApp::~GameApp() = default;
    void GameApp::run()
    {
        if (!init())
        {
            spdlog::error("初始化失败，无法运行。");
            return;
        }
        _time->setTargetFPS(144);
        while (_is_running)
        {
            _time->update();
            float delta_time = _time->getDeltaTime();
            handleEvents();
            update(delta_time);
            render();

            //spdlog::info("delta_time: {}", delta_time);
        }
        close();
    }
    bool GameApp::init()
    {
        spdlog::trace("初始化游戏 GameApp");
        if (!initSDL())
            return false;
        if (!initTime())
            return false;
        if (!initResourceManager())
            return false;
        test();
        _is_running = true;
        spdlog::trace("初始化游戏成功 GameApp");
        return true;
    }
    void GameApp::handleEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                _is_running = false;
            }
        }
    }

    void GameApp::update(float /*delta_time*/)
    {
        // TODO: 更新游戏状态
    }

    void GameApp::render()
    {
        SDL_RenderClear(_sdl_renderer);
        // TODO: 渲染游戏内容
        SDL_RenderPresent(_sdl_renderer);
    }

    void GameApp::close()
    {
        spdlog::trace("关闭游戏");
        if (_sdl_renderer)
        {
            SDL_DestroyRenderer(_sdl_renderer);
            _sdl_renderer = nullptr;
        }
        if (_window)
        {
            SDL_DestroyWindow(_window);
            _window = nullptr;
        }
        SDL_Quit();
        _is_running = false;
    }
    bool GameApp::initSDL()
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
        {
            spdlog::error("SDL初始化失败，SDL错误信息：{}", SDL_GetError());
            return false;
        }
        _window = SDL_CreateWindow("SunnyLand", 1280, 720, SDL_WINDOW_RESIZABLE);
        if (!_window)
        {
            spdlog::error("SDL窗口创建失败，SDL错误信息：{}", SDL_GetError());
            return false;
        }
        _sdl_renderer = SDL_CreateRenderer(_window, nullptr);
        if (!_sdl_renderer)
        {
            spdlog::error("SDL渲染器创建失败，SDL错误信息：{}", SDL_GetError());
            return false;
        }

        return true;
    }
    bool GameApp::initTime()
    {
        try
        {
            _time = std::make_unique<Time>();
        }
        catch (const std::exception &e)
        {
            spdlog::error("初始化时间管理器失败，错误信息：{}", e.what());
            return false;
        }
        spdlog::trace("初始化时间管理器成功");
        return true;
    }
    bool GameApp::initResourceManager()
    {
        try
        {
            _resource_manager = std::make_unique<engine::resource::ResourceManager>(_sdl_renderer);
        }
        catch (const std::exception &e)
        {
            spdlog::error("初始化资源管理器失败，错误信息：{}", e.what());
        }
        spdlog::trace("初始化资源管理器成功");
        return true;
    }
    void GameApp::test()
    {
        _resource_manager->loadTexture("assets/textures/Actors/eagle-attack.png");
        _resource_manager->loadFont("assets/fonts/VonwaonBitmap-16px.ttf", 32);
    }
}
