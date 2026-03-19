#include "chunk.h"
#include "../render/renderer.h"
#include "../world/world_config.h"
#include "../core/context.h"
#include "../render/render_types.h"
#include "../resource/resource_manager.h"
#include "../physics/physics_manager.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
#include <box2d/box2d.h>

namespace engine::world
{
    Chunk::Chunk(int chunkX, int chunkY)
        : m_chunkX(chunkX), m_chunkY(chunkY)
    {
        for (auto &tile : m_tiles)
            tile = TileData(TileType::Stone);
    }

    Chunk::~Chunk()
    {
        // GL 资源由 renderer 管理，这里只做简单清理
        // 注意：只在 GL context 有效时才能调用 GL 函数
        if (m_gl_vao || m_gl_vbo)
        {
            // 无法在析构里安全调用 GL，资源泄漏可接受（程序退出时 OS 会回收）
            m_gl_vao = 0;
            m_gl_vbo = 0;
        }
    }

    bool Chunk::buildMesh(const std::string &textureId,
                          const glm::ivec2 &tileSize,
                          engine::resource::ResourceManager *resMgr)
    {
        m_textureId = textureId;
        SDL_GPUDevice *device = resMgr->getGPUDevice();
        if (!device)
            return false;

        for (auto &[tex, batch] : m_batches)
        {
            if (batch.vertexBuffer)
                SDL_ReleaseGPUBuffer(device, batch.vertexBuffer);
        }
        m_batches.clear();

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

            SDL_GPUTransferBufferCreateInfo tbInfo{};
            tbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            tbInfo.size = dataSize;
            SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(device, &tbInfo);
            void *mapped = SDL_MapGPUTransferBuffer(device, staging, false);
            memcpy(mapped, vertices.data(), dataSize);
            SDL_UnmapGPUTransferBuffer(device, staging);

            SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
            SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
            SDL_GPUTransferBufferLocation src{staging, 0};
            SDL_GPUBufferRegion dst{buffer, 0, (Uint32)dataSize};
            SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
            SDL_EndGPUCopyPass(copyPass);
            SDL_SubmitGPUCommandBuffer(cmd);

            SDL_ReleaseGPUTransferBuffer(device, staging);
            m_batches[texture] = {buffer, (Uint32)vertices.size()};
        }

        m_dirty = false;
        return true;
    }

    bool Chunk::buildMeshGL(const std::string &textureId,
                             const glm::ivec2 &tileSize,
                             engine::resource::ResourceManager *resMgr,
                             engine::render::Renderer &renderer)
    {
        m_textureId = textureId;
        m_gl_tex = resMgr->getGLTexture(textureId);
        glm::vec2 texture_size = resMgr->getTextureSize(textureId);
        if (!m_gl_tex || texture_size.x == 0 || texture_size.y == 0)
            return false;

        std::vector<float> vertices;
        vertices.reserve(SIZE * SIZE * 6 * 4);

        float inv_w = 1.0f / texture_size.x;
        float inv_h = 1.0f / texture_size.y;

        for (int ly = 0; ly < SIZE; ++ly)
        {
            for (int lx = 0; lx < SIZE; ++lx)
            {
                const auto &tile = m_tiles[ly * SIZE + lx];
                if (tile.type == TileType::Air)
                    continue;

                float x0 = lx * tileSize.x;
                float y0 = ly * tileSize.y;
                float x1 = x0 + tileSize.x;
                float y1 = y0 + tileSize.y;

                float u0 = tile.uv_rect.x * inv_w;
                float v0 = tile.uv_rect.y * inv_h;
                float u1 = u0 + tile.uv_rect.z * inv_w;
                float v1 = v0 + tile.uv_rect.w * inv_h;

                vertices.insert(vertices.end(), {x0, y0, u0, v0});
                vertices.insert(vertices.end(), {x1, y0, u1, v0});
                vertices.insert(vertices.end(), {x0, y1, u0, v1});
                vertices.insert(vertices.end(), {x1, y0, u1, v0});
                vertices.insert(vertices.end(), {x1, y1, u1, v1});
                vertices.insert(vertices.end(), {x0, y1, u0, v1});
            }
        }

        bool ok = renderer.buildChunkMeshGL(m_gl_vao, m_gl_vbo, m_gl_vertex_count, vertices);
        if (ok) m_dirty = false;
        return ok;
    }

    void Chunk::render(engine::core::Context &ctx)
    {
        draw(ctx);
    }

    void Chunk::draw(engine::core::Context &ctx)
    {
        auto *resMgr = &ctx.getResourceManager();
        bool useGL = (resMgr->getGPUDevice() == nullptr);

        if (m_dirty)
        {
            if (useGL)
                buildMeshGL(m_textureId, glm::ivec2(WorldConfig::TILE_SIZE), resMgr, ctx.getRenderer());
            else
                buildMesh(m_textureId, glm::ivec2(WorldConfig::TILE_SIZE), resMgr);
        }

        glm::vec2 worldOffset = glm::vec2(m_chunkX * SIZE * WorldConfig::TILE_SIZE.x,
                                          m_chunkY * SIZE * WorldConfig::TILE_SIZE.y);

        if (useGL)
            ctx.getRenderer().drawChunkGL(ctx.getCamera(), m_gl_vao, m_gl_vbo, m_gl_vertex_count, m_gl_tex, worldOffset);
        else
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

                float worldX = (m_chunkX * SIZE + lx) * tileSize.x + tileSize.x * 0.5f;
                float worldY = (m_chunkY * SIZE + ly) * tileSize.y + tileSize.y * 0.5f;
                b2Vec2 physPos = {worldX / pixelsPerMeter, worldY / pixelsPerMeter};
                b2Vec2 halfSize = {tileSize.x * 0.5f / pixelsPerMeter, tileSize.y * 0.5f / pixelsPerMeter};

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
            physicsMgr->destroyBody(it->second);
            m_physicsBodies.erase(it);
        }
        else if (isSolid && it == m_physicsBodies.end())
        {
            float worldX = (m_chunkX * SIZE + localX) * m_tileSize.x + m_tileSize.x * 0.5f;
            float worldY = (m_chunkY * SIZE + localY) * m_tileSize.y + m_tileSize.y * 0.5f;
            b2Vec2 physPos = {worldX / pixelsPerMeter, worldY / pixelsPerMeter};
            b2Vec2 halfSize = {m_tileSize.x * 0.5f / pixelsPerMeter, m_tileSize.y * 0.5f / pixelsPerMeter};
            b2BodyId bodyId = physicsMgr->createStaticBody(physPos, halfSize, reinterpret_cast<void *>(index));
            m_physicsBodies[index] = bodyId;
        }
    }

} // namespace engine::world
