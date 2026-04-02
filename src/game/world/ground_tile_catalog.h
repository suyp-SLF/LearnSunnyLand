#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::world
{
    class ChunkManager;
    enum class TileType : uint8_t;
}

namespace game::world
{
    struct GroundTileKind
    {
        std::string key;
        std::string displayName;
        engine::world::TileType tileType;
        float heightPx = 0.0f;
    };

    class GroundTileCatalog
    {
    public:
        bool loadFromFile(const std::string& filePath);

        const std::vector<GroundTileKind>& kinds() const { return m_kinds; }
        const GroundTileKind* kindForKey(const std::string& key) const;
        const GroundTileKind* kindForType(engine::world::TileType type) const;
        std::optional<engine::world::TileType> typeForKey(const std::string& key) const;
        std::optional<float> heightForType(engine::world::TileType type) const;

        bool placeTileByKey(engine::world::ChunkManager& chunkManager,
                            int worldTileX,
                            int worldTileY,
                            const std::string& key) const;

        void fillRectByKey(engine::world::ChunkManager& chunkManager,
                           int minTileX,
                           int minTileY,
                           int maxTileX,
                           int maxTileY,
                           const std::string& key) const;

    private:
        static std::optional<engine::world::TileType> parseTileType(const std::string& typeName);

        std::vector<GroundTileKind> m_kinds;
        std::unordered_map<std::string, engine::world::TileType> m_typeByKey;
        std::unordered_map<int, float> m_heightByType;
    };
}
