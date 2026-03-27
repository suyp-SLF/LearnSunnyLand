// DNF 风格地下城地形生成器
// 按路线生成类地下城与勇士的横版地图：
//   - 固定高度的房间（天花板 + 地板）
//   - 可左右无限延伸，每个路线 section 一个"房间带"
//   - 房间内有随机高度平台、矿石精灵点位等
//   - section 之间通过带通道的隔墙连接
#pragma once
#include "../../engine/world/terrain_generator.h"
#include "../../engine/world/tile_info.h"
#include "../../game/route/route_data.h"
#include <functional>
#include <cstdint>

namespace game::scene
{
    class DnfTerrainGenerator : public engine::world::TerrainGenerator
    {
    public:
        using BiomeLookup = std::function<int(int tileX)>;

        explicit DnfTerrainGenerator(const engine::world::WorldConfig &config);
        ~DnfTerrainGenerator() override = default;

        // 设置路线节区查询（tileX → CellTerrain int）
        void setBiomeLookup(BiomeLookup fn) { m_biomeLookup = std::move(fn); }

        void generateChunk(int chunkX, int chunkY,
                           std::vector<engine::world::TileData> &outTiles) const override;

        float getHeightAt(int /*worldX*/, int /*worldY*/) const override { return 0.f; }

    private:
        BiomeLookup m_biomeLookup;

        // 房间常数（瓦片单位）
        // Chunk::SIZE=8，chunk row 0 = tile Y [0..7] = pixel [0..127]
        // tile Y=0: 天花板  Y=1-4: 空气(走廊)  Y=5: 草(地面)  Y=6: 泥土  Y=7: 石头
        static constexpr int ROOM_TOP    = 1;   // 天花板起始行（wy=0 是1格石头）
        static constexpr int ROOM_BOTTOM = 5;   // 地板行（tile Y=5, pixel 80..95 = 草/石）
        static constexpr int ROOM_HEIGHT = ROOM_BOTTOM - ROOM_TOP; // = 4 格可行走空间

        // 每个路线格子宽度（来自 RouteData）
        static constexpr int SECTION_W = game::route::RouteData::TILES_PER_CELL; // 100

        // 隔墙宽度 & 通道高度
        static constexpr int WALL_WIDTH    = 2;
        static constexpr int PORTAL_HEIGHT = 3;  // 通道口高度（开口 wy=3,4）
        static constexpr int PORTAL_FLOOR  = ROOM_BOTTOM - PORTAL_HEIGHT; // = 2

        // 根据 sectionX 和局部X坐标判断是否是隔墙区
        bool isWallZone(int tileX) const;

        // 生成平台（section 内随机，由种子决定）
        bool isPlatformAt(int tileX, int tileY, int sectionIdx, int biome) const;

        // 生成矿石（Mountain 地形特有）
        bool isOreAt(int tileX, int tileY, int biome, uint64_t seed) const;

        // 内联哈希（无外部依赖）
        static uint64_t hash2(int x, int y)
        {
            uint64_t v = (static_cast<uint64_t>(x) * 2654435761ULL) ^
                         (static_cast<uint64_t>(y) * 2246822519ULL);
            v ^= v >> 33; v *= 0xff51afd7ed558ccdULL;
            v ^= v >> 33; v *= 0xc4ceb9fe1a85ec53ULL;
            v ^= v >> 33;
            return v;
        }
    };

} // namespace game::scene
