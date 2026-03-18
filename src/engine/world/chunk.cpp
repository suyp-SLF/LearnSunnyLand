#include "chunk.h"
#include "../render/renderer.h"
#include "../world/world_config.h"
#include "../core/context.h"
#include "../render/render_types.h" // 根据实际路径调整，确保包含 GPUVertex 的完整定义
#include "../resource/resource_manager.h"
#include "../physics/physics_manager.h"
#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>
#include <box2d/box2d.h>

namespace engine::world
{
    Chunk::Chunk(int chunkX, int chunkY)
        : m_chunkX(chunkX), m_chunkY(chunkY)
    {
        // 可以初始化瓦片数据，例如全部设为空气
        for (auto &tile : m_tiles)
        {
            tile = TileData(TileType::Stone);
        }
    }

    Chunk::~Chunk()
    {
        // 析构函数（智能指针会自动释放）
    }

    bool Chunk::buildMesh(const std::string &textureId,
                          const glm::ivec2 &tileSize,
                          engine::resource::ResourceManager *resMgr)
    {
        m_textureId = textureId;
        // 获取设备指针
        SDL_GPUDevice *device = resMgr->getGPUDevice();

        // 先释放旧缓冲区
        for (auto &[tex, batch] : m_batches)
        {
            if (batch.vertexBuffer)
                SDL_ReleaseGPUBuffer(device, batch.vertexBuffer);
        }
        m_batches.clear();

        // 临时收集每个纹理的顶点数据
        std::unordered_map<SDL_GPUTexture *, std::vector<engine::render::GPUVertex>> tempVertices;

        for (int ly = 0; ly < SIZE; ++ly)
        {
            for (int lx = 0; lx < SIZE; ++lx)
            {
                const auto &tile = m_tiles[ly * SIZE + lx];
                if (tile.type == TileType::Air)
                    continue;

                SDL_GPUTexture *texture = resMgr->getGPUTexture(m_textureId);
                glm::vec2 texture_size = resMgr->getTextureSize(m_textureId);
                if (!texture)
                    continue;

                float x0 = lx * tileSize.x;
                float y0 = ly * tileSize.y;
                float x1 = x0 + tileSize.x;
                float y1 = y0 + tileSize.y;

                float inv_w = 1.0f / texture_size.x;
                float inv_h = 1.0f / texture_size.y;
                float u0 = tile.uv_rect.x * inv_w;
                float v0 = tile.uv_rect.y * inv_h;
                float u1 = u0 + tile.uv_rect.z * inv_w;
                float v1 = v0 + tile.uv_rect.w * inv_h;

                glm::vec4 white = {1.0f, 1.0f, 1.0f, 1.0f};
                auto &vertices = tempVertices[texture];
                vertices.push_back({{x0, y0}, white, {u0, v0}});
                vertices.push_back({{x1, y0}, white, {u1, v0}});
                vertices.push_back({{x0, y1}, white, {u0, v1}});
                vertices.push_back({{x1, y0}, white, {u1, v0}});
                vertices.push_back({{x0, y1}, white, {u0, v1}});
                vertices.push_back({{x1, y1}, white, {u1, v1}});
            }
        }

        // 为每个纹理创建 GPU 缓冲区并上传数据
        for (const auto &[texture, vertices] : tempVertices)
        {
            if (vertices.empty())
                continue;

            size_t dataSize = vertices.size() * sizeof(engine::render::GPUVertex);
            SDL_GPUBufferCreateInfo bufInfo{};
            bufInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            bufInfo.size = dataSize;
            SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &bufInfo);
            if (!buffer)
                continue;

            // 创建暂存缓冲区并上传
            SDL_GPUTransferBufferCreateInfo tbInfo{};
            tbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            tbInfo.size = dataSize;
            SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(device, &tbInfo);
            void *mapped = SDL_MapGPUTransferBuffer(device, staging, false);
            memcpy(mapped, vertices.data(), dataSize);
            SDL_UnmapGPUTransferBuffer(device, staging);

            // 获取命令缓冲并执行拷贝
            SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
            SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
            SDL_GPUTransferBufferLocation src{staging, 0};
            SDL_GPUBufferRegion dst{buffer, 0, (Uint32)dataSize};
            SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
            SDL_EndGPUCopyPass(copyPass);
            SDL_SubmitGPUCommandBuffer(cmd); // 注意：此处会阻塞等待，但通常 buildMesh 在加载时调用，可以接受

            SDL_ReleaseGPUTransferBuffer(device, staging);

            // 保存批次信息
            m_batches[texture] = {buffer, (Uint32)vertices.size()};
        }

        m_dirty = false;
        return true;
    }

    void Chunk::render(engine::core::Context &ctx)
    {
        // 判断是否需要渲染
        draw(ctx);
    }

    void Chunk::draw(engine::core::Context &ctx)
    {
        if (m_dirty)
        {
            // 需要传入 device 和 resMgr，可以从 ctx 获取
            auto *device = ctx.getResourceManager().getGPUDevice();
            auto *resMgr = &ctx.getResourceManager();
            buildMesh(m_textureId, glm::vec2(WorldConfig::TILE_SIZE), &ctx.getResourceManager());
        }

        glm::vec2 worldOffset = glm::vec2(m_chunkX * SIZE * WorldConfig::TILE_SIZE.x,
                                          m_chunkY * SIZE * WorldConfig::TILE_SIZE.y);

        // 调用渲染器的绘制函数，传递批次信息
        ctx.getRenderer().drawChunkBatches(ctx.getCamera(), m_batches, worldOffset);
    }

    void Chunk::createPhysicsBodies(engine::physics::PhysicsManager *physicsMgr, glm::vec2 tileSize, float pixelsPerMeter)
    {
        m_tileSize = tileSize;
        for (int ly = 0; ly < SIZE; ++ly)
        {
            for (int lx = 0; lx < SIZE; ++lx)
            {
                const auto &tile = m_tiles[ly * SIZE + lx];
                if (tile.type == TileType::Air)
                    continue;

                // 计算世界坐标（像素）并转换为米
                float worldX = (m_chunkX * SIZE + lx) * tileSize.x + tileSize.x * 0.5f;
                float worldY = (m_chunkY * SIZE + ly) * tileSize.y + tileSize.y * 0.5f;
                b2Vec2 physPos = {worldX / pixelsPerMeter, worldY / pixelsPerMeter};
                b2Vec2 halfSize = {tileSize.x * 0.5f / pixelsPerMeter, tileSize.y * 0.5f / pixelsPerMeter};

                // 用户数据可以存储瓦片索引（或指针），用于后续查找
                int tileIndex = ly * SIZE + lx;
                b2BodyId bodyId = physicsMgr->createStaticBody(physPos, halfSize, reinterpret_cast<void *>(tileIndex));
                m_physicsBodies[tileIndex] = bodyId;
            }
        }
    }

    void Chunk::destroyPhysicsBodies(engine::physics::PhysicsManager *physicsMgr)
    {
    }

    void Chunk::updatePhysicsBody(int localX, int localY, engine::physics::PhysicsManager *physicsMgr, float pixelsPerMeter)
    {
        int index = localY * SIZE + localX;
        const auto &tile = m_tiles[index];
        bool isSolid = (tile.type != TileType::Air);

        auto it = m_physicsBodies.find(index);
        if (!isSolid && it != m_physicsBodies.end())
        {
            // 瓦片变为空气，销毁物理体
            physicsMgr->destroyBody(it->second);
            m_physicsBodies.erase(it);
        }
        else if (isSolid && it == m_physicsBodies.end())
        {
            // 瓦片从空气变为实体，创建物理体
            float worldX = (m_chunkX * SIZE + localX) * m_tileSize.x + m_tileSize.x * 0.5f;
            float worldY = (m_chunkY * SIZE + localY) * m_tileSize.y + m_tileSize.y * 0.5f;
            b2Vec2 physPos = {worldX / pixelsPerMeter, worldY / pixelsPerMeter};
            b2Vec2 halfSize = {m_tileSize.x * 0.5f / pixelsPerMeter, m_tileSize.y * 0.5f / pixelsPerMeter};
            b2BodyId bodyId = physicsMgr->createStaticBody(physPos, halfSize, reinterpret_cast<void *>(index));
            m_physicsBodies[index] = bodyId;
        }
    }

} // namespace engine::world