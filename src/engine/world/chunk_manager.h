#pragma once
#include "chunk.h"
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <glm/glm.hpp>

namespace engine::world
{
    class TerrainGenerator;
    class ChunkManager
    {
    public:
        ChunkManager(const std::string &atlasTextureId,
                     const glm::ivec2 &tileSize,
                     engine::resource::ResourceManager *resMgr,
                     engine::physics::PhysicsManager *physicsMgr);
        ~ChunkManager();

        void setTerrainGenerator(std::unique_ptr<engine::world::TerrainGenerator> generator);

        // 获取指定世界坐标的瓦片（线程安全？暂不考虑）
        TileData &tileAt(int worldX, int worldY);

        // 设置瓦片并标记对应块为脏
        void setTile(int worldX, int worldY, TileData tile);

        // 批量写入 API：只标脏，不立即重建（适用于树木生成等批量操作）
        void setTileSilent(int worldX, int worldY, TileData tile);
        // 重建脏块。maxChunksToRebuild < 0 表示重建全部；>=0 表示本次最多重建 N 个。
        void rebuildDirtyChunks(int maxChunksToRebuild = -1);

        glm::ivec2 worldToTile(const glm::vec2 &worldPos) const;
        glm::vec2 tileToWorld(const glm::ivec2 &tilePos) const;
        const glm::ivec2 &getTileSize() const { return m_tileSize; }

        // 更新可见块（根据相机位置和视距）
        // viewDistanceY 独立控制垂直方向视距，默认与水平相同；DNF 模式传 0 仅加载 1 行
        void updateVisibleChunks(const glm::vec2 &cameraPos, int viewDistanceInChunks,
                                  int viewDistanceYOverride = -1);

        // 横向单行模式：启用后 updateVisibleChunks 始终锁定到指定 chunkRowY，垂直视距=0
        // fixedWorldY 为世界坐标 Y，内部转换为 chunk row（默认 0 即 worldY=0）
        void setHorizontalOnly(bool enable, float fixedWorldY = 0.0f);

        // 渲染所有已加载的块
        void renderAll(engine::core::Context &ctx) const;
        void renderActiveChunkHighlights(engine::core::Context &ctx) const;

        // 获取所有已加载区块的世界坐标与尺寸（用于外部调试绘制）
        // 返回： pair<worldPos, worldSize>
        std::vector<std::pair<glm::vec2, glm::vec2>> getLoadedChunkBounds() const;

        // 获取已加载区块数量
        size_t loadedChunkCount() const { return m_chunks.size(); }
        size_t pendingChunkLoadCount() const { return m_pendingChunkLoads.size(); }

        // 加载/卸载块（内部调用）
        void loadChunk(int chunkX, int chunkY);
        void unloadChunk(int chunkX, int chunkY);

    private:
        engine::resource::ResourceManager *m_resMgr; // 资源管理器指针（非拥有）
        engine::physics::PhysicsManager* m_physicsMgr; // 物理管理器指针（非拥有）

        bool  m_horizontalOnly   = true;  // 横向单行模式（默认开启）
        int   m_fixedChunkRowY   = 0;     // 锁定的 chunk row（Y 轴）
        size_t m_prevChunkCount  = SIZE_MAX; // 上次日志时的 chunk 数量，避免每帧打印
        int   m_streamingLoadBudget = 2;

        std::unordered_map<uint64_t, std::unique_ptr<Chunk>> m_chunks;
        std::deque<std::pair<int, int>> m_pendingChunkLoads;
        std::unordered_set<uint64_t> m_pendingChunkLoadKeys;
        std::string m_atlasTextureId;
        glm::ivec2 m_tileSize;
        std::unique_ptr<TerrainGenerator> m_terrainGenerator; // 地形生成器

        void rebuildChunkMesh(Chunk &chunk);
        void enqueueChunkLoad(int chunkX, int chunkY);
        void processPendingChunkLoads();

        // 将世界坐标转换为区块坐标 + 区块内局部坐标（正确处理负坐标的向下取整）
        static void worldToChunkCoords(int worldX, int worldY,
                                       int &cx, int &cy, int &lx, int &ly)
        {
            cx = worldX / Chunk::SIZE; if (worldX < 0) cx--;
            cy = worldY / Chunk::SIZE; if (worldY < 0) cy--;
            lx = worldX - cx * Chunk::SIZE;
            ly = worldY - cy * Chunk::SIZE;
        }

        // 辅助函数：将 (chunkX, chunkY) 编码为 uint64_t 键
        static uint64_t encodeChunkKey(int x, int y)
        {
            return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
        }
    };
} // namespace engine::world