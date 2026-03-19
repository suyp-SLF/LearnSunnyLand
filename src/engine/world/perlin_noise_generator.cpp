// 柏林噪声地形生成器
#include "perlin_noise_generator.h"
#include "FastNoiseLite.h"
#include <memory>
#include <cmath>

namespace engine::world
{
    PerlinNoiseGenerator::PerlinNoiseGenerator(const WorldConfig &config)
        : TerrainGenerator(config)
    {
        m_noise = std::make_unique<FastNoiseLite>();
        m_noise->SetSeed(static_cast<int>(config.seed));
        m_noise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        m_noise->SetFrequency(config.noiseScale);
    }

    PerlinNoiseGenerator::~PerlinNoiseGenerator() = default;

    float PerlinNoiseGenerator::getHeightAt(int worldX, int worldY) const
    {
        // 2D 噪声值范围 [-1, 1]，映射到 [0, amplitude]
        float noiseVal = m_noise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldY));
        return m_config.amplitude * (noiseVal * 0.5f + 0.5f); // 0 到 amplitude
    }

    void PerlinNoiseGenerator::generateChunk(int chunkX, int chunkY, std::vector<TileData> &outTiles) const
    {
        outTiles.resize(WorldConfig::CHUNK_SIZE * WorldConfig::CHUNK_SIZE);

        int baseX = chunkX * WorldConfig::CHUNK_SIZE;
        int baseY = chunkY * WorldConfig::CHUNK_SIZE;

        for (int ly = 0; ly < WorldConfig::CHUNK_SIZE; ++ly)
        {
            for (int lx = 0; lx < WorldConfig::CHUNK_SIZE; ++lx)
            {
                int worldX = baseX + lx;
                int worldY = baseY + ly;

                // 使用X坐标生成地表高度（类似泰拉瑞亚）
                float noiseVal = m_noise->GetNoise(static_cast<float>(worldX), 0.0f);
                float heightOffset = m_config.amplitude * (noiseVal * 0.5f + 0.5f);
                int surfaceY = m_config.seaLevel - static_cast<int>(heightOffset);

                TileType type = TileType::Air;

                if (worldY < surfaceY)
                {
                    // 地表以上是空气
                    type = TileType::Air;
                }
                else if (worldY == surfaceY)
                {
                    // 地表是草地
                    type = TileType::Grass;
                }
                else if (worldY < surfaceY + m_config.grassDepth)
                {
                    // 草地下面是泥土
                    type = TileType::Dirt;
                }
                else
                {
                    // 更深处是石头
                    type = TileType::Stone;
                }

                outTiles[ly * WorldConfig::CHUNK_SIZE + lx] = TileData(type);
            }
        }
    }
} // namespace engine::world