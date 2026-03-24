#include "route_select_scene.h"
#include "game_scene.h"
#include "menu_scene.h"
#include "ship_scene.h"
#include "../../engine/core/context.h"
#include "../../engine/scene/scene_manager.h"
#include "../../engine/render/renderer.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace game::scene
{
    using namespace game::route;

    // ── 格子尺寸常量 ──
    static constexpr int   MAP_S    = RouteData::MAP_SIZE;   // 20
    static constexpr float CELL_SZ  = 22.0f;                 // 格子内框大小
    static constexpr float CELL_GAP = 2.0f;                  // 格子间距
    static constexpr float CELL_TOT = CELL_SZ + CELL_GAP;    // 24px/格
    static constexpr float GRID_PX  = MAP_S * CELL_TOT;      // 480px（含间距）

    // ──────────────────────────────────────────────
    // 构造 / 生命周期
    // ──────────────────────────────────────────────
    RouteSelectScene::RouteSelectScene(const std::string &name,
                                       engine::core::Context &context,
                                       engine::scene::SceneManager &sceneManager)
        : Scene(name, context, sceneManager)
    {}

    void RouteSelectScene::init()
    {
        Scene::init();
        SDL_Window *win = _context.getRenderer().getWindow();
        if (win)
        {
            m_glContext = SDL_GL_GetCurrentContext();
            if (m_glContext)
            {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO &io = ImGui::GetIO();
                io.Fonts->AddFontFromFileTTF(
                    "assets/fonts/VonwaonBitmap-16px.ttf", 16.0f, nullptr,
                    io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
                ImGui_ImplSDL3_InitForOpenGL(win, m_glContext);
                ImGui_ImplOpenGL3_Init("#version 330");
                spdlog::info("RouteSelectScene: ImGui 初始化完成");
            }
        }
        const auto &presets = RouteData::planetPresets();
        m_selectedPlanetIndex = 0;
        m_route.applyPlanetPreset(presets[0]);
        m_route.generateTerrain(12345);
    }

    void RouteSelectScene::update(float) {}
    void RouteSelectScene::handleInput() {}

    void RouteSelectScene::clean()
    {
        if (m_glContext)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            m_glContext = nullptr;
        }
    }

    // ──────────────────────────────────────────────
    // 路线逻辑
    // ──────────────────────────────────────────────
    bool RouteSelectScene::isAdjacent(glm::ivec2 a, glm::ivec2 b) const
    {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y) == 1;
    }

    int RouteSelectScene::pathIndexOf(glm::ivec2 cell) const
    {
        for (int i = 0; i < static_cast<int>(m_route.path.size()); ++i)
            if (m_route.path[i] == cell) return i;
        return -1;
    }

    void RouteSelectScene::handleCellClick(int cx, int cy, bool rightClick)
    {
        if (cx < 0 || cx >= MAP_S || cy < 0 || cy >= MAP_S) return;
        glm::ivec2 cell{cx, cy};

        if (rightClick)
        {
            if (!m_route.path.empty())
                m_route.path.pop_back();
            return;
        }

        int idx = pathIndexOf(cell);
        if (idx >= 0)
        {
            // 点已在路径上 → 截断到此格
            m_route.path.resize(static_cast<size_t>(idx + 1));
            return;
        }
        // 空路径 or 与最后一格相邻
        if (m_route.path.empty() || isAdjacent(m_route.path.back(), cell))
            m_route.path.push_back(cell);
    }

    void RouteSelectScene::confirmAndStart()
    {
        if (!m_route.isValid()) return;
        // 重新计算目标区域在路线中的索引
        m_route.objectiveZone = -1;
        for (int i = 0; i < static_cast<int>(m_route.path.size()); ++i)
        {
            if (m_route.path[i] == m_route.objectiveCell)
            {
                m_route.objectiveZone = i;
                break;
            }
        }
        spdlog::info("路线已确认：{} 步，出发 {} → 撤离 {}  目标格={} (zone={})",
            m_route.path.size(),
            RouteData::cellLabel(m_route.startCell()),
            RouteData::cellLabel(m_route.evacCell()),
            RouteData::cellLabel(m_route.objectiveCell),
            m_route.objectiveZone);
        auto gs = std::make_unique<GameScene>("GameScene", _context, _scene_manager, m_route);
        _scene_manager.requestReplaceScene(std::move(gs));
    }

    // ──────────────────────────────────────────────
    // ──────────────────────────────────────────────
    // 渲染 - 调度
    // ──────────────────────────────────────────────
    void RouteSelectScene::render()
    {
        if (!m_glContext) return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        renderPerformanceOverlay();

        if (m_phase == Phase::PlanetSelect)
            renderPlanetSelect();
        else
            renderRouteSelect();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // ──────────────────────────────────────────────
    // 阶段一：星球选择
    // ──────────────────────────────────────────────
    void RouteSelectScene::renderPlanetSelect()
    {
        ImGuiIO &io = ImGui::GetIO();
        float dw = io.DisplaySize.x;
        float dh = io.DisplaySize.y;

        const auto &presets = RouteData::planetPresets();
        const int   N       = static_cast<int>(presets.size());

        // 每张卡片尺寸
        const float CARD_W = 230.0f;
        const float CARD_H = 220.0f;
        const float GAP    = 18.0f;
        const float totalW = N * CARD_W + (N - 1) * GAP;
        const float startX = (dw - totalW) * 0.5f;
        const float startY = (dh - CARD_H) * 0.5f - 30.0f;

        // 全屏深色背景
        ImDrawList *bg = ImGui::GetBackgroundDrawList();
        bg->AddRectFilledMultiColor(
            {0, 0}, {dw, dh},
            IM_COL32(4, 8, 22, 255),   IM_COL32(4, 8, 22, 255),
            IM_COL32(10, 18, 40, 255), IM_COL32(10, 18, 40, 255));

        // 页面标题
        {
            const char *ttl = "选择目标星球";
            ImVec2 ts = ImGui::CalcTextSize(ttl);
            bg->AddText(ImGui::GetFont(), 20.0f,
                {(dw - ts.x * 1.25f) * 0.5f, startY - 50.0f},
                IM_COL32(140, 200, 255, 240), ttl);
        }

        ImGuiWindowFlags cardFlags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;

        // 星球预览颜色（草原/岩浆/冰雪/洞穴）
        static const ImVec4 kPlanetColors[] = {
            {0.15f, 0.55f, 0.22f, 1.0f},  // Verdant: green
            {0.72f, 0.28f, 0.08f, 1.0f},  // Emberfall: red-orange
            {0.55f, 0.78f, 0.92f, 1.0f},  // Frostveil: icy blue
            {0.24f, 0.16f, 0.42f, 1.0f},  // Hollowreach: purple
        };

        for (int i = 0; i < N; ++i)
        {
            const auto &p = presets[i];
            bool       sel = (i == m_selectedPlanetIndex);

            float cx = startX + i * (CARD_W + GAP);
            float cy = startY;

            ImGui::SetNextWindowPos({cx, cy}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({CARD_W, CARD_H}, ImGuiCond_Always);

            // 选中卡背景更亮
            ImGui::SetNextWindowBgAlpha(sel ? 0.96f : 0.78f);
            if (sel)
                ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.10f, 0.22f, 0.42f, 0.97f});
            else
                ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.06f, 0.10f, 0.22f, 0.80f});

            char winId[16];
            std::snprintf(winId, sizeof(winId), "##planet%d", i);
            ImGui::Begin(winId, nullptr, cardFlags);

            ImDrawList *dl = ImGui::GetWindowDrawList();
            ImVec2 wPos = ImGui::GetWindowPos();

            // 顶部星球图形（圆形色块）
            ImVec4 pc = kPlanetColors[i];
            float  cx2 = wPos.x + CARD_W * 0.5f;
            float  cy2 = wPos.y + 52.0f;
            float  r   = 34.0f;
            dl->AddCircleFilled({cx2, cy2}, r,
                IM_COL32((int)(pc.x*255),(int)(pc.y*255),(int)(pc.z*255), 220));
            // 光晕
            dl->AddCircle({cx2, cy2}, r + 5.0f,
                IM_COL32((int)(pc.x*255),(int)(pc.y*255),(int)(pc.z*255), 60), 60, 2.5f);
            // 选中高光环
            if (sel)
                dl->AddCircle({cx2, cy2}, r + 8.0f, IM_COL32(120, 200, 255, 180), 60, 1.5f);

            // 分割线
            dl->AddLine({wPos.x + 10, wPos.y + 94}, {wPos.x + CARD_W - 10, wPos.y + 94},
                        IM_COL32(70, 120, 200, 100), 1.0f);

            // 星球名称
            ImGui::SetCursorPos({0, 102});
            ImVec2 nameTs = ImGui::CalcTextSize(p.name);
            ImGui::SetCursorPosX((CARD_W - nameTs.x) * 0.5f);
            if (sel)
                ImGui::TextColored({0.4f, 0.9f, 1.0f, 1.0f}, "%s", p.name);
            else
                ImGui::TextColored({0.8f, 0.85f, 0.95f, 1.0f}, "%s", p.name);

            // 简介（换行）
            ImGui::SetCursorPosX(10.0f);
            ImGui::PushTextWrapPos(CARD_W - 10.0f);
            ImGui::TextColored({0.65f, 0.72f, 0.82f, 1.0f}, "%s", p.summary);
            ImGui::PopTextWrapPos();

            // 昼夜时长
            ImGui::SetCursorPosX(10.0f);
            ImGui::TextColored({0.5f, 0.7f, 0.5f, 1.0f},
                "昼夜: %.0fs", p.dayLengthSeconds);

            ImGui::End();
            ImGui::PopStyleColor(); // WindowBg

            // 选择按钮（卡片下方）
            ImGui::SetNextWindowPos({cx, cy + CARD_H + 6.0f}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({CARD_W, 40.0f}, ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.0f);
            char btnWinId[20];
            std::snprintf(btnWinId, sizeof(btnWinId), "##pbtn%d", i);
            ImGui::Begin(btnWinId, nullptr,
                cardFlags | ImGuiWindowFlags_NoBackground);

            if (sel)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,        {0.15f, 0.50f, 0.72f, 1.0f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.22f, 0.65f, 0.88f, 1.0f});
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button,        {0.18f, 0.22f, 0.35f, 0.92f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.26f, 0.34f, 0.54f, 1.0f});
            }
            char btnLbl[40];
            std::snprintf(btnLbl, sizeof(btnLbl), "%s###selBtn%d", sel ? "✓ 已选" : "选择", i);
            if (ImGui::Button(btnLbl, {CARD_W, 32.0f}))
                m_selectedPlanetIndex = i;
            ImGui::PopStyleColor(2);

            ImGui::End();
        }

        // 底部：确认按钮 & 返回
        const float BTN_W = 200.0f, BTN_H = 44.0f;
        float btnY = startY + CARD_H + 56.0f;

        ImGui::SetNextWindowPos({(dw - BTN_W * 2 - 16.0f) * 0.5f, btnY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({BTN_W * 2 + 16.0f, BTN_H + 4.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##planet_btns", nullptr, cardFlags | ImGuiWindowFlags_NoBackground);

        ImGui::PushStyleColor(ImGuiCol_Button,        {0.12f, 0.62f, 0.15f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.20f, 0.78f, 0.22f, 1.0f});
        if (ImGui::Button("确认星球，规划路线 →", {BTN_W, BTN_H}))
        {
            const auto &preset = presets[m_selectedPlanetIndex];
            m_route.path.clear();
            m_route.applyPlanetPreset(preset);
            m_route.generateTerrain(preset.seedBias ^ 0xABCD1234ULL);
            m_phase = Phase::RouteSelect;
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 16.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, {0.22f, 0.22f, 0.32f, 0.92f});
        if (ImGui::Button("← 返回飞船", {BTN_W, BTN_H}))
        {
            auto ss = std::make_unique<ShipScene>("ShipScene", _context, _scene_manager);
            _scene_manager.requestReplaceScene(std::move(ss));
        }
        ImGui::PopStyleColor();

        ImGui::End();
    }

    // ──────────────────────────────────────────────
    // 阶段二：路线规划
    // ──────────────────────────────────────────────
    void RouteSelectScene::renderRouteSelect()
    {
        ImGuiIO &io = ImGui::GetIO();
        float dw = io.DisplaySize.x;
        float dh = io.DisplaySize.y;

        // ═══════════════════════════════════
        // 左侧：地图+标题 窗口
        // ═══════════════════════════════════
        float rowLabelW = 22.0f;
        float colLabelH = 20.0f;
        float titleH    = 28.0f;
        float gridWinW  = rowLabelW + GRID_PX;
        float gridWinH  = titleH + colLabelH + GRID_PX;

        float rightPanelW = 215.0f;
        float totalW      = gridWinW + rightPanelW + 8.0f;
        float totalH      = gridWinH;

        float winX = (dw - totalW) * 0.5f;
        float winY = (dh - totalH) * 0.5f;

        ImGui::SetNextWindowPos({winX, winY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({gridWinW, gridWinH}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.95f);
        ImGui::Begin("##gridwin", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar  | ImGuiWindowFlags_NoNav);

        ImDrawList *dl = ImGui::GetWindowDrawList();

        // ── 标题 ──
        {
            const auto &p = RouteData::planetPresets()[m_selectedPlanetIndex];
            char ttl[64];
            std::snprintf(ttl, sizeof(ttl), " %s  —  规划任务路线  （20×20）", p.name);
            ImVec2 ts = ImGui::CalcTextSize(ttl);
            ImGui::SetCursorPosX((gridWinW - ts.x) * 0.5f);
            ImGui::TextColored({0.5f, 0.9f, 1.0f, 1.0f}, "%s", ttl);
        }

        // ── 列标签 A-T ──
        ImGui::SetCursorPosX(rowLabelW);
        {
            ImVec2 sp = ImGui::GetCursorScreenPos();
            for (int cx = 0; cx < MAP_S; ++cx)
            {
                char lbl[3] = {static_cast<char>('A' + cx), 0, 0};
                float lx = sp.x + cx * CELL_TOT + (CELL_SZ - 8.0f) * 0.5f;
                dl->AddText({lx, sp.y}, IM_COL32(150, 210, 255, 200), lbl);
            }
        }
        ImGui::Dummy({GRID_PX, colLabelH});

        // ── 行标签 1-20 ──
        ImGui::SetCursorPosX(0.0f);
        {
            ImVec2 rowStart = ImGui::GetCursorScreenPos();
            for (int ry = 0; ry < MAP_S; ++ry)
            {
                char lbl[4];
                std::snprintf(lbl, sizeof(lbl), "%2d", ry + 1);
                float ly = rowStart.y + ry * CELL_TOT + (CELL_SZ - 8.0f) * 0.5f;
                dl->AddText({rowStart.x, ly}, IM_COL32(150, 210, 255, 200), lbl);
            }
        }

        // ── 格子画布 ──
        ImGui::SetCursorPosX(rowLabelW);
        ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();

        ImVec2 mp = io.MousePos;
        int hoverCX = (mp.x >= canvasOrigin.x && mp.x < canvasOrigin.x + GRID_PX)
            ? static_cast<int>((mp.x - canvasOrigin.x) / CELL_TOT) : -1;
        int hoverCY = (mp.y >= canvasOrigin.y && mp.y < canvasOrigin.y + GRID_PX)
            ? static_cast<int>((mp.y - canvasOrigin.y) / CELL_TOT) : -1;

        for (int ry = 0; ry < MAP_S; ++ry)
        {
            for (int cx = 0; cx < MAP_S; ++cx)
            {
                float px = canvasOrigin.x + cx * CELL_TOT;
                float py = canvasOrigin.y + ry * CELL_TOT;
                ImVec2 p0{px, py}, p1{px + CELL_SZ, py + CELL_SZ};

                glm::ivec2 cell{cx, ry};
                int pidx  = pathIndexOf(cell);
                int psize = static_cast<int>(m_route.path.size());
                bool isObjective = (cell == m_route.objectiveCell);

                auto tc = RouteData::terrainColor(m_route.terrain[ry][cx]);
                ImU32 terrainFill = IM_COL32(tc.r, tc.g, tc.b, 180);

                ImU32 fill;
                if      (pidx == 0)
                    fill = IM_COL32(40,  200,  80, 255);
                else if (pidx == psize - 1 && psize >= 2)
                    fill = IM_COL32(220,  70,  70, 255);
                else if (pidx > 0)
                    fill = IM_COL32(tc.r/2 + 30, tc.g/2 + 65, tc.b/2 + 115, 230);
                else if (cx == hoverCX && ry == hoverCY)
                    fill = IM_COL32(std::min(255,tc.r+50), std::min(255,tc.g+50), std::min(255,tc.b+50), 230);
                else
                    fill = terrainFill;

                dl->AddRectFilled(p0, p1, fill, 3.0f);

                if (isObjective)
                {
                    dl->AddRect({p0.x-1.5f,p0.y-1.5f},{p1.x+1.5f,p1.y+1.5f}, IM_COL32(255,210,50,180), 4.0f, 0, 2.5f);
                    dl->AddRect(p0, p1, IM_COL32(255,230,100,255), 3.0f, 0, 1.5f);
                }
                else
                {
                    dl->AddRect(p0, p1,
                        (cx==hoverCX&&ry==hoverCY) ? IM_COL32(220,240,255,220) : IM_COL32(30,50,80,120),
                        3.0f, 0, 1.0f);
                }

                if (isObjective)
                    dl->AddText({px+5.0f, py+(CELL_SZ-13.0f)*0.5f}, IM_COL32(255,230,50,255), "*");

                if (pidx >= 0)
                {
                    char num[4];
                    std::snprintf(num, sizeof(num), "%d", pidx+1);
                    dl->AddText({px+3.0f, py+(CELL_SZ-13.0f)*0.5f}, IM_COL32(255,255,255,235), num);
                }
            }
        }

        // ── 路径连线 ──
        for (int i = 0; i+1 < static_cast<int>(m_route.path.size()); ++i)
        {
            const glm::ivec2 &a = m_route.path[i];
            const glm::ivec2 &b = m_route.path[i+1];
            ImVec2 pa{canvasOrigin.x + a.x*CELL_TOT + CELL_SZ*0.5f,
                      canvasOrigin.y + a.y*CELL_TOT + CELL_SZ*0.5f};
            ImVec2 pb{canvasOrigin.x + b.x*CELL_TOT + CELL_SZ*0.5f,
                      canvasOrigin.y + b.y*CELL_TOT + CELL_SZ*0.5f};
            dl->AddLine(pa, pb, IM_COL32(255,230,80,55), 8.0f);
            dl->AddLine(pa, pb, IM_COL32(255,230,80,210), 2.0f);
        }

        ImGui::InvisibleButton("##grid", {GRID_PX, GRID_PX});
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && hoverCX>=0 && hoverCY>=0)
            handleCellClick(hoverCX, hoverCY, false);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            handleCellClick(hoverCX, hoverCY, true);
        if (ImGui::IsItemHovered())
        {
            if (hoverCX>=0 && hoverCY>=0)
            {
                CellTerrain ht = m_route.terrain[hoverCY][hoverCX];
                bool isObj = (glm::ivec2{hoverCX,hoverCY} == m_route.objectiveCell);
                ImGui::SetTooltip("格子 %s | 地形: %s%s\n左键 选择格子  右键 撤销  再次点击已选格 截断路线",
                    RouteData::cellLabel({hoverCX,hoverCY}).c_str(),
                    RouteData::terrainName(ht),
                    isObj ? " [目标矿脉]" : "");
            }
        }

        ImGui::End(); // ##gridwin

        // ═══════════════════════════════════
        // 右侧面板
        // ═══════════════════════════════════
        ImGui::SetNextWindowPos({winX + gridWinW + 8.0f, winY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({rightPanelW, gridWinH}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGui::Begin("##panel", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

        const auto &presets = RouteData::planetPresets();
        const auto &curPlanet = presets[m_selectedPlanetIndex];

        // 当前星球信息（只读）
        ImGui::TextColored({0.9f, 0.9f, 0.5f, 1.0f}, "路线信息");
        ImGui::Separator();
        ImGui::TextColored({0.5f, 0.85f, 1.0f, 1.0f}, "星球: %s", curPlanet.name);
        ImGui::TextColored({0.6f, 0.7f, 0.8f, 1.0f}, "%s", curPlanet.summary);
        ImGui::Text("昼夜: %.0fs", curPlanet.dayLengthSeconds);

        ImGui::Spacing();
        ImGui::Separator();

        int psize = static_cast<int>(m_route.path.size());
        if (psize == 0)
            ImGui::TextDisabled("（点击地图格设置出发点）");
        else
        {
            ImGui::Text("出发:  %s", RouteData::cellLabel(m_route.startCell()).c_str());
            if (psize >= 2)
                ImGui::Text("撤离:  %s", RouteData::cellLabel(m_route.evacCell()).c_str());
            else
                ImGui::TextDisabled("撤离:  （继续选择）");
            ImGui::Text("步数:  %d 格", psize);
        }

        ImGui::Spacing();
        ImGui::TextColored({0.7f, 0.8f, 1.0f, 1.0f}, "完整路线:");
        if (psize > 0)
        {
            std::string s;
            for (int i = 0; i < psize; ++i)
            {
                if (i > 0) s += " → ";
                if (static_cast<int>(s.size()) > 72 && i < psize-1) { s += "..."; break; }
                s += RouteData::cellLabel(m_route.path[i]);
            }
            ImGui::TextWrapped("%s", s.c_str());
        }
        else
            ImGui::TextDisabled("（无）");

        ImGui::Spacing();
        ImGui::Separator();
        if (m_route.objectiveCell.x >= 0)
        {
            ImGui::TextColored({1.0f,0.9f,0.2f,1.0f}, "目标: %s (矿山)",
                RouteData::cellLabel(m_route.objectiveCell).c_str());
            if (m_route.objectiveZone >= 0)
                ImGui::TextColored({0.4f,1.0f,0.5f,1.0f}, "路线经过目标格!");
            else
                ImGui::TextDisabled("路线未经过目标格");
        }

        // 图例
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored({0.6f,0.6f,0.6f,1.0f}, "图例");
        {
            ImDrawList *wdl = ImGui::GetWindowDrawList();
            auto legend = [&](ImU32 col, const char *name)
            {
                ImVec2 sp = ImGui::GetCursorScreenPos();
                wdl->AddRectFilled(sp, {sp.x+13,sp.y+13}, col, 3.0f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX()+17);
                ImGui::Text(" %s", name);
            };
            legend(IM_COL32( 40,200, 80,255), "出发点");
            legend(IM_COL32(220, 70, 70,255), "撤离点");
            legend(IM_COL32( 55,140, 60,180), "草原");
            legend(IM_COL32( 25,100, 35,180), "森林");
            legend(IM_COL32(100,100,110,180), "岩地");
            legend(IM_COL32(120, 75,155,180), "矿山(*=目标)");
            legend(IM_COL32( 40, 40, 70,180), "洞穴");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored({0.6f,0.6f,0.6f,1.0f}, "操作");
        ImGui::TextWrapped("左键：添加相邻格\n右键：撤销最后格\n再次点击路径格：截断");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool canStart = m_route.isValid();
        if (!canStart) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button,        {0.12f,0.62f,0.15f,1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.20f,0.78f,0.22f,1.0f});
        if (ImGui::Button("确认出发", {rightPanelW-16.0f, 42.0f}))
            confirmAndStart();
        ImGui::PopStyleColor(2);
        if (!canStart) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.42f,0.18f,0.18f,1.0f});
        if (ImGui::Button("清空路线", {rightPanelW-16.0f, 30.0f}))
            m_route.path.clear();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.22f,0.22f,0.32f,0.9f});
        if (ImGui::Button("← 重新选择星球", {rightPanelW-16.0f, 30.0f}))
        {
            m_route.path.clear();
            m_phase = Phase::PlanetSelect;
        }
        ImGui::PopStyleColor();

        ImGui::End(); // ##panel
    }

    void RouteSelectScene::renderPerformanceOverlay() const
    {
        ImGui::SetNextWindowPos({10.0f, 10.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.45f);
        ImGui::Begin("##fps_route", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();
    }

} // namespace game::scene
