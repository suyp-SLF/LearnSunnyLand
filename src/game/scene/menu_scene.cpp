#include "menu_scene.h"
#include "game_scene.h"
#include "../../engine/core/context.h"
#include "../../engine/scene/scene_manager.h"
#include "../../engine/input/input_manager.h"
#include "../../engine/render/renderer.h"
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

namespace game::scene
{
    MenuScene::MenuScene(const std::string &name, engine::core::Context &context, engine::scene::SceneManager &sceneManager)
        : Scene(name, context, sceneManager)
    {
    }

    void MenuScene::init()
    {
        SDL_Window* window = _context.getRenderer().getWindow();
        if (window)
        {
            m_glContext = SDL_GL_GetCurrentContext();
            if (m_glContext)
            {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGui_ImplSDL3_InitForOpenGL(window, m_glContext);
                ImGui_ImplOpenGL3_Init("#version 330");
                spdlog::info("MenuScene: ImGui initialized");
            }
        }
        spdlog::info("MenuScene 初始化完成");
    }

    void MenuScene::update(float delta_time)
    {
    }

    void MenuScene::render()
    {
        if (m_glContext)
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(540, 300), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiCond_Always);
            ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

            if (ImGui::Button("Start Game", ImVec2(180, 60)))
            {
                startGame();
            }

            ImGui::End();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }

    void MenuScene::handleInput()
    {
    }

    void MenuScene::clean()
    {
        if (m_glContext)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            m_glContext = nullptr;
        }
    }

    void MenuScene::startGame()
    {
        spdlog::info("开始游戏按钮被点击");
        auto gameScene = std::make_unique<GameScene>("GameScene123", _context, _scene_manager);
        _scene_manager.requestReplaceScene(std::move(gameScene));
    }
}
