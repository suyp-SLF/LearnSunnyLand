// DNF 风格地下城地形生成器实现
#include "dnf_terrain_generator.h"
#include "../../engine/world/world_config.h"
#include <cmath>

namespace game::scene
{
    DnfTerrainGenerator::DnfTerrainGenerator(const engine::world::WorldConfig &config)
        : engine::world::TerrainGenerator(config)
    {
    }

    // ── 判断当前 X 是否落在 section 边界的隔墙区 ──────────────────────────────
    bool DnfTerrainGenerator::isWallZone(int tileX) const
    {
        // 每个 section 的最后 WALL_WIDTH 列是墙（从第 SECTION_W-WALL_WIDTH 开始）
        int posInSection = ((tileX % SECTION_W) + SECTION_W) % SECTION_W;
        return posInSection >= (SECTION_W - WALL_WIDTH);
    }

    // ── 判断当前位置是否有浮空平台 ────────────────────────────────────────────
    // 规则：每个 section 中间区域随机放 2-4 条平台，各有固定高度和横向范围
    bool DnfTerrainGenerator::isPlatformAt(int tileX, int tileY, int sectionIdx, int biome) const
    {
        // 房间只有4格高，无空间放浮空平台
        if (ROOM_HEIGHT <= 5) return false;

        // 平台只出现在房间内（天花板到地板之间）
        if (tileY <= ROOM_TOP || tileY >= ROOM_BOTTOM) return false;

        // 隔墙区无平台
        if (isWallZone(tileX)) return false;

        int posInSection = ((tileX % SECTION_W) + SECTION_W) % SECTION_W;

        // 每个 section 生成若干平台：用哈希决定平台的 Y、X起点、X宽度
        // 平台数量：洞穴/矿山多，草原少
        int numPlatforms = 2;
        switch (biome)
        {
        case 1: numPlatforms = 3; break; // 森林
        case 2: numPlatforms = 4; break; // 岩地
        case 3: numPlatforms = 4; break; // 矿山
        case 4: numPlatforms = 5; break; // 洞穴
        }

        for (int p = 0; p < numPlatforms; ++p)
        {
            uint64_t h = hash2(sectionIdx * 100 + p, 777);
            int platY   = ROOM_TOP + 3 + static_cast<int>(h % static_cast<uint64_t>(ROOM_HEIGHT - 5));
            int platX0  = 8  + static_cast<int>(hash2(sectionIdx * 100 + p, 1) % 20);
            int platLen = 8  + static_cast<int>(hash2(sectionIdx * 100 + p, 2) % 16);
            int platX1  = platX0 + platLen;

            if (tileY == platY && posInSection >= platX0 && posInSection < platX1)
                return true;
        }
        return false;
    }

    // ── 判断当前位置是否嵌有矿石（仅在厚墙 / 地板下方） ─────────────────────
    bool DnfTerrainGenerator::isOreAt(int tileX, int tileY, int biome, uint64_t seed) const
    {
        if (biome != 3 && biome != 2) return false; // 只在矿山/岩地
        if (tileY < ROOM_BOTTOM + 2) return false;  // 只在地板以下深处

        uint64_t h = hash2(tileX + static_cast<int>(seed & 0xFFFF), tileY);
        float thresh = (biome == 3) ? 0.08f : 0.04f;
        return (h & 0xFFFF) < static_cast<uint64_t>(thresh * 65536.0);
    }

    // ── 生成核心：每个 16×16 区块 ────────────────────────────────────────────
    void DnfTerrainGenerator::generateChunk(int chunkX, int chunkY,
                                             std::vector<engine::world::TileData> &outTiles) const
    {
        using T = engine::world::TileType;
        const int CS = engine::world::WorldConfig::CHUNK_SIZE; // = 16

        // 2.5D 地板平面布局（chunk row 0，tile Y [0..7] = pixel [0..127]）：
        //   wy=0 (px  0-15):  后背景墙 WallDecor（暗蓝灰，无物理）
        //   wy=1 (px 16-31):  地板走廊层起始 GroundDecor（无物理）
        //   wy=2..5:          地板走廊层      GroundDecor
        //   wy=6+:            Air（屏幕下方不可见区域）
        outTiles.resize(static_cast<size_t>(CS * CS));
        for (int ly = 0; ly < CS; ++ly)
        {
            int wy   = chunkY * CS + ly;
            T   type = (wy == 0)          ? T::WallDecor
                     : (wy >= 1 && wy <= 5) ? T::GroundDecor
                     :                        T::Air;
            // 整行同类型，直接 std::fill 填充一整行
            std::fill(outTiles.begin() + ly * CS, outTiles.begin() + (ly + 1) * CS,
                      engine::world::TileData(type));
        }
    }

} // namespace game::scene
