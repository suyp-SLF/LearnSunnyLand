#include "ground_tile_catalog.h"

#include "../../engine/world/chunk_manager.h"
#include "../../engine/world/tile_info.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace game::world
{
namespace
{
    std::string toLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }
}

std::optional<engine::world::TileType> GroundTileCatalog::parseTileType(const std::string& typeName)
{
    const std::string name = toLower(typeName);
    using engine::world::TileType;

    if (name == "air") return TileType::Air;
    if (name == "stone") return TileType::Stone;
    if (name == "dirt") return TileType::Dirt;
    if (name == "grass") return TileType::Grass;
    if (name == "wood") return TileType::Wood;
    if (name == "leaves") return TileType::Leaves;
    if (name == "ore") return TileType::Ore;
    if (name == "gravel") return TileType::Gravel;
    if (name == "grounddecor" || name == "ground_decor") return TileType::GroundDecor;
    if (name == "walldecor" || name == "wall_decor") return TileType::WallDecor;
    return std::nullopt;
}

bool GroundTileCatalog::loadFromFile(const std::string& filePath)
{
    m_kinds.clear();
    m_typeByKey.clear();
    m_heightByType.clear();

    nlohmann::json json;
    std::ifstream in(filePath);
    if (!in.is_open())
        return false;

    try
    {
        in >> json;
    }
    catch (const std::exception&)
    {
        return false;
    }

    if (!json.contains("kinds") || !json["kinds"].is_array())
        return false;

    for (const auto& item : json["kinds"])
    {
        if (!item.is_object())
            continue;

        const std::string key = item.value("key", "");
        const std::string typeName = item.value("tile_type", "");
        if (key.empty() || typeName.empty())
            continue;

        const auto maybeType = parseTileType(typeName);
        if (!maybeType.has_value())
            continue;

        GroundTileKind kind;
        kind.key = key;
        kind.displayName = item.value("display_name", key);
        kind.tileType = maybeType.value();
        kind.heightPx = item.value("height_px", 0.0f);

        m_kinds.push_back(kind);
        m_typeByKey[kind.key] = kind.tileType;
        m_heightByType[static_cast<int>(kind.tileType)] = kind.heightPx;
    }

    return !m_kinds.empty();
}

const GroundTileKind* GroundTileCatalog::kindForKey(const std::string& key) const
{
    for (const auto& kind : m_kinds)
    {
        if (kind.key == key)
            return &kind;
    }
    return nullptr;
}

const GroundTileKind* GroundTileCatalog::kindForType(engine::world::TileType type) const
{
    for (const auto& kind : m_kinds)
    {
        if (kind.tileType == type)
            return &kind;
    }
    return nullptr;
}

std::optional<engine::world::TileType> GroundTileCatalog::typeForKey(const std::string& key) const
{
    const auto it = m_typeByKey.find(key);
    if (it == m_typeByKey.end())
        return std::nullopt;
    return it->second;
}

std::optional<float> GroundTileCatalog::heightForType(engine::world::TileType type) const
{
    const auto it = m_heightByType.find(static_cast<int>(type));
    if (it == m_heightByType.end())
        return std::nullopt;
    return it->second;
}

bool GroundTileCatalog::placeTileByKey(engine::world::ChunkManager& chunkManager,
                                       int worldTileX,
                                       int worldTileY,
                                       const std::string& key) const
{
    const auto maybeType = typeForKey(key);
    if (!maybeType.has_value())
        return false;

    chunkManager.setTile(worldTileX, worldTileY, engine::world::TileData(maybeType.value()));
    return true;
}

void GroundTileCatalog::fillRectByKey(engine::world::ChunkManager& chunkManager,
                                      int minTileX,
                                      int minTileY,
                                      int maxTileX,
                                      int maxTileY,
                                      const std::string& key) const
{
    const auto maybeType = typeForKey(key);
    if (!maybeType.has_value())
        return;

    const int x0 = std::min(minTileX, maxTileX);
    const int x1 = std::max(minTileX, maxTileX);
    const int y0 = std::min(minTileY, maxTileY);
    const int y1 = std::max(minTileY, maxTileY);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
            chunkManager.setTileSilent(x, y, engine::world::TileData(maybeType.value()));
    }

    chunkManager.rebuildDirtyChunks();
}
}
