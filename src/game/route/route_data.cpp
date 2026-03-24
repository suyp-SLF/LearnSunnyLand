#include "route_data.h"

namespace game::route
{
    namespace
    {
        const std::array<PlanetPreset, 4> kPlanetPresets{{
            {PlanetType::Verdant, "盖娅原野", "温暖宜居，森林与草原交错，昼夜稳定。", 0x11F0A123ULL, {42, 28, 14, 8, 8}, 0, 1.00f, 10, 20, 4, 320.0f},
            {PlanetType::Emberfall, "余烬断层", "火山灰覆盖的大地，岩地与矿山比例更高。", 0x29BC7721ULL, {14, 8, 34, 26, 18}, -4, 1.22f, 4, 10, 9, 260.0f},
            {PlanetType::Frostveil, "霜纱冰冠", "空气稀薄而清澈，平原开阔，长昼长夜明显。", 0x47D3EE19ULL, {38, 10, 18, 12, 22}, 6, 0.82f, 3, 8, 12, 420.0f},
            {PlanetType::Hollowreach, "空洞深界", "地下空腔密集，洞穴与岩地纵横交织。", 0x98AD5013ULL, {12, 10, 24, 14, 40}, -8, 1.10f, 2, 6, 14, 280.0f},
        }};
    }

    // 简单的 xorshift64 随机数生成器
    static uint64_t xorshift64(uint64_t x)
    {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        return x;
    }

    const std::array<PlanetPreset, 4>& RouteData::planetPresets()
    {
        return kPlanetPresets;
    }

    const PlanetPreset& RouteData::selectedPlanetPreset() const
    {
        for (const auto &preset : kPlanetPresets)
        {
            if (preset.type == selectedPlanet)
                return preset;
        }
        return kPlanetPresets.front();
    }

    const char* RouteData::planetName(PlanetType type)
    {
        for (const auto &preset : kPlanetPresets)
        {
            if (preset.type == type)
                return preset.name;
        }
        return "未知星球";
    }

    void RouteData::applyPlanetPreset(const PlanetPreset &preset)
    {
        selectedPlanet = preset.type;
        planetSeed = preset.seedBias;
        planetSeaLevelOffset = preset.seaLevelOffset;
        planetAmplitudeScale = preset.amplitudeScale;
        planetTreeMin = preset.treeMinTrunkHeight;
        planetTreeMax = preset.treeMaxTrunkHeight;
        planetTreeSpacing = preset.treeSpacing;
        dayLengthSeconds = preset.dayLengthSeconds;
    }

    void RouteData::generateTerrain(uint64_t seed)
    {
        const auto &preset = selectedPlanetPreset();
        seed ^= preset.seedBias;

        // 为每个格子生成地形类型
        constexpr int N = static_cast<int>(MAP_SIZE);
        for (int y = 0; y < N; ++y)
        {
            for (int x = 0; x < N; ++x)
            {
                uint64_t s = xorshift64(seed ^ (static_cast<uint64_t>(x) * 0x9E3779B97F4A7C15ULL
                                               + static_cast<uint64_t>(y) * 0x6C62272E07BB0142ULL));
                s = xorshift64(s);
                int r = static_cast<int>(s % 100);
                int cumulative = 0;
                int index = 0;
                for (; index < static_cast<int>(preset.terrainWeights.size()); ++index)
                {
                    cumulative += preset.terrainWeights[index];
                    if (r < cumulative)
                        break;
                }
                terrain[y][x] = static_cast<CellTerrain>(std::min(index, 4));
            }
        }

        // 随机选定一个目标格（不在边缘，强制为 Mountain）
        uint64_t os = xorshift64(seed * 0xDEADBEEFCAFEBABEULL);
        os = xorshift64(os);
        int ox = 2 + static_cast<int>(os % static_cast<uint64_t>(N - 4));
        os = xorshift64(os + 1);
        int oy = 2 + static_cast<int>(os % static_cast<uint64_t>(N - 4));
        objectiveCell = {ox, oy};
        terrain[oy][ox] = CellTerrain::Mountain;

        // 更新：检查目标格是否在已有路线上
        objectiveZone = -1;
        for (int i = 0; i < static_cast<int>(path.size()); ++i)
        {
            if (path[i] == objectiveCell)
            {
                objectiveZone = i;
                break;
            }
        }
    }

} // namespace game::route
