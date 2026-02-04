#pragma once

#include <memory>

struct SDL_Window;
struct SDL_Renderer;

namespace engine::resource {
    class ResourceManager; 
}

namespace engine::core
{
    class Time;
    /**
     * @brief GameApp 类，表示游戏应用程序的主类
     * 该类使用SDL库创建窗口和渲染器，并管理游戏的主循环状态
     */
    class GameApp final
    {
    private:
        SDL_Window *_window = nullptr;         // SDL窗口指针，用于创建和管理游戏窗口
        SDL_Renderer *_sdl_renderer = nullptr; // SDL渲染器指针，用于在窗口上绘制图形
        bool _is_running = false;              // 游戏运行状态标志，true表示游戏正在运行

        //引擎组件
        std::unique_ptr<engine::core::Time> _time;
        std::unique_ptr<engine::resource::ResourceManager> _resource_manager;

    public:
        GameApp();  // 构造函数，初始化游戏应用程序
        ~GameApp(); // 析构函数，清理游戏应用程序资源

        void run(); // 运行游戏主循环

        // 禁止拷贝和移动
        GameApp(const GameApp &) = delete;
        GameApp &operator=(const GameApp &) = delete;
        GameApp(GameApp &&) = delete;
        GameApp &operator=(GameApp &&) = delete;

    private:
        [[nodiscard]] bool init();
        void handleEvents();
        void update(float delta_time);
        void render();
        void close();
        // 初始化函数
        bool initSDL();
        bool initTime();
        bool initResourceManager();

        void test(); 
    };
}
