#include "universe_editor.h"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace game::scene
{
    // ─────────────────────────────────────────────────────────────────────────
    //  内部辅助
    // ─────────────────────────────────────────────────────────────────────────
    static bool writeJsonFile(const std::string& path, const nlohmann::json& j)
    {
        std::ofstream out(path);
        if (!out.is_open()) { spdlog::warn("[UniverseEditor] 无法写入: {}", path); return false; }
        out << j.dump(4);
        return true;
    }

    static nlohmann::json readJsonFile(const std::string& path)
    {
        std::ifstream in(path);
        if (!in.is_open()) return {};
        nlohmann::json j;
        try { in >> j; } catch (...) { return {}; }
        return j;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  加载
    // ─────────────────────────────────────────────────────────────────────────
    UeMonsterSpawn UniverseEditor::parseMonsterSpawn(const nlohmann::json& j)
    {
        UeMonsterSpawn ms;
        ms.enabled          = j.value("enabled", true);
        ms.spawnIntervalSec = j.value("spawn_interval_sec", 1.6f);
        ms.maxMonsters      = j.value("max_monsters", 10);
        ms.innerRadius      = j.value("spawn_inner_radius", 420.0f);
        ms.outerRadius      = j.value("spawn_outer_radius", 960.0f);
        ms.cleanupRadius    = j.value("cleanup_radius", 1500.0f);
        if (j.contains("monster_types") && j["monster_types"].is_array())
        {
            for (const auto& t : j["monster_types"])
            {
                UeMonsterType mt;
                mt.id     = t.value("id", "slime");
                mt.weight = t.value("weight", 1.0f);
                ms.types.push_back(mt);
            }
        }
        return ms;
    }

    nlohmann::json UniverseEditor::serializeMonsterSpawn(const UeMonsterSpawn& ms)
    {
        nlohmann::json j;
        j["enabled"]            = ms.enabled;
        j["spawn_interval_sec"] = ms.spawnIntervalSec;
        j["max_monsters"]       = ms.maxMonsters;
        j["spawn_inner_radius"] = ms.innerRadius;
        j["spawn_outer_radius"] = ms.outerRadius;
        j["cleanup_radius"]     = ms.cleanupRadius;
        auto arr = nlohmann::json::array();
        for (const auto& t : ms.types)
            arr.push_back({ {"id", t.id}, {"weight", t.weight} });
        j["monster_types"] = arr;
        return j;
    }

    std::vector<UeTileMix> UniverseEditor::parseTileMix(const nlohmann::json& j)
    {
        std::vector<UeTileMix> result;
        if (!j.is_array()) return result;
        for (const auto& item : j)
        {
            UeTileMix tm;
            tm.key    = item.value("key", "ground_stone");
            tm.weight = item.value("weight", 1.0f);
            result.push_back(tm);
        }
        return result;
    }

    nlohmann::json UniverseEditor::serializeTileMix(const std::vector<UeTileMix>& tm)
    {
        auto arr = nlohmann::json::array();
        for (const auto& t : tm)
            arr.push_back({ {"key", t.key}, {"weight", t.weight} });
        return arr;
    }

    void UniverseEditor::loadPlanets()
    {
        m_planets.clear();
        const std::filesystem::path dir = "assets/planets";
        if (!std::filesystem::exists(dir)) return;
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            const nlohmann::json j = readJsonFile(entry.path().string());
            if (j.is_null()) continue;

            UePlanet p;
            p.filePath        = entry.path().string();
            p.id              = j.value("id", entry.path().stem().string());
            p.displayName     = j.value("display_name", p.id);
            p.atmosphereHeight= j.value("atmosphere_height", 800.0f);
            p.planetRadius    = j.value("planet_radius", 3000.0f);
            p.mapFile         = j.value("map_file", std::string{"assets/maps/level0.tmj"});
            p.tileCatalog     = j.value("tile_catalog", std::string{"assets/ground_tiles/tile_kinds.json"});
            if (j.contains("characters") && j["characters"].is_array())
                for (const auto& c : j["characters"])
                    if (c.is_string()) p.characters.push_back(c.get<std::string>());
            if (j.contains("tile_mix"))     p.tileMix     = parseTileMix(j["tile_mix"]);
            if (j.contains("monster_spawn"))p.monsterSpawn = parseMonsterSpawn(j["monster_spawn"]);
            m_planets.push_back(std::move(p));
        }
        std::sort(m_planets.begin(), m_planets.end(),
            [](const UePlanet& a, const UePlanet& b){ return a.id < b.id; });
    }

    void UniverseEditor::loadGalaxies()
    {
        m_galaxies.clear();
        const std::filesystem::path dir = "assets/galaxies";
        if (!std::filesystem::exists(dir)) return;
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            const nlohmann::json j = readJsonFile(entry.path().string());
            if (j.is_null()) continue;
            UeGalaxy g;
            g.filePath    = entry.path().string();
            g.id          = j.value("id", entry.path().stem().string());
            g.displayName = j.value("display_name", g.id);
            g.starCount   = j.value("star_count", 200);
            g.armCount    = j.value("arm_count", 4);
            if (j.contains("planets") && j["planets"].is_array())
                for (const auto& pid : j["planets"])
                    if (pid.is_string()) g.planetIds.push_back(pid.get<std::string>());
            m_galaxies.push_back(std::move(g));
        }
        std::sort(m_galaxies.begin(), m_galaxies.end(),
            [](const UeGalaxy& a, const UeGalaxy& b){ return a.id < b.id; });
    }

    void UniverseEditor::loadUniverses()
    {
        m_universes.clear();
        const std::filesystem::path dir = "assets/universe";
        if (!std::filesystem::exists(dir)) return;
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            const nlohmann::json j = readJsonFile(entry.path().string());
            if (j.is_null()) continue;
            UeUniverse u;
            u.filePath      = entry.path().string();
            u.id            = j.value("id", entry.path().stem().string());
            u.displayName   = j.value("display_name", u.id);
            u.galaxyDensity = j.value("galaxy_density", 0.5f);
            if (j.contains("galaxies") && j["galaxies"].is_array())
                for (const auto& gid : j["galaxies"])
                    if (gid.is_string()) u.galaxyIds.push_back(gid.get<std::string>());
            m_universes.push_back(std::move(u));
        }
    }

    void UniverseEditor::reload()
    {
        loadPlanets();
        loadGalaxies();
        loadUniverses();
        loadCharacterTexCache();
        m_loaded = true;
        m_selLevel = SelectionLevel::None;
        m_selUniverseIdx = m_selGalaxyIdx = m_selPlanetIdx = -1;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  保存
    // ─────────────────────────────────────────────────────────────────────────
    void UniverseEditor::savePlanet(UePlanet& p)
    {
        nlohmann::json j;
        j["id"]               = p.id;
        j["display_name"]     = p.displayName;
        j["map_file"]         = p.mapFile;
        j["tile_catalog"]     = p.tileCatalog;
        j["atmosphere_height"]= p.atmosphereHeight;
        j["planet_radius"]    = p.planetRadius;
        auto chars = nlohmann::json::array();
        for (const auto& c : p.characters) chars.push_back(c);
        j["characters"]     = chars;
        j["tile_mix"]       = serializeTileMix(p.tileMix);
        j["monster_spawn"]  = serializeMonsterSpawn(p.monsterSpawn);
        if (writeJsonFile(p.filePath, j)) p.dirty = false;
    }

    void UniverseEditor::saveGalaxy(UeGalaxy& g)
    {
        nlohmann::json j;
        j["id"]           = g.id;
        j["display_name"] = g.displayName;
        j["star_count"]   = g.starCount;
        j["arm_count"]    = g.armCount;
        auto arr = nlohmann::json::array();
        for (const auto& pid : g.planetIds) arr.push_back(pid);
        j["planets"] = arr;
        if (writeJsonFile(g.filePath, j)) g.dirty = false;
    }

    void UniverseEditor::saveUniverse(UeUniverse& u)
    {
        nlohmann::json j;
        j["id"]             = u.id;
        j["display_name"]   = u.displayName;
        j["galaxy_density"] = u.galaxyDensity;
        auto arr = nlohmann::json::array();
        for (const auto& gid : u.galaxyIds) arr.push_back(gid);
        j["galaxies"] = arr;
        if (writeJsonFile(u.filePath, j)) u.dirty = false;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  新建
    // ─────────────────────────────────────────────────────────────────────────
    int UniverseEditor::findPlanetIndexById(const std::string& id) const
    {
        for (int i = 0; i < static_cast<int>(m_planets.size()); ++i)
            if (m_planets[i].id == id) return i;
        return -1;
    }

    int UniverseEditor::findGalaxyIndexById(const std::string& id) const
    {
        for (int i = 0; i < static_cast<int>(m_galaxies.size()); ++i)
            if (m_galaxies[i].id == id) return i;
        return -1;
    }

    void UniverseEditor::createNewPlanet()
    {
        const std::string rawId(m_newPlanetName);
        std::string safeId;
        for (char c : rawId)
            safeId += (std::isalnum(static_cast<unsigned char>(c)) || c == '_') ? c : '_';
        if (safeId.empty()) safeId = "new_planet";

        // 防重名
        int suffix = 0;
        std::string finalId = safeId;
        while (findPlanetIndexById(finalId) >= 0)
            finalId = safeId + "_" + std::to_string(++suffix);

        UePlanet p;
        p.id          = finalId;
        p.displayName = rawId.empty() ? finalId : rawId;
        p.filePath    = "assets/planets/" + finalId + ".json";
        p.dirty       = true;
        savePlanet(p);
        m_planets.push_back(std::move(p));
        m_selPlanetIdx  = static_cast<int>(m_planets.size()) - 1;
        m_selLevel      = SelectionLevel::Planet;
        m_newPlanetName[0] = '\0';
    }

    void UniverseEditor::createNewGalaxy()
    {
        const std::string rawId(m_newGalaxyName);
        std::string safeId;
        for (char c : rawId)
            safeId += (std::isalnum(static_cast<unsigned char>(c)) || c == '_') ? c : '_';
        if (safeId.empty()) safeId = "new_galaxy";
        int suffix = 0;
        std::string finalId = safeId;
        while (findGalaxyIndexById(finalId) >= 0)
            finalId = safeId + "_" + std::to_string(++suffix);

        UeGalaxy g;
        g.id          = finalId;
        g.displayName = rawId.empty() ? finalId : rawId;
        g.filePath    = "assets/galaxies/" + finalId + ".json";
        g.dirty       = true;
        saveGalaxy(g);
        m_galaxies.push_back(std::move(g));
        m_selGalaxyIdx  = static_cast<int>(m_galaxies.size()) - 1;
        m_selLevel      = SelectionLevel::Galaxy;
        m_newGalaxyName[0] = '\0';
    }

    void UniverseEditor::createNewUniverse()
    {
        UeUniverse u;
        const std::string rawId(m_newUniverseName);
        std::string safeId;
        for (char c : rawId)
            safeId += (std::isalnum(static_cast<unsigned char>(c)) || c == '_') ? c : '_';
        if (safeId.empty()) safeId = "new_universe";
        u.id          = safeId;
        u.displayName = rawId.empty() ? safeId : rawId;
        u.filePath    = "assets/universe/" + safeId + ".json";
        u.dirty       = true;
        saveUniverse(u);
        m_universes.push_back(std::move(u));
        m_selUniverseIdx = static_cast<int>(m_universes.size()) - 1;
        m_selLevel       = SelectionLevel::Universe;
        m_newUniverseName[0] = '\0';
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  选中实体访问
    // ─────────────────────────────────────────────────────────────────────────
    UeGalaxy* UniverseEditor::selectedGalaxy()
    {
        if (m_selGalaxyIdx >= 0 && m_selGalaxyIdx < static_cast<int>(m_galaxies.size()))
            return &m_galaxies[m_selGalaxyIdx];
        return nullptr;
    }

    UePlanet* UniverseEditor::selectedPlanet()
    {
        if (m_selPlanetIdx >= 0 && m_selPlanetIdx < static_cast<int>(m_planets.size()))
            return &m_planets[m_selPlanetIdx];
        return nullptr;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  渲染 — 左侧树形面板
    // ─────────────────────────────────────────────────────────────────────────
    void UniverseEditor::renderLeftTree()
    {
        ImGui::SeparatorText("宇宙编辑器");

        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.0f);

        // ── 宇宙层 ────────────────────────────────────────────────────────────
        for (int ui = 0; ui < static_cast<int>(m_universes.size()); ++ui)
        {
            auto& u = m_universes[ui];
            ImGuiTreeNodeFlags uFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                        ImGuiTreeNodeFlags_SpanAvailWidth;
            if (m_selLevel == SelectionLevel::Universe && m_selUniverseIdx == ui)
                uFlags |= ImGuiTreeNodeFlags_Selected;

            ImGui::PushID(("u" + u.id).c_str());
            const bool uOpen = ImGui::TreeNodeEx(
                (u.displayName + (u.dirty ? " *" : "")).c_str(), uFlags);

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                m_selUniverseIdx = ui;
                m_selGalaxyIdx   = -1;
                m_selPlanetIdx   = -1;
                m_selLevel       = SelectionLevel::Universe;
            }

            if (uOpen)
            {
                // ── 星系层 ────────────────────────────────────────────────────
                for (const auto& gid : u.galaxyIds)
                {
                    const int gi = findGalaxyIndexById(gid);
                    if (gi < 0) continue;
                    auto& g = m_galaxies[gi];

                    ImGuiTreeNodeFlags gFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                               ImGuiTreeNodeFlags_SpanAvailWidth;
                    if (m_selLevel == SelectionLevel::Galaxy && m_selGalaxyIdx == gi)
                        gFlags |= ImGuiTreeNodeFlags_Selected;

                    ImGui::PushID(("g" + g.id).c_str());
                    const bool gOpen = ImGui::TreeNodeEx(
                        (g.displayName + (g.dirty ? " *" : "")).c_str(), gFlags);

                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                    {
                        m_selUniverseIdx = ui;
                        m_selGalaxyIdx   = gi;
                        m_selPlanetIdx   = -1;
                        m_selLevel       = SelectionLevel::Galaxy;
                    }

                    if (gOpen)
                    {
                        // ── 星球层 ────────────────────────────────────────────
                        for (const auto& pid : g.planetIds)
                        {
                            const int pi = findPlanetIndexById(pid);
                            if (pi < 0) continue;
                            auto& p = m_planets[pi];

                            ImGuiTreeNodeFlags pFlags =
                                ImGuiTreeNodeFlags_Leaf |
                                ImGuiTreeNodeFlags_SpanAvailWidth |
                                ImGuiTreeNodeFlags_NoTreePushOnOpen;
                            if (m_selLevel == SelectionLevel::Planet && m_selPlanetIdx == pi)
                                pFlags |= ImGuiTreeNodeFlags_Selected;

                            ImGui::PushID(("p" + p.id).c_str());
                            ImGui::TreeNodeEx(
                                (p.displayName + (p.dirty ? " *" : "")).c_str(), pFlags);
                            if (ImGui::IsItemClicked())
                            {
                                m_selUniverseIdx = ui;
                                m_selGalaxyIdx   = gi;
                                m_selPlanetIdx   = pi;
                                m_selLevel       = SelectionLevel::Planet;
                            }
                            ImGui::PopID();
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        ImGui::PopStyleVar();

        // ── 独立星球/星系浏览（未被任何宇宙引用的也可查看） ─────────────────
        ImGui::Spacing();
        ImGui::SeparatorText("所有星球");
        for (int pi = 0; pi < static_cast<int>(m_planets.size()); ++pi)
        {
            auto& p = m_planets[pi];
            ImGuiTreeNodeFlags pFlags =
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth |
                ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (m_selLevel == SelectionLevel::Planet && m_selPlanetIdx == pi)
                pFlags |= ImGuiTreeNodeFlags_Selected;
            ImGui::PushID(("all_p" + p.id).c_str());
            ImGui::TreeNodeEx((p.displayName + (p.dirty ? " *" : "")).c_str(), pFlags);
            if (ImGui::IsItemClicked())
            {
                m_selPlanetIdx   = pi;
                m_selLevel       = SelectionLevel::Planet;
                m_selGalaxyIdx   = -1;
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::SeparatorText("所有星系");
        for (int gi = 0; gi < static_cast<int>(m_galaxies.size()); ++gi)
        {
            auto& g = m_galaxies[gi];
            ImGuiTreeNodeFlags gFlags =
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth |
                ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (m_selLevel == SelectionLevel::Galaxy && m_selGalaxyIdx == gi)
                gFlags |= ImGuiTreeNodeFlags_Selected;
            ImGui::PushID(("all_g" + g.id).c_str());
            ImGui::TreeNodeEx((g.displayName + (g.dirty ? " *" : "")).c_str(), gFlags);
            if (ImGui::IsItemClicked())
            {
                m_selGalaxyIdx = gi;
                m_selLevel     = SelectionLevel::Galaxy;
            }
            ImGui::PopID();
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  渲染 — 瓦片混合编辑器
    // ─────────────────────────────────────────────────────────────────────────
    void UniverseEditor::renderTileMixEditor(std::vector<UeTileMix>& mix, bool& dirty)
    {
        ImGui::SeparatorText("瓦片种类与比例");
        ImGui::TextDisabled("配置星球表面生成使用的瓦片种类及权重比例。");

        if (ImGui::BeginTable("##tile_mix", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("瓦片 Key",  ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("权重",       ImGuiTableColumnFlags_WidthStretch, 0.3f);
            ImGui::TableSetupColumn("操作",       ImGuiTableColumnFlags_WidthFixed,   48.0f);
            ImGui::TableHeadersRow();

            int toRemove = -1;
            for (int i = 0; i < static_cast<int>(mix.size()); ++i)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(i);
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%s", mix[i].key.c_str());
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##tk", buf, sizeof(buf)))
                { mix[i].key = buf; dirty = true; }

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##tw", &mix[i].weight, 0.01f, 0.0f, 10.0f, "%.2f"))
                    dirty = true;

                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton("删"))
                { toRemove = i; dirty = true; }
                ImGui::PopID();
            }
            if (toRemove >= 0)
                mix.erase(mix.begin() + toRemove);

            ImGui::EndTable();
        }
        if (ImGui::Button("+ 新增瓦片"))
        { mix.push_back({"ground_stone", 1.0f}); dirty = true; }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  渲染 — 怪物生成编辑器
    // ─────────────────────────────────────────────────────────────────────────
    void UniverseEditor::renderMonsterSpawnEditor(UeMonsterSpawn& ms, bool& dirty)
    {
        ImGui::SeparatorText("自动生成怪物");

        if (ImGui::Checkbox("启用自动生成##ms_en", &ms.enabled)) dirty = true;
        if (!ms.enabled) { ImGui::BeginDisabled(true); }

        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::DragFloat("生成间隔(秒)##ms_int", &ms.spawnIntervalSec, 0.05f, 0.1f, 30.0f, "%.2f"))
            dirty = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::DragInt("最大数量##ms_max", &ms.maxMonsters, 1, 1, 200))
            dirty = true;

        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::DragFloat("内圈半径(px)##ms_in", &ms.innerRadius, 5.0f, 0.0f, 2000.0f, "%.0f"))
            dirty = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::DragFloat("外圈半径(px)##ms_out", &ms.outerRadius, 5.0f, 0.0f, 5000.0f, "%.0f"))
            dirty = true;
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::DragFloat("清理半径(px)##ms_cl", &ms.cleanupRadius, 5.0f, 0.0f, 10000.0f, "%.0f"))
            dirty = true;

        ImGui::Spacing();
        ImGui::TextUnformatted("怪物类型与权重：");
        if (ImGui::BeginTable("##ms_types", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("怪物 ID",  ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("权重",     ImGuiTableColumnFlags_WidthStretch, 0.3f);
            ImGui::TableSetupColumn("操作",     ImGuiTableColumnFlags_WidthFixed,   48.0f);
            ImGui::TableHeadersRow();

            int toRemove = -1;
            for (int i = 0; i < static_cast<int>(ms.types.size()); ++i)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(i + 1000);
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%s", ms.types[i].id.c_str());
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##mid", buf, sizeof(buf)))
                { ms.types[i].id = buf; dirty = true; }

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##mw", &ms.types[i].weight, 0.05f, 0.0f, 10.0f, "%.2f"))
                    dirty = true;

                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton("删##mt"))
                { toRemove = i; dirty = true; }
                ImGui::PopID();
            }
            if (toRemove >= 0)
                ms.types.erase(ms.types.begin() + toRemove);
            ImGui::EndTable();
        }
        if (ImGui::Button("+ 新增怪物类型"))
        { ms.types.push_back({"slime", 1.0f}); dirty = true; }

        if (!ms.enabled) { ImGui::EndDisabled(); }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  渲染 — 属性面板
    // ─────────────────────────────────────────────────────────────────────────
    void UniverseEditor::renderUniverseProps(UeUniverse& u)
    {
        ImGui::SeparatorText(("宇宙: " + u.displayName).c_str());

        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", u.displayName.c_str());
        if (ImGui::InputText("显示名称##u_name", nameBuf, sizeof(nameBuf)))
        { u.displayName = nameBuf; u.dirty = true; }

        if (ImGui::DragFloat("星系密度##u_dens", &u.galaxyDensity, 0.01f, 0.0f, 1.0f, "%.3f"))
            u.dirty = true;
        ImGui::TextDisabled("星系密度用于过程化生成中控制随机星系的数量分布。");

        // 关联星系列表
        ImGui::Spacing();
        ImGui::SeparatorText("关联星系");
        int toRemove = -1;
        for (int i = 0; i < static_cast<int>(u.galaxyIds.size()); ++i)
        {
            ImGui::PushID(i + 5000);
            ImGui::TextUnformatted(u.galaxyIds[i].c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("移除"))
            { toRemove = i; u.dirty = true; }
            ImGui::PopID();
        }
        if (toRemove >= 0) u.galaxyIds.erase(u.galaxyIds.begin() + toRemove);

        // 从已有星系中选一个加入
        if (!m_galaxies.empty() && ImGui::BeginCombo("添加星系##u_addg", "选择星系..."))
        {
            for (const auto& g : m_galaxies)
            {
                const bool alreadyIn = std::find(u.galaxyIds.begin(), u.galaxyIds.end(), g.id) != u.galaxyIds.end();
                if (alreadyIn) continue;
                if (ImGui::Selectable(g.displayName.c_str()))
                { u.galaxyIds.push_back(g.id); u.dirty = true; }
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        if (u.dirty)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.52f, 0.32f, 1.0f));
            if (ImGui::Button("保存宇宙##u_save", ImVec2(-1.0f, 0.0f))) saveUniverse(u);
            ImGui::PopStyleColor();
        }
    }

    void UniverseEditor::renderGalaxyProps(UeGalaxy& g)
    {
        ImGui::SeparatorText(("星系: " + g.displayName).c_str());

        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", g.displayName.c_str());
        if (ImGui::InputText("显示名称##g_name", nameBuf, sizeof(nameBuf)))
        { g.displayName = nameBuf; g.dirty = true; }

        if (ImGui::DragInt("恒星数量##g_stars", &g.starCount, 1, 1, 10000))
            g.dirty = true;
        if (ImGui::DragInt("旋臂数量##g_arms", &g.armCount, 1, 0, 16))
            g.dirty = true;

        // 关联星球列表
        ImGui::Spacing();
        ImGui::SeparatorText("关联星球");
        int toRemove = -1;
        for (int i = 0; i < static_cast<int>(g.planetIds.size()); ++i)
        {
            ImGui::PushID(i + 6000);
            const int pi = findPlanetIndexById(g.planetIds[i]);
            const std::string label = (pi >= 0)
                ? (m_planets[pi].displayName + " [" + g.planetIds[i] + "]")
                : g.planetIds[i];
            ImGui::TextUnformatted(label.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("移除##gp"))
            { toRemove = i; g.dirty = true; }
            ImGui::PopID();
        }
        if (toRemove >= 0) g.planetIds.erase(g.planetIds.begin() + toRemove);

        if (!m_planets.empty() && ImGui::BeginCombo("添加星球##g_addp", "选择星球..."))
        {
            for (const auto& p : m_planets)
            {
                const bool alreadyIn = std::find(g.planetIds.begin(), g.planetIds.end(), p.id) != g.planetIds.end();
                if (alreadyIn) continue;
                if (ImGui::Selectable((p.displayName + " [" + p.id + "]").c_str()))
                { g.planetIds.push_back(p.id); g.dirty = true; }
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        if (g.dirty)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.52f, 0.32f, 1.0f));
            if (ImGui::Button("保存星系##g_save", ImVec2(-1.0f, 0.0f))) saveGalaxy(g);
            ImGui::PopStyleColor();
        }
    }

    void UniverseEditor::renderPlanetProps(UePlanet& p)
    {
        ImGui::SeparatorText(("星球: " + p.displayName).c_str());

        // ── 基础参数 ──────────────────────────────────────────────────────────
        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", p.displayName.c_str());
        if (ImGui::InputText("显示名称##p_name", nameBuf, sizeof(nameBuf)))
        { p.displayName = nameBuf; p.dirty = true; }

        ImGui::Spacing();
        ImGui::SeparatorText("基础参数");
        if (ImGui::DragFloat("大气高度(px)##p_atm", &p.atmosphereHeight, 10.0f, 0.0f, 20000.0f, "%.0f"))
            p.dirty = true;
        ImGui::TextDisabled("角色在星球内可飞行的最大高度（地面 Y=0），超过此高度即脱离该星球。");

        if (ImGui::DragFloat("星球半径(px)##p_rad", &p.planetRadius, 10.0f, 100.0f, 100000.0f, "%.0f"))
            p.dirty = true;
        ImGui::TextDisabled("导航地图的有效范围，决定星球大小。");

        // ── 瓦片 ─────────────────────────────────────────────────────────────
        ImGui::Spacing();
        renderTileMixEditor(p.tileMix, p.dirty);

        // ── 怪物生成 ─────────────────────────────────────────────────────────
        ImGui::Spacing();
        renderMonsterSpawnEditor(p.monsterSpawn, p.dirty);

        // ── 保存 ─────────────────────────────────────────────────────────────
        ImGui::Spacing();
        if (p.dirty)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.52f, 0.32f, 1.0f));
            if (ImGui::Button("保存星球##p_save", ImVec2(-1.0f, 0.0f))) savePlanet(p);
            ImGui::PopStyleColor();
        }
        ImGui::TextDisabled("文件: %s", p.filePath.c_str());
    }

    void UniverseEditor::renderRightProperties()
    {
        // 顶部工具行
        if (ImGui::Button("刷新全部")) reload();
        ImGui::SameLine();

        // 新建系列按钮 + popup
        if (ImGui::Button("+ 新宇宙"))
        { m_newUniverseName[0] = '\0'; ImGui::OpenPopup("new_universe_popup"); }
        ImGui::SameLine();
        if (ImGui::Button("+ 新星系"))
        { m_newGalaxyName[0] = '\0'; ImGui::OpenPopup("new_galaxy_popup"); }
        ImGui::SameLine();
        if (ImGui::Button("+ 新星球"))
        { m_newPlanetName[0] = '\0'; ImGui::OpenPopup("new_planet_popup"); }

        if (ImGui::BeginPopup("new_universe_popup"))
        {
            ImGui::InputText("宇宙名称##nu", m_newUniverseName, sizeof(m_newUniverseName));
            if (ImGui::Button("创建")) { createNewUniverse(); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("取消")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("new_galaxy_popup"))
        {
            ImGui::InputText("星系名称##ng", m_newGalaxyName, sizeof(m_newGalaxyName));
            if (ImGui::Button("创建")) { createNewGalaxy(); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("取消")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("new_planet_popup"))
        {
            ImGui::InputText("星球名称##np", m_newPlanetName, sizeof(m_newPlanetName));
            if (ImGui::Button("创建")) { createNewPlanet(); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("取消")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::Separator();

        // 属性面板
        switch (m_selLevel)
        {
        case SelectionLevel::Universe:
            if (m_selUniverseIdx >= 0 && m_selUniverseIdx < static_cast<int>(m_universes.size()))
                renderUniverseProps(m_universes[m_selUniverseIdx]);
            break;
        case SelectionLevel::Galaxy:
            if (auto* g = selectedGalaxy()) renderGalaxyProps(*g);
            break;
        case SelectionLevel::Planet:
            if (auto* p = selectedPlanet()) renderPlanetProps(*p);
            break;
        default:
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "← 在左侧树形列表中选择宇宙、星系或星球\n以查看和编辑其属性。");
            break;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  主渲染入口（全屏三列：左树 | 中属性 | 右预览）
    // ─────────────────────────────────────────────────────────────────────────
    void UniverseEditor::render()
    {
        if (!m_loaded) reload();

        // 全屏覆盖主视口
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(vp->WorkSize, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);

        constexpr ImGuiWindowFlags kHostFlags =
            ImGuiWindowFlags_NoDecoration  |
            ImGuiWindowFlags_NoMove        |
            ImGuiWindowFlags_NoResize      |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNav;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if (!ImGui::Begin("##ue_host", nullptr, kHostFlags))
        {
            ImGui::End();
            ImGui::PopStyleVar(2);
            return;
        }
        ImGui::PopStyleVar(2);

        // 三列宽度
        constexpr float kLeftW    = 220.0f;
        constexpr float kPreviewW = 340.0f;
        const float totalW = ImGui::GetContentRegionAvail().x;
        const float midW   = std::max(100.0f, totalW - kLeftW - kPreviewW - 4.0f);

        // ── 左侧树 ──────────────────────────────────────────────────────────
        ImGui::BeginChild("##ue_left", ImVec2(kLeftW, 0.0f),
                          ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar);
        renderLeftTree();
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 2.0f);

        // ── 中部属性 ─────────────────────────────────────────────────────────
        ImGui::BeginChild("##ue_mid", ImVec2(midW, 0.0f),
                          ImGuiChildFlags_Borders);
        renderRightProperties();
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 2.0f);

        // ── 右侧预览 ─────────────────────────────────────────────────────────
        ImGui::BeginChild("##ue_preview", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_Borders);
        if (const UePlanet* p = selectedPlanet())
            renderPreviewPanel(*p);
        else
            ImGui::TextDisabled("\xe2\x86\x90 选择一个星球以查看预览");
        ImGui::EndChild();

        ImGui::End();
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  角色文件缓存（id → 纹理路径 / 显示名称）
    // ─────────────────────────────────────────────────────────────────────────
    void UniverseEditor::loadCharacterTexCache()
    {
        m_monsterTexCache.clear();
        m_monsterNameCache.clear();
        const std::filesystem::path dir = "assets/characters";
        if (!std::filesystem::exists(dir)) return;
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            const nlohmann::json j = readJsonFile(entry.path().string());
            if (j.is_null()) continue;
            const std::string id  = j.value("id", "");
            const std::string tex = j.value("texture", "");
            const std::string nm  = j.value("display_name", id);
            if (!id.empty())
            {
                if (!tex.empty()) m_monsterTexCache[id]  = tex;
                m_monsterNameCache[id] = nm;
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  预览面板（星球选中时右侧显示）
    // ─────────────────────────────────────────────────────────────────────────
    void UniverseEditor::renderPreviewPanel(const UePlanet& p)
    {
        // 瓦片 key → 纹理路径（固定映射）
        static const std::unordered_map<std::string, std::string> kTileTexMap{
            {"ground_stone", "assets/textures/Tiles/stone.svg"},
            {"ground_dirt",  "assets/textures/Tiles/dirt.svg"},
            {"ground_grass", "assets/textures/Tiles/grass.svg"},
        };
        constexpr float kImgW = 88.0f;
        constexpr float kImgH = 88.0f;

        ImGui::SeparatorText((p.displayName + "  预览").c_str());
        ImGui::TextDisabled("星球半径 %.0f px  大气高度 %.0f px",
                            p.planetRadius, p.atmosphereHeight);
        ImGui::Spacing();

        // ── 怪物预览 ──────────────────────────────────────────────────────────
        ImGui::SeparatorText("怪物预览");
        if (!p.monsterSpawn.enabled || p.monsterSpawn.types.empty())
        {
            ImGui::TextDisabled("未配置怪物生成");
        }
        else
        {
            ImGui::BeginChild("##mon_preview",
                ImVec2(0.0f, kImgH + ImGui::GetTextLineHeightWithSpacing() * 2.0f + 6.0f),
                ImGuiChildFlags_None,
                ImGuiWindowFlags_HorizontalScrollbar);

            for (const auto& mt : p.monsterSpawn.types)
            {
                // 查缓存
                auto texIt  = m_monsterTexCache.find(mt.id);
                auto nameIt = m_monsterNameCache.find(mt.id);
                const std::string texPath = (texIt  != m_monsterTexCache.end())  ? texIt->second  : "";
                const std::string label   = (nameIt != m_monsterNameCache.end()) ? nameIt->second : mt.id;

                ImGui::BeginGroup();

                // 图片或占位框
                unsigned int glTex = 0;
                if (!texPath.empty() && m_texLoader)
                    glTex = m_texLoader(texPath);

                if (glTex != 0)
                    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(glTex)),
                                 ImVec2(kImgW, kImgH));
                else
                {
                    ImGui::Dummy(ImVec2(kImgW, kImgH));
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    const ImVec2 p0 = ImGui::GetItemRectMin();
                    const ImVec2 p1 = ImGui::GetItemRectMax();
                    dl->AddRectFilled(p0, p1, IM_COL32(40, 40, 50, 200), 4.0f);
                    dl->AddRect(p0, p1, IM_COL32(90, 90, 110, 200), 4.0f);
                    dl->AddText({p0.x + 4.0f, p0.y + (kImgH - ImGui::GetTextLineHeight()) * 0.5f},
                                IM_COL32(140, 140, 160, 255), "?");
                }

                // 显示名 + 权重
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kImgW);
                ImGui::TextUnformatted(label.c_str());
                ImGui::PopTextWrapPos();
                char wbuf[16];
                std::snprintf(wbuf, sizeof(wbuf), "x%.1f", mt.weight);
                ImGui::TextDisabled("%s", wbuf);

                ImGui::EndGroup();
                ImGui::SameLine(0.0f, 6.0f);
            }
            ImGui::Dummy(ImVec2(0.0f, 0.0f)); // end row
            ImGui::EndChild();
        }

        ImGui::Spacing();

        // ── 地形预览 ──────────────────────────────────────────────────────────
        ImGui::SeparatorText("地形预览");
        if (p.tileMix.empty())
        {
            ImGui::TextDisabled("未配置瓦片混合");
        }
        else
        {
            ImGui::BeginChild("##tile_preview",
                ImVec2(0.0f, kImgH + ImGui::GetTextLineHeightWithSpacing() * 2.0f + 6.0f),
                ImGuiChildFlags_None,
                ImGuiWindowFlags_HorizontalScrollbar);

            for (const auto& tm : p.tileMix)
            {
                auto it = kTileTexMap.find(tm.key);
                const std::string texPath = (it != kTileTexMap.end())
                    ? it->second
                    : "assets/textures/Tiles/tileset.svg";

                ImGui::BeginGroup();

                unsigned int glTex = 0;
                if (m_texLoader) glTex = m_texLoader(texPath);

                if (glTex != 0)
                    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(glTex)),
                                 ImVec2(kImgW, kImgH));
                else
                {
                    ImGui::Dummy(ImVec2(kImgW, kImgH));
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    const ImVec2 p0 = ImGui::GetItemRectMin();
                    const ImVec2 p1 = ImGui::GetItemRectMax();
                    dl->AddRectFilled(p0, p1, IM_COL32(30, 50, 30, 200), 4.0f);
                    dl->AddRect(p0, p1, IM_COL32(60, 110, 60, 200), 4.0f);
                }

                // key 缩写（从 ground_ 中取后半段）
                std::string shortKey = tm.key;
                if (shortKey.rfind("ground_", 0) == 0) shortKey = shortKey.substr(7);
                if (shortKey.rfind("decor_",  0) == 0) shortKey = shortKey.substr(6);
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kImgW);
                ImGui::TextUnformatted(shortKey.c_str());
                ImGui::PopTextWrapPos();
                char wbuf[16];
                std::snprintf(wbuf, sizeof(wbuf), "x%.1f", tm.weight);
                ImGui::TextDisabled("%s", wbuf);

                ImGui::EndGroup();
                ImGui::SameLine(0.0f, 6.0f);
            }
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
            ImGui::EndChild();
        }
    }


} // namespace game::scene
