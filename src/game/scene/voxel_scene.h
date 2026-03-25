#pragma once

#include "../../engine/scene/scene.h"
#include "../inventory/inventory.h"
#include "../route/route_data.h"
#include "../weapon/weapon.h"
#include "../skill/star_skill.h"
#include "../world/time_of_day_system.h"
#include "../weather/weather_system.h"
#include <SDL3/SDL.h>
#include <cstdint>
#include <glm/glm.hpp>
#include <array>
#include <unordered_map>
#include <vector>

namespace game::scene
{
    class VoxelScene final : public engine::scene::Scene
    {
    public:
        VoxelScene(const std::string &name,
                   engine::core::Context &context,
                   engine::scene::SceneManager &sceneManager);

        void init() override;
        void update(float dt) override;
        void render() override;
        void handleInput() override;
        void clean() override;

        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 color;
            glm::vec3 normal;
        };

        enum class VoxelMonsterType : uint8_t
        {
            Slime,
            Wolf,
            WhiteApe,
        };

        struct VoxelMonster
        {
            glm::vec3 pos{0.0f};
            glm::vec3 velocity{0.0f};
            VoxelMonsterType type = VoxelMonsterType::Slime;
            float health = 30.0f;
            float maxHealth = 30.0f;
            float attackCooldown = 0.0f;
            float hurtFlash = 0.0f;
            bool onGround = false;
        };

        struct VoxelChunkMesh
        {
            int chunkX = 0;
            int chunkZ = 0;
            unsigned int vao = 0;
            unsigned int vbo = 0;
            int vertexCount = 0;
            bool dirty = true;
            bool densityCacheDirty = true;
            bool generated = false;
            std::vector<unsigned char> voxels;
            std::vector<float> densities;
            std::vector<float> cornerDensityCache;
        };

        enum class SettingsPage : uint8_t
        {
            World,
            Combat,
            Input,
            Diagnostics,
        };

        enum class SetupPhase : uint8_t
        {
            PlanetSelect,
            RouteSelect,
            Playing,
        };

    private:

        struct TargetBlock
        {
            bool hit = false;
            glm::ivec3 block{0, 0, 0};
            glm::ivec3 place{0, 0, 0};
        };

        static constexpr int WORLD_Y = 24;
        static constexpr int CHUNK_SIZE_X = 16;
        static constexpr int CHUNK_SIZE_Z = 16;
        static constexpr int LOAD_CHUNK_RADIUS = 4;
        static constexpr int KEEP_CHUNK_RADIUS = 6;

        SDL_GLContext m_glContext = nullptr;
        unsigned int m_shader = 0;
        unsigned int m_monsterVao = 0;
        unsigned int m_monsterVbo = 0;
        int m_monsterVertexCount = 0;
        unsigned int m_viewModelVao = 0;
        unsigned int m_viewModelVbo = 0;
        int m_viewModelVertexCount = 0;
        unsigned int m_playerVao = 0;
        unsigned int m_playerVbo = 0;
        int m_playerVertexCount = 0;
        using DrawArraysProc = void(*)(unsigned int, int, int);
        using CullFaceProc = void(*)(unsigned int);
        using Uniform3fvProc = void(*)(int, int, const float *);
        using Uniform1fProc = void(*)(int, float);
        DrawArraysProc m_glDrawArrays = nullptr;
        CullFaceProc m_glCullFace = nullptr;
        Uniform3fvProc m_glUniform3fv = nullptr;
        Uniform1fProc m_glUniform1f = nullptr;

        std::unordered_map<int64_t, VoxelChunkMesh> m_chunkMeshes;
        std::vector<int64_t> m_activeChunkKeys;
        glm::vec3 m_cameraPos{24.0f, 11.0f, 42.0f};
        float m_yaw = -90.0f;
        float m_pitch = -18.0f;
        glm::vec2 m_lastMousePos{0.0f, 0.0f};
        bool m_firstMouseFrame = true;
        bool m_prevLeftMouse = false;
        bool m_prevRightMouse = false;
        bool m_prevInventoryKey = false;
        bool m_prevSettingsKey = false;
        bool m_prevSkillKey = false;
        bool m_prevPerspectiveKey = false;
        bool m_prevPauseKey = false;
        std::array<bool, game::weapon::WeaponBar::SLOTS> m_prevWeaponKeys{};
        bool m_showInventory = false;
        bool m_showSettings = false;
        bool m_showPauseMenu = false;
        bool m_thirdPersonView = false;
        bool m_mouseCaptured = false;
        bool m_showInputHints = true;
        bool m_highlightTargetBlock = true;
        bool m_showManagerDetails = false;
        float m_thirdPersonDistance = 5.6f;
        int m_chunkLoadBudget = 3;
        int m_chunkMeshBudget = 2;
        SettingsPage m_settingsPage = SettingsPage::World;
        SetupPhase m_setupPhase = SetupPhase::PlanetSelect;
        game::route::RouteData m_routeData;
        int m_selectedPlanetIndex = 0;

        game::inventory::Inventory m_inventory;
        game::weapon::WeaponBar m_weaponBar;
        std::array<game::inventory::InventorySlot, 6> m_starSockets;
        std::array<float, 6> m_skillCooldowns{};

        game::world::TimeOfDaySystem m_timeOfDaySystem;
        game::weather::WeatherSystem m_weatherSystem;

        float m_hp = 100.0f;
        float m_maxHp = 100.0f;
        float m_starEnergy = 100.0f;
        float m_maxStarEnergy = 100.0f;
        float m_attack = 24.0f;
        float m_defense = 6.0f;
        float m_baseMoveSpeed = 8.5f;
        float m_dashCooldown = 0.0f;
        bool m_windStarEquipped = false;
        bool m_iceStarEquipped = false;
        float m_monsterSpawnTimer = 0.0f;
        uint64_t m_rngState = 0xC0FFEE123456789ULL;
        int m_routeProgressIndex = -1;
        bool m_routeObjectiveReached = false;
        bool m_showSettlement = false;

        int m_selectedInventorySlot = -1;
        std::vector<VoxelMonster> m_monsters;
        std::vector<int64_t> m_pendingChunkLoads;

        void initImGui();
        void initGLResources();
        void initGameplaySystems();
        void initChunkMeshes();
        void generateWorld();
        void rebuildMesh();
        void rebuildChunkMesh(VoxelChunkMesh &chunk);
        void rebuildDirtyChunkMeshes();
        void updateStreamedChunks();
        void releaseChunk(VoxelChunkMesh &chunk);
        void rebuildMonsterMesh();
        void rebuildViewModelMesh();
        void rebuildPlayerMesh();
        void renderOverlay(const TargetBlock &target);
        void renderPlanetSelectUI();
        void renderRouteSelectUI();
        void renderWeaponBar();
        void renderSkillHUD();
        void renderPlayerStatusHUD();
        void renderInventoryUI();
        void renderSettingsUI();
        void renderInputHintsUI();
        void renderManagerDiagnosticsUI();
        void tickGameplaySystems(float dt, int displayW, int displayH);
        void tickStarSkillPassives(float dt);
        void updateMonsters(float dt);
        void spawnMonster();
        int findGroundY(int x, int z) const;
        void triggerAttackStarSkills(const glm::vec3 &attackCenter);
        void triggerActiveStarSkills();
        void performWeaponAttack(const TargetBlock &target);
        void explodeBlocks(const glm::ivec3 &center, int radius);
        int damageMonstersInRadius(const glm::vec3 &center, float radius, float damage, const glm::vec3 &impulse);
        int slashMonsters(const glm::vec3 &origin, const glm::vec3 &forward, float range, float radius, float damage);
        void updateMouseCapture();
        glm::vec3 getCameraEyePosition() const;
        glm::vec3 getRenderCameraPosition() const;
        glm::vec3 getPlayerModelForward() const;
        glm::ivec2 worldToChunkXZ(int x, int z) const;
        int64_t chunkKey(int chunkX, int chunkZ) const;
        int chunkVoxelIndex(int localX, int y, int localZ) const;
        VoxelChunkMesh *findChunk(int chunkX, int chunkZ);
        const VoxelChunkMesh *findChunk(int chunkX, int chunkZ) const;
        VoxelChunkMesh &ensureChunk(int chunkX, int chunkZ);
        void requestChunkLoad(int chunkX, int chunkZ);
        void generateChunk(VoxelChunkMesh &chunk);
        void markChunkDirtyAt(int x, int z);
        void processChunkStreamingBudget(int loadBudget, int meshBudget);
        bool isRouteSetupComplete() const;
        bool isAdjacent(glm::ivec2 a, glm::ivec2 b) const;
        int pathIndexOf(glm::ivec2 cell) const;
        glm::ivec2 worldToRouteCell(const glm::vec3 &worldPos) const;
        void updateRouteProgress();
        void handleRouteCellClick(int cx, int cy, bool rightClick);
        void confirmRouteSelection();
        glm::vec3 getCellWorldCenter(glm::ivec2 cell) const;
        int worldWidth() const;
        int worldDepth() const;
        uint64_t nextRand();
        float randFloat();

        int voxelIndex(int x, int y, int z) const;
        int cornerDensityIndex(int localX, int y, int localZ) const;
        bool isInside(int x, int y, int z) const;
        bool isSolid(int x, int y, int z) const;
        float densityAt(int x, int y, int z) const;
        unsigned char voxelAt(int x, int y, int z) const;
        unsigned char rawVoxelAt(int x, int y, int z) const;
        void setVoxel(int x, int y, int z, unsigned char value);
        bool applyDensityBrush(const glm::vec3 &center, float radius, float delta, unsigned char fillMaterial);
        void updateChunkDensityCache(VoxelChunkMesh &chunk);

        glm::vec3 getForward() const;
        glm::vec3 getRight() const;
        glm::vec3 blockColor(unsigned char type, float shade) const;
        TargetBlock raycastBlock() const;
    };
}