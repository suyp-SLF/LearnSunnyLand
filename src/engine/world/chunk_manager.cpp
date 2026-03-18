#include "chunk_manager.h"
#include "../core/context.h"
#include "world_config.h"
#include "tile_info.h"
#include <algorithm>

namespace engine::world
{
    ChunkManager::ChunkManager(const std::string &atlasTextureId,
                               const glm::ivec2 &tileSize,
                               engine::resource::ResourceManager *resMgr,
                               engine::physics::PhysicsManager *physicsMgr)
        : m_atlasTextureId(atlasTextureId),
          m_tileSize(tileSize),
          m_resMgr(resMgr),
          m_physicsMgr(physicsMgr)
    {
    }

    ChunkManager::~ChunkManager() = default;

    TileData &ChunkManager::tileAt(int worldX, int worldY)
    {
        int chunkX = worldX / Chunk::SIZE;
        int chunkY = worldY / Chunk::SIZE;
        if (worldX < 0)
            chunkX--;
        if (worldY < 0)
            chunkY--;

        auto it = m_chunks.find(encodeChunkKey(chunkX, chunkY));
        if (it == m_chunks.end())
        {
            static TileData air(TileType::Air);
            return air;
        }
        int localX = worldX - chunkX * Chunk::SIZE;
        int localY = worldY - chunkY * Chunk::SIZE;
        return it->second->tileAt(localX, localY);
    }

    void ChunkManager::setTile(int worldX, int worldY, TileData tile)
    {
        int chunkX = worldX / Chunk::SIZE;
        int chunkY = worldY / Chunk::SIZE;
        if (worldX < 0)
            chunkX--;
        if (worldY < 0)
            chunkY--;

        uint64_t key = encodeChunkKey(chunkX, chunkY);
        auto it = m_chunks.find(key);
        if (it == m_chunks.end())
        {
            loadChunk(chunkX, chunkY);
            it = m_chunks.find(key);
            if (it == m_chunks.end())
                return;
        }

        int localX = worldX - chunkX * Chunk::SIZE;
        int localY = worldY - chunkY * Chunk::SIZE;
        it->second->tileAt(localX, localY) = tile;
        it->second->setDirty();
        it->second->updatePhysicsBody(localX, localY, m_physicsMgr, WorldConfig::PIXELS_PER_METER);
    }

    void ChunkManager::updateVisibleChunks(const glm::vec2 &cameraPos, int viewDistanceInChunks)
    {
        int camChunkX = static_cast<int>(std::floor(cameraPos.x / (Chunk::SIZE * m_tileSize.x)));
        int camChunkY = static_cast<int>(std::floor(cameraPos.y / (Chunk::SIZE * m_tileSize.y)));

        std::vector<uint64_t> toUnload;
        for (const auto &[key, chunk] : m_chunks)
        {
            int cx = static_cast<int>(key >> 32);
            int cy = static_cast<int>(key & 0xFFFFFFFF);
            int distX = std::abs(cx - camChunkX);
            int distY = std::abs(cy - camChunkY);
            if (distX > viewDistanceInChunks || distY > viewDistanceInChunks)
            {
                toUnload.push_back(key);
            }
        }
        for (auto key : toUnload)
        {
            m_chunks.erase(key);
        }

        for (int dx = -viewDistanceInChunks; dx <= viewDistanceInChunks; ++dx)
        {
            for (int dy = -viewDistanceInChunks; dy <= viewDistanceInChunks; ++dy)
            {
                int cx = camChunkX + dx;
                int cy = camChunkY + dy;
                uint64_t key = encodeChunkKey(cx, cy);
                if (m_chunks.find(key) == m_chunks.end())
                {
                    loadChunk(cx, cy);
                }
            }
        }
    }

    void ChunkManager::loadChunk(int chunkX, int chunkY)
    {
        auto chunk = std::make_unique<Chunk>(chunkX, chunkY);

        for (int ly = 0; ly < Chunk::SIZE; ++ly)
        {
            for (int lx = 0; lx < Chunk::SIZE; ++lx)
            {
                int worldY = chunkY * Chunk::SIZE + ly;
                if (worldY == -8)
                {
                    chunk->tileAt(lx, ly) = engine::world::TileData(engine::world::TileType::Stone);
                }
                else if (worldY == 0)
                {
                    chunk->tileAt(lx, ly) = engine::world::TileData(engine::world::TileType::Dirt);
                }
                else
                {
                    chunk->tileAt(lx, ly) = engine::world::TileData(engine::world::TileType::Air);
                }
            }
        }
        chunk->createPhysicsBodies(m_physicsMgr, WorldConfig::TILE_SIZE, WorldConfig::PIXELS_PER_METER);
        chunk->buildMesh("assets/dimensions/tileset_atlas.svg", m_tileSize, m_resMgr);
        m_chunks[encodeChunkKey(chunkX, chunkY)] = std::move(chunk);
    }

    void ChunkManager::setTerrainGenerator(std::unique_ptr<TerrainGenerator> generator)
    {
    }

    void ChunkManager::unloadChunk(int chunkX, int chunkY)
    {
        auto it = m_chunks.find(encodeChunkKey(chunkX, chunkY));
        if (it != m_chunks.end())
        {
            it->second->destroyPhysicsBodies(m_physicsMgr);
            m_chunks.erase(it);
        }
    }

    void ChunkManager::renderAll(engine::core::Context &ctx) const
    {
        for (const auto &[_, chunk] : m_chunks)
        {
            chunk->draw(ctx);
        }
    }
} // namespace engine::world