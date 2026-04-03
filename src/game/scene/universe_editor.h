#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>

// ─────────────────────────────────────────────────────────────────────────────
//  宇宙编辑器（Universe Editor）
//  树形结构：宇宙 → 星系 → 星球（含大气/半径/瓦片比例/怪物生成）
//  所有配置变更立即写回 JSON 文件，不依赖 GameScene 内部状态。
// ─────────────────────────────────────────────────────────────────────────────

namespace game::scene
{
    // ── 怪物生成条目 ──────────────────────────────────────────────────────────
    struct UeMonsterType
    {
        std::string id    = "slime";
        float       weight = 1.0f;
    };

    struct UeMonsterSpawn
    {
        bool    enabled          = true;
        float   spawnIntervalSec = 1.6f;
        int     maxMonsters      = 10;
        float   innerRadius      = 420.0f;
        float   outerRadius      = 960.0f;
        float   cleanupRadius    = 1500.0f;
        std::vector<UeMonsterType> types;
    };

    // ── 瓦片混合条目 ─────────────────────────────────────────────────────────
    struct UeTileMix
    {
        std::string key    = "ground_stone";
        float       weight = 1.0f;
    };

    // ── 星球 ─────────────────────────────────────────────────────────────────
    struct UePlanet
    {
        std::string  id              = "new_planet";
        std::string  displayName     = "新星球";
        std::string  filePath;               // assets/planets/<id>.json
        float        atmosphereHeight = 800.0f;
        float        planetRadius     = 3000.0f;
        std::string  mapFile          = "assets/maps/level0.tmj";
        std::string  tileCatalog      = "assets/ground_tiles/tile_kinds.json";
        std::vector<std::string> characters;
        std::vector<UeTileMix>   tileMix;
        UeMonsterSpawn           monsterSpawn;
        bool dirty = false;
    };

    // ── 星系 ─────────────────────────────────────────────────────────────────
    struct UeGalaxy
    {
        std::string  id          = "new_galaxy";
        std::string  displayName = "新星系";
        std::string  filePath;               // assets/galaxies/<id>.json
        int          starCount   = 200;
        int          armCount    = 4;
        std::vector<std::string> planetIds;  // 仅记录 id，星球数据在 UePlanet 中
        bool dirty = false;
    };

    // ── 宇宙 ─────────────────────────────────────────────────────────────────
    struct UeUniverse
    {
        std::string  id             = "default_universe";
        std::string  displayName    = "默认宇宙";
        std::string  filePath;               // assets/universe/<id>.json
        float        galaxyDensity  = 0.5f;
        std::vector<std::string> galaxyIds;
        bool dirty = false;
    };

    // ── 编辑器主类 ────────────────────────────────────────────────────────────
    class UniverseEditor
    {
    public:
        UniverseEditor() = default;

        // 纹理加载回调（由 GameScene 注入 getGLTexture 绑定）
        using TexLoader = std::function<unsigned int(const std::string&)>;
        void setTexLoader(TexLoader fn) { m_texLoader = std::move(fn); }

        // 从磁盘扫描并加载全部资产
        void reload();
        // 渲染 ImGui 面板（应在 ImGui::NewFrame 之后调用）
        void render();

        bool isOpen() const { return m_open; }
        void setOpen(bool v) { m_open = v; }

    private:
        bool m_open = true;

        // ── 树节点选择状态 ────────────────────────────────────────────────────
        enum class SelectionLevel { None, Universe, Galaxy, Planet };
        SelectionLevel m_selLevel  = SelectionLevel::None;
        int m_selUniverseIdx = -1;
        int m_selGalaxyIdx   = -1;
        int m_selPlanetIdx   = -1;

        // ── 数据 ──────────────────────────────────────────────────────────────
        std::vector<UeUniverse> m_universes;
        std::vector<UeGalaxy>   m_galaxies;
        std::vector<UePlanet>   m_planets;

        bool m_loaded = false;

        // ── 加载 / 保存辅助 ───────────────────────────────────────────────────
        void loadUniverses();
        void loadGalaxies();
        void loadPlanets();

        void saveUniverse(UeUniverse& u);
        void saveGalaxy(UeGalaxy& g);
        void savePlanet(UePlanet& p);

        static UeMonsterSpawn  parseMonsterSpawn(const nlohmann::json& j);
        static nlohmann::json  serializeMonsterSpawn(const UeMonsterSpawn& ms);
        static std::vector<UeTileMix> parseTileMix(const nlohmann::json& j);
        static nlohmann::json  serializeTileMix(const std::vector<UeTileMix>& tm);

        // ── 渲染子面板 ────────────────────────────────────────────────────────
        void renderLeftTree();
        void renderRightProperties();

        void renderUniverseProps(UeUniverse& u);
        void renderGalaxyProps(UeGalaxy& g);
        void renderPlanetProps(UePlanet& p);

        void renderTileMixEditor(std::vector<UeTileMix>& mix, bool& dirty);
        void renderMonsterSpawnEditor(UeMonsterSpawn& ms, bool& dirty);

        UeGalaxy*  selectedGalaxy();
        UePlanet*  selectedPlanet();

        // ── 新建辅助 ──────────────────────────────────────────────────────────
        void createNewUniverse();
        void createNewGalaxy();
        void createNewPlanet();

        int findPlanetIndexById(const std::string& id) const;
        int findGalaxyIndexById(const std::string& id) const;

        // 新建名称缓冲
        char m_newUniverseName[64] = {};
        char m_newGalaxyName[64]   = {};
        char m_newPlanetName[64]   = {};

        // 纹理加载回调及缓存
        TexLoader m_texLoader;
        std::unordered_map<std::string, std::string> m_monsterTexCache;  // id → texture path
        std::unordered_map<std::string, std::string> m_monsterNameCache; // id → display_name

        // 预览面板
        void renderPreviewPanel(const UePlanet& p);
        void loadCharacterTexCache();
    };

} // namespace game::scene
