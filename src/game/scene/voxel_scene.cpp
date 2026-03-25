#include "voxel_scene.h"

#include "menu_scene.h"

#include "../../engine/core/context.h"
#include "../../engine/input/input_manager.h"
#include "../../engine/resource/resource_manager.h"
#include "../../engine/render/renderer.h"
#include "../../engine/render/opengl_renderer.h"
#include "../../engine/scene/scene_manager.h"
#include "../locale/locale_manager.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <imgui_impl_opengl3_loader.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_DEPTH_BUFFER_BIT
#define GL_DEPTH_BUFFER_BIT 0x00000100
#endif
#ifndef GL_BACK
#define GL_BACK 0x0405
#endif

namespace game::scene
{
    namespace
    {
        constexpr float kMouseSensitivity = 0.12f;
        constexpr float kVoxelUnitPerMeter = 32.0f;
        constexpr float kMonsterGravity = 28.0f;
        constexpr float kMonsterJumpImpulse = 8.0f;
        constexpr float kMonsterMaxRange = 34.0f;
        constexpr int kRouteMapSize = game::route::RouteData::MAP_SIZE;
        constexpr float kRouteCellSize = 22.0f;
        constexpr float kRouteCellGap = 2.0f;
        constexpr float kRouteCellTotal = kRouteCellSize + kRouteCellGap;
        constexpr float kRouteGridPixels = kRouteMapSize * kRouteCellTotal;
        constexpr float kEvacInteractRadius = 2.8f;

        const char* skillKeyHint(game::skill::SkillEffect effect)
        {
            switch (effect)
            {
            case game::skill::SkillEffect::FireBlast: return "[攻击]";
            case game::skill::SkillEffect::IceAura:   return "[被动]";
            case game::skill::SkillEffect::WindBoost: return "[被动]";
            case game::skill::SkillEffect::LightDash: return "[Q]";
            }
            return "";
        }

        unsigned char routeMarkerBlockType(bool isStart, bool isObjective, bool isEvac)
        {
            if (isObjective)
                return 7;
            if (isEvac)
                return 6;
            if (isStart)
                return 5;
            return 4;
        }

        glm::vec3 faceNormal(int faceIndex)
        {
            static const glm::vec3 kNormals[6] = {
                { 0.0f,  0.0f,  1.0f},
                { 0.0f,  0.0f, -1.0f},
                {-1.0f,  0.0f,  0.0f},
                { 1.0f,  0.0f,  0.0f},
                { 0.0f,  1.0f,  0.0f},
                { 0.0f, -1.0f,  0.0f},
            };
            return kNormals[faceIndex];
        }

        void appendBox(std::vector<VoxelScene::Vertex> &vertices,
                       const glm::vec3 &minCorner,
                       const glm::vec3 &maxCorner,
                       const glm::vec3 &color)
        {
            const glm::vec3 p000{minCorner.x, minCorner.y, minCorner.z};
            const glm::vec3 p001{minCorner.x, minCorner.y, maxCorner.z};
            const glm::vec3 p010{minCorner.x, maxCorner.y, minCorner.z};
            const glm::vec3 p011{minCorner.x, maxCorner.y, maxCorner.z};
            const glm::vec3 p100{maxCorner.x, minCorner.y, minCorner.z};
            const glm::vec3 p101{maxCorner.x, minCorner.y, maxCorner.z};
            const glm::vec3 p110{maxCorner.x, maxCorner.y, minCorner.z};
            const glm::vec3 p111{maxCorner.x, maxCorner.y, maxCorner.z};

            const glm::vec3 faces[6][4] = {
                {p001, p101, p011, p111},
                {p100, p000, p110, p010},
                {p000, p001, p010, p011},
                {p101, p100, p111, p110},
                {p011, p111, p010, p110},
                {p000, p100, p001, p101},
            };

            for (int face = 0; face < 6; ++face)
            {
                const glm::vec3 normal = faceNormal(face);
                vertices.push_back({faces[face][0], color, normal});
                vertices.push_back({faces[face][1], color, normal});
                vertices.push_back({faces[face][2], color, normal});
                vertices.push_back({faces[face][1], color, normal});
                vertices.push_back({faces[face][3], color, normal});
                vertices.push_back({faces[face][2], color, normal});
            }
        }

        void appendOrientedBox(std::vector<VoxelScene::Vertex> &vertices,
                               const glm::vec3 &center,
                               const glm::vec3 &halfExtents,
                               const glm::vec3 &axisX,
                               const glm::vec3 &axisY,
                               const glm::vec3 &axisZ,
                               const glm::vec3 &color)
        {
            const glm::vec3 x = axisX * halfExtents.x;
            const glm::vec3 y = axisY * halfExtents.y;
            const glm::vec3 z = axisZ * halfExtents.z;

            const glm::vec3 p000 = center - x - y - z;
            const glm::vec3 p001 = center - x - y + z;
            const glm::vec3 p010 = center - x + y - z;
            const glm::vec3 p011 = center - x + y + z;
            const glm::vec3 p100 = center + x - y - z;
            const glm::vec3 p101 = center + x - y + z;
            const glm::vec3 p110 = center + x + y - z;
            const glm::vec3 p111 = center + x + y + z;

            const glm::vec3 faceNormals[6] = { axisZ, -axisZ, -axisX, axisX, axisY, -axisY };
            const glm::vec3 faces[6][4] = {
                {p001, p101, p011, p111},
                {p100, p000, p110, p010},
                {p000, p001, p010, p011},
                {p101, p100, p111, p110},
                {p011, p111, p010, p110},
                {p000, p100, p001, p101},
            };

            for (int face = 0; face < 6; ++face)
            {
                const glm::vec3 normal = glm::normalize(faceNormals[face]);
                vertices.push_back({faces[face][0], color, normal});
                vertices.push_back({faces[face][1], color, normal});
                vertices.push_back({faces[face][2], color, normal});
                vertices.push_back({faces[face][1], color, normal});
                vertices.push_back({faces[face][3], color, normal});
                vertices.push_back({faces[face][2], color, normal});
            }
        }

        glm::vec3 monsterColor(game::scene::VoxelScene::VoxelMonsterType type, float hurtFlash)
        {
            glm::vec3 base{0.6f, 0.8f, 0.5f};
            switch (type)
            {
            case game::scene::VoxelScene::VoxelMonsterType::Slime: base = {0.38f, 0.86f, 0.45f}; break;
            case game::scene::VoxelScene::VoxelMonsterType::Wolf: base = {0.62f, 0.66f, 0.74f}; break;
            case game::scene::VoxelScene::VoxelMonsterType::WhiteApe: base = {0.88f, 0.89f, 0.92f}; break;
            }
            return glm::mix(base, glm::vec3(1.0f, 0.35f, 0.25f), std::clamp(hurtFlash, 0.0f, 1.0f));
        }

        glm::vec3 monsterHalfExtents(game::scene::VoxelScene::VoxelMonsterType type)
        {
            switch (type)
            {
            case game::scene::VoxelScene::VoxelMonsterType::Slime: return {0.42f, 0.45f, 0.42f};
            case game::scene::VoxelScene::VoxelMonsterType::Wolf: return {0.45f, 0.52f, 0.70f};
            case game::scene::VoxelScene::VoxelMonsterType::WhiteApe: return {0.55f, 0.92f, 0.48f};
            }
            return {0.5f, 0.5f, 0.5f};
        }

        float monsterBaseHealth(game::scene::VoxelScene::VoxelMonsterType type)
        {
            switch (type)
            {
            case game::scene::VoxelScene::VoxelMonsterType::Slime: return 38.0f;
            case game::scene::VoxelScene::VoxelMonsterType::Wolf: return 54.0f;
            case game::scene::VoxelScene::VoxelMonsterType::WhiteApe: return 82.0f;
            }
            return 40.0f;
        }

        float monsterMoveSpeed(game::scene::VoxelScene::VoxelMonsterType type)
        {
            switch (type)
            {
            case game::scene::VoxelScene::VoxelMonsterType::Slime: return 2.6f;
            case game::scene::VoxelScene::VoxelMonsterType::Wolf: return 3.8f;
            case game::scene::VoxelScene::VoxelMonsterType::WhiteApe: return 2.2f;
            }
            return 2.5f;
        }

        float monsterContactDamage(game::scene::VoxelScene::VoxelMonsterType type)
        {
            switch (type)
            {
            case game::scene::VoxelScene::VoxelMonsterType::Slime: return 9.0f;
            case game::scene::VoxelScene::VoxelMonsterType::Wolf: return 13.0f;
            case game::scene::VoxelScene::VoxelMonsterType::WhiteApe: return 18.0f;
            }
            return 10.0f;
        }

        unsigned int compileShader(unsigned int type, const char *source)
        {
            unsigned int shader = glCreateShader(type);
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);
            int success = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                char infoLog[512];
                glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
                SDL_Log("Voxel shader compile error: %s", infoLog);
            }
            return shader;
        }
    }

    VoxelScene::VoxelScene(const std::string &name,
                           engine::core::Context &context,
                           engine::scene::SceneManager &sceneManager)
        : Scene(name, context, sceneManager)
    {
    }

    void VoxelScene::init()
    {
        Scene::init();
        initImGui();
        initGLResources();
        initChunkMeshes();
        initGameplaySystems();
        generateWorld();
        rebuildMesh();
        updateMouseCapture();
        m_lastMousePos = _context.getInputManager().getMousePosition();
    }

    void VoxelScene::initGameplaySystems()
    {
        using namespace game::inventory;
        using Cat = ItemCategory;

        const auto &presets = game::route::RouteData::planetPresets();
        m_selectedPlanetIndex = 0;
        m_routeData.applyPlanetPreset(presets[0]);
        m_routeData.generateTerrain(presets[0].seedBias ^ 0xABCD1234ULL);

        m_timeOfDaySystem.dayLengthSeconds = 360.0f;
        m_hp = m_maxHp;
        m_starEnergy = m_maxStarEnergy;

        m_inventory.addItem({"alloy_greatsword", "合金巨剑", 1, Cat::Weapon}, 1);
        auto &slot0 = m_weaponBar.getSlot(0);
        slot0.item = Item{"alloy_greatsword", "合金巨剑", 1, Cat::Weapon};
        slot0.count = 1;

        m_inventory.addItem({"gold_coin", "金币", 99, Cat::Misc}, 42);
        m_inventory.addItem({"apple", "苹果", 20, Cat::Consumable}, 5);
        m_inventory.addItem({"wood", "木材", 64, Cat::Material}, 24);
        m_inventory.addItem({"stone", "石块", 64, Cat::Material}, 24);
        m_inventory.addItem({"star_fire", "炎焰星技", 1, Cat::StarSkill}, 1);
        m_inventory.addItem({"star_ice", "寒冰星技", 1, Cat::StarSkill}, 1);
        m_inventory.addItem({"star_wind", "疾风星技", 1, Cat::StarSkill}, 1);
        m_inventory.addItem({"star_light", "闪光星技", 1, Cat::StarSkill}, 1);

        m_starSockets[0].item = Item{"star_fire", "炎焰星技", 1, Cat::StarSkill};
        m_starSockets[0].count = 1;
        m_starSockets[1].item = Item{"star_wind", "疾风星技", 1, Cat::StarSkill};
        m_starSockets[1].count = 1;
        m_starSockets[2].item = Item{"star_light", "闪光星技", 1, Cat::StarSkill};
        m_starSockets[2].count = 1;

        m_routeProgressIndex = -1;
        m_routeObjectiveReached = false;
        m_showSettlement = false;
    }

    void VoxelScene::initImGui()
    {
        SDL_Window *window = _context.getRenderer().getWindow();
        if (!window)
            return;

        m_glContext = SDL_GL_GetCurrentContext();
        if (!m_glContext)
            return;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->AddFontFromFileTTF(
            "assets/fonts/VonwaonBitmap-16px.ttf",
            16.0f, nullptr,
            io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

        ImGui_ImplSDL3_InitForOpenGL(window, m_glContext);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    void VoxelScene::initGLResources()
    {
        const char *vertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec3 aNormal;
out vec3 vColor;
out vec3 vNormal;
out vec3 vWorldPos;
uniform mat4 uMVP;
void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
    vNormal = aNormal;
    vWorldPos = aPos;
}
)";

        const char *fragSrc = R"(
#version 330 core
in vec3 vColor;
in vec3 vNormal;
in vec3 vWorldPos;
out vec4 FragColor;
uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform vec3 uFogColor;
uniform float uAmbientStrength;
uniform float uDiffuseStrength;
uniform float uFogNear;
uniform float uFogFar;
uniform float uFlash;
void main()
{
    vec3 normal = normalize(vNormal);
    float lambert = max(dot(normal, normalize(-uLightDir)), 0.0);
    vec3 lit = vColor * (uAmbientStrength + lambert * uDiffuseStrength + uFlash * 0.25);
    float dist = length(vWorldPos - uCameraPos);
    float fogSpan = max(uFogFar - uFogNear, 0.001);
    float fogFactor = clamp((uFogFar - dist) / fogSpan, 0.0, 1.0);
    vec3 finalColor = mix(uFogColor, lit, fogFactor);
    FragColor = vec4(finalColor, 1.0);
}
)";

        unsigned int vs = compileShader(GL_VERTEX_SHADER, vertSrc);
        unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
        m_shader = glCreateProgram();
        glAttachShader(m_shader, vs);
        glAttachShader(m_shader, fs);
        glLinkProgram(m_shader);
        glDeleteShader(vs);
        glDeleteShader(fs);

        m_glDrawArrays = reinterpret_cast<DrawArraysProc>(SDL_GL_GetProcAddress("glDrawArrays"));
        m_glCullFace = reinterpret_cast<CullFaceProc>(SDL_GL_GetProcAddress("glCullFace"));
        m_glUniform3fv = reinterpret_cast<Uniform3fvProc>(SDL_GL_GetProcAddress("glUniform3fv"));
        m_glUniform1f = reinterpret_cast<Uniform1fProc>(SDL_GL_GetProcAddress("glUniform1f"));

        glGenVertexArrays(1, &m_monsterVao);
        glGenBuffers(1, &m_monsterVbo);
        glBindVertexArray(m_monsterVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_monsterVbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

        glGenVertexArrays(1, &m_viewModelVao);
        glGenBuffers(1, &m_viewModelVbo);
        glBindVertexArray(m_viewModelVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_viewModelVbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

        glGenVertexArrays(1, &m_playerVao);
        glGenBuffers(1, &m_playerVbo);
        glBindVertexArray(m_playerVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_playerVbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glBindVertexArray(0);
    }

    void VoxelScene::initChunkMeshes()
    {
        m_chunkMeshes.clear();
        m_activeChunkKeys.clear();
        glBindVertexArray(0);
    }

    int VoxelScene::voxelIndex(int x, int y, int z) const
    {
        return x + y * CHUNK_SIZE_X + z * CHUNK_SIZE_X * WORLD_Y;
    }

    int VoxelScene::cornerDensityIndex(int localX, int y, int localZ) const
    {
        return localX + y * (CHUNK_SIZE_X + 1) + localZ * (CHUNK_SIZE_X + 1) * (WORLD_Y + 1);
    }

    bool VoxelScene::isInside(int x, int y, int z) const
    {
        return x >= 0 && x < worldWidth() && y >= 0 && y < WORLD_Y && z >= 0 && z < worldDepth();
    }

    bool VoxelScene::isSolid(int x, int y, int z) const
    {
        return densityAt(x, y, z) > 0.08f;
    }

    float VoxelScene::densityAt(int x, int y, int z) const
    {
        if (!isInside(x, y, z))
            return 0.0f;

        glm::ivec2 chunkCoord = worldToChunkXZ(x, z);
        const VoxelChunkMesh *chunk = findChunk(chunkCoord.x, chunkCoord.y);
        if (!chunk || !chunk->generated)
            return 0.0f;

        int localX = x - chunkCoord.x * CHUNK_SIZE_X;
        int localZ = z - chunkCoord.y * CHUNK_SIZE_Z;
        if (chunk->densities.empty())
            return chunk->voxels[chunkVoxelIndex(localX, y, localZ)] != 0 ? 1.0f : 0.0f;
        return chunk->densities[chunkVoxelIndex(localX, y, localZ)];
    }

    unsigned char VoxelScene::rawVoxelAt(int x, int y, int z) const
    {
        if (!isInside(x, y, z))
            return 0;

        glm::ivec2 chunkCoord = worldToChunkXZ(x, z);
        const VoxelChunkMesh *chunk = findChunk(chunkCoord.x, chunkCoord.y);
        if (!chunk || !chunk->generated)
            return 0;

        int localX = x - chunkCoord.x * CHUNK_SIZE_X;
        int localZ = z - chunkCoord.y * CHUNK_SIZE_Z;
        return chunk->voxels[chunkVoxelIndex(localX, y, localZ)];
    }

    unsigned char VoxelScene::voxelAt(int x, int y, int z) const
    {
        return densityAt(x, y, z) > 0.08f ? rawVoxelAt(x, y, z) : 0;
    }

    void VoxelScene::setVoxel(int x, int y, int z, unsigned char value)
    {
        if (!isInside(x, y, z))
            return;

        glm::ivec2 chunkCoord = worldToChunkXZ(x, z);
        VoxelChunkMesh &chunk = ensureChunk(chunkCoord.x, chunkCoord.y);
        int localX = x - chunkCoord.x * CHUNK_SIZE_X;
        int localZ = z - chunkCoord.y * CHUNK_SIZE_Z;
        const int index = chunkVoxelIndex(localX, y, localZ);
        chunk.voxels[index] = value;
        if (chunk.densities.empty())
            chunk.densities.assign(CHUNK_SIZE_X * WORLD_Y * CHUNK_SIZE_Z, 0.0f);
        chunk.densities[index] = value != 0 ? 1.0f : 0.0f;
        markChunkDirtyAt(x, z);
    }

    bool VoxelScene::applyDensityBrush(const glm::vec3 &center, float radius, float delta, unsigned char fillMaterial)
    {
        const int minX = std::max(0, static_cast<int>(std::floor(center.x - radius - 1.0f)));
        const int maxX = std::min(worldWidth() - 1, static_cast<int>(std::ceil(center.x + radius + 1.0f)));
        const int minY = std::max(0, static_cast<int>(std::floor(center.y - radius - 1.0f)));
        const int maxY = std::min(WORLD_Y - 1, static_cast<int>(std::ceil(center.y + radius + 1.0f)));
        const int minZ = std::max(0, static_cast<int>(std::floor(center.z - radius - 1.0f)));
        const int maxZ = std::min(worldDepth() - 1, static_cast<int>(std::ceil(center.z + radius + 1.0f)));

        bool changed = false;
        for (int z = minZ; z <= maxZ; ++z)
        {
            for (int y = minY; y <= maxY; ++y)
            {
                for (int x = minX; x <= maxX; ++x)
                {
                    glm::vec3 voxelCenter{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f};
                    float distance = glm::distance(voxelCenter, center);
                    if (distance > radius)
                        continue;

                    glm::ivec2 chunkCoord = worldToChunkXZ(x, z);
                    VoxelChunkMesh &chunk = ensureChunk(chunkCoord.x, chunkCoord.y);
                    if (chunk.densities.empty())
                        chunk.densities.assign(CHUNK_SIZE_X * WORLD_Y * CHUNK_SIZE_Z, 0.0f);

                    int localX = x - chunkCoord.x * CHUNK_SIZE_X;
                    int localZ = z - chunkCoord.y * CHUNK_SIZE_Z;
                    int index = chunkVoxelIndex(localX, y, localZ);
                    float falloff = 1.0f - (distance / std::max(radius, 0.001f));
                    float current = chunk.densities[index];
                    float next = std::clamp(current + delta * falloff, 0.0f, 1.0f);
                    if (std::abs(next - current) < 0.001f)
                        continue;

                    if (next > 0.05f)
                    {
                        if (chunk.voxels[index] == 0)
                            chunk.voxels[index] = fillMaterial != 0 ? fillMaterial : 1;
                    }
                    else
                    {
                        next = 0.0f;
                        chunk.voxels[index] = 0;
                    }

                    chunk.densities[index] = next;
                    markChunkDirtyAt(x, z);
                    changed = true;
                }
            }
        }

        return changed;
    }

    glm::vec3 VoxelScene::blockColor(unsigned char type, float shade) const
    {
        glm::vec3 base(0.7f);
        switch (type)
        {
        case 1: base = {0.30f, 0.72f, 0.28f}; break;
        case 2: base = {0.46f, 0.31f, 0.18f}; break;
        case 3: base = {0.55f, 0.57f, 0.62f}; break;
        case 4: base = {0.85f, 0.77f, 0.32f}; break;
        case 5: base = {0.20f, 0.92f, 0.36f}; break;
        case 6: base = {0.26f, 0.78f, 0.98f}; break;
        case 7: base = {1.00f, 0.83f, 0.24f}; break;
        default: break;
        }
        return glm::clamp(base * shade, 0.0f, 1.0f);
    }

    void VoxelScene::generateWorld()
    {
        for (auto &[key, chunk] : m_chunkMeshes)
            releaseChunk(chunk);
        m_chunkMeshes.clear();
        m_activeChunkKeys.clear();
        m_pendingChunkLoads.clear();
    }

    void VoxelScene::rebuildMesh()
    {
        for (auto &[key, chunk] : m_chunkMeshes)
            chunk.dirty = true;
        updateStreamedChunks();
        processChunkStreamingBudget(LOAD_CHUNK_RADIUS * 2, LOAD_CHUNK_RADIUS * 2);
    }

    void VoxelScene::rebuildChunkMesh(VoxelChunkMesh &chunk)
    {
        std::vector<Vertex> vertices;
        vertices.reserve(CHUNK_SIZE_X * WORLD_Y * CHUNK_SIZE_Z * 12);

        const int startX = chunk.chunkX * CHUNK_SIZE_X;
        const int endX = std::min(startX + CHUNK_SIZE_X, worldWidth());
        const int startZ = chunk.chunkZ * CHUNK_SIZE_Z;
        const int endZ = std::min(startZ + CHUNK_SIZE_Z, worldDepth());
        const int sizeX = endX - startX;
        const int sizeZ = endZ - startZ;

        for (int dz = -1; dz <= 1; ++dz)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                int nx = chunk.chunkX + dx;
                int nz = chunk.chunkZ + dz;
                if (nx < 0 || nz < 0)
                    continue;
                if (nx * CHUNK_SIZE_X >= worldWidth() || nz * CHUNK_SIZE_Z >= worldDepth())
                    continue;
                ensureChunk(nx, nz);
            }
        }
        updateChunkDensityCache(chunk);

        const glm::ivec3 cubeOffsets[8] = {
            {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
            {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
        };
        const int tetrahedra[6][4] = {
            {0, 5, 1, 6},
            {0, 5, 6, 4},
            {0, 1, 2, 6},
            {0, 2, 3, 6},
            {0, 3, 7, 6},
            {0, 7, 4, 6},
        };

        auto sampleDensity = [&](int localX, int y, int localZ) -> float
        {
            localX = std::clamp(localX, 0, sizeX);
            y = std::clamp(y, 0, WORLD_Y);
            localZ = std::clamp(localZ, 0, sizeZ);
            return chunk.cornerDensityCache[cornerDensityIndex(localX, y, localZ)];
        };

        auto sampleMaterial = [&](const glm::vec3 &position) -> unsigned char
        {
            int baseX = static_cast<int>(std::floor(position.x));
            int baseY = static_cast<int>(std::floor(position.y));
            int baseZ = static_cast<int>(std::floor(position.z));
            for (int radius = 0; radius <= 1; ++radius)
            {
                for (int dz = -radius; dz <= radius; ++dz)
                {
                    for (int dy = -radius; dy <= radius; ++dy)
                    {
                        for (int dx = -radius; dx <= radius; ++dx)
                        {
                            unsigned char material = rawVoxelAt(baseX + dx, baseY + dy, baseZ + dz);
                            if (material != 0)
                                return material;
                        }
                    }
                }
            }
            return 1;
        };

        auto estimateNormal = [&](const glm::vec3 &position) -> glm::vec3
        {
            int gx = static_cast<int>(std::round(position.x)) - startX;
            int gy = static_cast<int>(std::round(position.y));
            int gz = static_cast<int>(std::round(position.z)) - startZ;

            float dx = sampleDensity(gx + 1, gy, gz) - sampleDensity(gx - 1, gy, gz);
            float dy = sampleDensity(gx, gy + 1, gz) - sampleDensity(gx, gy - 1, gz);
            float dz = sampleDensity(gx, gy, gz + 1) - sampleDensity(gx, gy, gz - 1);
            glm::vec3 normal{-dx, -dy, -dz};
            if (glm::length(normal) < 0.0001f)
                return {0.0f, 1.0f, 0.0f};
            return glm::normalize(normal);
        };

        auto interpolate = [](const glm::vec3 &a, const glm::vec3 &b, float da, float db)
        {
            float denom = da - db;
            float t = std::abs(denom) > 0.00001f ? da / denom : 0.5f;
            t = std::clamp(t, 0.0f, 1.0f);
            return a + (b - a) * t;
        };

        auto emitTriangle = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2)
        {
            glm::vec3 n0 = estimateNormal(p0);
            glm::vec3 n1 = estimateNormal(p1);
            glm::vec3 n2 = estimateNormal(p2);
            glm::vec3 desiredNormal = n0 + n1 + n2;
            if (glm::length(desiredNormal) < 0.0001f)
                desiredNormal = {0.0f, 1.0f, 0.0f};
            else
                desiredNormal = glm::normalize(desiredNormal);

            glm::vec3 triNormal = glm::cross(p1 - p0, p2 - p0);
            if (glm::length(triNormal) < 0.0001f)
                return;
            triNormal = glm::normalize(triNormal);
            if (glm::dot(triNormal, desiredNormal) < 0.0f)
            {
                std::swap(p1, p2);
                std::swap(n1, n2);
            }

            glm::vec3 centroid = (p0 + p1 + p2) / 3.0f;
            glm::vec3 color = blockColor(sampleMaterial(centroid), 1.0f);
            vertices.push_back({p0, color, n0});
            vertices.push_back({p1, color, n1});
            vertices.push_back({p2, color, n2});
        };

        for (int localZ = 0; localZ < sizeZ; ++localZ)
        {
            for (int y = 0; y < WORLD_Y; ++y)
            {
                for (int localX = 0; localX < sizeX; ++localX)
                {
                    glm::vec3 cubePos[8];
                    float cubeDensity[8];
                    bool hasSurface = false;

                    for (int i = 0; i < 8; ++i)
                    {
                        const glm::ivec3 offset = cubeOffsets[i];
                        cubePos[i] = glm::vec3(startX + localX + offset.x,
                                               y + offset.y,
                                               startZ + localZ + offset.z);
                        cubeDensity[i] = sampleDensity(localX + offset.x, y + offset.y, localZ + offset.z);
                    }

                    for (int i = 1; i < 8; ++i)
                    {
                        if ((cubeDensity[i] > 0.0f) != (cubeDensity[0] > 0.0f))
                        {
                            hasSurface = true;
                            break;
                        }
                    }
                    if (!hasSurface)
                        continue;

                    for (const auto &tetra : tetrahedra)
                    {
                        int inside[4];
                        int outside[4];
                        int insideCount = 0;
                        int outsideCount = 0;

                        for (int i = 0; i < 4; ++i)
                        {
                            int idx = tetra[i];
                            if (cubeDensity[idx] > 0.0f)
                                inside[insideCount++] = idx;
                            else
                                outside[outsideCount++] = idx;
                        }

                        if (insideCount == 0 || insideCount == 4)
                            continue;

                        if (insideCount == 1)
                        {
                            int solid = inside[0];
                            glm::vec3 p0 = interpolate(cubePos[solid], cubePos[outside[0]], cubeDensity[solid], cubeDensity[outside[0]]);
                            glm::vec3 p1 = interpolate(cubePos[solid], cubePos[outside[1]], cubeDensity[solid], cubeDensity[outside[1]]);
                            glm::vec3 p2 = interpolate(cubePos[solid], cubePos[outside[2]], cubeDensity[solid], cubeDensity[outside[2]]);
                            emitTriangle(p0, p1, p2);
                        }
                        else if (insideCount == 3)
                        {
                            int air = outside[0];
                            glm::vec3 p0 = interpolate(cubePos[air], cubePos[inside[0]], cubeDensity[air], cubeDensity[inside[0]]);
                            glm::vec3 p1 = interpolate(cubePos[air], cubePos[inside[1]], cubeDensity[air], cubeDensity[inside[1]]);
                            glm::vec3 p2 = interpolate(cubePos[air], cubePos[inside[2]], cubeDensity[air], cubeDensity[inside[2]]);
                            emitTriangle(p0, p2, p1);
                        }
                        else if (insideCount == 2)
                        {
                            glm::vec3 p0 = interpolate(cubePos[inside[0]], cubePos[outside[0]], cubeDensity[inside[0]], cubeDensity[outside[0]]);
                            glm::vec3 p1 = interpolate(cubePos[inside[1]], cubePos[outside[0]], cubeDensity[inside[1]], cubeDensity[outside[0]]);
                            glm::vec3 p2 = interpolate(cubePos[inside[1]], cubePos[outside[1]], cubeDensity[inside[1]], cubeDensity[outside[1]]);
                            glm::vec3 p3 = interpolate(cubePos[inside[0]], cubePos[outside[1]], cubeDensity[inside[0]], cubeDensity[outside[1]]);
                            emitTriangle(p0, p1, p2);
                            emitTriangle(p0, p2, p3);
                        }
                    }
                }
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
        chunk.vertexCount = static_cast<int>(vertices.size());
        chunk.dirty = false;
    }

    void VoxelScene::rebuildDirtyChunkMeshes()
    {
        processChunkStreamingBudget(0, std::max(m_chunkMeshBudget, 1));
    }

    void VoxelScene::processChunkStreamingBudget(int loadBudget, int meshBudget)
    {
        if (loadBudget > 0)
        {
            int generatedCount = 0;
            while (!m_pendingChunkLoads.empty() && generatedCount < loadBudget)
            {
                int64_t key = m_pendingChunkLoads.front();
                m_pendingChunkLoads.erase(m_pendingChunkLoads.begin());
                int chunkX = static_cast<int>(key >> 32);
                int chunkZ = static_cast<int>(key & 0xffffffffu);
                ensureChunk(chunkX, chunkZ);
                ++generatedCount;
            }
        }

        if (meshBudget <= 0)
            return;

        glm::ivec2 cameraChunk = worldToChunkXZ(static_cast<int>(std::floor(m_cameraPos.x)), static_cast<int>(std::floor(m_cameraPos.z)));
        struct DirtyCandidate
        {
            float distanceSq;
            int64_t key;
        };
        std::vector<DirtyCandidate> dirtyCandidates;
        dirtyCandidates.reserve(m_chunkMeshes.size());
        for (const auto &[key, chunk] : m_chunkMeshes)
        {
            if (!chunk.dirty || !chunk.generated)
                continue;
            float dx = static_cast<float>(chunk.chunkX - cameraChunk.x);
            float dz = static_cast<float>(chunk.chunkZ - cameraChunk.y);
            dirtyCandidates.push_back({dx * dx + dz * dz, key});
        }

        std::sort(dirtyCandidates.begin(), dirtyCandidates.end(), [](const DirtyCandidate &lhs, const DirtyCandidate &rhs)
        {
            return lhs.distanceSq < rhs.distanceSq;
        });

        int rebuiltCount = 0;
        for (const DirtyCandidate &candidate : dirtyCandidates)
        {
            auto it = m_chunkMeshes.find(candidate.key);
            if (it == m_chunkMeshes.end() || !it->second.dirty || !it->second.generated)
                continue;
            rebuildChunkMesh(it->second);
            if (++rebuiltCount >= meshBudget)
                break;
        }
    }

    void VoxelScene::releaseChunk(VoxelChunkMesh &chunk)
    {
        if (chunk.vbo)
        {
            glDeleteBuffers(1, &chunk.vbo);
            chunk.vbo = 0;
        }
        if (chunk.vao)
        {
            glDeleteVertexArrays(1, &chunk.vao);
            chunk.vao = 0;
        }
    }

    void VoxelScene::updateStreamedChunks()
    {
        glm::ivec2 cameraChunk = worldToChunkXZ(static_cast<int>(std::floor(m_cameraPos.x)), static_cast<int>(std::floor(m_cameraPos.z)));

        for (int dz = -LOAD_CHUNK_RADIUS; dz <= LOAD_CHUNK_RADIUS; ++dz)
        {
            for (int dx = -LOAD_CHUNK_RADIUS; dx <= LOAD_CHUNK_RADIUS; ++dx)
            {
                int chunkX = cameraChunk.x + dx;
                int chunkZ = cameraChunk.y + dz;
                if (chunkX < 0 || chunkZ < 0)
                    continue;
                if (chunkX * CHUNK_SIZE_X >= worldWidth() || chunkZ * CHUNK_SIZE_Z >= worldDepth())
                    continue;

                requestChunkLoad(chunkX, chunkZ);
            }
        }

        m_pendingChunkLoads.erase(
            std::remove_if(m_pendingChunkLoads.begin(), m_pendingChunkLoads.end(), [&](int64_t key)
            {
                int chunkX = static_cast<int>(key >> 32);
                int chunkZ = static_cast<int>(key & 0xffffffffu);
                return std::abs(chunkX - cameraChunk.x) > KEEP_CHUNK_RADIUS ||
                       std::abs(chunkZ - cameraChunk.y) > KEEP_CHUNK_RADIUS;
            }),
            m_pendingChunkLoads.end());

        for (auto it = m_chunkMeshes.begin(); it != m_chunkMeshes.end(); )
        {
            const auto &chunk = it->second;
            if (std::abs(chunk.chunkX - cameraChunk.x) > KEEP_CHUNK_RADIUS ||
                std::abs(chunk.chunkZ - cameraChunk.y) > KEEP_CHUNK_RADIUS)
            {
                releaseChunk(it->second);
                it = m_chunkMeshes.erase(it);
            }
            else
            {
                ++it;
            }
        }

        m_activeChunkKeys.clear();
        for (const auto &[key, chunk] : m_chunkMeshes)
        {
            if (std::abs(chunk.chunkX - cameraChunk.x) <= LOAD_CHUNK_RADIUS &&
                std::abs(chunk.chunkZ - cameraChunk.y) <= LOAD_CHUNK_RADIUS)
            {
                m_activeChunkKeys.push_back(key);
            }
        }
    }

    void VoxelScene::rebuildMonsterMesh()
    {
        std::vector<Vertex> vertices;
        vertices.reserve(m_monsters.size() * 36);

        for (const auto &monster : m_monsters)
        {
            glm::vec3 half = monsterHalfExtents(monster.type);
            glm::vec3 minCorner = {monster.pos.x - half.x, monster.pos.y, monster.pos.z - half.z};
            glm::vec3 maxCorner = {monster.pos.x + half.x, monster.pos.y + half.y * 2.0f, monster.pos.z + half.z};
            appendBox(vertices, minCorner, maxCorner, monsterColor(monster.type, monster.hurtFlash));
        }

        glBindBuffer(GL_ARRAY_BUFFER, m_monsterVbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
        m_monsterVertexCount = static_cast<int>(vertices.size());
    }

    void VoxelScene::rebuildViewModelMesh()
    {
        std::vector<Vertex> vertices;
        vertices.reserve(216);

        const glm::vec3 eye = getCameraEyePosition();
        const glm::vec3 forward = getForward();
        const glm::vec3 right = getRight();
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));

        const glm::vec3 handCenter = eye + forward * 0.72f + right * 0.34f - up * 0.28f;
        appendOrientedBox(vertices, handCenter, {0.10f, 0.14f, 0.18f}, right, up, forward, {0.91f, 0.78f, 0.64f});

        const glm::vec3 gripCenter = eye + forward * 0.98f + right * 0.28f - up * 0.22f;
        appendOrientedBox(vertices, gripCenter, {0.05f, 0.16f, 0.05f}, right, up, forward, {0.26f, 0.18f, 0.12f});

        const glm::vec3 bladeCenter = eye + forward * 1.36f + right * 0.19f + up * 0.02f;
        appendOrientedBox(vertices, bladeCenter, {0.04f, 0.14f, 0.44f}, right, up, forward, {0.78f, 0.80f, 0.86f});

        glBindBuffer(GL_ARRAY_BUFFER, m_viewModelVbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
        m_viewModelVertexCount = static_cast<int>(vertices.size());
    }

    void VoxelScene::rebuildPlayerMesh()
    {
        std::vector<Vertex> vertices;
        vertices.reserve(360);

        const glm::vec3 origin = m_cameraPos + glm::vec3(0.0f, -0.95f, 0.0f);
        const glm::vec3 forward = getPlayerModelForward();
        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 up{0.0f, 1.0f, 0.0f};

        appendOrientedBox(vertices, origin + up * 1.05f, {0.30f, 0.46f, 0.18f}, right, up, forward, {0.22f, 0.32f, 0.48f});
        appendOrientedBox(vertices, origin + up * 1.78f, {0.22f, 0.22f, 0.22f}, right, up, forward, {0.92f, 0.81f, 0.68f});
        appendOrientedBox(vertices, origin + up * 1.06f + right * 0.42f, {0.08f, 0.34f, 0.08f}, right, up, forward, {0.91f, 0.78f, 0.64f});
        appendOrientedBox(vertices, origin + up * 1.06f - right * 0.42f, {0.08f, 0.34f, 0.08f}, right, up, forward, {0.91f, 0.78f, 0.64f});
        appendOrientedBox(vertices, origin + up * 0.34f + right * 0.16f, {0.09f, 0.34f, 0.09f}, right, up, forward, {0.18f, 0.18f, 0.24f});
        appendOrientedBox(vertices, origin + up * 0.34f - right * 0.16f, {0.09f, 0.34f, 0.09f}, right, up, forward, {0.18f, 0.18f, 0.24f});
        appendOrientedBox(vertices, origin + up * 0.98f + right * 0.58f + forward * 0.12f, {0.05f, 0.14f, 0.40f}, right, up, forward, {0.78f, 0.80f, 0.86f});

        glBindBuffer(GL_ARRAY_BUFFER, m_playerVbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
        m_playerVertexCount = static_cast<int>(vertices.size());
    }

    uint64_t VoxelScene::nextRand()
    {
        m_rngState ^= m_rngState << 13;
        m_rngState ^= m_rngState >> 7;
        m_rngState ^= m_rngState << 17;
        return m_rngState;
    }

    float VoxelScene::randFloat()
    {
        return static_cast<float>(nextRand() & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    int VoxelScene::findGroundY(int x, int z) const
    {
        if (x < 0 || x >= worldWidth() || z < 0 || z >= worldDepth())
            return -1;

        glm::ivec2 chunkCoord = worldToChunkXZ(x, z);
        const_cast<VoxelScene*>(this)->ensureChunk(chunkCoord.x, chunkCoord.y);

        for (int y = WORLD_Y - 2; y >= 0; --y)
        {
            if (isSolid(x, y, z) && !isSolid(x, y + 1, z))
                return y;
        }
        return -1;
    }

    glm::ivec2 VoxelScene::worldToChunkXZ(int x, int z) const
    {
        return {x / CHUNK_SIZE_X, z / CHUNK_SIZE_Z};
    }

    int64_t VoxelScene::chunkKey(int chunkX, int chunkZ) const
    {
        return (static_cast<int64_t>(chunkX) << 32) ^ static_cast<uint32_t>(chunkZ);
    }

    int VoxelScene::chunkVoxelIndex(int localX, int y, int localZ) const
    {
        return voxelIndex(localX, y, localZ);
    }

    VoxelScene::VoxelChunkMesh *VoxelScene::findChunk(int chunkX, int chunkZ)
    {
        auto it = m_chunkMeshes.find(chunkKey(chunkX, chunkZ));
        return it == m_chunkMeshes.end() ? nullptr : &it->second;
    }

    const VoxelScene::VoxelChunkMesh *VoxelScene::findChunk(int chunkX, int chunkZ) const
    {
        auto it = m_chunkMeshes.find(chunkKey(chunkX, chunkZ));
        return it == m_chunkMeshes.end() ? nullptr : &it->second;
    }

    VoxelScene::VoxelChunkMesh &VoxelScene::ensureChunk(int chunkX, int chunkZ)
    {
        auto [it, inserted] = m_chunkMeshes.try_emplace(chunkKey(chunkX, chunkZ));
        VoxelChunkMesh &chunk = it->second;
        if (inserted)
        {
            chunk.chunkX = chunkX;
            chunk.chunkZ = chunkZ;
            glGenVertexArrays(1, &chunk.vao);
            glGenBuffers(1, &chunk.vbo);
            glBindVertexArray(chunk.vao);
            glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
            glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            glBindVertexArray(0);
        }

        if (!chunk.generated)
            generateChunk(chunk);
        return chunk;
    }

    void VoxelScene::requestChunkLoad(int chunkX, int chunkZ)
    {
        int64_t key = chunkKey(chunkX, chunkZ);
        auto it = m_chunkMeshes.find(key);
        if (it != m_chunkMeshes.end() && it->second.generated)
            return;
        if (std::find(m_pendingChunkLoads.begin(), m_pendingChunkLoads.end(), key) != m_pendingChunkLoads.end())
            return;
        m_pendingChunkLoads.push_back(key);
    }

    void VoxelScene::generateChunk(VoxelChunkMesh &chunk)
    {
        chunk.voxels.assign(CHUNK_SIZE_X * WORLD_Y * CHUNK_SIZE_Z, 0);
        chunk.densities.assign(CHUNK_SIZE_X * WORLD_Y * CHUNK_SIZE_Z, 0.0f);
        chunk.cornerDensityCache.assign((CHUNK_SIZE_X + 1) * (WORLD_Y + 1) * (CHUNK_SIZE_Z + 1), -1.0f);

        const auto &planet = m_routeData.selectedPlanetPreset();
        const int startX = chunk.chunkX * CHUNK_SIZE_X;
        const int startZ = chunk.chunkZ * CHUNK_SIZE_Z;
        const int routeCellSize = game::route::RouteData::TILES_PER_CELL;

        for (int localZ = 0; localZ < CHUNK_SIZE_Z; ++localZ)
        {
            for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
            {
                int worldX = startX + localX;
                int worldZ = startZ + localZ;
                if (worldX >= worldWidth() || worldZ >= worldDepth())
                    continue;

                glm::ivec2 routeCell = worldToRouteCell(glm::vec3(worldX + 0.5f, 0.0f, worldZ + 0.5f));
                game::route::CellTerrain terrain = m_routeData.terrain[routeCell.y][routeCell.x];

                float macroWave = std::sin(static_cast<float>(worldX) * 0.021f) * 5.8f
                                + std::cos(static_cast<float>(worldZ) * 0.018f) * 4.9f
                                + std::sin(static_cast<float>(worldX + worldZ) * 0.010f) * 2.8f;
                float ridgeWave = std::sin(static_cast<float>(worldX) * 0.006f + static_cast<float>(worldZ) * 0.004f) * 7.0f;

                int terrainBias = 0;
                unsigned char surfaceType = 1;
                unsigned char subsurfaceType = 2;
                unsigned char coreType = 3;

                switch (terrain)
                {
                case game::route::CellTerrain::Plains:
                    terrainBias = 0;
                    surfaceType = 1;
                    subsurfaceType = 2;
                    coreType = 3;
                    break;
                case game::route::CellTerrain::Forest:
                    terrainBias = 1;
                    surfaceType = 1;
                    subsurfaceType = 2;
                    coreType = 2;
                    break;
                case game::route::CellTerrain::Rocky:
                    terrainBias = 2;
                    surfaceType = 3;
                    subsurfaceType = 3;
                    coreType = 2;
                    break;
                case game::route::CellTerrain::Mountain:
                    terrainBias = 4;
                    surfaceType = 4;
                    subsurfaceType = 3;
                    coreType = 3;
                    break;
                case game::route::CellTerrain::Cave:
                    terrainBias = -1;
                    surfaceType = 2;
                    subsurfaceType = 3;
                    coreType = 4;
                    break;
                }

                if (planet.type == game::route::PlanetType::Frostveil)
                {
                    surfaceType = 3;
                    subsurfaceType = 3;
                    coreType = 2;
                }
                else if (planet.type == game::route::PlanetType::Emberfall)
                {
                    surfaceType = (terrain == game::route::CellTerrain::Mountain) ? 4 : surfaceType;
                    coreType = 3;
                }

                int height = 8 + planet.seaLevelOffset / 2 + terrainBias
                    + static_cast<int>((macroWave + ridgeWave) * planet.amplitudeScale * 0.45f);
                height = std::clamp(height, 4, WORLD_Y - 3);

                for (int y = 0; y <= height; ++y)
                {
                    unsigned char block = coreType;
                    if (y == height)
                        block = surfaceType;
                    else if (y >= height - 2)
                        block = subsurfaceType;

                    bool carveCave = false;
                    if (terrain == game::route::CellTerrain::Cave || planet.type == game::route::PlanetType::Hollowreach)
                    {
                        float caveNoise = std::sin(static_cast<float>(worldX) * 0.13f)
                                        + std::cos(static_cast<float>(worldZ) * 0.11f)
                                        + std::sin(static_cast<float>(y) * 0.75f);
                        carveCave = (y > 4 && y < height - 1 && caveNoise > 2.0f);
                    }
                    if (!carveCave)
                    {
                        const int index = chunkVoxelIndex(localX, y, localZ);
                        chunk.voxels[index] = block;
                        chunk.densities[index] = 1.0f;
                    }
                }

                const int cellOriginX = routeCell.x * routeCellSize;
                const int cellOriginZ = routeCell.y * routeCellSize;
                const int cellCenterX = cellOriginX + routeCellSize / 2;
                const int cellCenterZ = cellOriginZ + routeCellSize / 2;
                int dx = std::abs(worldX - cellCenterX);
                int dz = std::abs(worldZ - cellCenterZ);
                int pathIdx = pathIndexOf(routeCell);
                if (pathIdx >= 0 && dx <= 1 && dz <= 1)
                {
                    const bool isStart = routeCell == m_routeData.startCell();
                    const bool isObjective = routeCell == m_routeData.objectiveCell;
                    const bool isEvac = routeCell == m_routeData.evacCell();
                    const unsigned char marker = routeMarkerBlockType(isStart, isObjective, isEvac);
                    int pillarHeight = isObjective ? 6 : (isEvac ? 5 : 4);
                    for (int y = height; y <= std::min(WORLD_Y - 2, height + pillarHeight); ++y)
                    {
                        const int index = chunkVoxelIndex(localX, y, localZ);
                        chunk.voxels[index] = marker;
                        chunk.densities[index] = 1.0f;
                    }
                }
                else if (pathIdx >= 0 && dx <= 2 && dz <= 2)
                {
                    const int index = chunkVoxelIndex(localX, height, localZ);
                    chunk.voxels[index] = 4;
                    chunk.densities[index] = 1.0f;
                }
            }
        }

        chunk.generated = true;
        chunk.densityCacheDirty = true;
        chunk.dirty = true;
    }

    void VoxelScene::markChunkDirtyAt(int x, int z)
    {
        glm::ivec2 chunkCoord = worldToChunkXZ(x, z);
        VoxelChunkMesh *chunk = findChunk(chunkCoord.x, chunkCoord.y);
        if (chunk)
        {
            chunk->dirty = true;
            chunk->densityCacheDirty = true;
        }

        const int localX = x - chunkCoord.x * CHUNK_SIZE_X;
        const int localZ = z - chunkCoord.y * CHUNK_SIZE_Z;
        if (localX == 0)
        {
            VoxelChunkMesh *left = findChunk(chunkCoord.x - 1, chunkCoord.y);
            if (left)
            {
                left->dirty = true;
                left->densityCacheDirty = true;
            }
        }
        else if (localX == CHUNK_SIZE_X - 1)
        {
            VoxelChunkMesh *right = findChunk(chunkCoord.x + 1, chunkCoord.y);
            if (right)
            {
                right->dirty = true;
                right->densityCacheDirty = true;
            }
        }
        if (localZ == 0)
        {
            VoxelChunkMesh *back = findChunk(chunkCoord.x, chunkCoord.y - 1);
            if (back)
            {
                back->dirty = true;
                back->densityCacheDirty = true;
            }
        }
        else if (localZ == CHUNK_SIZE_Z - 1)
        {
            VoxelChunkMesh *front = findChunk(chunkCoord.x, chunkCoord.y + 1);
            if (front)
            {
                front->dirty = true;
                front->densityCacheDirty = true;
            }
        }
    }

    void VoxelScene::updateChunkDensityCache(VoxelChunkMesh &chunk)
    {
        if (!chunk.densityCacheDirty && !chunk.cornerDensityCache.empty())
            return;

        if (chunk.cornerDensityCache.size() != static_cast<size_t>((CHUNK_SIZE_X + 1) * (WORLD_Y + 1) * (CHUNK_SIZE_Z + 1)))
            chunk.cornerDensityCache.assign((CHUNK_SIZE_X + 1) * (WORLD_Y + 1) * (CHUNK_SIZE_Z + 1), -1.0f);

        const int startX = chunk.chunkX * CHUNK_SIZE_X;
        const int startZ = chunk.chunkZ * CHUNK_SIZE_Z;
        const int endX = std::min(startX + CHUNK_SIZE_X, worldWidth());
        const int endZ = std::min(startZ + CHUNK_SIZE_Z, worldDepth());
        const int sizeX = endX - startX;
        const int sizeZ = endZ - startZ;

        auto signedDensity = [&](int worldX, int y, int worldZ)
        {
            if (y < 0)
                return 1.0f;
            if (y >= WORLD_Y)
                return -1.0f;
            return densityAt(worldX, y, worldZ) * 2.0f - 1.0f;
        };

        for (int localZ = 0; localZ <= sizeZ; ++localZ)
        {
            for (int y = 0; y <= WORLD_Y; ++y)
            {
                for (int localX = 0; localX <= sizeX; ++localX)
                {
                    const int worldX = startX + localX;
                    const int worldZ = startZ + localZ;
                    float density = 0.0f;
                    for (int oz = 0; oz < 2; ++oz)
                    {
                        for (int oy = 0; oy < 2; ++oy)
                        {
                            for (int ox = 0; ox < 2; ++ox)
                                density += signedDensity(worldX - 1 + ox, y - 1 + oy, worldZ - 1 + oz);
                        }
                    }
                    chunk.cornerDensityCache[cornerDensityIndex(localX, y, localZ)] = density / 8.0f;
                }
            }
        }

        chunk.densityCacheDirty = false;
    }

    glm::vec3 VoxelScene::getCameraEyePosition() const
    {
        return m_cameraPos;
    }

    glm::vec3 VoxelScene::getRenderCameraPosition() const
    {
        if (!m_thirdPersonView)
            return getCameraEyePosition();

        glm::vec3 forward = getForward();
        glm::vec3 candidate = getCameraEyePosition() - forward * m_thirdPersonDistance + glm::vec3(0.0f, 1.7f, 0.0f);
        candidate.x = std::clamp(candidate.x, 0.5f, static_cast<float>(worldWidth()) - 0.5f);
        candidate.y = std::clamp(candidate.y, 1.0f, WORLD_Y - 0.5f);
        candidate.z = std::clamp(candidate.z, 0.5f, static_cast<float>(worldDepth()) - 0.5f);
        return candidate;
    }

    glm::vec3 VoxelScene::getPlayerModelForward() const
    {
        glm::vec3 forward = getForward();
        glm::vec3 flat{forward.x, 0.0f, forward.z};
        if (glm::length(flat) < 0.001f)
            return {0.0f, 0.0f, -1.0f};
        return glm::normalize(flat);
    }

    void VoxelScene::updateMouseCapture()
    {
        SDL_Window *window = _context.getRenderer().getWindow();
        if (!window)
            return;

        bool shouldCapture = isRouteSetupComplete() && !m_showInventory && !m_showSettings && !m_showSettlement && !m_showPauseMenu;
        if (shouldCapture == m_mouseCaptured)
            return;

        SDL_SetWindowRelativeMouseMode(window, shouldCapture);
        SDL_SetWindowMouseGrab(window, shouldCapture);
        SDL_CaptureMouse(shouldCapture);
        if (shouldCapture)
        {
            int width = 0;
            int height = 0;
            SDL_GetWindowSize(window, &width, &height);
            SDL_WarpMouseInWindow(window, width * 0.5f, height * 0.5f);
        }
        SDL_HideCursor();
        if (!shouldCapture)
            SDL_ShowCursor();
        m_mouseCaptured = shouldCapture;
        m_firstMouseFrame = true;
    }

    bool VoxelScene::isRouteSetupComplete() const
    {
        return m_setupPhase == SetupPhase::Playing;
    }

    bool VoxelScene::isAdjacent(glm::ivec2 a, glm::ivec2 b) const
    {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y) == 1;
    }

    int VoxelScene::pathIndexOf(glm::ivec2 cell) const
    {
        for (int i = 0; i < static_cast<int>(m_routeData.path.size()); ++i)
        {
            if (m_routeData.path[static_cast<size_t>(i)] == cell)
                return i;
        }
        return -1;
    }

    glm::ivec2 VoxelScene::worldToRouteCell(const glm::vec3 &worldPos) const
    {
        const float scaleX = static_cast<float>(kRouteMapSize) / static_cast<float>(worldWidth());
        const float scaleZ = static_cast<float>(kRouteMapSize) / static_cast<float>(worldDepth());
        int cellX = std::clamp(static_cast<int>(std::floor(worldPos.x * scaleX)), 0, kRouteMapSize - 1);
        int cellY = std::clamp(static_cast<int>(std::floor(worldPos.z * scaleZ)), 0, kRouteMapSize - 1);
        return {cellX, cellY};
    }

    void VoxelScene::updateRouteProgress()
    {
        if (!m_routeData.isValid())
            return;

        glm::ivec2 currentCell = worldToRouteCell(m_cameraPos);
        int pathIndex = pathIndexOf(currentCell);
        if (pathIndex >= 0)
            m_routeProgressIndex = std::max(m_routeProgressIndex, pathIndex);

        if (currentCell == m_routeData.objectiveCell)
            m_routeObjectiveReached = true;

        if (!m_routeObjectiveReached && m_routeData.objectiveZone >= 0 && m_routeProgressIndex >= m_routeData.objectiveZone)
            m_routeObjectiveReached = true;
    }

    void VoxelScene::handleRouteCellClick(int cx, int cy, bool rightClick)
    {
        if (cx < 0 || cx >= kRouteMapSize || cy < 0 || cy >= kRouteMapSize)
            return;
        glm::ivec2 cell{cx, cy};
        if (rightClick)
        {
            if (!m_routeData.path.empty())
                m_routeData.path.pop_back();
            return;
        }

        int idx = pathIndexOf(cell);
        if (idx >= 0)
        {
            m_routeData.path.resize(static_cast<size_t>(idx + 1));
            return;
        }

        if (m_routeData.path.empty() || isAdjacent(m_routeData.path.back(), cell))
            m_routeData.path.push_back(cell);
    }

    glm::vec3 VoxelScene::getCellWorldCenter(glm::ivec2 cell) const
    {
        const float scaleX = static_cast<float>(worldWidth()) / static_cast<float>(game::route::RouteData::MAP_SIZE);
        const float scaleZ = static_cast<float>(worldDepth()) / static_cast<float>(game::route::RouteData::MAP_SIZE);
        float worldX = (static_cast<float>(cell.x) + 0.5f) * scaleX;
        float worldZ = (static_cast<float>(cell.y) + 0.5f) * scaleZ;
        glm::ivec2 chunkCoord = worldToChunkXZ(static_cast<int>(std::floor(worldX)), static_cast<int>(std::floor(worldZ)));
        const_cast<VoxelScene*>(this)->ensureChunk(chunkCoord.x, chunkCoord.y);
        int groundY = findGroundY(static_cast<int>(std::floor(worldX)), static_cast<int>(std::floor(worldZ)));
        return {std::clamp(worldX, 1.5f, static_cast<float>(worldWidth()) - 1.5f), static_cast<float>(std::max(groundY + 2, 6)), std::clamp(worldZ, 1.5f, static_cast<float>(worldDepth()) - 1.5f)};
    }

    int VoxelScene::worldWidth() const
    {
        return game::route::RouteData::MAP_SIZE * game::route::RouteData::TILES_PER_CELL;
    }

    int VoxelScene::worldDepth() const
    {
        return game::route::RouteData::MAP_SIZE * game::route::RouteData::TILES_PER_CELL;
    }

    void VoxelScene::confirmRouteSelection()
    {
        if (!m_routeData.isValid())
            return;

        m_routeData.objectiveZone = -1;
        for (int i = 0; i < static_cast<int>(m_routeData.path.size()); ++i)
        {
            if (m_routeData.path[static_cast<size_t>(i)] == m_routeData.objectiveCell)
            {
                m_routeData.objectiveZone = i;
                break;
            }
        }

        m_timeOfDaySystem.dayLengthSeconds = m_routeData.dayLengthSeconds;
        generateWorld();
        m_cameraPos = getCellWorldCenter(m_routeData.startCell());
        updateStreamedChunks();
        processChunkStreamingBudget(LOAD_CHUNK_RADIUS * LOAD_CHUNK_RADIUS, LOAD_CHUNK_RADIUS * LOAD_CHUNK_RADIUS);
        m_cameraPos = getCellWorldCenter(m_routeData.startCell());
        m_routeProgressIndex = 0;
        m_routeObjectiveReached = (m_routeData.startCell() == m_routeData.objectiveCell);
        m_showSettlement = false;
        m_monsters.clear();
        rebuildMonsterMesh();
        rebuildPlayerMesh();
        rebuildViewModelMesh();
        m_setupPhase = SetupPhase::Playing;
        updateMouseCapture();
    }

    void VoxelScene::spawnMonster()
    {
        if (m_monsters.size() >= 18)
            return;

        glm::vec2 playerXZ{m_cameraPos.x, m_cameraPos.z};
        for (int attempt = 0; attempt < 24; ++attempt)
        {
            float angle = randFloat() * static_cast<float>(M_PI) * 2.0f;
            float radius = 9.0f + randFloat() * 11.0f;
            glm::vec2 spawnXZ = playerXZ + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
            int sx = static_cast<int>(std::floor(spawnXZ.x));
            int sz = static_cast<int>(std::floor(spawnXZ.y));
            int groundY = findGroundY(sx, sz);
            if (groundY < 0 || groundY + 3 >= WORLD_Y)
                continue;

            glm::vec3 spawnPos{sx + 0.5f, groundY + 1.0f, sz + 0.5f};
            if (glm::distance(spawnPos, m_cameraPos) < 7.0f)
                continue;

            VoxelMonsterType type = static_cast<VoxelMonsterType>(static_cast<int>(nextRand() % 3));
            float hp = monsterBaseHealth(type);
            m_monsters.push_back({spawnPos, glm::vec3(0.0f), type, hp, hp, 0.0f, 0.0f, true});
            rebuildMonsterMesh();
            return;
        }
    }

    glm::vec3 VoxelScene::getForward() const
    {
        glm::vec3 forward;
        forward.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
        forward.y = std::sin(glm::radians(m_pitch));
        forward.z = std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
        return glm::normalize(forward);
    }

    glm::vec3 VoxelScene::getRight() const
    {
        return glm::normalize(glm::cross(getForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    VoxelScene::TargetBlock VoxelScene::raycastBlock() const
    {
        TargetBlock result;
        glm::vec3 origin = m_cameraPos;
        glm::vec3 dir = getForward();
        glm::vec3 lastAir = origin;

        for (float t = 0.0f; t < 8.0f; t += 0.08f)
        {
            glm::vec3 sample = origin + dir * t;
            glm::ivec3 block = glm::floor(sample);
            if (isSolid(block.x, block.y, block.z))
            {
                result.hit = true;
                result.block = block;
                result.place = glm::floor(lastAir);
                return result;
            }
            lastAir = sample;
        }

        return result;
    }

    void VoxelScene::update(float dt)
    {
        const bool *keys = SDL_GetKeyboardState(nullptr);
        SDL_Window *window = _context.getRenderer().getWindow();
        int displayW = 1280;
        int displayH = 720;
        if (window)
            SDL_GetWindowSize(window, &displayW, &displayH);

        bool gameplayPaused = isRouteSetupComplete() && (m_showPauseMenu || m_showSettings || m_showSettlement);
        if (!gameplayPaused)
            tickGameplaySystems(dt, displayW, displayH);
        updateStreamedChunks();
        processChunkStreamingBudget(m_chunkLoadBudget, m_chunkMeshBudget);

        if (!isRouteSetupComplete() || gameplayPaused)
        {
            rebuildPlayerMesh();
            if (!m_thirdPersonView)
                rebuildViewModelMesh();
            return;
        }

        glm::vec3 forward = getForward();
        glm::vec3 flatForward = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
        if (glm::length(flatForward) < 0.001f)
            flatForward = {0.0f, 0.0f, -1.0f};
        glm::vec3 right = getRight();

        glm::vec3 move(0.0f);
        if (keys[SDL_SCANCODE_W]) move += flatForward;
        if (keys[SDL_SCANCODE_S]) move -= flatForward;
        if (keys[SDL_SCANCODE_D]) move += right;
        if (keys[SDL_SCANCODE_A]) move -= right;
        if (keys[SDL_SCANCODE_SPACE]) move.y += 0.76f * (m_windStarEquipped ? 1.18f : 1.0f);
        if (keys[SDL_SCANCODE_LSHIFT]) move.y -= 0.76f;

        float moveSpeed = m_baseMoveSpeed * (m_windStarEquipped ? 1.35f : 1.0f);
        if (glm::length(move) > 0.001f)
            m_cameraPos += glm::normalize(move) * moveSpeed * dt;
        else
            m_cameraPos += move * moveSpeed * dt;

        if (m_mouseCaptured)
        {
            float deltaX = 0.0f;
            float deltaY = 0.0f;
            SDL_GetRelativeMouseState(&deltaX, &deltaY);
            m_yaw += deltaX * kMouseSensitivity;
            m_pitch -= deltaY * kMouseSensitivity;
            m_pitch = std::clamp(m_pitch, -88.0f, 88.0f);
        }

        if (keys[SDL_SCANCODE_LEFT])  m_yaw -= 90.0f * dt;
        if (keys[SDL_SCANCODE_RIGHT]) m_yaw += 90.0f * dt;
        if (keys[SDL_SCANCODE_UP])    m_pitch = std::min(m_pitch + 70.0f * dt, 88.0f);
        if (keys[SDL_SCANCODE_DOWN])  m_pitch = std::max(m_pitch - 70.0f * dt, -88.0f);

        m_cameraPos.x = std::clamp(m_cameraPos.x, 1.5f, static_cast<float>(worldWidth()) - 1.5f);
        m_cameraPos.y = std::clamp(m_cameraPos.y, 2.0f, WORLD_Y - 1.5f);
        m_cameraPos.z = std::clamp(m_cameraPos.z, 1.5f, static_cast<float>(worldDepth()) - 1.5f);

        updateRouteProgress();

        rebuildPlayerMesh();
        if (!m_thirdPersonView)
            rebuildViewModelMesh();
    }

    void VoxelScene::tickGameplaySystems(float dt, int displayW, int displayH)
    {
        m_timeOfDaySystem.update(dt);
        m_weatherSystem.update(dt, static_cast<float>(displayW), static_cast<float>(displayH));

        for (float &cooldown : m_skillCooldowns)
            cooldown = std::max(0.0f, cooldown - dt);

        m_dashCooldown = std::max(0.0f, m_dashCooldown - dt);
        m_starEnergy = std::min(m_maxStarEnergy, m_starEnergy + dt * 7.5f);
        tickStarSkillPassives(dt);
        updateMonsters(dt);
    }

    void VoxelScene::updateMonsters(float dt)
    {
        m_monsterSpawnTimer -= dt;
        if (m_monsterSpawnTimer <= 0.0f)
        {
            m_monsterSpawnTimer = 1.4f + randFloat() * 1.1f;
            spawnMonster();
        }

        bool changed = false;
        for (auto &monster : m_monsters)
        {
            monster.attackCooldown = std::max(0.0f, monster.attackCooldown - dt);
            monster.hurtFlash = std::max(0.0f, monster.hurtFlash - dt * 4.0f);

            glm::vec3 delta = m_cameraPos - monster.pos;
            float horizontalDist = glm::length(glm::vec2(delta.x, delta.z));
            glm::vec3 desiredMove(0.0f);
            if (horizontalDist < kMonsterMaxRange && horizontalDist > 0.001f)
            {
                glm::vec2 dir2 = glm::normalize(glm::vec2(delta.x, delta.z));
                desiredMove = {dir2.x, 0.0f, dir2.y};
                monster.velocity.x = desiredMove.x * monsterMoveSpeed(monster.type);
                monster.velocity.z = desiredMove.z * monsterMoveSpeed(monster.type);
            }
            else
            {
                monster.velocity.x *= std::max(0.0f, 1.0f - dt * 4.0f);
                monster.velocity.z *= std::max(0.0f, 1.0f - dt * 4.0f);
            }

            int groundY = findGroundY(static_cast<int>(std::floor(monster.pos.x)), static_cast<int>(std::floor(monster.pos.z)));
            float desiredY = groundY >= 0 ? groundY + 1.0f : monster.pos.y;
            if (groundY >= 0 && monster.pos.y <= desiredY + 0.01f)
            {
                monster.pos.y = desiredY;
                monster.velocity.y = 0.0f;
                monster.onGround = true;
            }
            else
            {
                monster.onGround = false;
                monster.velocity.y -= kMonsterGravity * dt;
            }

            if (groundY >= 0 && monster.onGround && desiredMove != glm::vec3(0.0f))
            {
                int nextX = static_cast<int>(std::floor(monster.pos.x + desiredMove.x * 0.8f));
                int nextZ = static_cast<int>(std::floor(monster.pos.z + desiredMove.z * 0.8f));
                if (isInside(nextX, groundY + 1, nextZ) && isSolid(nextX, groundY + 1, nextZ))
                    monster.velocity.y = kMonsterJumpImpulse;
            }

            monster.pos += monster.velocity * dt;
            monster.pos.x = std::clamp(monster.pos.x, 1.0f, static_cast<float>(worldWidth()) - 1.0f);
            monster.pos.z = std::clamp(monster.pos.z, 1.0f, static_cast<float>(worldDepth()) - 1.0f);
            monster.pos.y = std::clamp(monster.pos.y, 0.0f, static_cast<float>(WORLD_Y - 3));

            if (horizontalDist < 1.45f && monster.attackCooldown <= 0.0f)
            {
                float damage = std::max(1.0f, monsterContactDamage(monster.type) - m_defense * 0.45f);
                m_hp = std::max(0.0f, m_hp - damage);
                monster.attackCooldown = 1.0f + randFloat() * 0.35f;
            }

            changed = true;
        }

        const glm::vec3 playerPos = m_cameraPos;
        const size_t oldCount = m_monsters.size();
        m_monsters.erase(
            std::remove_if(m_monsters.begin(), m_monsters.end(), [&](const VoxelMonster &monster)
            {
                return monster.health <= 0.0f || glm::distance(monster.pos, playerPos) > 48.0f;
            }),
            m_monsters.end());
        changed = changed || oldCount != m_monsters.size();

        if (changed)
            rebuildMonsterMesh();
    }

    void VoxelScene::tickStarSkillPassives(float /*dt*/)
    {
        m_windStarEquipped = false;
        m_iceStarEquipped = false;

        for (size_t i = 0; i < m_starSockets.size(); ++i)
        {
            const auto &slot = m_starSockets[i];
            if (slot.isEmpty())
                continue;

            const auto *def = game::skill::getStarSkillDef(slot.item->id);
            if (!def)
                continue;

            if (def->effect == game::skill::SkillEffect::WindBoost)
                m_windStarEquipped = true;

            if (def->effect == game::skill::SkillEffect::IceAura)
            {
                m_iceStarEquipped = true;
                if (m_skillCooldowns[i] <= 0.0f && m_starEnergy >= 8.0f)
                {
                    glm::vec3 center = m_cameraPos + getForward() * 1.8f;
                    explodeBlocks(glm::floor(center), 1);
                    damageMonstersInRadius(center, 2.4f, 48.0f, glm::vec3(0.0f, 4.5f, 0.0f));
                    m_skillCooldowns[i] = def->cooldown;
                    m_starEnergy = std::max(0.0f, m_starEnergy - 8.0f);
                }
            }
        }
    }

    void VoxelScene::explodeBlocks(const glm::ivec3 &center, int radius)
    {
        applyDensityBrush(glm::vec3(center) + glm::vec3(0.5f), static_cast<float>(radius) + 0.9f, -1.35f, 0);
    }

    void VoxelScene::performWeaponAttack(const TargetBlock &target)
    {
        const auto &activeSlot = m_weaponBar.getActiveSlot();
        if (activeSlot.isEmpty())
            return;

        const auto *def = game::weapon::getWeaponDef(activeSlot.item->id);
        if (!def)
            return;

        if (def->attack_type == game::weapon::AttackType::Melee)
        {
            glm::vec3 forward = getForward();
            float range = std::max(2.6f, def->range / kVoxelUnitPerMeter);
            glm::vec3 attackCenter = target.hit ? glm::vec3(target.block) + glm::vec3(0.5f) : m_cameraPos + forward * std::min(range, 2.4f);
            slashMonsters(m_cameraPos, forward, range, 1.35f, static_cast<float>(def->damage));
            if (target.hit && glm::distance(attackCenter, m_cameraPos) <= range + 1.0f)
                explodeBlocks(target.block, m_iceStarEquipped ? 2 : 1);
            triggerAttackStarSkills(attackCenter);
            m_starEnergy = std::min(m_maxStarEnergy, m_starEnergy + 6.0f);
        }
    }

    void VoxelScene::triggerAttackStarSkills(const glm::vec3 &attackCenter)
    {
        for (size_t i = 0; i < m_starSockets.size(); ++i)
        {
            const auto &slot = m_starSockets[i];
            if (slot.isEmpty() || m_skillCooldowns[i] > 0.0f)
                continue;

            const auto *def = game::skill::getStarSkillDef(slot.item->id);
            if (!def || def->effect != game::skill::SkillEffect::FireBlast)
                continue;

            if (m_starEnergy < 18.0f)
                return;

            explodeBlocks(glm::floor(attackCenter), 2);
            damageMonstersInRadius(attackCenter, 3.4f, 170.0f, glm::vec3(0.0f, 6.0f, 0.0f));
            m_skillCooldowns[i] = def->cooldown;
            m_starEnergy = std::max(0.0f, m_starEnergy - 18.0f);
        }
    }

    void VoxelScene::triggerActiveStarSkills()
    {
        for (size_t i = 0; i < m_starSockets.size(); ++i)
        {
            const auto &slot = m_starSockets[i];
            if (slot.isEmpty() || m_skillCooldowns[i] > 0.0f)
                continue;

            const auto *def = game::skill::getStarSkillDef(slot.item->id);
            if (!def || def->effect != game::skill::SkillEffect::LightDash)
                continue;

            if (m_starEnergy < 22.0f || m_dashCooldown > 0.0f)
                return;

            m_cameraPos += getForward() * def->param;
            m_cameraPos.x = std::clamp(m_cameraPos.x, 1.5f, static_cast<float>(worldWidth()) - 1.5f);
            m_cameraPos.y = std::clamp(m_cameraPos.y, 2.0f, WORLD_Y - 1.5f);
            m_cameraPos.z = std::clamp(m_cameraPos.z, 1.5f, static_cast<float>(worldDepth()) - 1.5f);
            m_dashCooldown = def->cooldown;
            m_skillCooldowns[i] = def->cooldown;
            m_starEnergy = std::max(0.0f, m_starEnergy - 22.0f);
            damageMonstersInRadius(m_cameraPos, 1.8f, 72.0f, getForward() * 8.0f + glm::vec3(0.0f, 3.5f, 0.0f));
            return;
        }
    }

    int VoxelScene::damageMonstersInRadius(const glm::vec3 &center, float radius, float damage, const glm::vec3 &impulse)
    {
        int hitCount = 0;
        bool changed = false;
        const float radiusSq = radius * radius;
        for (auto &monster : m_monsters)
        {
            glm::vec3 delta = monster.pos - center;
            delta.y += 0.6f;
            if (glm::dot(delta, delta) > radiusSq)
                continue;

            glm::vec3 direction = glm::length(delta) > 0.001f ? glm::normalize(delta) : glm::vec3(0.0f, 1.0f, 0.0f);
            monster.health -= damage;
            monster.velocity += impulse + direction * 4.5f;
            monster.hurtFlash = 1.0f;
            if (monster.health <= 0.0f)
            {
                m_starEnergy = std::min(m_maxStarEnergy, m_starEnergy + 8.0f);
                m_inventory.addItem({"gold_coin", "金币", 99, game::inventory::ItemCategory::Misc}, 1);
            }
            ++hitCount;
            changed = true;
        }

        if (changed)
            rebuildMonsterMesh();
        return hitCount;
    }

    int VoxelScene::slashMonsters(const glm::vec3 &origin, const glm::vec3 &forward, float range, float radius, float damage)
    {
        int hitCount = 0;
        bool changed = false;
        glm::vec3 normalizedForward = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
        for (auto &monster : m_monsters)
        {
            glm::vec3 toMonster = monster.pos - origin;
            glm::vec3 flatDelta{toMonster.x, 0.0f, toMonster.z};
            float forwardDist = glm::dot(flatDelta, normalizedForward);
            if (forwardDist < 0.2f || forwardDist > range)
                continue;

            glm::vec3 nearest = normalizedForward * forwardDist;
            float lateral = glm::length(flatDelta - nearest);
            if (lateral > radius || std::abs(toMonster.y) > 2.0f)
                continue;

            glm::vec3 impulse = normalizedForward * 7.0f + glm::vec3(0.0f, 5.0f, 0.0f);
            monster.health -= damage;
            monster.velocity += impulse;
            monster.hurtFlash = 1.0f;
            if (monster.health <= 0.0f)
            {
                m_starEnergy = std::min(m_maxStarEnergy, m_starEnergy + 10.0f);
                m_inventory.addItem({"gold_coin", "金币", 99, game::inventory::ItemCategory::Misc}, 2);
            }
            ++hitCount;
            changed = true;
        }

        if (changed)
            rebuildMonsterMesh();
        return hitCount;
    }

    void VoxelScene::renderWeaponBar()
    {
        constexpr float slotW = 94.0f;
        constexpr float slotH = 58.0f;
        constexpr float gap = 6.0f;
        const float width = game::weapon::WeaponBar::SLOTS * slotW + (game::weapon::WeaponBar::SLOTS - 1) * gap + 16.0f;
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({(display.x - width) * 0.5f, display.y - 108.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({width, 92.0f}, ImGuiCond_Always);
        ImGui::Begin(locale::T("weapon_bar.title").c_str(), nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

        for (int i = 0; i < game::weapon::WeaponBar::SLOTS; ++i)
        {
            if (i > 0)
                ImGui::SameLine();

            const auto &slot = m_weaponBar.getSlot(i);
            const bool active = i == m_weaponBar.getActiveIndex();
            ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(0.68f, 0.48f, 0.12f, 1.0f) : ImVec4(0.18f, 0.20f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? ImVec4(0.82f, 0.60f, 0.16f, 1.0f) : ImVec4(0.28f, 0.31f, 0.44f, 1.0f));

            char label[96];
            if (slot.isEmpty())
                std::snprintf(label, sizeof(label), "%d", i + 1);
            else
            {
                const auto *def = game::weapon::getWeaponDef(slot.item->id);
                std::snprintf(label, sizeof(label), "%s\n%s", def ? def->icon_label.c_str() : "[W]", slot.item->name.c_str());
            }
            ImGui::Button(label, {slotW, slotH});
            if (ImGui::IsItemClicked())
                m_weaponBar.setActiveIndex(i);

            if (!slot.isEmpty() && ImGui::IsItemHovered())
            {
                const auto *def = game::weapon::getWeaponDef(slot.item->id);
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(slot.item->name.c_str());
                if (def)
                {
                    ImGui::Separator();
                    ImGui::TextDisabled("%s: %d", locale::T("weapon.damage").c_str(), def->damage);
                    ImGui::TextDisabled("%s: %.1f/s", locale::T("weapon.speed").c_str(), def->attack_speed);
                }
                ImGui::EndTooltip();
            }

            ImGui::PopStyleColor(2);
        }

        ImGui::End();
    }

    void VoxelScene::renderSkillHUD()
    {
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({display.x - 248.0f, 18.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({230.0f, 210.0f}, ImGuiCond_Always);
        ImGui::Begin("星技", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

        for (size_t i = 0; i < m_starSockets.size(); ++i)
        {
            const auto &slot = m_starSockets[i];
            if (slot.isEmpty())
                continue;

            const auto *def = game::skill::getStarSkillDef(slot.item->id);
            if (!def)
                continue;

            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%d. %s %s", static_cast<int>(i + 1), slot.item->name.c_str(), skillKeyHint(def->effect));
            float cooldownRatio = def->cooldown > 0.0f ? 1.0f - (m_skillCooldowns[i] / def->cooldown) : 1.0f;
            cooldownRatio = std::clamp(cooldownRatio, 0.0f, 1.0f);
            ImGui::ProgressBar(cooldownRatio, {-1.0f, 8.0f});
            ImGui::PopID();
        }

        ImGui::End();
    }

    void VoxelScene::renderPlayerStatusHUD()
    {
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({display.x - 246.0f, display.y - 254.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({228.0f, 132.0f}, ImGuiCond_Always);
        ImGui::Begin("状态", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
        ImGui::Text("HP %.0f / %.0f", m_hp, m_maxHp);
        ImGui::ProgressBar(m_hp / std::max(m_maxHp, 1.0f), {-1.0f, 10.0f}, "");
        ImGui::Text("SE %.0f / %.0f", m_starEnergy, m_maxStarEnergy);
        ImGui::ProgressBar(m_starEnergy / std::max(m_maxStarEnergy, 1.0f), {-1.0f, 10.0f}, "");
        ImGui::Separator();
        ImGui::Text("攻击 %.0f", m_attack);
        ImGui::Text("防御 %.0f", m_defense);
        ImGui::Text("时刻 %02d:%02d", m_timeOfDaySystem.getHour24(), m_timeOfDaySystem.getMinute());
        ImGui::Text("时段 %s", m_timeOfDaySystem.getPhaseName());
        ImGui::Text("天气 %s", m_weatherSystem.getCurrentWeatherName());
        ImGui::Text("怪物 %d", static_cast<int>(m_monsters.size()));
        ImGui::End();
    }

    void VoxelScene::renderInventoryUI()
    {
        if (!m_showInventory)
            return;

        constexpr float slot = 48.0f;
        constexpr float gap = 4.0f;
        const float width = game::inventory::Inventory::COLS * slot + (game::inventory::Inventory::COLS - 1) * gap + 24.0f;
        const float height = game::inventory::Inventory::ROWS * slot + (game::inventory::Inventory::ROWS - 1) * gap + 90.0f;
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({(display.x - width) * 0.5f, (display.y - height) * 0.5f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({width, height}, ImGuiCond_Always);
        ImGui::Begin(locale::T("inventory.title").c_str(), &m_showInventory,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        for (int idx = 0; idx < m_inventory.getSlotCount(); ++idx)
        {
            if (idx > 0 && idx % game::inventory::Inventory::COLS != 0)
                ImGui::SameLine();

            auto &slotRef = m_inventory.getSlot(idx);
            ImGui::PushID(idx);
            ImGui::PushStyleColor(ImGuiCol_Button,
                slotRef.isEmpty() ? ImVec4(0.16f, 0.18f, 0.24f, 1.0f) :
                (slotRef.item->category == game::inventory::ItemCategory::StarSkill ? ImVec4(0.15f, 0.26f, 0.46f, 1.0f) :
                 slotRef.item->category == game::inventory::ItemCategory::Weapon ? ImVec4(0.36f, 0.22f, 0.12f, 1.0f) : ImVec4(0.22f, 0.26f, 0.22f, 1.0f)));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.34f, 0.38f, 0.48f, 1.0f));

            std::string label = slotRef.isEmpty() ? "" : slotRef.item->name.substr(0, std::min<size_t>(6, slotRef.item->name.size()));
            if (ImGui::Button(label.c_str(), {slot, slot}))
            {
                m_selectedInventorySlot = idx;
                if (!slotRef.isEmpty() && slotRef.item->category == game::inventory::ItemCategory::Weapon)
                    m_weaponBar.equipFromInventory(m_weaponBar.getActiveIndex(), idx, m_inventory);
            }

            if (!slotRef.isEmpty() && ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(slotRef.item->name.c_str());
                ImGui::TextDisabled("%s: %d / %d", locale::T("inventory.quantity").c_str(), slotRef.count, slotRef.item->max_stack);
                ImGui::EndTooltip();
            }

            if (!slotRef.isEmpty())
            {
                ImVec2 min = ImGui::GetItemRectMin();
                ImGui::GetWindowDrawList()->AddText({min.x + 4.0f, min.y + 28.0f}, IM_COL32(240, 240, 240, 220), std::to_string(slotRef.count).c_str());
            }

            ImGui::PopStyleColor(2);
            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::TextDisabled("I: 背包  O: 设置  1-5: 切武器  Q: 闪光冲刺");
        ImGui::End();
    }

    void VoxelScene::renderSettingsUI()
    {
        if (!m_showSettings)
            return;

        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({18.0f, std::max(136.0f, display.y * 0.16f)}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({356.0f, 278.0f}, ImGuiCond_Always);
        ImGui::Begin("设置", &m_showSettings,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        auto pageButton = [&](const char *label, SettingsPage page)
        {
            if (page != SettingsPage::World)
                ImGui::SameLine();
            if (ImGui::Selectable(label, m_settingsPage == page, 0, {78.0f, 0.0f}))
                m_settingsPage = page;
        };

        pageButton("世界", SettingsPage::World);
        pageButton("战斗", SettingsPage::Combat);
        pageButton("输入", SettingsPage::Input);
        pageButton("内存/调试", SettingsPage::Diagnostics);
        ImGui::Separator();

        if (m_settingsPage == SettingsPage::World)
        {
            int weatherIndex = static_cast<int>(m_weatherSystem.getCurrentWeather());
            const char *weatherNames[] = {"晴天", "小雨", "中雨", "大雨", "雷雨"};
            if (ImGui::Combo("天气", &weatherIndex, weatherNames, IM_ARRAYSIZE(weatherNames)))
                m_weatherSystem.setWeather(static_cast<game::weather::WeatherType>(weatherIndex), 1.2f);
            ImGui::SliderFloat("第三人称距离", &m_thirdPersonDistance, 3.2f, 8.5f, "%.1f");
            ImGui::Checkbox("显示输入提示", &m_showInputHints);
            ImGui::Checkbox("显示目标提示", &m_highlightTargetBlock);
            ImGui::TextDisabled("世界页用于调整探索与环境显示。");
        }
        else if (m_settingsPage == SettingsPage::Combat)
        {
            ImGui::SliderFloat("基础移速", &m_baseMoveSpeed, 5.0f, 16.0f, "%.1f");
            ImGui::SliderFloat("攻击", &m_attack, 10.0f, 80.0f, "%.0f");
            ImGui::SliderFloat("防御", &m_defense, 2.0f, 20.0f, "%.0f");
            ImGui::TextDisabled("近战与技能都会吃当前攻击/防御参数。");
        }
        else if (m_settingsPage == SettingsPage::Input)
        {
            ImGui::TextUnformatted("操作提示");
            ImGui::Separator();
            ImGui::BulletText("WASD: 平移    Space/Shift: 升降");
            ImGui::BulletText("鼠标: 视角    F3: 第一/第三人称");
            ImGui::BulletText("左键: 巨剑攻击    右键: 填补地形");
            ImGui::BulletText("Q: 主动星技    B: 撤离交互");
            ImGui::BulletText("I: 背包    O: 设置    ESC: 暂停");
            ImGui::TextDisabled("暂停或打开界面时会自动释放鼠标。");
        }
        else
        {
            ImGui::Checkbox("显示管理器详情", &m_showManagerDetails);
            ImGui::SliderInt("每帧区块加载预算", &m_chunkLoadBudget, 1, 8);
            ImGui::SliderInt("每帧网格重建预算", &m_chunkMeshBudget, 1, 8);
            ImGui::TextDisabled("降低卡顿可减少预算，提升首屏速度可增大预算。");
            if (m_showManagerDetails)
                renderManagerDiagnosticsUI();
        }

        ImGui::End();
    }

    void VoxelScene::renderManagerDiagnosticsUI()
    {
        const auto inputStats = _context.getInputManager().getDebugStats();
        const auto resourceStats = _context.getResourceManager().getDebugStats();
        const auto logicalSize = _context.getRenderer().getLogicalSize();
        SDL_Window *window = _context.getRenderer().getWindow();

        int windowW = 0;
        int windowH = 0;
        if (window)
            SDL_GetWindowSize(window, &windowW, &windowH);

        size_t generatedChunks = 0;
        size_t dirtyChunks = 0;
        size_t totalVertices = 0;
        size_t totalVoxelBytes = 0;
        size_t totalDensityBytes = 0;
        size_t totalCornerBytes = 0;
        for (const auto &[key, chunk] : m_chunkMeshes)
        {
            generatedChunks += chunk.generated ? 1u : 0u;
            dirtyChunks += chunk.dirty ? 1u : 0u;
            totalVertices += static_cast<size_t>(std::max(chunk.vertexCount, 0));
            totalVoxelBytes += chunk.voxels.size() * sizeof(unsigned char);
            totalDensityBytes += chunk.densities.size() * sizeof(float);
            totalCornerBytes += chunk.cornerDensityCache.size() * sizeof(float);
        }

        const float totalChunkMB = static_cast<float>(totalVoxelBytes + totalDensityBytes + totalCornerBytes) / (1024.0f * 1024.0f);

        ImGui::SeparatorText("体素场景");
        ImGui::Text("已加载区块: %d", static_cast<int>(m_chunkMeshes.size()));
        ImGui::Text("活跃区块: %d", static_cast<int>(m_activeChunkKeys.size()));
        ImGui::Text("待加载队列: %d", static_cast<int>(m_pendingChunkLoads.size()));
        ImGui::Text("已生成/脏区块: %d / %d", static_cast<int>(generatedChunks), static_cast<int>(dirtyChunks));
        ImGui::Text("总顶点数: %d", static_cast<int>(totalVertices));
        ImGui::Text("区块CPU内存: %.2f MB", totalChunkMB);
        ImGui::Text("怪物/背包槽位: %d / %d", static_cast<int>(m_monsters.size()), m_inventory.getSlotCount());

        ImGui::SeparatorText("输入管理器");
        ImGui::Text("动作绑定: %d", static_cast<int>(inputStats.actionBindingCount));
        ImGui::Text("输入映射: %d", static_cast<int>(inputStats.inputBindingCount));
        ImGui::Text("状态项: %d", static_cast<int>(inputStats.actionStateCount));
        ImGui::Text("鼠标: %.1f, %.1f", inputStats.mousePosition.x, inputStats.mousePosition.y);
        ImGui::Text("滚轮/退出: %.1f / %s", inputStats.mouseWheelDelta, inputStats.shouldQuit ? "true" : "false");

        ImGui::SeparatorText("渲染器");
        ImGui::Text("窗口尺寸: %d x %d", windowW, windowH);
        ImGui::Text("逻辑尺寸: %.0f x %.0f", logicalSize.x, logicalSize.y);
        ImGui::Text("GL对象: shader=%u chunkVAO≈%d", m_shader, static_cast<int>(m_chunkMeshes.size()));
        ImGui::Text("视角模式: %s", m_thirdPersonView ? "第三人称" : "第一人称");

        ImGui::SeparatorText("资源管理器");
        ImGui::Text("后端: renderer=%s gpu=%s sampler=%s",
            resourceStats.hasRenderer ? "on" : "off",
            resourceStats.hasGPUDevice ? "on" : "off",
            resourceStats.hasDefaultSampler ? "on" : "off");
        ImGui::Text("纹理/音效/音乐: %d / %d / %d",
            static_cast<int>(resourceStats.textureCount),
            static_cast<int>(resourceStats.audioCount),
            static_cast<int>(resourceStats.musicCount));
        ImGui::Text("字体/着色器: %d / %d",
            static_cast<int>(resourceStats.fontCount),
            static_cast<int>(resourceStats.shaderCount));

        ImGui::SeparatorText("玩法系统");
        ImGui::Text("天气/时段: %s / %s", m_weatherSystem.getCurrentWeatherName(), m_timeOfDaySystem.getPhaseName());
        ImGui::Text("路线进度: %d / %d", std::max(m_routeProgressIndex + 1, 0), static_cast<int>(m_routeData.path.size()));
        ImGui::Text("星技槽位: %d", static_cast<int>(m_starSockets.size()));
    }

    void VoxelScene::renderInputHintsUI()
    {
        if (!m_showInputHints || m_showPauseMenu || m_showSettlement)
            return;

        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({18.0f, display.y - 156.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({278.0f, 112.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.42f);
        ImGui::Begin("操作提示", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
        ImGui::TextDisabled("WASD 移动  Space/Shift 升降");
        ImGui::TextDisabled("鼠标视角  F3 切视角  ESC 暂停");
        ImGui::TextDisabled("左键攻击  右键填补  Q 技能  B 撤离");
        ImGui::TextDisabled("I 背包  O 设置");
        ImGui::End();
    }

    void VoxelScene::handleInput()
    {
        const bool *keys = SDL_GetKeyboardState(nullptr);
        bool inventoryKey = keys[SDL_SCANCODE_I];
        bool settingsKey = keys[SDL_SCANCODE_O];
        bool skillKey = keys[SDL_SCANCODE_Q];
        bool perspectiveKey = keys[SDL_SCANCODE_F3];
        bool pauseKey = keys[SDL_SCANCODE_ESCAPE];

        if (pauseKey && !m_prevPauseKey && isRouteSetupComplete())
        {
            if (m_showSettings)
                m_showSettings = false;
            else
                m_showPauseMenu = !m_showPauseMenu;
        }

        if (inventoryKey && !m_prevInventoryKey)
            m_showInventory = !m_showInventory;
        if (settingsKey && !m_prevSettingsKey)
            m_showSettings = !m_showSettings;
        if (skillKey && !m_prevSkillKey)
            triggerActiveStarSkills();
        if (perspectiveKey && !m_prevPerspectiveKey)
            m_thirdPersonView = !m_thirdPersonView;

        updateMouseCapture();

        if (!isRouteSetupComplete())
        {
            m_prevInventoryKey = inventoryKey;
            m_prevSettingsKey = settingsKey;
            m_prevSkillKey = skillKey;
            m_prevPerspectiveKey = perspectiveKey;
            m_prevPauseKey = pauseKey;
            m_prevLeftMouse = false;
            m_prevRightMouse = false;
            return;
        }

        if (m_showSettlement || m_showPauseMenu)
        {
            m_prevInventoryKey = inventoryKey;
            m_prevSettingsKey = settingsKey;
            m_prevSkillKey = skillKey;
            m_prevPerspectiveKey = perspectiveKey;
            m_prevPauseKey = pauseKey;
            m_prevLeftMouse = false;
            m_prevRightMouse = false;
            return;
        }

        for (int i = 0; i < game::weapon::WeaponBar::SLOTS; ++i)
        {
            const SDL_Scancode code = static_cast<SDL_Scancode>(SDL_SCANCODE_1 + i);
            bool pressed = keys[code];
            if (pressed && !m_prevWeaponKeys[i])
                m_weaponBar.setActiveIndex(i);
            m_prevWeaponKeys[i] = pressed;
        }

        auto target = raycastBlock();
        Uint32 mouseButtons = SDL_GetMouseState(nullptr, nullptr);
        bool leftDown = (mouseButtons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
        bool rightDown = (mouseButtons & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
        bool evacuateKey = keys[SDL_SCANCODE_B];

        glm::vec3 evacCenter = getCellWorldCenter(m_routeData.evacCell());
        bool inEvacZone = glm::distance(glm::vec2(m_cameraPos.x, m_cameraPos.z), glm::vec2(evacCenter.x, evacCenter.z)) <= kEvacInteractRadius;
        if (evacuateKey && inEvacZone && m_routeObjectiveReached)
        {
            m_showSettlement = true;
            updateMouseCapture();
        }

        if (!m_showInventory && leftDown && !m_prevLeftMouse)
        {
            performWeaponAttack(target);
        }
        if (!m_showInventory && target.hit && rightDown && !m_prevRightMouse)
        {
            if (isInside(target.place.x, target.place.y, target.place.z) && !isSolid(target.place.x, target.place.y, target.place.z))
            {
                applyDensityBrush(glm::vec3(target.place) + glm::vec3(0.5f), 0.9f, 1.2f, 1);
            }
        }

        m_prevInventoryKey = inventoryKey;
        m_prevSettingsKey = settingsKey;
        m_prevSkillKey = skillKey;
        m_prevPerspectiveKey = perspectiveKey;
        m_prevPauseKey = pauseKey;
        m_prevLeftMouse = leftDown;
        m_prevRightMouse = rightDown;
    }

    void VoxelScene::renderOverlay(const TargetBlock &target)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (m_setupPhase == SetupPhase::PlanetSelect)
        {
            renderPlanetSelectUI();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            return;
        }
        if (m_setupPhase == SetupPhase::RouteSelect)
        {
            renderRouteSelectUI();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            return;
        }

        ImGui::SetNextWindowPos({10.0f, 10.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.45f);
        ImGui::Begin("##voxel_debug", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Voxel 3D Adventure");
        ImGui::Text("Pos: %.1f %.1f %.1f", m_cameraPos.x, m_cameraPos.y, m_cameraPos.z);
        ImGui::Text("Yaw/Pitch: %.1f / %.1f", m_yaw, m_pitch);
        ImGui::Text("WASD 移动  Space/Shift 升降");
        ImGui::Text("鼠标已%s  F3切换%s", m_mouseCaptured ? "固定" : "释放", m_thirdPersonView ? "第一人称" : "第三人称");
        ImGui::Text("左键巨剑  右键放置");
        ImGui::Text("昼夜 %.0f%%  天空 %.0f%%", m_timeOfDaySystem.getDaylightFactor() * 100.0f, m_weatherSystem.getSkyVisibility() * 100.0f);
        ImGui::Text("区块 %d / %d", static_cast<int>(m_activeChunkKeys.size()), static_cast<int>(m_chunkMeshes.size()));
        ImGui::Text("星球 %s", game::route::RouteData::planetName(m_routeData.selectedPlanet));
        if (m_routeData.isValid())
        {
            ImGui::Text("撤离 %s", game::route::RouteData::cellLabel(m_routeData.evacCell()).c_str());
            ImGui::Text("路线 %d / %d", std::max(m_routeProgressIndex + 1, 0), static_cast<int>(m_routeData.path.size()));
        }
        if (target.hit && m_highlightTargetBlock)
            ImGui::Text("Target: %d %d %d", target.block.x, target.block.y, target.block.z);
        else
            ImGui::TextUnformatted("Target: <none>");
        ImGui::End();

        renderWeaponBar();
        renderSkillHUD();
        renderPlayerStatusHUD();
        renderInventoryUI();
        renderSettingsUI();
        renderInputHintsUI();

        if (m_routeData.isValid())
        {
            glm::vec3 evacCenter = getCellWorldCenter(m_routeData.evacCell());
            bool inEvacZone = glm::distance(glm::vec2(m_cameraPos.x, m_cameraPos.z), glm::vec2(evacCenter.x, evacCenter.z)) <= kEvacInteractRadius;

            ImGui::SetNextWindowPos({18.0f, 302.0f}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({276.0f, 156.0f}, ImGuiCond_Always);
            ImGui::Begin("任务", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
            ImGui::Text("出发点: %s", game::route::RouteData::cellLabel(m_routeData.startCell()).c_str());
            ImGui::Text("目标点: %s", game::route::RouteData::cellLabel(m_routeData.objectiveCell).c_str());
            ImGui::Text("撤离点: %s", game::route::RouteData::cellLabel(m_routeData.evacCell()).c_str());
            ImGui::Separator();
            ImGui::Text("路线进度: %d / %d", std::max(m_routeProgressIndex + 1, 1), static_cast<int>(m_routeData.path.size()));
            ImGui::TextColored(m_routeObjectiveReached ? ImVec4(0.45f, 1.0f, 0.55f, 1.0f) : ImVec4(1.0f, 0.82f, 0.28f, 1.0f),
                m_routeObjectiveReached ? "目标矿区已抵达" : "先前往目标矿区");
            if (inEvacZone)
            {
                ImGui::TextColored(m_routeObjectiveReached ? ImVec4(0.45f, 1.0f, 0.55f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
                    m_routeObjectiveReached ? "按 B 执行撤离" : "已到撤离点，但目标尚未完成");
            }
            else
            {
                ImGui::TextDisabled("沿着信标柱推进到撤离点");
            }
            ImGui::End();
        }

        if (m_showPauseMenu)
        {
            ImVec2 display = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos({display.x * 0.5f, display.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
            ImGui::SetNextWindowSize({360.0f, 276.0f}, ImGuiCond_Always);
            ImGui::Begin("暂停菜单", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::TextDisabled("ESC 继续游戏");
            ImGui::TextDisabled("F3 视角切换  O 设置  I 背包");
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button("继续游戏", {-1.0f, 38.0f}))
            {
                m_showPauseMenu = false;
                updateMouseCapture();
            }
            if (ImGui::Button("设置", {-1.0f, 38.0f}))
            {
                m_settingsPage = SettingsPage::Input;
                m_showSettings = true;
            }
            if (ImGui::Button("返回主菜单", {-1.0f, 38.0f}))
            {
                auto scene = std::make_unique<MenuScene>("MenuScene", _context, _scene_manager);
                _scene_manager.requestReplaceScene(std::move(scene));
            }
            ImGui::End();
        }

        if (m_showSettlement)
        {
            ImVec2 display = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos({display.x * 0.5f, display.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
            ImGui::SetNextWindowSize({360.0f, 220.0f}, ImGuiCond_Always);
            ImGui::Begin("撤离完成", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::TextColored({0.45f, 1.0f, 0.55f, 1.0f}, "任务完成，已成功撤离");
            ImGui::Separator();
            ImGui::Text("星球: %s", game::route::RouteData::planetName(m_routeData.selectedPlanet));
            ImGui::Text("目标: %s", game::route::RouteData::cellLabel(m_routeData.objectiveCell).c_str());
            ImGui::Text("撤离: %s", game::route::RouteData::cellLabel(m_routeData.evacCell()).c_str());
            ImGui::Text("推进: %d / %d", std::max(m_routeProgressIndex + 1, 1), static_cast<int>(m_routeData.path.size()));
            ImGui::Spacing();
            if (ImGui::Button("继续探索", {-1.0f, 36.0f}))
            {
                m_showSettlement = false;
                updateMouseCapture();
            }
            if (ImGui::Button("规划下一次任务", {-1.0f, 36.0f}))
            {
                m_showSettlement = false;
                m_setupPhase = SetupPhase::PlanetSelect;
                m_routeData.path.clear();
                m_routeProgressIndex = -1;
                m_routeObjectiveReached = false;
                updateMouseCapture();
            }
            ImGui::End();
        }

        m_weatherSystem.render(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);

        ImDrawList *dl = ImGui::GetForegroundDrawList();
        ImVec2 center = {ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f};
        dl->AddLine({center.x - 8.0f, center.y}, {center.x + 8.0f, center.y}, IM_COL32(255,255,255,220), 1.8f);
        dl->AddLine({center.x, center.y - 8.0f}, {center.x, center.y + 8.0f}, IM_COL32(255,255,255,220), 1.8f);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void VoxelScene::renderPlanetSelectUI()
    {
        ImGuiIO &io = ImGui::GetIO();
        float dw = io.DisplaySize.x;
        float dh = io.DisplaySize.y;
        const auto &presets = game::route::RouteData::planetPresets();
        const int count = static_cast<int>(presets.size());
        const float cardW = 230.0f;
        const float cardH = 220.0f;
        const float gap = 18.0f;
        const float totalW = count * cardW + (count - 1) * gap;
        const float startX = (dw - totalW) * 0.5f;
        const float startY = (dh - cardH) * 0.5f - 30.0f;

        ImDrawList *bg = ImGui::GetBackgroundDrawList();
        bg->AddRectFilledMultiColor({0, 0}, {dw, dh},
            IM_COL32(4, 8, 22, 255), IM_COL32(4, 8, 22, 255), IM_COL32(10, 18, 40, 255), IM_COL32(10, 18, 40, 255));
        bg->AddText(ImGui::GetFont(), 20.0f, {(dw - 150.0f) * 0.5f, startY - 50.0f}, IM_COL32(140, 200, 255, 240), "选择目标星球");

        static const ImVec4 planetColors[] = {
            {0.15f, 0.55f, 0.22f, 1.0f},
            {0.72f, 0.28f, 0.08f, 1.0f},
            {0.55f, 0.78f, 0.92f, 1.0f},
            {0.24f, 0.16f, 0.42f, 1.0f},
        };

        for (int i = 0; i < count; ++i)
        {
            bool selected = i == m_selectedPlanetIndex;
            float cx = startX + i * (cardW + gap);
            ImGui::SetNextWindowPos({cx, startY}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({cardW, cardH}, ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(selected ? 0.96f : 0.78f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, selected ? ImVec4(0.10f, 0.22f, 0.42f, 0.97f) : ImVec4(0.06f, 0.10f, 0.22f, 0.80f));
            char winId[16];
            std::snprintf(winId, sizeof(winId), "##planet%d", i);
            ImGui::Begin(winId, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);
            ImDrawList *dl = ImGui::GetWindowDrawList();
            ImVec2 wPos = ImGui::GetWindowPos();
            ImVec4 pc = planetColors[i];
            float ccx = wPos.x + cardW * 0.5f;
            float ccy = wPos.y + 52.0f;
            dl->AddCircleFilled({ccx, ccy}, 34.0f, IM_COL32(static_cast<int>(pc.x * 255), static_cast<int>(pc.y * 255), static_cast<int>(pc.z * 255), 220));
            if (selected)
                dl->AddCircle({ccx, ccy}, 42.0f, IM_COL32(120, 200, 255, 180), 60, 1.5f);
            dl->AddLine({wPos.x + 10.0f, wPos.y + 94.0f}, {wPos.x + cardW - 10.0f, wPos.y + 94.0f}, IM_COL32(70, 120, 200, 100), 1.0f);
            ImGui::SetCursorPos({10.0f, 102.0f});
            ImGui::TextColored(selected ? ImVec4(0.4f, 0.9f, 1.0f, 1.0f) : ImVec4(0.8f, 0.85f, 0.95f, 1.0f), "%s", presets[static_cast<size_t>(i)].name);
            ImGui::TextWrapped("%s", presets[static_cast<size_t>(i)].summary);
            ImGui::TextColored({0.5f, 0.7f, 0.5f, 1.0f}, "昼夜: %.0fs", presets[static_cast<size_t>(i)].dayLengthSeconds);
            ImGui::End();
            ImGui::PopStyleColor();

            ImGui::SetNextWindowPos({cx, startY + cardH + 6.0f}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({cardW, 40.0f}, ImGuiCond_Always);
            char btnWinId[20];
            std::snprintf(btnWinId, sizeof(btnWinId), "##pbtn%d", i);
            ImGui::Begin(btnWinId, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
            if (ImGui::Button(selected ? "✓ 已选" : "选择", {cardW, 32.0f}))
                m_selectedPlanetIndex = i;
            ImGui::End();
        }

        ImGui::SetNextWindowPos({(dw - 416.0f) * 0.5f, startY + cardH + 56.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({416.0f, 52.0f}, ImGuiCond_Always);
        ImGui::Begin("##planet_confirm", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
        if (ImGui::Button("确认星球，规划撤离路线", {200.0f, 44.0f}))
        {
            const auto &preset = presets[static_cast<size_t>(m_selectedPlanetIndex)];
            m_routeData.path.clear();
            m_routeData.applyPlanetPreset(preset);
            m_routeData.generateTerrain(preset.seedBias ^ 0xABCD1234ULL);
            m_routeProgressIndex = -1;
            m_routeObjectiveReached = false;
            m_showSettlement = false;
            m_setupPhase = SetupPhase::RouteSelect;
        }
        ImGui::SameLine(0.0f, 16.0f);
        if (ImGui::Button("跳过并直接开始", {200.0f, 44.0f}))
        {
            m_routeData.path = {{1, 1}, {2, 1}};
            confirmRouteSelection();
        }
        ImGui::End();
    }

    void VoxelScene::renderRouteSelectUI()
    {
        ImGuiIO &io = ImGui::GetIO();
        float dw = io.DisplaySize.x;
        float dh = io.DisplaySize.y;
        float rowLabelW = 22.0f;
        float colLabelH = 20.0f;
        float titleH = 28.0f;
        float gridWinW = rowLabelW + kRouteGridPixels;
        float gridWinH = titleH + colLabelH + kRouteGridPixels;
        float rightPanelW = 215.0f;
        float totalW = gridWinW + rightPanelW + 8.0f;
        float winX = (dw - totalW) * 0.5f;
        float winY = (dh - gridWinH) * 0.5f;

        ImGui::SetNextWindowPos({winX, winY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({gridWinW, gridWinH}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.95f);
        ImGui::Begin("##route_grid", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav);
        ImDrawList *dl = ImGui::GetWindowDrawList();

        const auto &planet = game::route::RouteData::planetPresets()[static_cast<size_t>(m_selectedPlanetIndex)];
        ImGui::SetCursorPosX((gridWinW - 220.0f) * 0.5f);
        ImGui::TextColored({0.5f, 0.9f, 1.0f, 1.0f}, "%s  —  选择出发与撤离路线", planet.name);

        ImGui::SetCursorPosX(rowLabelW);
        ImVec2 top = ImGui::GetCursorScreenPos();
        for (int cx = 0; cx < kRouteMapSize; ++cx)
        {
            char lbl[3] = {static_cast<char>('A' + cx), 0, 0};
            dl->AddText({top.x + cx * kRouteCellTotal + 6.0f, top.y}, IM_COL32(150, 210, 255, 200), lbl);
        }
        ImGui::Dummy({kRouteGridPixels, colLabelH});
        ImVec2 rowStart = ImGui::GetCursorScreenPos();
        for (int ry = 0; ry < kRouteMapSize; ++ry)
        {
            char lbl[4];
            std::snprintf(lbl, sizeof(lbl), "%2d", ry + 1);
            dl->AddText({rowStart.x, rowStart.y + ry * kRouteCellTotal + 5.0f}, IM_COL32(150, 210, 255, 200), lbl);
        }

        ImGui::SetCursorPosX(rowLabelW);
        ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
        ImVec2 mp = io.MousePos;
        int hoverCX = (mp.x >= canvasOrigin.x && mp.x < canvasOrigin.x + kRouteGridPixels) ? static_cast<int>((mp.x - canvasOrigin.x) / kRouteCellTotal) : -1;
        int hoverCY = (mp.y >= canvasOrigin.y && mp.y < canvasOrigin.y + kRouteGridPixels) ? static_cast<int>((mp.y - canvasOrigin.y) / kRouteCellTotal) : -1;

        for (int ry = 0; ry < kRouteMapSize; ++ry)
        {
            for (int cx = 0; cx < kRouteMapSize; ++cx)
            {
                float px = canvasOrigin.x + cx * kRouteCellTotal;
                float py = canvasOrigin.y + ry * kRouteCellTotal;
                glm::ivec2 cell{cx, ry};
                int pidx = pathIndexOf(cell);
                int psize = static_cast<int>(m_routeData.path.size());
                bool isObjective = cell == m_routeData.objectiveCell;
                auto tc = game::route::RouteData::terrainColor(m_routeData.terrain[ry][cx]);
                ImU32 fill = IM_COL32(tc.r, tc.g, tc.b, 180);
                if (pidx == 0)
                    fill = IM_COL32(40, 200, 80, 255);
                else if (pidx == psize - 1 && psize >= 2)
                    fill = IM_COL32(220, 70, 70, 255);
                else if (pidx > 0)
                    fill = IM_COL32(tc.r / 2 + 30, tc.g / 2 + 65, tc.b / 2 + 115, 230);
                else if (cx == hoverCX && ry == hoverCY)
                    fill = IM_COL32(std::min(255, tc.r + 50), std::min(255, tc.g + 50), std::min(255, tc.b + 50), 230);
                dl->AddRectFilled({px, py}, {px + kRouteCellSize, py + kRouteCellSize}, fill, 3.0f);
                dl->AddRect({px, py}, {px + kRouteCellSize, py + kRouteCellSize}, isObjective ? IM_COL32(255, 230, 100, 255) : IM_COL32(30, 50, 80, 120), 3.0f, 0, isObjective ? 1.5f : 1.0f);
                if (isObjective)
                    dl->AddText({px + 5.0f, py + 4.0f}, IM_COL32(255, 230, 50, 255), "*");
                if (pidx >= 0)
                {
                    char num[4];
                    std::snprintf(num, sizeof(num), "%d", pidx + 1);
                    dl->AddText({px + 3.0f, py + 4.0f}, IM_COL32(255, 255, 255, 235), num);
                }
            }
        }

        for (int i = 0; i + 1 < static_cast<int>(m_routeData.path.size()); ++i)
        {
            const auto &a = m_routeData.path[static_cast<size_t>(i)];
            const auto &b = m_routeData.path[static_cast<size_t>(i + 1)];
            ImVec2 pa{canvasOrigin.x + a.x * kRouteCellTotal + kRouteCellSize * 0.5f, canvasOrigin.y + a.y * kRouteCellTotal + kRouteCellSize * 0.5f};
            ImVec2 pb{canvasOrigin.x + b.x * kRouteCellTotal + kRouteCellSize * 0.5f, canvasOrigin.y + b.y * kRouteCellTotal + kRouteCellSize * 0.5f};
            dl->AddLine(pa, pb, IM_COL32(255, 230, 80, 55), 8.0f);
            dl->AddLine(pa, pb, IM_COL32(255, 230, 80, 210), 2.0f);
        }

        ImGui::InvisibleButton("##route_canvas", {kRouteGridPixels, kRouteGridPixels});
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && hoverCX >= 0 && hoverCY >= 0)
            handleRouteCellClick(hoverCX, hoverCY, false);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            handleRouteCellClick(hoverCX, hoverCY, true);
        ImGui::End();

        ImGui::SetNextWindowPos({winX + gridWinW + 8.0f, winY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({rightPanelW, gridWinH}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGui::Begin("##route_panel", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
        ImGui::TextColored({0.9f, 0.9f, 0.5f, 1.0f}, "路线信息");
        ImGui::Separator();
        ImGui::TextColored({0.5f, 0.85f, 1.0f, 1.0f}, "星球: %s", planet.name);
        ImGui::TextColored({0.6f, 0.7f, 0.8f, 1.0f}, "%s", planet.summary);
        ImGui::Text("昼夜: %.0fs", planet.dayLengthSeconds);
        ImGui::Spacing();
        ImGui::Separator();

        int psize = static_cast<int>(m_routeData.path.size());
        if (psize == 0)
            ImGui::TextDisabled("点击地图格设置出发点");
        else
        {
            ImGui::Text("出发: %s", game::route::RouteData::cellLabel(m_routeData.startCell()).c_str());
            if (psize >= 2)
                ImGui::Text("撤离: %s", game::route::RouteData::cellLabel(m_routeData.evacCell()).c_str());
            else
                ImGui::TextDisabled("撤离: 继续选择");
            ImGui::Text("步数: %d 格", psize);
        }
        ImGui::Spacing();
        if (m_routeData.objectiveCell.x >= 0)
            ImGui::TextColored({1.0f, 0.9f, 0.2f, 1.0f}, "目标: %s", game::route::RouteData::cellLabel(m_routeData.objectiveCell).c_str());
        ImGui::Separator();
        bool canStart = m_routeData.isValid();
        if (!canStart)
            ImGui::BeginDisabled();
        if (ImGui::Button("确认出发", {rightPanelW - 16.0f, 42.0f}))
            confirmRouteSelection();
        if (!canStart)
            ImGui::EndDisabled();
        if (ImGui::Button("清空路线", {rightPanelW - 16.0f, 30.0f}))
        {
            m_routeData.path.clear();
            m_routeProgressIndex = -1;
        }
        if (ImGui::Button("返回星球选择", {rightPanelW - 16.0f, 30.0f}))
        {
            m_routeProgressIndex = -1;
            m_routeObjectiveReached = false;
            m_setupPhase = SetupPhase::PlanetSelect;
        }
        ImGui::End();
    }

    void VoxelScene::render()
    {
        SDL_Window *window = _context.getRenderer().getWindow();
        if (!window)
            return;

        int width = 1280;
        int height = 720;
        SDL_GetWindowSize(window, &width, &height);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        if (m_glCullFace)
            m_glCullFace(GL_BACK);

        const float daylight = m_timeOfDaySystem.getDaylightFactor();
        const float skyVisibility = m_weatherSystem.getSkyVisibility();
        const glm::vec3 nightSky{0.03f, 0.05f, 0.10f};
        const glm::vec3 daySky{0.46f, 0.68f, 0.92f};
        const glm::vec3 duskSky{0.82f, 0.46f, 0.28f};
        float duskMix = std::max(0.0f, 1.0f - std::abs(m_timeOfDaySystem.getTimeOfDay() - 0.58f) / 0.12f);
        float dawnMix = std::max(0.0f, 1.0f - std::abs(m_timeOfDaySystem.getTimeOfDay() - 0.25f) / 0.12f);
        glm::vec3 clearSky = glm::mix(nightSky, daySky, daylight);
        clearSky = glm::mix(clearSky, duskSky, std::max(duskMix, dawnMix) * 0.55f);
        glm::vec3 fogColor = glm::mix(clearSky * 0.55f, clearSky, skyVisibility * 0.75f + 0.15f);

        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 eye = getCameraEyePosition();
        glm::vec3 renderCamera = getRenderCameraPosition();
        glm::vec3 lookTarget = eye + getForward() * 4.0f;
        glm::mat4 proj = glm::perspective(glm::radians(72.0f), static_cast<float>(width) / static_cast<float>(std::max(height, 1)), 0.05f, 400.0f);
        glm::mat4 view = glm::lookAt(renderCamera, lookTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 mvp = proj * view;

        float sunAngle = m_timeOfDaySystem.getTimeOfDay() * static_cast<float>(M_PI) * 2.0f;
        glm::vec3 lightDir = glm::normalize(glm::vec3(std::cos(sunAngle), -0.55f - daylight * 0.55f, std::sin(sunAngle) * 0.45f));
        float ambientStrength = std::clamp(0.22f + (1.0f - daylight) * 0.26f + (1.0f - skyVisibility) * 0.18f, 0.18f, 0.7f);
        float diffuseStrength = std::clamp(0.22f + daylight * 0.85f * skyVisibility, 0.12f, 1.0f);
        float fogNear = 8.0f + skyVisibility * 8.0f;
        float fogFar = 20.0f + skyVisibility * 26.0f;
        float flash = m_weatherSystem.getCurrentWeather() == game::weather::WeatherType::Thunderstorm ? (1.0f - skyVisibility) * 0.25f : 0.0f;

        glUseProgram(m_shader);
        glUniformMatrix4fv(glGetUniformLocation(m_shader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        if (m_glUniform3fv)
        {
            m_glUniform3fv(glGetUniformLocation(m_shader, "uLightDir"), 1, glm::value_ptr(lightDir));
            m_glUniform3fv(glGetUniformLocation(m_shader, "uCameraPos"), 1, glm::value_ptr(renderCamera));
            m_glUniform3fv(glGetUniformLocation(m_shader, "uFogColor"), 1, glm::value_ptr(fogColor));
        }
        if (m_glUniform1f)
        {
            m_glUniform1f(glGetUniformLocation(m_shader, "uAmbientStrength"), ambientStrength);
            m_glUniform1f(glGetUniformLocation(m_shader, "uDiffuseStrength"), diffuseStrength);
            m_glUniform1f(glGetUniformLocation(m_shader, "uFogNear"), fogNear);
            m_glUniform1f(glGetUniformLocation(m_shader, "uFogFar"), fogFar);
            m_glUniform1f(glGetUniformLocation(m_shader, "uFlash"), flash);
        }
        for (int64_t chunkKeyValue : m_activeChunkKeys)
        {
            auto it = m_chunkMeshes.find(chunkKeyValue);
            if (it == m_chunkMeshes.end())
                continue;
            const auto &chunk = it->second;
            glBindVertexArray(chunk.vao);
            if (m_glDrawArrays && chunk.vertexCount > 0)
                m_glDrawArrays(GL_TRIANGLES, 0, chunk.vertexCount);
        }
        glBindVertexArray(m_monsterVao);
        if (m_glDrawArrays && m_monsterVertexCount > 0)
            m_glDrawArrays(GL_TRIANGLES, 0, m_monsterVertexCount);
        if (m_thirdPersonView)
        {
            glBindVertexArray(m_playerVao);
            if (m_glDrawArrays && m_playerVertexCount > 0)
                m_glDrawArrays(GL_TRIANGLES, 0, m_playerVertexCount);
        }
        glBindVertexArray(0);

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);

        if (!m_thirdPersonView)
        {
            glUseProgram(m_shader);
            glUniformMatrix4fv(glGetUniformLocation(m_shader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
            glBindVertexArray(m_viewModelVao);
            if (m_glDrawArrays && m_viewModelVertexCount > 0)
                m_glDrawArrays(GL_TRIANGLES, 0, m_viewModelVertexCount);
            glBindVertexArray(0);
        }

        renderOverlay(raycastBlock());
    }

    void VoxelScene::clean()
    {
        if (m_shader)
        {
            glDeleteProgram(m_shader);
            m_shader = 0;
        }
        for (auto &[key, chunk] : m_chunkMeshes)
            releaseChunk(chunk);
        m_chunkMeshes.clear();
        if (m_monsterVbo)
        {
            glDeleteBuffers(1, &m_monsterVbo);
            m_monsterVbo = 0;
        }
        if (m_viewModelVbo)
        {
            glDeleteBuffers(1, &m_viewModelVbo);
            m_viewModelVbo = 0;
        }
        if (m_playerVbo)
        {
            glDeleteBuffers(1, &m_playerVbo);
            m_playerVbo = 0;
        }
        if (m_monsterVao)
        {
            glDeleteVertexArrays(1, &m_monsterVao);
            m_monsterVao = 0;
        }
        if (m_viewModelVao)
        {
            glDeleteVertexArrays(1, &m_viewModelVao);
            m_viewModelVao = 0;
        }
        if (m_playerVao)
        {
            glDeleteVertexArrays(1, &m_playerVao);
            m_playerVao = 0;
        }

        if (m_glContext)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            m_glContext = nullptr;
        }
    }
}