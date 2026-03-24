#include "ship_scene.h"
#include "menu_scene.h"
#include "route_select_scene.h"
#include "../../engine/core/context.h"
#include "../../engine/scene/scene_manager.h"
#include "../../engine/input/input_manager.h"
#include "../../engine/render/renderer.h"
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <cmath>

namespace game::scene
{
    ShipScene::ShipScene(const std::string &name,
                         engine::core::Context &context,
                         engine::scene::SceneManager &sceneManager)
        : Scene(name, context, sceneManager)
    {
    }

    void ShipScene::init()
    {
        Scene::init();

        SDL_Window *window = _context.getRenderer().getWindow();
        if (window)
        {
            m_glContext = SDL_GL_GetCurrentContext();
            if (m_glContext)
            {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();

                ImGuiIO &io = ImGui::GetIO();
                io.Fonts->AddFontFromFileTTF(
                    "assets/fonts/VonwaonBitmap-16px.ttf",
                    16.0f, nullptr,
                    io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

                ImGui_ImplSDL3_InitForOpenGL(window, m_glContext);
                ImGui_ImplOpenGL3_Init("#version 330");
                spdlog::info("ShipScene: ImGui 初始化完成");
            }
        }
        spdlog::info("ShipScene 初始化完成");
    }

    void ShipScene::update(float dt)
    {
        m_time += dt;
    }

    void ShipScene::render()
    {
        if (!m_glContext) return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        renderPerformanceOverlay();
        renderShipInterior();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void ShipScene::handleInput()
    {
        auto &input = _context.getInputManager();
        if (input.isActionPressed("interact"))
        {
            // F 键：打开星图仪 → 路线选择场景
            spdlog::info("ShipScene: 打开星图仪 → 路线选择");
            auto scene = std::make_unique<RouteSelectScene>("RouteSelectScene", _context, _scene_manager);
            _scene_manager.requestReplaceScene(std::move(scene));
        }
    }

    void ShipScene::clean()
    {
        if (m_glContext)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            m_glContext = nullptr;
        }
    }

    // ──────────────────────────────────────────────────────────────────
    //  飞船内部界面渲染
    // ──────────────────────────────────────────────────────────────────
    void ShipScene::renderShipInterior()
    {
        ImGuiIO &io = ImGui::GetIO();
        const float W = io.DisplaySize.x;
        const float H = io.DisplaySize.y;

        ImDrawList *dl = ImGui::GetBackgroundDrawList();

        // ── 背景：深空深蓝渐变 ──
        dl->AddRectFilledMultiColor(
            {0, 0}, {W, H},
            IM_COL32(4, 8, 22, 255),   IM_COL32(4, 8, 22, 255),
            IM_COL32(10, 18, 40, 255), IM_COL32(10, 18, 40, 255));

        // ── 舱底地板 ──
        const float floorY = H * 0.72f;
        dl->AddRectFilled({0, floorY}, {W, H}, IM_COL32(22, 28, 45, 255));
        // 地板高光线
        dl->AddLine({0, floorY}, {W, floorY}, IM_COL32(60, 90, 150, 180), 2.0f);
        // 地板网格线（透视感）
        for (int i = 1; i < 8; ++i)
        {
            float x = W * i / 8.0f;
            dl->AddLine({x, floorY}, {W * 0.5f, H * 1.4f}, IM_COL32(40, 60, 100, 60), 1.0f);
        }
        for (int i = 1; i < 5; ++i)
        {
            float y = floorY + (H - floorY) * i / 5.0f;
            dl->AddLine({0, y}, {W, y}, IM_COL32(40, 60, 100, 40), 1.0f);
        }

        // ── 舱壁（左右挡板） ──
        dl->AddRectFilled({0, 0}, {W * 0.12f, H}, IM_COL32(18, 24, 42, 255));
        dl->AddRectFilled({W * 0.88f, 0}, {W, H}, IM_COL32(18, 24, 42, 255));
        dl->AddLine({W * 0.12f, 0}, {W * 0.12f, H}, IM_COL32(50, 80, 140, 120), 2.0f);
        dl->AddLine({W * 0.88f, 0}, {W * 0.88f, H}, IM_COL32(50, 80, 140, 120), 2.0f);

        // ── 星空窗口（中上方） ──
        const float winLeft = W * 0.20f, winRight = W * 0.80f;
        const float winTop = H * 0.04f, winBot = H * 0.56f;
        dl->AddRectFilled({winLeft, winTop}, {winRight, winBot}, IM_COL32(2, 5, 18, 255), 6.0f);
        dl->AddRect({winLeft, winTop}, {winRight, winBot}, IM_COL32(60, 100, 200, 200), 6.0f, 0, 2.0f);
        // 星点
        struct StarSeed { float x, y, r; };
        static const StarSeed kStars[] = {
            {0.25f,0.08f,1.2f},{0.35f,0.20f,1.0f},{0.45f,0.12f,1.5f},{0.55f,0.30f,0.8f},
            {0.60f,0.10f,1.2f},{0.70f,0.22f,1.0f},{0.30f,0.35f,0.9f},{0.50f,0.42f,1.1f},
            {0.65f,0.38f,1.3f},{0.28f,0.48f,0.7f},{0.72f,0.44f,1.0f},{0.42f,0.25f,1.4f},
        };
        for (auto &s : kStars)
        {
            float twinkle = 0.7f + 0.3f * std::sin(m_time * 1.5f + s.x * 30.0f);
            ImU32 col = IM_COL32(
                (int)(220 * twinkle), (int)(230 * twinkle), (int)(255 * twinkle), 255);
            float sx = winLeft + (winRight - winLeft) * s.x;
            float sy = winTop  + (winBot  - winTop)  * s.y;
            dl->AddCircleFilled({sx, sy}, s.r, col);
        }
        // 窗口框架横档
        float midX = (winLeft + winRight) * 0.5f;
        float midY = (winTop  + winBot)   * 0.5f;
        dl->AddLine({winLeft, midY}, {winRight, midY}, IM_COL32(50, 80, 140, 140), 2.0f);
        dl->AddLine({midX, winTop}, {midX, winBot}, IM_COL32(50, 80, 140, 140), 2.0f);

        // ── 星图仪装置（屏幕中下）──
        const float devCX = W * 0.5f;
        const float devTop = floorY - 140.0f;
        const float devW = 200.0f, devH = 130.0f;
        const float devLeft = devCX - devW * 0.5f;

        // 装置底座
        dl->AddRectFilled({devLeft - 10, floorY - 10}, {devLeft + devW + 10, floorY + 6},
                          IM_COL32(30, 42, 68, 255), 3.0f);
        // 装置主体
        dl->AddRectFilled({devLeft, devTop}, {devLeft + devW, floorY},
                          IM_COL32(20, 32, 58, 255), 4.0f);
        dl->AddRect({devLeft, devTop}, {devLeft + devW, floorY},
                    IM_COL32(70, 130, 220, 200), 4.0f, 0, 1.5f);

        // 屏幕
        float scrPad = 12.0f;
        float scrLeft = devLeft + scrPad, scrRight = devLeft + devW - scrPad;
        float scrTop = devTop + scrPad, scrBot = floorY - 30.0f;
        dl->AddRectFilled({scrLeft, scrTop}, {scrRight, scrBot}, IM_COL32(5, 12, 30, 255), 3.0f);

        // 屏幕上的星球轮廓（脉冲圆）
        float pulseFast = 0.5f + 0.5f * std::sin(m_time * 3.0f);
        float pulseSlow = 0.5f + 0.5f * std::sin(m_time * 1.2f);
        float scrCX = (scrLeft + scrRight) * 0.5f;
        float scrCY = (scrTop + scrBot) * 0.5f;
        float baseR = (scrBot - scrTop) * 0.30f;
        // 轨道环
        dl->AddCircle({scrCX, scrCY}, baseR * 1.6f, IM_COL32(40, 70, 140, 80), 60, 1.0f);
        dl->AddCircle({scrCX, scrCY}, baseR * 2.1f, IM_COL32(30, 55, 110, 50), 60, 1.0f);
        // 星球
        dl->AddCircleFilled({scrCX, scrCY}, baseR,
            IM_COL32((int)(40 + 30*pulseSlow), (int)(100 + 20*pulseSlow), (int)(60 + 20*pulseSlow), 255));
        // 光晕
        dl->AddCircle({scrCX, scrCY}, baseR + 4.0f + 3.0f * pulseFast,
            IM_COL32(80, 200, 100, (int)(100 * pulseFast)), 60, 1.5f);

        // 屏幕底部小按钮
        float btnY = scrBot + 6.0f;
        float btnH2 = 14.0f;
        for (int i = 0; i < 4; ++i)
        {
            float bx = devLeft + scrPad + i * 46.0f;
            ImU32 btnCol = (i == 0)
                ? IM_COL32((int)(50+80*pulseFast),(int)(160+60*pulseFast),80,255)
                : IM_COL32(30, 50, 90, 255);
            dl->AddRectFilled({bx, btnY}, {bx+36.0f, btnY+btnH2}, btnCol, 3.0f);
        }

        // ── 装置标牌 ──
        {
            const char *label = "星 图 仪";
            ImVec2 tsz = ImGui::CalcTextSize(label);
            float lx = devCX - tsz.x * 0.5f;
            float ly = devTop - tsz.y - 8.0f;
            dl->AddRectFilled({lx - 6, ly - 4}, {lx + tsz.x + 6, ly + tsz.y + 4},
                              IM_COL32(0, 0, 0, 160), 4.0f);
            dl->AddText({lx, ly}, IM_COL32(100, 200, 255, 255), label);
        }

        // ── F 键交互提示（脉冲） ──
        {
            float alpha = 0.55f + 0.45f * std::sin(m_time * 2.5f);
            ImU32 col = IM_COL32(255, 230, 80, (int)(255 * alpha));
            const char *hint = "[F]  启动星图仪 — 选择目标星球";
            ImVec2 tsz = ImGui::CalcTextSize(hint);
            float hx = devCX - tsz.x * 0.5f;
            float hy = floorY + 22.0f;
            dl->AddRectFilled({hx - 8, hy - 4}, {hx + tsz.x + 8, hy + tsz.y + 4},
                              IM_COL32(0, 0, 0, 140), 4.0f);
            dl->AddText({hx, hy}, col, hint);
        }

        // ── 飞船名称（左上角） ──
        dl->AddText({W * 0.14f, 18.0f}, IM_COL32(120, 180, 255, 200), "深空探索飞船  [ORIGIN-1]");

        // ── 左上角返回主菜单按钮 ──
        ImGui::SetNextWindowPos({W * 0.14f, 48.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##ship_menu_btn", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings);
        ImGui::PushStyleColor(ImGuiCol_Button,        {0.12f, 0.20f, 0.40f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.20f, 0.35f, 0.65f, 1.0f});
        if (ImGui::Button("返回主菜单", {120.0f, 28.0f}))
        {
            auto ms = std::make_unique<MenuScene>("MenuScene", _context, _scene_manager);
            _scene_manager.requestReplaceScene(std::move(ms));
        }
        ImGui::PopStyleColor(2);
        ImGui::End();
    }

    void ShipScene::renderPerformanceOverlay() const
    {
        ImGui::SetNextWindowPos({10.0f, 10.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.45f);
        ImGui::Begin("##fps_ship", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();
    }
} // namespace game::scene
