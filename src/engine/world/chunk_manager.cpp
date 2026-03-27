#include "chunk_manager.h"
#include "terrain_generator.h"
#include "../core/context.h"
#include "../render/camera.h"
#include "../render/renderer.h"
#include "../resource/resource_manager.h"
#include "world_config.h"
#include "tile_info.h"
#include <algorithm>
#include <set>
#include <spdlog/spdlog.h>

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

    void ChunkManager::rebuildChunkMesh(Chunk &chunk)
    {
        if (!m_resMgr)
            return;

        if (m_resMgr->getGPUDevice() == nullptr)
        {
            if (engine::core::Context::Current)
            {
                chunk.buildMeshGL(m_atlasTextureId, m_tileSize, m_resMgr, engine::core::Context::Current->getRenderer());
            }
            return;
        }

        chunk.buildMesh(m_atlasTextureId, m_tileSize, m_resMgr);
    }

    TileData &ChunkManager::tileAt(int worldX, int worldY)
    {
        int cx, cy, lx, ly;
        worldToChunkCoords(worldX, worldY, cx, cy, lx, ly);
        auto it = m_chunks.find(encodeChunkKey(cx, cy));
        if (it == m_chunks.end())
        {
            static TileData air(TileType::Air);
            return air;
        }
        return it->second->tileAt(lx, ly);
    }

    void ChunkManager::setTile(int worldX, int worldY, TileData tile)
    {
        int cx, cy, lx, ly;
        worldToChunkCoords(worldX, worldY, cx, cy, lx, ly);
        uint64_t key = encodeChunkKey(cx, cy);
        auto it = m_chunks.find(key);
        if (it == m_chunks.end())
        {
            loadChunk(cx, cy);
            it = m_chunks.find(key);
            if (it == m_chunks.end())
                return;
        }

        TileData &currentTile = it->second->tileAt(lx, ly);
        if (currentTile.type == tile.type)
            return;

        currentTile = std::move(tile);
        it->second->setDirty();
        it->second->rebuildPhysicsBodies(m_physicsMgr, WorldConfig::PIXELS_PER_METER);
        rebuildChunkMesh(*it->second);
    }

    void ChunkManager::setTileSilent(int worldX, int worldY, TileData tile)
    {
        int cx, cy, lx, ly;
        worldToChunkCoords(worldX, worldY, cx, cy, lx, ly);
        uint64_t key = encodeChunkKey(cx, cy);
        auto it = m_chunks.find(key);
        if (it == m_chunks.end())
        {
            loadChunk(cx, cy);
            it = m_chunks.find(key);
            if (it == m_chunks.end())
                return;
        }

        TileData &currentTile = it->second->tileAt(lx, ly);
        if (currentTile.type == tile.type)
            return;

        currentTile = std::move(tile);
        it->second->setDirty(); // 只标脏，延迟重建
    }

    void ChunkManager::rebuildDirtyChunks()
    {
        for (auto &[key, chunk] : m_chunks)
        {
            if (chunk->isDirty())
            {
                chunk->rebuildPhysicsBodies(m_physicsMgr, WorldConfig::PIXELS_PER_METER);
                rebuildChunkMesh(*chunk);
            }
        }
    }

    glm::ivec2 ChunkManager::worldToTile(const glm::vec2 &worldPos) const
    {
        return {
            static_cast<int>(std::floor(worldPos.x / static_cast<float>(m_tileSize.x))),
            static_cast<int>(std::floor(worldPos.y / static_cast<float>(m_tileSize.y)))};
    }

    glm::vec2 ChunkManager::tileToWorld(const glm::ivec2 &tilePos) const
    {
        return {
            tilePos.x * static_cast<float>(m_tileSize.x),
            tilePos.y * static_cast<float>(m_tileSize.y)};
    }

    void ChunkManager::setHorizontalOnly(bool enable, float fixedWorldY)
    {
        m_horizontalOnly = enable;
        if (enable)
            m_fixedChunkRowY = static_cast<int>(std::floor(fixedWorldY / (Chunk::SIZE * m_tileSize.y)));
    }

    void ChunkManager::updateVisibleChunks(const glm::vec2 &cameraPos, int viewDistanceInChunks,
                                            int viewDistanceYOverride)
    {
        const int vdX = viewDistanceInChunks;
        // 横向单行模式：垂直视距=0，chunk row Y 锁定；否则使用 override 参数
        const int vdY = m_horizontalOnly ? 0
                      : (viewDistanceYOverride >= 0) ? viewDistanceYOverride
                      : viewDistanceInChunks;

        int camChunkX = static_cast<int>(std::floor(cameraPos.x / (Chunk::SIZE * m_tileSize.x)));
        int camChunkY = m_horizontalOnly ? m_fixedChunkRowY
                      : static_cast<int>(std::floor(cameraPos.y / (Chunk::SIZE * m_tileSize.y)));

        std::vector<uint64_t> toUnload;
        for (const auto &[key, chunk] : m_chunks)
        {
            int cx = static_cast<int>(key >> 32);
            int cy = static_cast<int>(key & 0xFFFFFFFF);
            int distX = std::abs(cx - camChunkX);
            int distY = std::abs(cy - camChunkY);
            if (distX > vdX || distY > vdY)
            {
                toUnload.push_back(key);
            }
        }
        for (auto key : toUnload)
        {
            int cx = static_cast<int>(key >> 32);
            int cy = static_cast<int>(key & 0xFFFFFFFFu);
            unloadChunk(cx, cy);
        }

        for (int dx = -vdX; dx <= vdX; ++dx)
        {
            for (int dy = -vdY; dy <= vdY; ++dy)
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

        // 仅在区块集合发生变化时打印（避免每帧日志）
        if (m_horizontalOnly && m_chunks.size() != m_prevChunkCount)
        {
            m_prevChunkCount = m_chunks.size();
            std::set<int> rows;
            for (const auto &[key, _] : m_chunks)
                rows.insert(static_cast<int>(static_cast<uint32_t>(key & 0xFFFFFFFFu)));
            std::string rowStr;
            for (int r : rows) rowStr += std::to_string(r) + " ";
            spdlog::info("[ChunkManager] loaded chunk rows: [{}] total={}", rowStr, m_chunks.size());
        }
    }

    void ChunkManager::loadChunk(int chunkX, int chunkY)
    {
        // 横向单行模式：拒绝加载非指定 chunk 行，防止 setTile/ore/tree 绕过约束
        if (m_horizontalOnly && chunkY != m_fixedChunkRowY)
        {
            spdlog::warn("[ChunkManager] BLOCKED load chunk ({},{}) fixedRow={}", chunkX, chunkY, m_fixedChunkRowY);
            return;
        }
        spdlog::debug("[ChunkManager] load chunk ({},{}) fixedRow={} horizontalOnly={}", chunkX, chunkY, m_fixedChunkRowY, m_horizontalOnly);

        auto chunk = std::make_unique<Chunk>(chunkX, chunkY);

        // 使用地形生成器生成瓦片
        if (m_terrainGenerator)
        {
            std::vector<TileData> tiles;
            m_terrainGenerator->generateChunk(chunkX, chunkY, tiles);

            for (int ly = 0; ly < Chunk::SIZE; ++ly)
            {
                for (int lx = 0; lx < Chunk::SIZE; ++lx)
                {
                    chunk->tileAt(lx, ly) = tiles[ly * Chunk::SIZE + lx];
                }
            }
        }

        chunk->createPhysicsBodies(m_physicsMgr, glm::vec2(m_tileSize), WorldConfig::PIXELS_PER_METER);
        rebuildChunkMesh(*chunk);
        m_chunks[encodeChunkKey(chunkX, chunkY)] = std::move(chunk);
    }

    void ChunkManager::setTerrainGenerator(std::unique_ptr<TerrainGenerator> generator)
    {
        m_terrainGenerator = std::move(generator);
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
        const glm::vec2 chunkWorldSize = glm::vec2(Chunk::SIZE * m_tileSize.x,
                                                   Chunk::SIZE * m_tileSize.y);

        for (const auto &[_, chunk] : m_chunks)
        {
            if (!chunk)
                continue;

            if (!ctx.getCamera().isBoxInView(chunk->getWorldPosition(m_tileSize), chunkWorldSize))
                continue;

            chunk->draw(ctx);
        }
    }

    std::vector<std::pair<glm::vec2, glm::vec2>> ChunkManager::getLoadedChunkBounds() const
    {
        std::vector<std::pair<glm::vec2, glm::vec2>> result;
        result.reserve(m_chunks.size());
        const glm::vec2 chunkSize{
            static_cast<float>(Chunk::SIZE * m_tileSize.x),
            static_cast<float>(Chunk::SIZE * m_tileSize.y)
        };
        for (const auto &[_, chunk] : m_chunks)
        {
            if (chunk)
                result.push_back({chunk->getWorldPosition(m_tileSize), chunkSize});
        }
        return result;
    }

    void ChunkManager::renderActiveChunkHighlights(engine::core::Context &ctx) const
    {
        auto &renderer = ctx.getRenderer();
        const auto &camera = ctx.getCamera();
        const glm::vec2 chunkWorldSize = glm::vec2(Chunk::SIZE * m_tileSize.x,
                                                   Chunk::SIZE * m_tileSize.y);
        constexpr float borderThickness = 3.0f;
        const glm::vec4 fillColor(0.20f, 0.80f, 1.00f, 0.10f);
        const glm::vec4 borderColor(1.00f, 0.78f, 0.18f, 0.55f);

        for (const auto &[_, chunk] : m_chunks)
        {
            if (!chunk)
                continue;

            glm::vec2 worldPos = chunk->getWorldPosition(m_tileSize);
            if (!camera.isBoxInView(worldPos, chunkWorldSize))
                continue;

            renderer.drawRect(camera, worldPos.x, worldPos.y,
                              chunkWorldSize.x, chunkWorldSize.y, fillColor);

            renderer.drawRect(camera, worldPos.x, worldPos.y,
                              chunkWorldSize.x, borderThickness, borderColor);
            renderer.drawRect(camera, worldPos.x, worldPos.y + chunkWorldSize.y - borderThickness,
                              chunkWorldSize.x, borderThickness, borderColor);
            renderer.drawRect(camera, worldPos.x, worldPos.y,
                              borderThickness, chunkWorldSize.y, borderColor);
            renderer.drawRect(camera, worldPos.x + chunkWorldSize.x - borderThickness, worldPos.y,
                              borderThickness, chunkWorldSize.y, borderColor);
        }
    }
} // namespace engine::world