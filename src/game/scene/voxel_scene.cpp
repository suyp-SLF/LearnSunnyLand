#include "voxel_scene.h"

#include "menu_scene.h"

#include "../../engine/core/context.h"
#include "../../engine/input/input_manager.h"
#include "../../engine/resource/resource_manager.h"
#include "../../engine/render/renderer.h"
#include "../../engine/render/opengl_renderer.h"
#include "../../engine/scene/scene_manager.h"
#include "../locale/locale_manager.h"
#include <SDL3_image/SDL_image.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <imgui_impl_opengl3_loader.h>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_DEPTH_BUFFER_BIT
#define GL_DEPTH_BUFFER_BIT 0x00000100
#endif
#ifndef GL_BACK
#define GL_BACK 0x0405
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_RGB
#define GL_RGB 0x1907
#endif
#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif
#ifndef GL_POINTS
#define GL_POINTS 0x0000
#endif
// HD-2D FBO constants (OpenGL 3.0+)
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER            0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0      0x8CE0
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT       0x8D00
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER           0x8D41
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24      0x81A6
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F                0x881A
#endif
#ifndef GL_RGB16F
#define GL_RGB16F                 0x881B
#endif

namespace game::scene
{
    namespace
    {
        enum ModelInteractionType : uint8_t
        {
            kModelInteractionNone = 0,
            kModelInteractionSupplyCache,
            kModelInteractionHealStation,
            kModelInteractionPickupAid,
            kModelInteractionSurvivor,
        };

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
        constexpr float kPlayerGravity = 26.0f;
        constexpr float kPlayerJumpVelocity = 8.8f;
        constexpr float kPlayerEyeHeight = 1.72f;
        constexpr float kFireProjectileGravity = 15.0f;
        constexpr float kFireProjectileHorizontalSpeed = 15.5f;
        constexpr float kFireProjectileMinFlightTime = 0.24f;
        constexpr float kFireProjectileMaxFlightTime = 0.72f;

        int loadConfiguredTargetFps()
        {
            std::ifstream file("assets/config.json");
            if (!file.is_open())
                return 144;

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::smatch match;
            const std::regex targetFpsPattern("\\\"target_fps\\\"\\s*:\\s*(\\d+)");
            const std::string content = buffer.str();
            if (std::regex_search(content, match, targetFpsPattern) && match.size() > 1)
                return std::max(1, std::stoi(match[1].str()));
            return 144;
        }

        void saveConfiguredTargetFps(int fps)
        {
            std::ifstream inFile("assets/config.json");
            if (!inFile.is_open())
                return;

            std::stringstream buffer;
            buffer << inFile.rdbuf();
            std::string content = buffer.str();
            inFile.close();

            const std::regex targetFpsPattern("(\\\"target_fps\\\"\\s*:\\s*)\\d+");
            content = std::regex_replace(content, targetFpsPattern, std::string("$1") + std::to_string(fps), std::regex_constants::format_first_only);

            std::ofstream outFile("assets/config.json", std::ios::trunc);
            if (!outFile.is_open())
                return;
            outFile << content;
        }

        const char* skillKeyHint(game::skill::SkillEffect effect)
        {
            switch (effect)
            {
            case game::skill::SkillEffect::FireBlast: return "[攻击]";
            case game::skill::SkillEffect::IceAura:   return "[被动]";
            case game::skill::SkillEffect::WindBoost: return "[被动]";
            case game::skill::SkillEffect::LightDash: return "[Q]";
            case game::skill::SkillEffect::StarJump:  return "[Q]";
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
            const bool flipWinding = glm::dot(glm::cross(axisX, axisY), axisZ) < 0.0f;

            for (int face = 0; face < 6; ++face)
            {
                const glm::vec3 normal = glm::normalize(faceNormals[face]);
                if (flipWinding)
                {
                    vertices.push_back({faces[face][0], color, normal});
                    vertices.push_back({faces[face][2], color, normal});
                    vertices.push_back({faces[face][1], color, normal});
                    vertices.push_back({faces[face][1], color, normal});
                    vertices.push_back({faces[face][2], color, normal});
                    vertices.push_back({faces[face][3], color, normal});
                }
                else
                {
                    vertices.push_back({faces[face][0], color, normal});
                    vertices.push_back({faces[face][1], color, normal});
                    vertices.push_back({faces[face][2], color, normal});
                    vertices.push_back({faces[face][1], color, normal});
                    vertices.push_back({faces[face][3], color, normal});
                    vertices.push_back({faces[face][2], color, normal});
                }
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

        std::vector<std::string> splitString(const std::string &text, char delimiter)
        {
            std::vector<std::string> parts;
            std::stringstream stream(text);
            std::string part;
            while (std::getline(stream, part, delimiter))
                parts.push_back(part);
            return parts;
        }

        struct ObjIndex
        {
            int position = 0;
            int uv = 0;
            int normal = 0;
        };

        ObjIndex parseObjIndex(const std::string &token)
        {
            ObjIndex index{};
            auto parts = splitString(token, '/');
            if (!parts.empty() && !parts[0].empty())
                index.position = std::stoi(parts[0]);
            if (parts.size() > 1 && !parts[1].empty())
                index.uv = std::stoi(parts[1]);
            if (parts.size() > 2 && !parts[2].empty())
                index.normal = std::stoi(parts[2]);
            return index;
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

        bool projectWorldToScreen(const glm::vec3 &worldPos,
                                  const glm::mat4 &proj,
                                  const glm::mat4 &view,
                                  const glm::vec2 &displaySize,
                                  ImVec2 &screen)
        {
            glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
            if (clip.w <= 0.001f)
                return false;

            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (ndc.z < -1.0f || ndc.z > 1.0f)
                return false;

            screen.x = (ndc.x * 0.5f + 0.5f) * displaySize.x;
            screen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * displaySize.y;
            return true;
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
        initModelResources();
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
        m_inventory.addItem({"star_jump",  "星跳星技", 1, Cat::StarSkill}, 1);

        m_starSockets[0].item = Item{"star_fire", "炎焰星技", 1, Cat::StarSkill};
        m_starSockets[0].count = 1;
        m_starSockets[1].item = Item{"star_wind", "疾风星技", 1, Cat::StarSkill};
        m_starSockets[1].count = 1;
        m_starSockets[2].item = Item{"star_light", "闪光星技", 1, Cat::StarSkill};
        m_starSockets[2].count = 1;
        m_starSockets[3].item = Item{"star_jump",  "星跳星技", 1, Cat::StarSkill};
        m_starSockets[3].count = 1;

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
        m_glUniform2f = reinterpret_cast<Uniform2fProc>(SDL_GL_GetProcAddress("glUniform2f"));
        m_glBlendFunc = reinterpret_cast<BlendFuncProc>(SDL_GL_GetProcAddress("glBlendFunc"));
        m_glDepthMask = reinterpret_cast<DepthMaskProc>(SDL_GL_GetProcAddress("glDepthMask"));

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

        glGenVertexArrays(1, &m_effectVao);
        glGenBuffers(1, &m_effectVbo);
        glBindVertexArray(m_effectVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_effectVbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glBindVertexArray(0);

        const char *modelVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;
uniform mat4 uMVP;
uniform mat4 uModel;
void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uMVP * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vUV = aUV;
}
)";

        const char *modelFragSrc = R"(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
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
    vec3 baseColor = texture(uTex, vUV).rgb;
    vec3 normal = normalize(vNormal);
    float lambert = max(dot(normal, normalize(-uLightDir)), 0.0);
    vec3 lit = baseColor * (uAmbientStrength + lambert * uDiffuseStrength + uFlash * 0.25);
    float dist = length(vWorldPos - uCameraPos);
    float fogSpan = max(uFogFar - uFogNear, 0.001);
    float fogFactor = clamp((uFogFar - dist) / fogSpan, 0.0, 1.0);
    vec3 finalColor = mix(uFogColor, lit, fogFactor);
    FragColor = vec4(finalColor, 1.0);
}
)";

        unsigned int modelVs = compileShader(GL_VERTEX_SHADER, modelVertSrc);
        unsigned int modelFs = compileShader(GL_FRAGMENT_SHADER, modelFragSrc);
        m_modelShader = glCreateProgram();
        glAttachShader(m_modelShader, modelVs);
        glAttachShader(m_modelShader, modelFs);
        glLinkProgram(m_modelShader);
        glDeleteShader(modelVs);
        glDeleteShader(modelFs);

        const char *dashStarVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aParams;
layout(location = 2) in vec4 aExtra;
out vec4 vParams;
out vec4 vExtra;
out vec3 vWorldPos;
uniform mat4 uView;
uniform mat4 uProj;
uniform float uViewportHeight;
void main()
{
    vec4 viewPos = uView * vec4(aPos, 1.0);
    gl_Position = uProj * viewPos;
    float dist = max(-viewPos.z, 0.15);
    gl_PointSize = max(6.0, aParams.x * uViewportHeight / dist);
    vParams = aParams;
    vExtra = aExtra;
    vWorldPos = aPos;
}
)";

        const char *dashStarFragSrc = R"(
#version 330 core
in vec4 vParams;
in vec4 vExtra;
in vec3 vWorldPos;
out vec4 FragColor;
uniform sampler2D uGradientA;
uniform sampler2D uGradientB;
uniform vec3 uFogColor;
uniform vec3 uCameraPos;
uniform float uFogNear;
uniform float uFogFar;
uniform float uGlobalTime;

mat2 rot2(float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

float crossBeam(vec2 uv, float widthScale, float lengthScale)
{
    vec2 a = abs(uv);
    float vertical = 1.0 / (a.x * widthScale + 1.0);
    float horizontal = 1.0 / (a.y * widthScale + 1.0);
    float diagonalA = 1.0 / (abs(uv.x + uv.y) * lengthScale + 1.1);
    float diagonalB = 1.0 / (abs(uv.x - uv.y) * lengthScale + 1.1);
    return vertical * 1.25 + horizontal * 1.25 + 0.22 * (diagonalA + diagonalB);
}

float rand2(vec2 st, float seed)
{
    return fract(sin(dot(st, vec2(seed + 12.9898, 78.233))) * 43758.5453123);
}

float remap01(float prob, float starValue)
{
    return clamp((starValue - prob) / max(1.0 - prob, 0.0001), 0.0, 1.0);
}

void main()
{
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float radial = dot(uv, uv);
    if (radial > 1.85)
        discard;

    float age = clamp(vParams.y, 0.0, 1.0);
    float life = 1.0 - age;
    float seedA = vParams.z;
    float seedB = vParams.w;
    float brightness = vExtra.x;
    float alphaScale = vExtra.y;
    float driftScale = vExtra.z;
    float variant = vExtra.w;

    float phase = uGlobalTime * (5.0 + variant * 3.0) + seedA * 37.0 + seedB * 13.0;
    float sizeStar = max(vParams.x * (2.2 + variant * 0.6), 32.0);
    float frequencyStar = 0.11;
    float probability = 1.0 - frequencyStar;
    float brightnessStar = 2.1 * brightness;
    float shineFrequencyStar = 7.0 + variant * 4.0;
    float frequencyBgStar = 0.992;
    float shineFrequencyBgStar = 1.0 + variant * 1.5;
    vec2 travel = vec2(uGlobalTime * (0.06 + seedA * 0.03), uGlobalTime * (0.05 + seedB * 0.03)) * (0.25 + driftScale);
    vec2 fragCoord = gl_PointCoord * sizeStar;
    vec4 gradA = texture(uGradientA, vec2(fract(seedA + 0.05 * sin(phase)), 0.5));
    vec4 gradB = texture(uGradientB, vec2(fract(seedB + 0.05 * cos(phase)), 0.5));

    vec3 color = vec3(0.0);
    float alpha = 0.0;
    int starIterations = 3;
    for (int iter = 1; iter <= starIterations; ++iter)
    {
        float layer = float(iter);
        float cellSize = sizeStar / layer;
        vec2 pos = floor(fragCoord / cellSize + travel * (0.85 + 0.25 * layer));
        float starValue = rand2(pos + vec2(seedA * 9.0, seedB * 11.0), 1.0 + variant * 17.0);
        if (starValue > probability)
        {
            vec2 center = cellSize * pos + vec2(cellSize * 0.5);
            float twinkle = 0.9 + 0.2 * sin(uGlobalTime * shineFrequencyStar + remap01(probability, starValue) * 45.0 + layer * 0.8);
            vec2 modifiedCoords = fragCoord + travel * cellSize;
            float dx = clamp(abs(modifiedCoords.x - center.x), 0.5, max(cellSize * 0.5 - 1.0, 0.5));
            float dy = clamp(abs(modifiedCoords.y - center.y), 0.5, max(cellSize * 0.5 - 1.0, 0.5));
            float cross = twinkle * twinkle * brightnessStar / layer / dx / dy;
            vec2 beamUv = (modifiedCoords - center) / max(cellSize * 0.5, 1.0);
            float beam = crossBeam(rot2(phase * 0.08 + layer * 0.04) * beamUv, 11.0 + layer * 1.2, 9.5 + layer * 1.0);
            float glow = exp(-length(beamUv) * (2.4 + layer * 0.45));
            float starMask = cross * beam * glow;
            vec4 colormapA = texture(uGradientA, vec2(remap01(probability, starValue), 0.5));
            color += colormapA.rgb * starMask;
            alpha += starMask * (1.0 - age);
        }
    }

    float bgRand = rand2(gl_PointCoord * 19.0 + vec2(seedB * 3.0, seedA * 7.0), 2.0 + seedA * 13.0);
    if (bgRand > frequencyBgStar)
    {
        float bgColor = bgRand * (0.85 * sin(uGlobalTime * shineFrequencyBgStar * (bgRand * 5.0) + 720.0 * bgRand) + 0.95);
        vec4 colormapB = texture(uGradientB, vec2(bgRand, 0.5));
        color += colormapB.rgb * bgColor * 0.26;
        alpha += bgColor * 0.12;
    }

    float core = exp(-radial * (13.0 - variant * 2.0)) * (1.0 + variant * 1.1);
    float halo = exp(-radial * 4.2) * 0.18;
    color += mix(gradA.rgb, vec3(1.0), 0.42) * core;
    color += mix(gradB.rgb, gradA.rgb, 0.5) * halo;

    float outerFade = smoothstep(1.25, 0.08, sqrt(radial));
    alpha = clamp((alpha * 0.58 + core * 0.52 + halo) * alphaScale * life * outerFade, 0.0, 1.0);
    if (alpha <= 0.01)
        discard;

    color *= brightness;
    float dist = length(vWorldPos - uCameraPos);
    float fogSpan = max(uFogFar - uFogNear, 0.001);
    float fogFactor = clamp((uFogFar - dist) / fogSpan, 0.0, 1.0);
    vec3 finalColor = mix(uFogColor, color, fogFactor);
    FragColor = vec4(finalColor, alpha);
}
)";

        unsigned int dashStarVs = compileShader(GL_VERTEX_SHADER, dashStarVertSrc);
        unsigned int dashStarFs = compileShader(GL_FRAGMENT_SHADER, dashStarFragSrc);
        m_dashStarShader = glCreateProgram();
        glAttachShader(m_dashStarShader, dashStarVs);
        glAttachShader(m_dashStarShader, dashStarFs);
        glLinkProgram(m_dashStarShader);
        glDeleteShader(dashStarVs);
        glDeleteShader(dashStarFs);

        const char *fireFieldFragSrc = R"(
#version 330 core
in vec4 vParams;
in vec4 vExtra;
in vec3 vWorldPos;
out vec4 FragColor;
uniform vec3 uFogColor;
uniform vec3 uCameraPos;
uniform float uFogNear;
uniform float uFogFar;
uniform float uGlobalTime;

// IQ simplex noise
vec2 hashFire(vec2 p)
{
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float noiseFire(in vec2 p)
{
    const float K1 = 0.366025404;
    const float K2 = 0.211324865;
    vec2 i = floor(p + (p.x + p.y) * K1);
    vec2 a = p - i + (i.x + i.y) * K2;
    vec2 o = (a.x > a.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec2 b = a - o + K2;
    vec2 c = a - 1.0 + 2.0 * K2;
    vec3 h = max(0.5 - vec3(dot(a,a), dot(b,b), dot(c,c)), 0.0);
    vec3 n = h*h*h*h * vec3(dot(a, hashFire(i)), dot(b, hashFire(i+o)), dot(c, hashFire(i+1.0)));
    return dot(n, vec3(70.0));
}

float fbmFire(vec2 uv)
{
    float f;
    mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
    f  = 0.5000 * noiseFire(uv); uv = m * uv;
    f += 0.2500 * noiseFire(uv); uv = m * uv;
    f += 0.1250 * noiseFire(uv); uv = m * uv;
    f += 0.0625 * noiseFire(uv);
    f = 0.5 + 0.5 * f;
    return f;
}

void main()
{
    float age = clamp(vParams.y, 0.0, 1.0);
    float life = 1.0 - age;
    float seedA = vParams.z;
    float seedB = vParams.w;
    float brightness = vExtra.x;
    float alphaScale = vExtra.y;
    float variant = vExtra.w;

    // --- Shock / impact ring mode (vExtra.z < 0) ---
    if (vExtra.z < 0.0)
    {
        vec2 suv = gl_PointCoord * 2.0 - 1.0;
        float radial = length(suv);
        float ringRadius = mix(0.08, 0.92, age);
        float ringWidth = mix(0.34, 0.09, age);
        float ring = 1.0 - smoothstep(ringRadius, ringRadius + ringWidth, radial);
        ring *= smoothstep(max(ringRadius - ringWidth * 0.85, 0.0), ringRadius, radial);
        float core = exp(-radial * radial * (18.0 + variant * 9.0));
        float halo = exp(-radial * (4.8 - variant * 1.2));
        float spokes = pow(max(0.0, 1.0 - abs(suv.x * suv.y) * (14.0 - variant * 4.0)), 5.0);
        float flash = max(core * 1.45, ring * 1.18 + halo * 0.34 + spokes * 0.16);
        float alpha = clamp(flash * alphaScale * life, 0.0, 1.0);
        if (alpha <= 0.01) discard;
        vec3 hotCore = vec3(1.0, 0.98, 0.92);
        vec3 warmEdge = vec3(1.0, 0.72, 0.36);
        vec3 color = mix(warmEdge, hotCore, clamp(core * 1.3 + spokes * 0.18, 0.0, 1.0));
        color += hotCore * ring * 0.42;
        color *= brightness;
        float dist = length(vWorldPos - uCameraPos);
        float fogSpan = max(uFogFar - uFogNear, 0.001);
        float fogFactor = clamp((uFogFar - dist) / fogSpan, 0.0, 1.0);
        FragColor = vec4(mix(uFogColor, color, fogFactor), alpha);
        return;
    }

    // --- IQ procedural fire flame ---
    // gl_PointCoord: y=0=top, y=1=bottom => flip so 0=base, 1=tip
    vec2 uv;
    uv.x = gl_PointCoord.x;
    uv.y = 1.0 - gl_PointCoord.y;

    // Map to IQ flame coordinate space (one sub-flame per sprite)
    vec2 q;
    q.x = uv.x - 0.5;   // center X: -0.5 .. 0.5
    q.y = uv.y * 2.0 - 0.25;  // -0.25 .. 1.75

    float strength = 2.0 + seedA * 2.0;   // per-sprite scale variety (2..4)
    float T3 = max(3.0, 1.25 * strength) * (uGlobalTime + seedB * 5.0);

    float n = fbmFire(strength * q - vec2(0.0, T3));
    float c = 1.0 - 16.0 * pow(
        max(0.0, length(q * vec2(1.8 + q.y * 1.5, 0.75)) - n * max(0.0, q.y + 0.25)),
        1.2);
    float c1 = n * c * (1.5 - pow(2.5 * uv.y, 4.0));
    c1 = clamp(c1, 0.0, 1.0);

    vec3 col = vec3(1.5*c1, 1.5*c1*c1*c1, c1*c1*c1*c1*c1*c1);
    float a = c * (1.0 - pow(max(uv.y, 0.0), 3.0));

    float alpha = clamp(a * alphaScale * life, 0.0, 1.0);
    if (alpha <= 0.01) discard;

    col *= brightness;
    float dist = length(vWorldPos - uCameraPos);
    float fogSpan = max(uFogFar - uFogNear, 0.001);
    float fogFactor = clamp((uFogFar - dist) / fogSpan, 0.0, 1.0);
    FragColor = vec4(mix(uFogColor, col, fogFactor), alpha);
}
)";

        const char *dashScreenVertSrc = R"(
#version 330 core
out vec2 vUV;
void main()
{
    vec2 pos;
    if (gl_VertexID == 0)
        pos = vec2(-1.0, -1.0);
    else if (gl_VertexID == 1)
        pos = vec2(3.0, -1.0);
    else
        pos = vec2(-1.0, 3.0);
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";

        const char *dashScreenFragSrc = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uGradientA;
uniform sampler2D uGradientB;
uniform vec2 uResolution;
uniform float uTime;
uniform float uIntensity;

#define S(a, b, t) smoothstep(a, b, t)
#define NUM_LAYERS 4.0

float N21(vec2 p)
{
    vec3 a = fract(vec3(p.xyx) * vec3(213.897, 653.453, 253.098));
    a += dot(a, a.yzx + 79.76);
    return fract((a.x + a.y) * a.z);
}

vec2 GetPos(vec2 id, vec2 offs, float t)
{
    float n = N21(id + offs);
    float n1 = fract(n * 10.0);
    float n2 = fract(n * 100.0);
    float a = t + n;
    return offs + vec2(sin(a * n1), cos(a * n2)) * 0.4;
}

float df_line(vec2 a, vec2 b, vec2 p)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 0.0001), 0.0, 1.0);
    return length(pa - ba * h);
}

float line(vec2 a, vec2 b, vec2 uv)
{
    float r1 = 0.04;
    float r2 = 0.01;
    float d = df_line(a, b, uv);
    float d2 = length(a - b);
    float fade = S(1.5, 0.5, d2);
    fade += S(0.05, 0.02, abs(d2 - 0.75));
    return S(r1, r2, d) * fade;
}

float NetLayer(vec2 st, float n, float t, out float sparkleMask)
{
    vec2 id = floor(st) + n;
    st = fract(st) - 0.5;

    vec2 p[9];
    int idx = 0;
    for (float y = -1.0; y <= 1.0; y += 1.0)
    {
        for (float x = -1.0; x <= 1.0; x += 1.0)
        {
            p[idx++] = GetPos(id, vec2(x, y), t);
        }
    }

    float m = 0.0;
    float sparkle = 0.0;
    for (int i = 0; i < 9; ++i)
    {
        m += line(p[4], p[i], st);
        float d = length(st - p[i]);
        float s = 0.005 / max(d * d, 0.0001);
        s *= S(1.0, 0.7, d);
        float pulse = sin((fract(p[i].x) + fract(p[i].y) + t) * 5.0) * 0.4 + 0.6;
        pulse = pow(pulse, 20.0);
        s *= pulse;
        sparkle += s;
    }

    m += line(p[1], p[3], st);
    m += line(p[1], p[5], st);
    m += line(p[7], p[5], st);
    m += line(p[7], p[3], st);

    float sPhase = (sin(t + n) + sin(t * 0.1)) * 0.25 + 0.5;
    sPhase += pow(sin(t * 0.1) * 0.5 + 0.5, 50.0) * 5.0;
    sparkleMask = sparkle * sPhase;
    return m + sparkleMask;
}

void main()
{
    vec2 fragCoord = vUV * uResolution;
    vec2 uv = (fragCoord - uResolution * 0.5) / uResolution.y;
    float t = uTime * 0.10;

    float s = sin(t);
    float c = cos(t);
    mat2 rot = mat2(c, -s, s, c);
    vec2 st = uv * rot;
    vec2 parallax = vec2(sin(t * 0.7), cos(t * 0.5)) * 0.18;
    parallax *= rot * 2.0;

    float m = 0.0;
    float sparkleAccum = 0.0;
    for (float i = 0.0; i < 1.0; i += 1.0 / NUM_LAYERS)
    {
        float z = fract(t + i);
        float size = mix(15.0, 1.0, z);
        float fade = S(0.0, 0.6, z) * S(1.0, 0.8, z);
        float sparkle = 0.0;
        float layer = NetLayer(st * size - parallax * z, i, uTime, sparkle);
        m += fade * layer;
        sparkleAccum += fade * sparkle;
    }

    float gradSeedA = fract(0.35 + t * 0.03 + sparkleAccum * 0.002);
    float gradSeedB = fract(0.72 - t * 0.02 + m * 0.0015);
    vec3 gradA = texture(uGradientA, vec2(gradSeedA, 0.5)).rgb;
    vec3 gradB = texture(uGradientB, vec2(gradSeedB, 0.5)).rgb;
    vec3 baseCol = mix(gradA, gradB, 0.5 + 0.5 * sin(t * 0.7));
    vec3 col = baseCol * m;
    col += mix(gradB, vec3(1.0), 0.28) * sparkleAccum * 0.22;
    col *= 1.0 - dot(uv, uv);

    float envelope = S(0.0, 0.25, uIntensity) * S(0.0, 0.85, 1.0 - (1.0 - uIntensity));
    float alpha = clamp((m * 0.22 + sparkleAccum * 0.05) * uIntensity, 0.0, 0.78);
    FragColor = vec4(col * uIntensity * envelope, alpha);
}
)";

            const char *fireScreenFragSrc = R"(
    #version 330 core
    in vec2 vUV;
    out vec4 FragColor;

    uniform vec2 uResolution;
    uniform float uTime;
    uniform float uIntensity;

    float hash31(vec3 p)
    {
        p = fract(p * 0.1031);
        p += dot(p, p.yzx + 33.33);
        return fract((p.x + p.y) * p.z);
    }

    float noise3(vec3 p)
    {
        vec3 i = floor(p);
        vec3 f = fract(p);
        f = f * f * (3.0 - 2.0 * f);

        float n000 = hash31(i + vec3(0.0, 0.0, 0.0));
        float n100 = hash31(i + vec3(1.0, 0.0, 0.0));
        float n010 = hash31(i + vec3(0.0, 1.0, 0.0));
        float n110 = hash31(i + vec3(1.0, 1.0, 0.0));
        float n001 = hash31(i + vec3(0.0, 0.0, 1.0));
        float n101 = hash31(i + vec3(1.0, 0.0, 1.0));
        float n011 = hash31(i + vec3(0.0, 1.0, 1.0));
        float n111 = hash31(i + vec3(1.0, 1.0, 1.0));

        float nx00 = mix(n000, n100, f.x);
        float nx10 = mix(n010, n110, f.x);
        float nx01 = mix(n001, n101, f.x);
        float nx11 = mix(n011, n111, f.x);
        float nxy0 = mix(nx00, nx10, f.y);
        float nxy1 = mix(nx01, nx11, f.y);
        return mix(nxy0, nxy1, f.z);
    }

    float fbm(vec3 p)
    {
        float value = 0.0;
        float amplitude = 0.55;
        for (int i = 0; i < 5; ++i)
        {
            value += noise3(p) * amplitude;
            p = p * 2.03 + vec3(1.7, -1.3, 0.9);
            amplitude *= 0.5;
        }
        return value;
    }

    void main()
    {
        vec2 fragCoord = vUV * uResolution;
        vec2 centered = (fragCoord - uResolution * vec2(0.5, 0.0)) / uResolution.y;
        vec2 flameUv = centered;
        flameUv.x *= 1.45;
        flameUv.y *= 1.75;

        float t = uTime * 0.85;
        vec3 flow = vec3(flameUv.x * 2.1, flameUv.y * 3.6 - t * 1.45, t * 0.24);
        float baseNoise = fbm(flow);
        float detailNoise = fbm(flow * vec3(1.9, 1.45, 1.2) + vec3(2.4, -0.8, 1.7));
        float curlNoise = noise3(vec3(flameUv.x * 5.2 + detailNoise * 0.8, flameUv.y * 6.3 - t * 2.1, t * 0.45));

        float body = 1.0 - smoothstep(0.18 + flameUv.y * 0.55, 1.1 + flameUv.y * 0.42, abs(flameUv.x));
        body *= 1.0 - smoothstep(0.95, 1.65, flameUv.y);
        body = max(body, 0.0);

        float flame = baseNoise * 1.35 + detailNoise * 0.75 + curlNoise * 0.25;
        flame -= flameUv.y * 1.1;
        flame = smoothstep(0.18, 1.08, flame) * body;

        float core = smoothstep(0.32, 1.05, baseNoise + 0.28 - flameUv.y * 1.55);
        core *= body * (1.0 - smoothstep(0.4, 1.45, abs(flameUv.x)));

        float smoke = smoothstep(0.35, 0.95, detailNoise + curlNoise * 0.35 - flameUv.y * 0.82);
        smoke *= body * 0.45;

        vec3 deep = vec3(0.24, 0.01, 0.00);
        vec3 hot = vec3(0.96, 0.18, 0.03);
        vec3 bright = vec3(1.00, 0.58, 0.08);
        vec3 whiteHot = vec3(1.00, 0.90, 0.62);

        vec3 col = mix(deep, hot, clamp(flame * 1.2, 0.0, 1.0));
        col = mix(col, bright, clamp(flame * 0.85 + core * 0.5, 0.0, 1.0));
        col = mix(col, whiteHot, clamp(core * core * 1.15, 0.0, 1.0));
        col += vec3(0.22, 0.08, 0.04) * smoke;

        float embers = pow(max(0.0, 1.0 - length(vec2(flameUv.x * 0.9, flameUv.y * 0.55 - 0.18))), 5.0);
        embers *= noise3(vec3(fragCoord * 0.032, t * 1.7));
        col += vec3(1.0, 0.34, 0.05) * embers * 0.16;

        float alpha = clamp((flame * 0.78 + core * 0.26 + smoke * 0.12) * uIntensity, 0.0, 0.86);
        FragColor = vec4(col * uIntensity, alpha);
    }
    )";

        unsigned int dashScreenVs = compileShader(GL_VERTEX_SHADER, dashScreenVertSrc);
        unsigned int dashScreenFs = compileShader(GL_FRAGMENT_SHADER, dashScreenFragSrc);
        m_dashScreenShader = glCreateProgram();
        glAttachShader(m_dashScreenShader, dashScreenVs);
        glAttachShader(m_dashScreenShader, dashScreenFs);
        glLinkProgram(m_dashScreenShader);
        glDeleteShader(dashScreenVs);
        glDeleteShader(dashScreenFs);

        unsigned int fireScreenVs = compileShader(GL_VERTEX_SHADER, dashScreenVertSrc);
        unsigned int fireScreenFs = compileShader(GL_FRAGMENT_SHADER, fireScreenFragSrc);
        m_fireScreenShader = glCreateProgram();
        glAttachShader(m_fireScreenShader, fireScreenVs);
        glAttachShader(m_fireScreenShader, fireScreenFs);
        glLinkProgram(m_fireScreenShader);
        glDeleteShader(fireScreenVs);
        glDeleteShader(fireScreenFs);

        unsigned int fireFieldVs = compileShader(GL_VERTEX_SHADER, dashStarVertSrc);
        unsigned int fireFieldFs = compileShader(GL_FRAGMENT_SHADER, fireFieldFragSrc);
        m_fireFieldShader = glCreateProgram();
        glAttachShader(m_fireFieldShader, fireFieldVs);
        glAttachShader(m_fireFieldShader, fireFieldFs);
        glLinkProgram(m_fireFieldShader);
        glDeleteShader(fireFieldVs);
        glDeleteShader(fireFieldFs);

        glGenVertexArrays(1, &m_dashScreenVao);
        glBindVertexArray(m_dashScreenVao);
        glBindVertexArray(0);

        glGenVertexArrays(1, &m_dashStarVao);
        glGenBuffers(1, &m_dashStarVbo);
        glBindVertexArray(m_dashStarVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_dashStarVbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DashStarVertex), (void*)offsetof(DashStarVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DashStarVertex), (void*)offsetof(DashStarVertex, params));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(DashStarVertex), (void*)offsetof(DashStarVertex, extra));
        glBindVertexArray(0);

        auto loadGradientTexture = [](const std::string &path) -> unsigned int
        {
            auto createFallbackGradient = []() -> unsigned int
            {
                const unsigned char pixels[] = {
                    255, 225, 140, 255,
                    120, 214, 255, 255
                };
                unsigned int texture = 0;
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                glBindTexture(GL_TEXTURE_2D, 0);
                return texture;
            };

            std::ifstream file(path);
            if (!file.is_open())
                return createFallbackGradient();

            std::string magic;
            file >> magic;
            if (magic != "P3")
                return createFallbackGradient();

            int width = 0;
            int height = 0;
            int maxValue = 0;
            file >> width >> height >> maxValue;
            if (!file || width <= 0 || height <= 0 || maxValue <= 0)
                return createFallbackGradient();

            std::vector<unsigned char> pixels;
            pixels.reserve(static_cast<size_t>(width * height * 4));
            for (int i = 0; i < width * height; ++i)
            {
                int r = 0;
                int g = 0;
                int b = 0;
                file >> r >> g >> b;
                if (!file)
                    return createFallbackGradient();
                pixels.push_back(static_cast<unsigned char>(std::clamp(r * 255 / maxValue, 0, 255)));
                pixels.push_back(static_cast<unsigned char>(std::clamp(g * 255 / maxValue, 0, 255)));
                pixels.push_back(static_cast<unsigned char>(std::clamp(b * 255 / maxValue, 0, 255)));
                pixels.push_back(255);
            }

            unsigned int texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            glBindTexture(GL_TEXTURE_2D, 0);
            return texture;
        };

        m_dashGradientATexture = loadGradientTexture("assets/textures/UI/dash_gradient_a.ppm");
        m_dashGradientBTexture = loadGradientTexture("assets/textures/UI/dash_gradient_b.ppm");

        initHD2DResources();
    }

    // ─── HD-2D Resources (sprite billboard + post-processing shaders/buffers) ──

    void VoxelScene::initHD2DResources()
    {
        // ── Billboard sprite vertex shader (cylindrical billboard, world-up Y) ──
        const char *spriteVertSrc = R"(
#version 330 core
layout(location=0) in vec2 aCorner;
uniform mat4 uProj;
uniform mat4 uView;
uniform vec3 uFeetPos;
uniform float uHalfW;
uniform float uHalfH;
out vec2 vUV;
void main()
{
    // Camera right without Y tilt (cylindrical billboard)
    vec3 camRight = normalize(vec3(uView[0][0], 0.0, uView[2][0]));
    vec3 worldUp   = vec3(0.0, 1.0, 0.0);
    // Center of quad is at feet + halfH
    vec3 center   = uFeetPos + worldUp * uHalfH;
    vec3 worldPos = center
                  + camRight * aCorner.x * uHalfW
                  + worldUp  * aCorner.y * uHalfH;
    gl_Position = uProj * uView * vec4(worldPos, 1.0);
    // UV: (0,0)=bottom-left  (1,1)=top-right
    vUV = vec2(aCorner.x + 0.5, aCorner.y + 0.5);
}
)";

        // ── Billboard sprite fragment shader (player_sheet.png 8x8 atlas) ──
        const char *spriteFragSrc = R"(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uSpriteTex;
uniform vec2  uUVOffset;
uniform float uUVScale;
uniform vec3  uFeetPos;
uniform vec3  uCameraPos;
uniform vec3  uFogColor;
uniform float uFogNear;
uniform float uFogFar;
uniform float uAmbientStrength;
uniform float uDiffuseStrength;
uniform float uFlash;

void main()
{
    vec2 texUV = uUVOffset + vUV * uUVScale;
    vec4 pix   = texture(uSpriteTex, texUV);
    if (pix.a < 0.1) discard;

    float litFactor = clamp(uAmbientStrength + uDiffuseStrength * 0.55, 0.2, 2.0);
    float gradY = 0.72 + 0.28 * vUV.y;
    float rimX  = 0.80 + 0.20 * vUV.x;
    vec3  col   = pix.rgb * litFactor * gradY * rimX;

    col += uFlash * 0.28;

    float dist      = distance(uFeetPos, uCameraPos);
    float fogSpan   = max(uFogFar - uFogNear, 0.001);
    float fogFactor = clamp((uFogFar - dist) / fogSpan, 0.0, 1.0);
    col = mix(uFogColor, col, fogFactor);

    fragColor = vec4(col, pix.a);
}
)";

        // ── Fullscreen pass vertex shader (3-vertex triangle, no VBO) ──
        const char *fsQuadVert = R"(
#version 330 core
out vec2 vUV;
void main()
{
    vec2 pos = vec2((gl_VertexID & 1) * 4.0 - 1.0,
                    (gl_VertexID >> 1) * 4.0 - 1.0);
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";

        // ── Bloom extract (threshold bright regions) ──
        const char *bloomExtractFrag = R"(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uScene;
uniform float uThreshold;
void main()
{
    vec3 c = texture(uScene, vUV).rgb;
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float strength = max(0.0, (lum - uThreshold) / max(1.0 - uThreshold, 0.001));
    fragColor = vec4(c * strength, 1.0);
}
)";

        // ── Bloom separable Gaussian blur ──
        const char *bloomBlurFrag = R"(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTex;
uniform vec2 uDir;       // (texelW,0) or (0,texelH)
void main()
{
    const float w[5] = float[](0.22702703, 0.19459459, 0.12162162, 0.05405405, 0.01621622);
    vec3 res = texture(uTex, vUV).rgb * w[0];
    for (int i = 1; i < 5; ++i)
    {
        res += texture(uTex, vUV + uDir * float(i)).rgb * w[i];
        res += texture(uTex, vUV - uDir * float(i)).rgb * w[i];
    }
    fragColor = vec4(res, 1.0);
}
)";

        // ── HD-2D composite: scene + bloom + tilt-shift DOF ──
        const char *hd2dCompositeFrag = R"(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform vec2 uTexelSize;
uniform float uBloomStrength;
uniform float uFocusY;      // screen-space Y of focus band center (0..1)
uniform float uTiltShift;   // tilt-shift blur intensity

void main()
{
    // Tilt-shift: horizontal bands above/below focus are blurred
    float dy        = abs(vUV.y - uFocusY);
    float blurAmt   = smoothstep(0.0, 0.40, dy) * uTiltShift;

    vec3 scene;
    if (blurAmt > 0.0005)
    {
        scene = vec3(0.0);
        // 7-tap vertical box blur proportional to blur amount
        const int TAPS = 7;
        for (int i = -TAPS/2; i <= TAPS/2; ++i)
        {
            vec2 off = vec2(0.0, float(i) * blurAmt * uTexelSize.y * 16.0);
            scene += texture(uScene, vUV + off).rgb;
        }
        scene /= float(TAPS);
    }
    else
    {
        scene = texture(uScene, vUV).rgb;
    }

    // Bloom add
    vec3 bloom = texture(uBloom, vUV).rgb;
    vec3 col   = scene + bloom * uBloomStrength;

    // Reinhard tone-map + gamma
    col = col / (col + vec3(1.0));
    col = pow(col, vec3(1.0 / 2.2));

    fragColor = vec4(col, 1.0);
}
)";

        auto compile = [&](GLenum type, const char *src) -> unsigned int {
            unsigned int s = glCreateShader(type);
            glShaderSource(s, 1, &src, nullptr);
            glCompileShader(s);
            return s;
        };
        auto link2 = [&](const char *v, const char *f) -> unsigned int {
            unsigned int vs = compile(GL_VERTEX_SHADER, v);
            unsigned int fs = compile(GL_FRAGMENT_SHADER, f);
            unsigned int p  = glCreateProgram();
            glAttachShader(p, vs); glAttachShader(p, fs);
            glLinkProgram(p);
            glDeleteShader(vs); glDeleteShader(fs);
            return p;
        };

        m_spriteShader        = link2(spriteVertSrc,    spriteFragSrc);
        m_bloomExtractShader  = link2(fsQuadVert,       bloomExtractFrag);
        m_bloomBlurShader     = link2(fsQuadVert,       bloomBlurFrag);
        m_hd2dCompositeShader = link2(fsQuadVert,       hd2dCompositeFrag);

        // Sprite quad: 6 vertices (2 triangles), each vertex = vec2(corner)
        static const float quadCorners[] = {
            -0.5f,-0.5f,  0.5f,-0.5f,  0.5f, 0.5f,
            -0.5f,-0.5f,  0.5f, 0.5f, -0.5f, 0.5f
        };
        glGenVertexArrays(1, &m_spriteQuadVao);
        glGenBuffers(1, &m_spriteQuadVbo);
        glBindVertexArray(m_spriteQuadVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_spriteQuadVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadCorners), quadCorners, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glBindVertexArray(0);

        // Load 8-direction player sprite sheet (256x256, 8 rows x 8 frames at 32x32px)
        {
            SDL_Surface *surf = IMG_Load("assets/textures/Characters/player_sheet.png");
            if (surf)
            {
                SDL_Surface *rgba = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
                SDL_DestroySurface(surf);
                if (rgba)
                {
                    glGenTextures(1, &m_playerSpriteTex);
                    glBindTexture(GL_TEXTURE_2D, m_playerSpriteTex);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0,
                                 GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    SDL_DestroySurface(rgba);
                }
            }
        }

        // Empty VAO for gl_VertexID fullscreen triangle
        glGenVertexArrays(1, &m_fullQuadVao);

        // Load FBO function pointers (OpenGL 3.0+, not in system GL headers on macOS)
        m_glGenFramebuffers      = reinterpret_cast<GenFramebuffersProc>(SDL_GL_GetProcAddress("glGenFramebuffers"));
        m_glBindFramebuffer      = reinterpret_cast<BindFramebufferProc>(SDL_GL_GetProcAddress("glBindFramebuffer"));
        m_glFramebufferTexture2D = reinterpret_cast<FramebufferTex2DProc>(SDL_GL_GetProcAddress("glFramebufferTexture2D"));
        m_glFramebufferRenderbuffer = reinterpret_cast<FramebufferRboProc>(SDL_GL_GetProcAddress("glFramebufferRenderbuffer"));
        m_glDeleteFramebuffers   = reinterpret_cast<DelFramebuffersProc>(SDL_GL_GetProcAddress("glDeleteFramebuffers"));
        m_glGenRenderbuffers     = reinterpret_cast<GenRenderbuffersProc>(SDL_GL_GetProcAddress("glGenRenderbuffers"));
        m_glBindRenderbuffer     = reinterpret_cast<BindRenderbufferProc>(SDL_GL_GetProcAddress("glBindRenderbuffer"));
        m_glRenderbufferStorage  = reinterpret_cast<RboStorageProc>(SDL_GL_GetProcAddress("glRenderbufferStorage"));
        m_glDeleteRenderbuffers  = reinterpret_cast<DelRenderbuffersProc>(SDL_GL_GetProcAddress("glDeleteRenderbuffers"));
    }

    void VoxelScene::resizeHD2DFBOs(int w, int h)
    {
        if (w == m_postFboW && h == m_postFboH) return;
        m_postFboW = w; m_postFboH = h;

        // ── Clean old resources ──
        if (m_hd2dFBO     && m_glDeleteFramebuffers)  { m_glDeleteFramebuffers(1,  &m_hd2dFBO);   m_hd2dFBO = 0; }
        if (m_hd2dColorTex)                           { glDeleteTextures(1, &m_hd2dColorTex);     m_hd2dColorTex = 0; }
        if (m_hd2dDepthRbo && m_glDeleteRenderbuffers){ m_glDeleteRenderbuffers(1, &m_hd2dDepthRbo); m_hd2dDepthRbo = 0; }
        if (m_bloomFBO[0]  && m_glDeleteFramebuffers) { m_glDeleteFramebuffers(2,  m_bloomFBO);   m_bloomFBO[0]=m_bloomFBO[1]=0; }
        if (m_bloomTex[0])                            { glDeleteTextures(2, m_bloomTex);           m_bloomTex[0]=m_bloomTex[1]=0; }

        // Early-out if FBO functions are not available
        if (!m_glGenFramebuffers || !m_glBindFramebuffer || !m_glGenRenderbuffers) return;

        // ── Main scene FBO (HDR color + depth renderbuffer) ──
        glGenTextures(1, &m_hd2dColorTex);
        glBindTexture(GL_TEXTURE_2D, m_hd2dColorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        m_glGenRenderbuffers(1, &m_hd2dDepthRbo);
        m_glBindRenderbuffer(GL_RENDERBUFFER, m_hd2dDepthRbo);
        m_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

        m_glGenFramebuffers(1, &m_hd2dFBO);
        m_glBindFramebuffer(GL_FRAMEBUFFER, m_hd2dFBO);
        m_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_hd2dColorTex, 0);
        m_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_hd2dDepthRbo);
        m_glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ── Bloom ping-pong FBOs (half resolution for performance) ──
        int bw = std::max(1, w / 2), bh = std::max(1, h / 2);
        glGenTextures(2, m_bloomTex);
        m_glGenFramebuffers(2, m_bloomFBO);
        for (int i = 0; i < 2; ++i)
        {
            glBindTexture(GL_TEXTURE_2D, m_bloomTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, bw, bh, 0, GL_RGB, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            m_glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[i]);
            m_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomTex[i], 0);
        }
        m_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void VoxelScene::renderPlayerSprite(
        const glm::mat4 &proj, const glm::mat4 &view,
        const glm::vec3 &fogColor, float fogNear, float fogFar,
        float ambientStr, float diffuseStr,
        const glm::vec3 &lightDir, float flash)
    {
        if (!m_spriteShader || !m_spriteQuadVao) return;

        // Feet position: same origin used in rebuildPlayerMesh
        glm::vec3 feetPos = m_cameraPos + glm::vec3(0.0f, -0.95f, 0.0f);
        // Sprite dimensions covering the whole character
        const float halfH = 1.10f;  // total height = 2.2 world units
        const float halfW = 0.73f;  // total width  = 1.46 (preserves 16:24 aspect ratio)

        glUseProgram(m_spriteShader);

        // Bind sprite sheet and set atlas UV for current direction/frame
        constexpr float kUVAtlasScale = 1.0f / 8.0f;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_playerSpriteTex);
        glUniform1i(glGetUniformLocation(m_spriteShader, "uSpriteTex"), 0);
        if (m_glUniform2f)
            m_glUniform2f(glGetUniformLocation(m_spriteShader, "uUVOffset"),
                          m_playerSpriteFrame * kUVAtlasScale,
                          (7 - m_playerSpriteRow) * kUVAtlasScale);

        glUniformMatrix4fv(glGetUniformLocation(m_spriteShader,"uProj"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_spriteShader,"uView"), 1, GL_FALSE, glm::value_ptr(view));
        if (m_glUniform3fv)
        {
            m_glUniform3fv(glGetUniformLocation(m_spriteShader,"uFeetPos"),    1, glm::value_ptr(feetPos));
            m_glUniform3fv(glGetUniformLocation(m_spriteShader,"uCameraPos"), 1, glm::value_ptr(m_cameraPos));
            m_glUniform3fv(glGetUniformLocation(m_spriteShader,"uFogColor"),  1, glm::value_ptr(fogColor));
            m_glUniform3fv(glGetUniformLocation(m_spriteShader,"uLightDir"),  1, glm::value_ptr(lightDir));
        }
        if (m_glUniform1f)
        {
            m_glUniform1f(glGetUniformLocation(m_spriteShader,"uHalfW"),           halfW);
            m_glUniform1f(glGetUniformLocation(m_spriteShader,"uHalfH"),           halfH);
            m_glUniform1f(glGetUniformLocation(m_spriteShader,"uFogNear"),         fogNear);
            m_glUniform1f(glGetUniformLocation(m_spriteShader,"uFogFar"),          fogFar);
            m_glUniform1f(glGetUniformLocation(m_spriteShader,"uAmbientStrength"), ambientStr);
            m_glUniform1f(glGetUniformLocation(m_spriteShader,"uDiffuseStrength"), diffuseStr);
            m_glUniform1f(glGetUniformLocation(m_spriteShader,"uFlash"),           flash);
            m_glUniform1f(glGetUniformLocation(m_spriteShader,"uUVScale"),         kUVAtlasScale);
        }

        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glBindVertexArray(m_spriteQuadVao);
        if (m_glDrawArrays) m_glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);
    }

    void VoxelScene::renderHD2DPostProcess(int w, int h)
    {
        if (!m_hd2dFBO || !m_bloomExtractShader || !m_fullQuadVao || !m_glBindFramebuffer) return;

        int bw = std::max(1, w / 2), bh = std::max(1, h / 2);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);

        // ── 1. Extract bright regions into bloomFBO[0] ──────────────────────
        glViewport(0, 0, bw, bh);
        m_glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[0]);
        glUseProgram(m_bloomExtractShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_hd2dColorTex);
        glUniform1i(glGetUniformLocation(m_bloomExtractShader,"uScene"), 0);
        if (m_glUniform1f) m_glUniform1f(glGetUniformLocation(m_bloomExtractShader,"uThreshold"), 0.65f);
        glBindVertexArray(m_fullQuadVao);
        if (m_glDrawArrays) m_glDrawArrays(GL_TRIANGLES, 0, 3);

        // ── 2. Blur ping-pong (2 full H+V iterations) ──────────────────────
        for (int iter = 0; iter < 2; ++iter)
        {
            // Horizontal blur: FBO[1] ← blur(FBO[0])
            m_glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[1]);
            glUseProgram(m_bloomBlurShader);
            glBindTexture(GL_TEXTURE_2D, m_bloomTex[0]);
            glUniform1i(glGetUniformLocation(m_bloomBlurShader,"uTex"), 0);
            if (m_glUniform2f) m_glUniform2f(glGetUniformLocation(m_bloomBlurShader,"uDir"),
                        1.0f / static_cast<float>(bw), 0.0f);
            if (m_glDrawArrays) m_glDrawArrays(GL_TRIANGLES, 0, 3);

            // Vertical blur: FBO[0] ← blur(FBO[1])
            m_glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[0]);
            glBindTexture(GL_TEXTURE_2D, m_bloomTex[1]);
            if (m_glUniform2f) m_glUniform2f(glGetUniformLocation(m_bloomBlurShader,"uDir"),
                        0.0f, 1.0f / static_cast<float>(bh));
            if (m_glDrawArrays) m_glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        glBindVertexArray(0);

        // ── 3. Composite → default framebuffer (full resolution) ───────────
        m_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
        glUseProgram(m_hd2dCompositeShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_hd2dColorTex);
        glUniform1i(glGetUniformLocation(m_hd2dCompositeShader,"uScene"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_bloomTex[0]);
        glUniform1i(glGetUniformLocation(m_hd2dCompositeShader,"uBloom"), 1);
        if (m_glUniform2f) m_glUniform2f(glGetUniformLocation(m_hd2dCompositeShader,"uTexelSize"),
                    1.0f/static_cast<float>(w), 1.0f/static_cast<float>(h));
        if (m_glUniform1f)
        {
            m_glUniform1f(glGetUniformLocation(m_hd2dCompositeShader,"uBloomStrength"), 0.45f);
            m_glUniform1f(glGetUniformLocation(m_hd2dCompositeShader,"uFocusY"),        0.42f);
            m_glUniform1f(glGetUniformLocation(m_hd2dCompositeShader,"uTiltShift"),     0.18f);
        }
        glBindVertexArray(m_fullQuadVao);
        if (m_glDrawArrays) m_glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);

        glEnable(GL_DEPTH_TEST);
        // Note: blend state is managed by individual screen-effect functions
    }

    void VoxelScene::initModelResources()
    {
        const std::string texturePath = "assets/models/OBJ format/Textures/colormap.png";
        loadStaticModelMesh("wheelchair", "assets/models/OBJ format/wheelchair.obj", texturePath);
        loadStaticModelMesh("defibrillator", "assets/models/OBJ format/aid-defibrillator-red.obj", texturePath);
        loadStaticModelMesh("crutch", "assets/models/OBJ format/aid-crutch.obj", texturePath);
        loadStaticModelMesh("character", "assets/models/OBJ format/character-male-a.obj", texturePath);
        loadStaticModelMesh("character_female", "assets/models/OBJ format/character-female-c.obj", texturePath);
        loadStaticModelMesh("character_male_b", "assets/models/OBJ format/character-male-d.obj", texturePath);
        loadStaticModelMesh("wheelchair_power", "assets/models/OBJ format/wheelchair-power.obj", texturePath);
        loadStaticModelMesh("wheelchair_deluxe", "assets/models/OBJ format/wheelchair-deluxe.obj", texturePath);
        loadStaticModelMesh("hearing_aid", "assets/models/OBJ format/aid_hearing.obj", texturePath);
        loadStaticModelMesh("glasses", "assets/models/OBJ format/aid-glasses.obj", texturePath);
        loadStaticModelMeshGLB("character_glb", "assets/models/GLB format/character-female-a.glb");
        loadStaticModelMeshGLB("wheelchair_glb", "assets/models/GLB format/wheelchair-power-deluxe.glb");
        loadStaticModelMeshGLB("mask_glb", "assets/models/GLB format/aid-mask.glb");
    }

    unsigned int VoxelScene::loadModelTexture(const std::string &path)
    {
        auto it = m_modelTextures.find(path);
        if (it != m_modelTextures.end())
            return it->second;

        auto createFallbackTexture = [&]() -> unsigned int
        {
            unsigned int texture = 0;
            const unsigned char whitePixel[4] = {255, 255, 255, 255};
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
            glBindTexture(GL_TEXTURE_2D, 0);
            return texture;
        };

        SDL_Surface *surface = IMG_Load(path.c_str());
        if (!surface)
        {
            unsigned int fallbackTexture = createFallbackTexture();
            m_modelTextures[path] = fallbackTexture;
            return fallbackTexture;
        }

        SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surface);
        if (!converted)
        {
            unsigned int fallbackTexture = createFallbackTexture();
            m_modelTextures[path] = fallbackTexture;
            return fallbackTexture;
        }

        unsigned int texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
        SDL_DestroySurface(converted);

        m_modelTextures[path] = texture;
        return texture;
    }

    bool VoxelScene::loadStaticModelMesh(const std::string &name, const std::string &objPath, const std::string &texturePath)
    {
        std::ifstream file(objPath);
        if (!file.is_open())
            return false;

        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        std::vector<ModelVertex> vertices;
        std::string line;

        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#')
                continue;

            std::istringstream stream(line);
            std::string type;
            stream >> type;
            if (type == "v")
            {
                glm::vec3 position{0.0f};
                stream >> position.x >> position.y >> position.z;
                positions.push_back(position);
            }
            else if (type == "vn")
            {
                glm::vec3 normal{0.0f};
                stream >> normal.x >> normal.y >> normal.z;
                normals.push_back(glm::normalize(normal));
            }
            else if (type == "vt")
            {
                glm::vec2 uv{0.0f};
                stream >> uv.x >> uv.y;
                uvs.push_back({uv.x, 1.0f - uv.y});
            }
            else if (type == "f")
            {
                std::vector<ObjIndex> face;
                std::string token;
                while (stream >> token)
                    face.push_back(parseObjIndex(token));
                if (face.size() < 3)
                    continue;

                for (size_t i = 1; i + 1 < face.size(); ++i)
                {
                    ObjIndex tri[3] = {face[0], face[i], face[i + 1]};
                    glm::vec3 fallbackNormal{0.0f, 1.0f, 0.0f};
                    if (tri[0].position > 0 && tri[1].position > 0 && tri[2].position > 0)
                    {
                        const glm::vec3 &a = positions[static_cast<size_t>(tri[0].position - 1)];
                        const glm::vec3 &b = positions[static_cast<size_t>(tri[1].position - 1)];
                        const glm::vec3 &c = positions[static_cast<size_t>(tri[2].position - 1)];
                        glm::vec3 n = glm::cross(b - a, c - a);
                        if (glm::length(n) > 0.0001f)
                            fallbackNormal = glm::normalize(n);
                    }

                    for (const ObjIndex &idx : tri)
                    {
                        ModelVertex vertex{};
                        if (idx.position > 0)
                            vertex.pos = positions[static_cast<size_t>(idx.position - 1)];
                        vertex.normal = idx.normal > 0 ? normals[static_cast<size_t>(idx.normal - 1)] : fallbackNormal;
                        vertex.uv = idx.uv > 0 ? uvs[static_cast<size_t>(idx.uv - 1)] : glm::vec2(0.0f);
                        vertices.push_back(vertex);
                    }
                }
            }
        }

        if (vertices.empty())
            return false;

        StaticModelMesh mesh;
        mesh.name = name;
        mesh.texture = loadModelTexture(texturePath);
        mesh.vertexCount = static_cast<int>(vertices.size());
        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(ModelVertex), vertices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, uv));
        glBindVertexArray(0);
        m_staticModelLibrary.push_back(mesh);
        return true;
    }

    bool VoxelScene::loadStaticModelMeshGLB(const std::string &name, const std::string &glbPath)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string warn;
        std::string err;
        if (!loader.LoadBinaryFromFile(&model, &err, &warn, glbPath))
            return false;

        const tinygltf::Mesh *meshRef = nullptr;
        if (!model.meshes.empty())
            meshRef = &model.meshes.front();
        if (!meshRef || meshRef->primitives.empty())
            return false;

        const tinygltf::Primitive &primitive = meshRef->primitives.front();
        auto posIt = primitive.attributes.find("POSITION");
        if (posIt == primitive.attributes.end())
            return false;

        auto readVec3 = [&](int accessorIndex) -> std::vector<glm::vec3>
        {
            std::vector<glm::vec3> out;
            const auto &accessor = model.accessors[static_cast<size_t>(accessorIndex)];
            const auto &view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
            const auto &buffer = model.buffers[static_cast<size_t>(view.buffer)];
            const unsigned char *data = buffer.data.data() + view.byteOffset + accessor.byteOffset;
            const size_t stride = accessor.ByteStride(view) > 0 ? static_cast<size_t>(accessor.ByteStride(view)) : sizeof(float) * 3;
            out.resize(accessor.count);
            for (size_t i = 0; i < accessor.count; ++i)
            {
                const float *src = reinterpret_cast<const float *>(data + i * stride);
                out[i] = {src[0], src[1], src[2]};
            }
            return out;
        };

        auto readVec2 = [&](int accessorIndex) -> std::vector<glm::vec2>
        {
            std::vector<glm::vec2> out;
            const auto &accessor = model.accessors[static_cast<size_t>(accessorIndex)];
            const auto &view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
            const auto &buffer = model.buffers[static_cast<size_t>(view.buffer)];
            const unsigned char *data = buffer.data.data() + view.byteOffset + accessor.byteOffset;
            const size_t stride = accessor.ByteStride(view) > 0 ? static_cast<size_t>(accessor.ByteStride(view)) : sizeof(float) * 2;
            out.resize(accessor.count);
            for (size_t i = 0; i < accessor.count; ++i)
            {
                const float *src = reinterpret_cast<const float *>(data + i * stride);
                out[i] = {src[0], 1.0f - src[1]};
            }
            return out;
        };

        std::vector<glm::vec3> positions = readVec3(posIt->second);
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        if (auto normalIt = primitive.attributes.find("NORMAL"); normalIt != primitive.attributes.end())
            normals = readVec3(normalIt->second);
        if (auto uvIt = primitive.attributes.find("TEXCOORD_0"); uvIt != primitive.attributes.end())
            uvs = readVec2(uvIt->second);

        std::vector<uint32_t> indices;
        if (primitive.indices >= 0)
        {
            const auto &accessor = model.accessors[static_cast<size_t>(primitive.indices)];
            const auto &view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
            const auto &buffer = model.buffers[static_cast<size_t>(view.buffer)];
            const unsigned char *data = buffer.data.data() + view.byteOffset + accessor.byteOffset;
            indices.resize(accessor.count);
            for (size_t i = 0; i < accessor.count; ++i)
            {
                switch (accessor.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    indices[i] = reinterpret_cast<const uint16_t *>(data)[i];
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    indices[i] = reinterpret_cast<const uint32_t *>(data)[i];
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    indices[i] = reinterpret_cast<const uint8_t *>(data)[i];
                    break;
                default:
                    indices[i] = 0;
                    break;
                }
            }
        }
        else
        {
            indices.resize(positions.size());
            for (size_t i = 0; i < positions.size(); ++i)
                indices[i] = static_cast<uint32_t>(i);
        }

        std::vector<ModelVertex> vertices;
        vertices.reserve(indices.size());
        for (uint32_t index : indices)
        {
            if (index >= positions.size())
                continue;
            ModelVertex vertex{};
            vertex.pos = positions[index];
            vertex.normal = index < normals.size() ? normals[index] : glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.uv = index < uvs.size() ? uvs[index] : glm::vec2(0.0f);
            vertices.push_back(vertex);
        }
        if (vertices.empty())
            return false;

        unsigned int texture = 0;
        if (primitive.material >= 0)
        {
            const auto &material = model.materials[static_cast<size_t>(primitive.material)];
            int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            if (textureIndex >= 0 && textureIndex < static_cast<int>(model.textures.size()))
            {
                int imageIndex = model.textures[static_cast<size_t>(textureIndex)].source;
                if (imageIndex >= 0 && imageIndex < static_cast<int>(model.images.size()))
                {
                    const auto &image = model.images[static_cast<size_t>(imageIndex)];
                    if (!image.image.empty() && image.width > 0 && image.height > 0)
                    {
                        glGenTextures(1, &texture);
                        glBindTexture(GL_TEXTURE_2D, texture);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                        GLenum format = image.component == 4 ? GL_RGBA : (image.component == 3 ? GL_RGB : GL_RGBA);
                        glTexImage2D(GL_TEXTURE_2D, 0, format, image.width, image.height, 0, format,
                                     GL_UNSIGNED_BYTE, image.image.data());
                        glBindTexture(GL_TEXTURE_2D, 0);
                    }
                }
            }
        }
        if (texture == 0)
            texture = loadModelTexture(glbPath + "#fallback");

        StaticModelMesh mesh;
        mesh.name = name;
        mesh.texture = texture;
        mesh.vertexCount = static_cast<int>(vertices.size());
        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(ModelVertex), vertices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, uv));
        glBindVertexArray(0);
        m_staticModelLibrary.push_back(mesh);
        return true;
    }

    void VoxelScene::releaseStaticModelMesh(StaticModelMesh &mesh)
    {
        if (mesh.vbo)
        {
            glDeleteBuffers(1, &mesh.vbo);
            mesh.vbo = 0;
        }
        if (mesh.vao)
        {
            glDeleteVertexArrays(1, &mesh.vao);
            mesh.vao = 0;
        }
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

        // ── 八方旅人风格：平面六面体网格（取消平滑插值，每个体素块逐面渲染）──
        // 只输出非空体素的可见（邻接为空）面，使用法线和 AO 阴影着色
        // 面朝向及其法线: +Z, -Z, -X, +X, +Y, -Y
        static const glm::vec3 faceNormals[6] = {
            { 0,  0,  1}, { 0,  0, -1},
            {-1,  0,  0}, { 1,  0,  0},
            { 0,  1,  0}, { 0, -1,  0},
        };
        static const glm::ivec3 faceNeighbors[6] = {
            { 0,  0,  1}, { 0,  0, -1},
            {-1,  0,  0}, { 1,  0,  0},
            { 0,  1,  0}, { 0, -1,  0},
        };
        // 每个面的4个顶点（以体素坐标为原点，单位立方体角落）
        static const glm::vec3 faceQuads[6][4] = {
            {{0,0,1},{1,0,1},{1,1,1},{0,1,1}},  // +Z
            {{1,0,0},{0,0,0},{0,1,0},{1,1,0}},  // -Z
            {{0,0,0},{0,0,1},{0,1,1},{0,1,0}},  // -X
            {{1,0,1},{1,0,0},{1,1,0},{1,1,1}},  // +X
            {{0,1,1},{1,1,1},{1,1,0},{0,1,0}},  // +Y
            {{0,0,0},{1,0,0},{1,0,1},{0,0,1}},  // -Y
        };
        // 每面两个三角形的顶点索引
        static const int faceTriA[3] = {0,1,2};
        static const int faceTriB[3] = {0,2,3};

        // 顶面（+Y）稍亮，底面(-Y)和侧面稍暗，增加立体感
        static const float faceBrightness[6] = {0.82f, 0.82f, 0.72f, 0.72f, 1.0f, 0.55f};

        for (int localZ = 0; localZ < sizeZ; ++localZ)
        {
            for (int y = 0; y < WORLD_Y; ++y)
            {
                for (int localX = 0; localX < sizeX; ++localX)
                {
                    int wx = startX + localX;
                    int wz = startZ + localZ;
                    unsigned char mat = rawVoxelAt(wx, y, wz);
                    if (mat == 0) continue;

                    glm::vec3 baseColor = blockColor(mat, 1.0f);
                    glm::vec3 origin(static_cast<float>(wx),
                                     static_cast<float>(y),
                                     static_cast<float>(wz));

                    for (int f = 0; f < 6; ++f)
                    {
                        const glm::ivec3 &nb = faceNeighbors[f];
                        if (rawVoxelAt(wx + nb.x, y + nb.y, wz + nb.z) != 0)
                            continue; // 邻接体素不透明，跳过该面

                        glm::vec3 col   = baseColor * faceBrightness[f];
                        glm::vec3 norm  = faceNormals[f];

                        glm::vec3 q[4];
                        for (int i = 0; i < 4; ++i)
                            q[i] = origin + faceQuads[f][i];

                        // 三角形 A
                        vertices.push_back({q[faceTriA[0]], col, norm});
                        vertices.push_back({q[faceTriA[1]], col, norm});
                        vertices.push_back({q[faceTriA[2]], col, norm});
                        // 三角形 B
                        vertices.push_back({q[faceTriB[0]], col, norm});
                        vertices.push_back({q[faceTriB[1]], col, norm});
                        vertices.push_back({q[faceTriB[2]], col, norm});
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
        return glm::mix(candidate, getCameraEyePosition(), m_skillAimBlend);
    }

    glm::vec3 VoxelScene::getPlayerModelForward() const
    {
        glm::vec3 forward = getForward();
        glm::vec3 flat{forward.x, 0.0f, forward.z};
        if (glm::length(flat) < 0.001f)
            return {0.0f, 0.0f, -1.0f};
        return glm::normalize(flat);
    }

    bool VoxelScene::isSkillAimFirstPerson() const
    {
        return !m_thirdPersonView || m_skillAimBlend >= 0.98f;
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
        populateRouteModels();
        rebuildMonsterMesh();
        rebuildPlayerMesh();
        rebuildViewModelMesh();
        m_setupPhase = SetupPhase::Playing;
        updateMouseCapture();
    }

    void VoxelScene::populateRouteModels()
    {
        m_worldModels.clear();
        if (m_staticModelLibrary.empty() || !m_routeData.isValid())
            return;

        auto addModelNearCell = [&](size_t meshIndex, glm::ivec2 cell, glm::vec3 offset, float yaw, float scale,
                                    const std::string &label, const std::string &prompt, uint8_t interactionType)
        {
            if (meshIndex >= m_staticModelLibrary.size())
                return;
            glm::vec3 pos = getCellWorldCenter(cell) + offset;
            pos.y = static_cast<float>(std::max(findGroundY(static_cast<int>(std::floor(pos.x)), static_cast<int>(std::floor(pos.z))) + 1, 1));
            m_worldModels.push_back({meshIndex, pos, yaw, scale, label, prompt, interactionType, false});
        };

        addModelNearCell(0, m_routeData.startCell(), {2.2f, 0.0f, 1.6f}, 25.0f, 4.0f,
            "野外轮椅补给点", "按 E 搜索补给", kModelInteractionSupplyCache);
        addModelNearCell(1, m_routeData.objectiveCell, {-1.8f, 0.0f, 1.4f}, -30.0f, 4.8f,
            "应急除颤器", "按 E 进行急救充能", kModelInteractionHealStation);
        addModelNearCell(2, m_routeData.startCell(), {-2.0f, 0.0f, -1.4f}, 85.0f, 4.2f,
            "医疗拐杖", "按 E 拾取援助器材", kModelInteractionPickupAid);
        addModelNearCell(3, m_routeData.evacCell(), {1.4f, 0.0f, -1.8f}, 180.0f, 4.0f,
            "幸存者", "按 E 交谈并领取补给", kModelInteractionSurvivor);

        addModelNearCell(4, m_routeData.startCell(), {3.8f, 0.0f, -0.8f}, -20.0f, 4.1f,
            "营地成员", "", kModelInteractionNone);
        addModelNearCell(5, m_routeData.startCell(), {-3.6f, 0.0f, 0.9f}, 35.0f, 4.0f,
            "营地成员", "", kModelInteractionNone);
        addModelNearCell(6, m_routeData.startCell(), {0.8f, 0.0f, 3.6f}, 90.0f, 4.0f,
            "动力轮椅", "", kModelInteractionNone);

        addModelNearCell(7, m_routeData.objectiveCell, {2.8f, 0.0f, -2.6f}, 40.0f, 4.4f,
            "重型轮椅平台", "", kModelInteractionNone);
        addModelNearCell(8, m_routeData.objectiveCell, {-3.0f, 0.0f, -0.8f}, 15.0f, 3.8f,
            "助听器设施", "", kModelInteractionNone);
        addModelNearCell(9, m_routeData.objectiveCell, {0.4f, 0.0f, 3.2f}, -55.0f, 3.8f,
            "眼镜补给", "", kModelInteractionNone);

        addModelNearCell(4, m_routeData.evacCell(), {-2.4f, 0.0f, 1.7f}, 130.0f, 4.0f,
            "撤离队员", "", kModelInteractionNone);
        addModelNearCell(5, m_routeData.evacCell(), {3.0f, 0.0f, 0.4f}, -145.0f, 4.0f,
            "撤离队员", "", kModelInteractionNone);
        addModelNearCell(6, m_routeData.evacCell(), {-0.2f, 0.0f, -3.5f}, -90.0f, 3.8f,
            "动力撤离椅", "", kModelInteractionNone);

        addModelNearCell(10, m_routeData.startCell(), {-4.4f, 0.0f, -2.8f}, 55.0f, 4.0f,
            "GLB 营地成员", "", kModelInteractionNone);
        addModelNearCell(11, m_routeData.evacCell(), {4.5f, 0.0f, -2.0f}, -35.0f, 4.0f,
            "GLB 动力轮椅", "", kModelInteractionNone);
        addModelNearCell(12, m_routeData.objectiveCell, {3.5f, 0.0f, 2.5f}, 15.0f, 3.8f,
            "GLB 面罩补给", "", kModelInteractionNone);
    }

    int VoxelScene::findNearbyInteractableModel() const
    {
        int bestIndex = -1;
        float bestDistanceSq = 7.5f * 7.5f;
        for (int i = 0; i < static_cast<int>(m_worldModels.size()); ++i)
        {
            const auto &model = m_worldModels[static_cast<size_t>(i)];
            if (model.consumed || model.interactionType == kModelInteractionNone)
                continue;
            glm::vec3 delta = model.position - m_cameraPos;
            delta.y = 0.0f;
            float distanceSq = glm::dot(delta, delta);
            if (distanceSq < bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                bestIndex = i;
            }
        }
        return bestIndex;
    }

    void VoxelScene::interactWithModel(size_t modelIndex)
    {
        if (modelIndex >= m_worldModels.size())
            return;

        auto &model = m_worldModels[modelIndex];
        if (model.consumed)
            return;

        using Cat = game::inventory::ItemCategory;
        switch (model.interactionType)
        {
        case kModelInteractionSupplyCache:
            m_inventory.addItem({"wood", "木材", 64, Cat::Material}, 8);
            m_inventory.addItem({"stone", "石块", 64, Cat::Material}, 6);
            m_inventory.addItem({"apple", "苹果", 20, Cat::Consumable}, 2);
            model.prompt = "补给已取得";
            model.consumed = true;
            break;
        case kModelInteractionHealStation:
            m_hp = m_maxHp;
            m_starEnergy = m_maxStarEnergy;
            model.prompt = "急救已完成";
            model.consumed = true;
            break;
        case kModelInteractionPickupAid:
            m_inventory.addItem({"aid_crutch", "援助拐杖", 1, Cat::Material}, 1);
            m_defense = std::min(m_defense + 1.0f, 20.0f);
            model.prompt = "援助器材已收纳";
            model.consumed = true;
            break;
        case kModelInteractionSurvivor:
            m_inventory.addItem({"gold_coin", "金币", 99, Cat::Misc}, 6);
            m_inventory.addItem({"apple", "苹果", 20, Cat::Consumable}, 3);
            model.prompt = "幸存者已转移";
            model.consumed = true;
            break;
        default:
            break;
        }
    }

    void VoxelScene::renderStaticModels(const glm::mat4 &proj, const glm::mat4 &view, const glm::vec3 &renderCamera,
                                        const glm::vec3 &lightDir, const glm::vec3 &fogColor,
                                        float ambientStrength, float diffuseStrength, float fogNear, float fogFar, float flash)
    {
        if (m_modelShader == 0 || m_worldModels.empty())
            return;

        glUseProgram(m_modelShader);
        glUniform1i(glGetUniformLocation(m_modelShader, "uTex"), 0);
        if (m_glUniform3fv)
        {
            m_glUniform3fv(glGetUniformLocation(m_modelShader, "uLightDir"), 1, glm::value_ptr(lightDir));
            m_glUniform3fv(glGetUniformLocation(m_modelShader, "uCameraPos"), 1, glm::value_ptr(renderCamera));
            m_glUniform3fv(glGetUniformLocation(m_modelShader, "uFogColor"), 1, glm::value_ptr(fogColor));
        }
        if (m_glUniform1f)
        {
            m_glUniform1f(glGetUniformLocation(m_modelShader, "uAmbientStrength"), ambientStrength);
            m_glUniform1f(glGetUniformLocation(m_modelShader, "uDiffuseStrength"), diffuseStrength);
            m_glUniform1f(glGetUniformLocation(m_modelShader, "uFogNear"), fogNear);
            m_glUniform1f(glGetUniformLocation(m_modelShader, "uFogFar"), fogFar);
            m_glUniform1f(glGetUniformLocation(m_modelShader, "uFlash"), flash);
        }

        glActiveTexture(GL_TEXTURE0);
        for (const PlacedModel &placed : m_worldModels)
        {
            if (placed.consumed)
                continue;
            if (placed.meshIndex >= m_staticModelLibrary.size())
                continue;
            const StaticModelMesh &mesh = m_staticModelLibrary[placed.meshIndex];
            if (mesh.vertexCount <= 0)
                continue;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), placed.position);
            model = glm::rotate(model, glm::radians(placed.yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(placed.scale));
            glm::mat4 mvp = proj * view * model;
            glUniformMatrix4fv(glGetUniformLocation(m_modelShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
            glUniformMatrix4fv(glGetUniformLocation(m_modelShader, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
            glBindTexture(GL_TEXTURE_2D, mesh.texture);
            glBindVertexArray(mesh.vao);
            if (m_glDrawArrays)
                m_glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
        }
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
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

        float aimTargetBlend = (m_skillAimActive && m_thirdPersonView) ? 1.0f : 0.0f;
        float aimBlendStep = dt / 0.2f;
        if (m_skillAimBlend < aimTargetBlend)
            m_skillAimBlend = std::min(aimTargetBlend, m_skillAimBlend + aimBlendStep);
        else
            m_skillAimBlend = std::max(aimTargetBlend, m_skillAimBlend - aimBlendStep);

        updateStreamedChunks();
        processChunkStreamingBudget(m_chunkLoadBudget, m_chunkMeshBudget);

        if (!isRouteSetupComplete() || gameplayPaused)
        {
            rebuildPlayerMesh();
            if (!m_thirdPersonView)
                rebuildViewModelMesh();
            return;
        }

        // --- StarJump 抛物线跳跃 tick ---
        if (m_jumpSkillActive)
        {
            m_jumpSkillAge += dt;
            float jt = std::clamp(m_jumpSkillAge / std::max(m_jumpSkillDuration, 0.001f), 0.0f, 1.0f);
            float th = jt * jt * (3.0f - 2.0f * jt);  // smoothstep 水平
            m_cameraPos.x = glm::mix(m_jumpSkillOrigin.x, m_jumpSkillTarget.x, th);
            m_cameraPos.z = glm::mix(m_jumpSkillOrigin.z, m_jumpSkillTarget.z, th);
            float baseY = glm::mix(m_jumpSkillOrigin.y, m_jumpSkillTarget.y, jt);
            m_cameraPos.y = baseY + m_jumpSkillArcHeight * 4.0f * jt * (1.0f - jt);
            m_playerVerticalVelocity = 0.0f;
            m_playerOnGround = false;

            m_jumpTrailTimer += dt;
            if (m_jumpTrailTimer >= 0.045f)
            {
                m_jumpTrailTimer = 0.0f;
                SkillVFX trail;
                trail.type = game::skill::SkillEffect::StarJump;
                trail.worldPos = m_cameraPos + glm::vec3(
                    (randFloat() - 0.5f) * 0.5f, -0.8f + randFloat() * 0.5f, (randFloat() - 0.5f) * 0.5f);
                trail.age      = 0.0f;
                trail.maxAge   = 0.38f + randFloat() * 0.18f;
                trail.param    = 0.10f + randFloat() * 0.08f;
                trail.extraData = glm::vec3(randFloat(), randFloat(), 0.80f + randFloat() * 0.20f);
                m_skillVfxList.push_back(trail);
            }

            if (jt >= 1.0f)
            {
                m_jumpSkillActive = false;
                m_cameraPos = m_jumpSkillTarget;
                m_playerVerticalVelocity = 0.0f;
                m_playerOnGround = true;
                // 落地爆发粒子
                for (int burst = 0; burst < 8; ++burst)
                {
                    SkillVFX p;
                    p.type = game::skill::SkillEffect::StarJump;
                    p.worldPos = m_jumpSkillTarget + glm::vec3(
                        (randFloat() - 0.5f) * 1.4f, randFloat() * 0.6f, (randFloat() - 0.5f) * 1.4f);
                    p.age     = 0.0f;
                    p.maxAge  = 0.45f + randFloat() * 0.22f;
                    p.param   = 0.12f + randFloat() * 0.10f;
                    p.extraData = glm::vec3(randFloat(), randFloat(), 1.0f);
                    m_skillVfxList.push_back(p);
                }
            }

            // HD-2D: yaw is fixed; pitch controlled only by scroll wheel (0° to -45° from horizon)
            if (m_mouseCaptured)
                SDL_GetRelativeMouseState(nullptr, nullptr); // consume to prevent accumulation
            {
                float wheel = _context.getInputManager().getMouseWheelDelta();
                m_pitch -= wheel * 4.0f;
                m_pitch  = std::clamp(m_pitch, -45.0f, 0.0f);
            }

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

        float moveSpeed = m_baseMoveSpeed * (m_windStarEquipped ? 1.35f : 1.0f);
        if (glm::length(move) > 0.001f)
        {
            glm::vec3 moveDir = glm::normalize(move);
            m_cameraPos += glm::vec3(moveDir.x, 0.0f, moveDir.z) * moveSpeed * dt;
        }

        // 8-direction sprite animation from movement vector
        {
            constexpr float kFrameDur = 0.10f;
            glm::vec3 flatMove(move.x, 0.0f, move.z);
            if (glm::length(flatMove) > 0.001f)
            {
                glm::vec3 fm  = glm::normalize(flatMove);
                float ang = glm::degrees(std::atan2(fm.x, -fm.z));
                if (ang < 0.0f) ang += 360.0f;
                int dirIdx = static_cast<int>((ang + 22.5f) / 45.0f) % 8;
                m_playerSpriteRow   = (dirIdx + 4) % 8;  // N=4, E=6, S=0, W=2
                m_playerAnimTimer  += dt;
                if (m_playerAnimTimer >= kFrameDur)
                {
                    m_playerAnimTimer -= kFrameDur;
                    m_playerSpriteFrame = (m_playerSpriteFrame + 1) % 8;
                }
            }
            else
            {
                m_playerSpriteFrame = 0;
                m_playerAnimTimer   = 0.0f;
            }
        }

        if (keys[SDL_SCANCODE_SPACE] && m_playerOnGround)
        {
            m_playerVerticalVelocity = kPlayerJumpVelocity * (m_windStarEquipped ? 1.12f : 1.0f);
            m_playerOnGround = false;
        }

        m_playerVerticalVelocity -= kPlayerGravity * dt;
        if (keys[SDL_SCANCODE_LSHIFT] && m_playerVerticalVelocity > -10.0f)
            m_playerVerticalVelocity -= 12.0f * dt;
        m_cameraPos.y += m_playerVerticalVelocity * dt;

        // HD-2D: yaw is fixed; pitch controlled only by scroll wheel (0° to -45° from horizon)
        if (m_mouseCaptured)
            SDL_GetRelativeMouseState(nullptr, nullptr); // consume to prevent accumulation
        {
            float wheel = _context.getInputManager().getMouseWheelDelta();
            m_pitch -= wheel * 4.0f;
            m_pitch  = std::clamp(m_pitch, -45.0f, 0.0f);
        }

        m_cameraPos.x = std::clamp(m_cameraPos.x, 1.5f, static_cast<float>(worldWidth()) - 1.5f);
        m_cameraPos.z = std::clamp(m_cameraPos.z, 1.5f, static_cast<float>(worldDepth()) - 1.5f);

        int groundY = findGroundY(static_cast<int>(std::floor(m_cameraPos.x)), static_cast<int>(std::floor(m_cameraPos.z)));
        float minEyeY = static_cast<float>(std::max(groundY + 1, 1)) + (kPlayerEyeHeight - 1.0f);
        if (m_cameraPos.y <= minEyeY)
        {
            m_cameraPos.y = minEyeY;
            m_playerVerticalVelocity = 0.0f;
            m_playerOnGround = true;
        }
        else
        {
            m_playerOnGround = false;
        }
        m_cameraPos.y = std::clamp(m_cameraPos.y, minEyeY, WORLD_Y - 1.5f);

        updateRouteProgress();

        rebuildPlayerMesh();
        if (!m_thirdPersonView)
            rebuildViewModelMesh();
    }

    void VoxelScene::tickGameplaySystems(float dt, int displayW, int displayH)
    {
        m_timeOfDaySystem.update(dt);
        m_weatherSystem.update(dt, static_cast<float>(displayW), static_cast<float>(displayH));
        m_dashScreenOverlay = std::max(0.0f, m_dashScreenOverlay - dt * 1.9f);
        m_fireScreenOverlay = std::max(0.0f, m_fireScreenOverlay - dt * 0.68f);

        for (float &cooldown : m_skillCooldowns)
            cooldown = std::max(0.0f, cooldown - dt);

        m_dashCooldown = std::max(0.0f, m_dashCooldown - dt);
        m_starEnergy = std::min(m_maxStarEnergy, m_starEnergy + dt * 7.5f);
        tickSkillEffects(dt);
        tickSkillProjectiles(dt);

        float fireSustain = 0.0f;
        for (const auto &vfx : m_skillVfxList)
        {
            if (vfx.type != game::skill::SkillEffect::FireBlast)
                continue;
            float life = vfx.maxAge > 0.0f ? 1.0f - std::clamp(vfx.age / vfx.maxAge, 0.0f, 1.0f) : 0.0f;
            float pulse = vfx.param < 0.0f ? 0.32f + life * 0.18f : 0.46f + life * 0.44f;
            fireSustain = std::max(fireSustain, pulse);
        }
        if (!m_skillProjectiles.empty())
            fireSustain = std::max(fireSustain, 0.38f + 0.07f * std::min(3.0f, static_cast<float>(m_skillProjectiles.size())));
        for (const auto &field : m_burnFields)
        {
            float life = field.maxAge > 0.0f ? 1.0f - std::clamp(field.age / field.maxAge, 0.0f, 1.0f) : 0.0f;
            fireSustain = std::max(fireSustain, 0.30f + life * 0.30f);
        }
        m_fireScreenOverlay = std::max(m_fireScreenOverlay, std::min(fireSustain, 1.0f));

        tickStarSkillPassives(dt);
        updateMonsters(dt);
    }

    void VoxelScene::tickSkillEffects(float dt)
    {
        for (auto &vfx : m_skillVfxList)
            vfx.age += dt;
        m_skillVfxList.erase(
            std::remove_if(m_skillVfxList.begin(), m_skillVfxList.end(),
                [](const SkillVFX &vfx) { return vfx.age >= vfx.maxAge; }),
            m_skillVfxList.end());

        for (auto &particle : m_fireTrailParticles)
        {
            particle.age += dt;
            particle.worldPos += particle.velocity * dt;
            particle.velocity *= std::max(0.0f, 1.0f - dt * 2.8f);
            particle.velocity.y += 1.6f * dt;
        }
        m_fireTrailParticles.erase(
            std::remove_if(m_fireTrailParticles.begin(), m_fireTrailParticles.end(),
                [](const FireTrailParticle &particle) { return particle.age >= particle.maxAge; }),
            m_fireTrailParticles.end());

        for (auto &field : m_burnFields)
        {
            field.age += dt;
            field.tickTimer += dt;
            while (field.tickTimer >= 0.35f)
            {
                field.tickTimer -= 0.35f;
                damageMonstersInRadius(field.center, field.radius, 18.0f, glm::vec3(0.0f, 0.8f, 0.0f));
            }
        }
        m_burnFields.erase(
            std::remove_if(m_burnFields.begin(), m_burnFields.end(),
                [](const BurnField &field) { return field.age >= field.maxAge; }),
            m_burnFields.end());
    }

    void VoxelScene::tickSkillProjectiles(float dt)
    {
        if (m_skillProjectiles.empty())
            return;

        for (auto &proj : m_skillProjectiles)
        {
            proj.age += dt;
            proj.lastWorldPos = proj.worldPos;
            proj.velocity.y -= kFireProjectileGravity * dt;
            proj.worldPos += proj.velocity * dt;

            glm::vec3 forward = glm::length(proj.velocity) > 0.001f ? glm::normalize(proj.velocity) : glm::vec3(0.0f, 0.2f, 1.0f);
            for (int i = 0; i < 2; ++i)
            {
                float spread = static_cast<float>(i) - 0.5f;
                FireTrailParticle particle;
                particle.worldPos = proj.worldPos - forward * (0.18f + 0.08f * static_cast<float>(i));
                particle.velocity = glm::vec3(spread * 0.45f, 0.45f + randFloat() * 0.35f, -spread * 0.30f);
                particle.maxAge = 0.24f + randFloat() * 0.26f;
                particle.size = 0.05f + randFloat() * 0.05f;
                m_fireTrailParticles.push_back(particle);
            }

            bool explode = false;
            glm::vec3 impactPos = proj.worldPos;

            if (proj.age > 0.04f)
            {
                constexpr int kSamples = 5;
                for (int sample = 1; sample <= kSamples; ++sample)
                {
                    float t = static_cast<float>(sample) / static_cast<float>(kSamples);
                    glm::vec3 probe = glm::mix(proj.lastWorldPos, proj.worldPos, t);
                    glm::ivec3 block = glm::floor(probe);
                    if (isInside(block.x, block.y, block.z) && isSolid(block.x, block.y, block.z))
                    {
                        impactPos = probe - glm::normalize(proj.velocity) * 0.15f;
                        explode = true;
                        break;
                    }
                }
            }

            glm::vec3 travel = proj.targetPos - proj.originPos;
            float travelLenSq = glm::dot(travel, travel);
            if (!explode && travelLenSq > 0.0001f)
            {
                glm::vec3 progressed = proj.worldPos - proj.originPos;
                if (glm::dot(progressed, travel) >= travelLenSq)
                {
                    explode = true;
                    impactPos = proj.targetPos;
                }
            }

            if (!explode && proj.age >= proj.maxAge)
            {
                explode = true;
                impactPos = proj.worldPos;
            }

            if (explode)
            {
                explodeBlocks(glm::floor(impactPos), 2);
                damageMonstersInRadius(impactPos, proj.radius, 170.0f, glm::vec3(0.0f, 6.0f, 0.0f));
                m_skillVfxList.push_back({game::skill::SkillEffect::FireBlast, impactPos, 0.0f, 0.72f, proj.radius});
                m_burnFields.push_back({impactPos, std::max(1.6f, proj.radius * 0.72f), 0.0f, 4.5f, 0.0f});
                m_fireScreenOverlay = std::max(m_fireScreenOverlay, 1.0f);
                proj.age = proj.maxAge;
            }
        }

        m_skillProjectiles.erase(
            std::remove_if(m_skillProjectiles.begin(), m_skillProjectiles.end(),
                [](const SkillProjectile &proj) { return proj.age >= proj.maxAge; }),
            m_skillProjectiles.end());
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

    glm::vec3 VoxelScene::getSkillCastOrigin() const
    {
        glm::vec3 forward = getForward();
        glm::vec3 right = getRight();
        glm::vec3 up = glm::normalize(glm::cross(right, forward));
        if (m_thirdPersonView && !isSkillAimFirstPerson())
        {
            glm::vec3 playerForward = getPlayerModelForward();
            glm::vec3 playerRight = glm::normalize(glm::cross(playerForward, glm::vec3(0.0f, 1.0f, 0.0f)));
            return m_cameraPos + glm::vec3(0.0f, 0.95f, 0.0f) + playerForward * 0.72f + playerRight * 0.42f;
        }
        return m_cameraPos + forward * 0.88f + right * 0.34f - up * 0.16f;
    }

    bool VoxelScene::canAimAttackStarSkill() const
    {
        for (size_t i = 0; i < m_starSockets.size(); ++i)
        {
            const auto &slot = m_starSockets[i];
            if (slot.isEmpty() || m_skillCooldowns[i] > 0.0f)
                continue;

            const auto *def = game::skill::getStarSkillDef(slot.item->id);
            if (def && def->effect == game::skill::SkillEffect::FireBlast && m_starEnergy >= 18.0f)
                return true;
        }
        return false;
    }

    bool VoxelScene::hasActiveSkillVisuals() const
    {
        return !m_skillProjectiles.empty() ||
               !m_skillVfxList.empty() ||
               !m_fireTrailParticles.empty() ||
               !m_burnFields.empty();
    }

    int VoxelScene::findAimedMonsterIndex() const
    {
        glm::vec3 origin = m_cameraPos;
        glm::vec3 dir = getForward();
        int bestIndex = -1;
        float bestT = 9999.0f;

        for (size_t index = 0; index < m_monsters.size(); ++index)
        {
            const auto &monster = m_monsters[index];
            glm::vec3 center = monster.pos + glm::vec3(0.0f, monsterHalfExtents(monster.type).y, 0.0f);
            float radius = std::max({monsterHalfExtents(monster.type).x, monsterHalfExtents(monster.type).y, monsterHalfExtents(monster.type).z}) * 1.35f;
            glm::vec3 toCenter = center - origin;
            float t = glm::dot(toCenter, dir);
            if (t < 0.2f || t > 28.0f)
                continue;

            glm::vec3 closest = origin + dir * t;
            float distanceSq = glm::dot(center - closest, center - closest);
            if (distanceSq > radius * radius)
                continue;
            if (t < bestT)
            {
                bestT = t;
                bestIndex = static_cast<int>(index);
            }
        }

        return bestIndex;
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

            glm::vec3 origin = getSkillCastOrigin();
            glm::vec3 delta = attackCenter - origin;
            glm::vec2 horizontalDelta{delta.x, delta.z};
            float horizontalDistance = glm::length(horizontalDelta);
            glm::vec2 horizontalDir = horizontalDistance > 0.001f
                ? horizontalDelta / horizontalDistance
                : glm::vec2(getForward().x, getForward().z);
            float flightTime = std::clamp(horizontalDistance / kFireProjectileHorizontalSpeed,
                                          kFireProjectileMinFlightTime,
                                          kFireProjectileMaxFlightTime);
            glm::vec3 velocity{
                horizontalDir.x * (horizontalDistance / flightTime),
                0.0f,
                horizontalDir.y * (horizontalDistance / flightTime)
            };
            velocity.y = (delta.y + 0.5f * kFireProjectileGravity * flightTime * flightTime) / flightTime;

            m_skillProjectiles.push_back({
                game::skill::SkillEffect::FireBlast,
                origin,
                origin,
                origin,
                attackCenter,
                velocity,
                0.0f,
                std::min(flightTime + 0.4f, 1.8f),
                3.4f
            });
            m_skillVfxList.push_back({game::skill::SkillEffect::FireBlast, origin, 0.0f, 0.18f, -1.0f});
            m_fireScreenOverlay = std::max(m_fireScreenOverlay, 0.58f);
            m_skillCooldowns[i] = def->cooldown;
            m_starEnergy = std::max(0.0f, m_starEnergy - 18.0f);
            return;
        }
    }

    bool VoxelScene::triggerAimedAttackStarSkill(const TargetBlock &target)
    {
        glm::vec3 aimPoint = target.hit
            ? glm::vec3(target.block) + glm::vec3(0.5f)
            : m_cameraPos + getForward() * 18.0f;
        const size_t projectileCountBefore = m_skillProjectiles.size();
        triggerAttackStarSkills(aimPoint);
        return m_skillProjectiles.size() > projectileCountBefore;
    }

    void VoxelScene::triggerActiveStarSkills()
    {
        for (size_t i = 0; i < m_starSockets.size(); ++i)
        {
            const auto &slot = m_starSockets[i];
            if (slot.isEmpty() || m_skillCooldowns[i] > 0.0f)
                continue;

            const auto *def = game::skill::getStarSkillDef(slot.item->id);
            if (!def || (def->effect != game::skill::SkillEffect::LightDash && def->effect != game::skill::SkillEffect::StarJump))
                continue;

            if (def->effect == game::skill::SkillEffect::StarJump)
            {
                if (m_jumpSkillActive) { continue; }
                if (m_starEnergy < 28.0f) return;

                glm::vec3 fwd = getForward();
                glm::vec3 horizFwd = glm::vec3(fwd.x, 0.0f, fwd.z);
                float horizLen = glm::length(horizFwd);
                if (horizLen > 0.001f) horizFwd /= horizLen;
                else horizFwd = {0.0f, 0.0f, -1.0f};

                glm::vec3 target = m_cameraPos + horizFwd * def->param;
                target.x = std::clamp(target.x, 1.5f, static_cast<float>(worldWidth()) - 1.5f);
                target.z = std::clamp(target.z, 1.5f, static_cast<float>(worldDepth()) - 1.5f);

                int targetGY = findGroundY(static_cast<int>(std::floor(target.x)), static_cast<int>(std::floor(target.z)));
                float targetY = static_cast<float>(std::max(targetGY + 1, 1)) + (kPlayerEyeHeight - 1.0f);
                target.y = std::clamp(targetY, 2.0f, WORLD_Y - 1.5f);

                float actualDist = glm::length(glm::vec2(target.x - m_cameraPos.x, target.z - m_cameraPos.z));
                m_jumpSkillArcHeight = 5.0f + actualDist * 0.35f;
                m_jumpSkillDuration  = 0.65f + actualDist * 0.018f;
                m_jumpSkillOrigin = m_cameraPos;
                m_jumpSkillTarget = target;
                m_jumpSkillAge    = 0.0f;
                m_jumpSkillActive = true;
                m_jumpTrailTimer  = 0.0f;
                m_skillCooldowns[i] = def->cooldown;
                m_starEnergy = std::max(0.0f, m_starEnergy - 28.0f);
                m_dashScreenOverlay = 0.5f;
                return;
            }

            if (m_starEnergy < 22.0f || m_dashCooldown > 0.0f)
                return;

            glm::vec3 dashStart = m_cameraPos;
            m_cameraPos += getForward() * def->param;
            m_cameraPos.x = std::clamp(m_cameraPos.x, 1.5f, static_cast<float>(worldWidth()) - 1.5f);
            m_cameraPos.y = std::clamp(m_cameraPos.y, 2.0f, WORLD_Y - 1.5f);
            m_cameraPos.z = std::clamp(m_cameraPos.z, 1.5f, static_cast<float>(worldDepth()) - 1.5f);
            glm::vec3 dashEnd = m_cameraPos;
            m_dashCooldown = def->cooldown;
            m_skillCooldowns[i] = def->cooldown;
            m_starEnergy = std::max(0.0f, m_starEnergy - 22.0f);
            m_dashScreenOverlay = 1.0f;

            glm::vec3 dashDelta = dashEnd - dashStart;
            float dashDistance = glm::length(glm::vec2(dashDelta.x, dashDelta.z));
            glm::vec3 dashDir = dashDistance > 0.001f
                ? glm::normalize(glm::vec3(dashDelta.x, 0.0f, dashDelta.z))
                : glm::normalize(glm::vec3(getForward().x, 0.0f, getForward().z));

            auto spawnDashSparkles = [&](const glm::vec3 &center, int count, float radiusMin, float radiusMax,
                                         float heightMin, float heightMax, float scaleMin, float scaleMax,
                                         float ageJitter, float lifeMin, float lifeMax, float directionalBias)
            {
                for (int star = 0; star < count; ++star)
                {
                    float angle = randFloat() * static_cast<float>(M_PI) * 2.0f;
                    float radius = radiusMin + randFloat() * (radiusMax - radiusMin);
                    float height = heightMin + randFloat() * (heightMax - heightMin);
                    glm::vec3 offset{std::cos(angle) * radius, height, std::sin(angle) * radius};
                    offset += dashDir * (directionalBias * (randFloat() - 0.35f));

                    SkillVFX sparkle;
                    sparkle.type = game::skill::SkillEffect::LightDash;
                    sparkle.worldPos = center + offset;
                    sparkle.age = randFloat() * ageJitter;
                    sparkle.maxAge = lifeMin + randFloat() * (lifeMax - lifeMin);
                    sparkle.param = scaleMin + randFloat() * (scaleMax - scaleMin);
                    sparkle.extraData = glm::vec3(randFloat(), randFloat(), directionalBias > 0.4f ? 0.85f : 0.28f);
                    m_skillVfxList.push_back(sparkle);
                }
            };

            auto spawnDashConstellation = [&](const glm::vec3 &center, float outerRadius, float innerRadius,
                                              float yOffset, float sizeScale, float maxAge, float burstFactor,
                                              float angleOffset)
            {
                glm::vec3 side = glm::cross(dashDir, glm::vec3(0.0f, 1.0f, 0.0f));
                if (glm::dot(side, side) < 0.0001f)
                    side = glm::vec3(1.0f, 0.0f, 0.0f);
                else
                    side = glm::normalize(side);

                const int pointCount = 5;
                static const float chainOffsets[5] = {-0.95f, -0.42f, 0.0f, 0.58f, 1.08f};
                static const float sideOffsets[5] = {-0.22f, 0.14f, -0.06f, 0.20f, -0.12f};
                static const float sizeWeights[5] = {1.18f, 0.74f, 1.0f, 0.68f, 0.92f};
                std::vector<glm::vec3> starPoints;
                starPoints.reserve(static_cast<size_t>(pointCount));
                for (int point = 0; point < pointCount; ++point)
                {
                    float chainT = chainOffsets[point];
                    float sideT = sideOffsets[point];
                    float localHeight = yOffset + 0.05f * static_cast<float>(point % 2) + 0.03f * static_cast<float>(point == 2);
                    glm::vec3 offset = dashDir * (chainT * outerRadius) + side * (sideT * innerRadius);

                    SkillVFX starPoint;
                    starPoint.type = game::skill::SkillEffect::LightDash;
                    starPoint.worldPos = center + offset + glm::vec3(0.0f, localHeight, 0.0f);
                    starPoint.age = 0.0f;
                    starPoint.maxAge = maxAge * (0.92f + 0.04f * static_cast<float>(point % 3));
                    starPoint.param = sizeScale * sizeWeights[point];
                    starPoint.extraData = glm::vec3(
                        glm::fract(0.19f * static_cast<float>(point) + 0.17f + angleOffset * 0.1f),
                        glm::fract(0.27f * static_cast<float>(point) + 0.31f + angleOffset * 0.07f),
                        burstFactor);
                    m_skillVfxList.push_back(starPoint);
                    starPoints.push_back(starPoint.worldPos);
                }

                for (int point = 0; point < pointCount - 1; ++point)
                {
                    const glm::vec3 &start = starPoints[static_cast<size_t>(point)];
                    const glm::vec3 &end = starPoints[static_cast<size_t>(point + 1)];
                    SkillVFX link;
                    link.type = game::skill::SkillEffect::LightDash;
                    link.worldPos = start;
                    link.age = 0.0f;
                    link.maxAge = maxAge * (0.94f - 0.05f * static_cast<float>(point % 2));
                    link.param = -(0.018f + sizeScale * 0.08f);
                    link.extraData = end;
                    m_skillVfxList.push_back(link);
                }

                SkillVFX core;
                core.type = game::skill::SkillEffect::LightDash;
                core.worldPos = center + dashDir * (0.1f * outerRadius) + glm::vec3(0.0f, yOffset + 0.02f, 0.0f);
                core.age = 0.0f;
                core.maxAge = maxAge * 0.92f;
                core.param = sizeScale * 0.82f;
                core.extraData = glm::vec3(0.82f, 0.18f, burstFactor);
                m_skillVfxList.push_back(core);
            };

            spawnDashSparkles(dashStart, 2, 0.20f, 0.52f, 0.08f, 0.42f, 0.06f, 0.10f, 0.10f, 0.18f, 0.28f, 0.08f);
            spawnDashSparkles(dashEnd, 5, 0.42f, 1.30f, 0.14f, 1.05f, 0.08f, 0.16f, 0.04f, 0.30f, 0.56f, 0.26f);
            spawnDashConstellation(dashStart, 0.38f, 0.18f, 0.58f, 0.08f, 0.30f, 0.52f, randFloat() * static_cast<float>(M_PI));
            spawnDashConstellation(dashEnd, 1.36f, 0.54f, 0.92f, 0.14f, 0.56f, 0.84f, randFloat() * static_cast<float>(M_PI));

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

    void VoxelScene::renderSkillEffects3D(const glm::mat4 &proj, const glm::mat4 &view, const glm::vec3 &renderCamera,
                                          const glm::vec3 &lightDir, const glm::vec3 &fogColor,
                                          float ambientStrength, float diffuseStrength, float fogNear, float fogFar, float flash)
    {
        std::vector<Vertex> vertices;
        vertices.reserve(m_skillVfxList.size() * 24 * 72);

        auto safeNormalize = [](const glm::vec3 &value, const glm::vec3 &fallback)
        {
            float len = glm::length(value);
            return len > 0.0001f ? value / len : fallback;
        };

        for (const auto &vfx : m_skillVfxList)
        {
            float t = vfx.maxAge > 0.0f ? std::clamp(vfx.age / vfx.maxAge, 0.0f, 1.0f) : 1.0f;
            float ease = 1.0f - t;

            switch (vfx.type)
            {
            case game::skill::SkillEffect::FireBlast:
                break;
            case game::skill::SkillEffect::LightDash:
            {
                if (vfx.param < 0.0f)
                {
                    glm::vec3 start = vfx.worldPos;
                    glm::vec3 end = vfx.extraData;
                    glm::vec3 delta = end - start;
                    float length = glm::length(delta);
                    if (length > 0.001f)
                    {
                        glm::vec3 forward = safeNormalize(delta, glm::vec3(0.0f, 0.0f, 1.0f));
                        glm::vec3 side = safeNormalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
                        glm::vec3 up = safeNormalize(glm::cross(side, forward), glm::vec3(0.0f, 1.0f, 0.0f));
                        float thickness = std::max(-vfx.param, 0.018f) * (0.82f + ease * 0.34f);
                        glm::vec3 lineColor = glm::mix(glm::vec3(0.72f, 0.90f, 1.0f), glm::vec3(1.0f, 0.86f, 0.58f), 0.35f + 0.25f * ease);
                        appendOrientedBox(vertices,
                            (start + end) * 0.5f,
                            {thickness, thickness * 0.58f, length * 0.5f},
                            side, up, forward,
                            lineColor);
                    }
                }
                break;
            }
            default:
                break;
            }
        }

        m_effectVertexCount = static_cast<int>(vertices.size());
        glBindBuffer(GL_ARRAY_BUFFER, m_effectVbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
        if (m_effectVertexCount <= 0)
            return;

        glUseProgram(m_shader);
        glm::mat4 mvp = proj * view;
        glUniformMatrix4fv(glGetUniformLocation(m_shader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        if (m_glUniform3fv)
        {
            m_glUniform3fv(glGetUniformLocation(m_shader, "uLightDir"), 1, glm::value_ptr(lightDir));
            m_glUniform3fv(glGetUniformLocation(m_shader, "uCameraPos"), 1, glm::value_ptr(renderCamera));
            m_glUniform3fv(glGetUniformLocation(m_shader, "uFogColor"), 1, glm::value_ptr(fogColor));
        }
        if (m_glUniform1f)
        {
            m_glUniform1f(glGetUniformLocation(m_shader, "uAmbientStrength"), ambientStrength + 0.15f);
            m_glUniform1f(glGetUniformLocation(m_shader, "uDiffuseStrength"), diffuseStrength + 0.18f);
            m_glUniform1f(glGetUniformLocation(m_shader, "uFogNear"), fogNear);
            m_glUniform1f(glGetUniformLocation(m_shader, "uFogFar"), fogFar);
            m_glUniform1f(glGetUniformLocation(m_shader, "uFlash"), flash + 0.45f);
        }
        glBindVertexArray(m_effectVao);
        if (m_glDrawArrays)
            m_glDrawArrays(GL_TRIANGLES, 0, m_effectVertexCount);
        glBindVertexArray(0);
    }

    void VoxelScene::renderDashStarEffects3D(const glm::mat4 &proj, const glm::mat4 &view, const glm::vec3 &renderCamera,
                                             const glm::vec3 &fogColor, float fogNear, float fogFar, int viewportHeight)
    {
        if (!m_dashStarShader || !m_dashStarVao || !m_dashStarVbo || !m_dashGradientATexture || !m_dashGradientBTexture)
            return;

        std::vector<DashStarVertex> vertices;
        vertices.reserve(m_skillVfxList.size() * 2);

        for (const auto &vfx : m_skillVfxList)
        {
            if ((vfx.type != game::skill::SkillEffect::LightDash && vfx.type != game::skill::SkillEffect::StarJump) || vfx.param < 0.0f)
                continue;

            float ageNorm = vfx.maxAge > 0.0f ? std::clamp(vfx.age / vfx.maxAge, 0.0f, 1.0f) : 1.0f;
            float life = 1.0f - ageNorm;
            if (life <= 0.0f)
                continue;

            float burstFactor = std::clamp(vfx.extraData.z, 0.0f, 1.0f);
            float sizePx = glm::mix(24.0f, 58.0f, std::clamp(vfx.param * 2.1f, 0.0f, 1.0f));
            sizePx *= glm::mix(0.86f, 1.24f, burstFactor);
            float seedA = glm::fract(vfx.extraData.x + vfx.param * 5.37f);
            float seedB = glm::fract(vfx.extraData.y + vfx.param * 3.19f);
            float sparklePulse = 0.84f + 0.16f * std::sin(vfx.age * 15.0f + seedA * 31.0f);
            float brightness = glm::mix(0.95f, 1.55f, burstFactor) * sparklePulse;
            float alphaScale = glm::mix(0.62f, 1.0f, burstFactor);
            float driftScale = glm::mix(0.14f, 0.46f, burstFactor);
            float variant = glm::fract(seedA * 0.67f + seedB * 0.41f + vfx.param * 2.1f);

            vertices.push_back({
                vfx.worldPos,
                glm::vec4(sizePx, ageNorm, seedA, seedB),
                glm::vec4(brightness, alphaScale, driftScale, variant)
            });

            if (burstFactor > 0.72f)
            {
                vertices.push_back({
                    vfx.worldPos,
                    glm::vec4(sizePx * 0.66f, ageNorm, glm::fract(seedA + 0.27f), glm::fract(seedB + 0.43f)),
                    glm::vec4(brightness * 1.12f, alphaScale * 0.92f, driftScale * 0.72f, glm::fract(variant + 0.33f))
                });
            }
        }

        m_dashStarVertexCount = static_cast<int>(vertices.size());
        if (m_dashStarVertexCount <= 0)
            return;

        glEnable(GL_BLEND);
        if (m_glBlendFunc)
            m_glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_PROGRAM_POINT_SIZE);
        if (m_glDepthMask)
            m_glDepthMask(GL_FALSE);

        glUseProgram(m_dashStarShader);
        glUniformMatrix4fv(glGetUniformLocation(m_dashStarShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(m_dashStarShader, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
        if (m_glUniform3fv)
        {
            m_glUniform3fv(glGetUniformLocation(m_dashStarShader, "uFogColor"), 1, glm::value_ptr(fogColor));
            m_glUniform3fv(glGetUniformLocation(m_dashStarShader, "uCameraPos"), 1, glm::value_ptr(renderCamera));
        }
        if (m_glUniform1f)
        {
            m_glUniform1f(glGetUniformLocation(m_dashStarShader, "uFogNear"), fogNear);
            m_glUniform1f(glGetUniformLocation(m_dashStarShader, "uFogFar"), fogFar);
            m_glUniform1f(glGetUniformLocation(m_dashStarShader, "uViewportHeight"), static_cast<float>(viewportHeight));
            m_glUniform1f(glGetUniformLocation(m_dashStarShader, "uGlobalTime"), m_timeOfDaySystem.getTimeOfDay() * 360.0f);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_dashGradientATexture);
        glUniform1i(glGetUniformLocation(m_dashStarShader, "uGradientA"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_dashGradientBTexture);
        glUniform1i(glGetUniformLocation(m_dashStarShader, "uGradientB"), 1);

        glBindVertexArray(m_dashStarVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_dashStarVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(DashStarVertex)), vertices.data(), GL_DYNAMIC_DRAW);
        if (m_glDrawArrays)
            m_glDrawArrays(GL_POINTS, 0, m_dashStarVertexCount);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        if (m_glDepthMask)
            m_glDepthMask(GL_TRUE);
        if (m_glBlendFunc)
            m_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void VoxelScene::renderFireFieldEffects3D(const glm::mat4 &proj, const glm::mat4 &view, const glm::vec3 &renderCamera,
                                              const glm::vec3 &fogColor, float fogNear, float fogFar, int viewportHeight)
    {
        if (!m_fireFieldShader || !m_dashStarVao || !m_dashStarVbo)
            return;

        std::vector<DashStarVertex> vertices;
        vertices.reserve(m_burnFields.size() * 32 + m_skillProjectiles.size() * 10 + m_skillVfxList.size() * 24 + m_fireTrailParticles.size() * 4);

        auto pushFireSprite = [&](const glm::vec3 &pos, float sizePx, float ageNorm, float seedA, float seedB,
                                  float brightness, float alphaScale, float lift, float variant)
        {
            vertices.push_back({
                pos,
                glm::vec4(sizePx, ageNorm, seedA, seedB),
                glm::vec4(brightness, alphaScale, lift, variant)
            });
        };

        auto safeNormalize = [](const glm::vec3 &value, const glm::vec3 &fallback)
        {
            float len = glm::length(value);
            return len > 0.0001f ? value / len : fallback;
        };

        for (const auto &proj : m_skillProjectiles)
        {
            float ageNorm = proj.maxAge > 0.0f ? std::clamp(proj.age / proj.maxAge, 0.0f, 1.0f) : 1.0f;
            float life = 1.0f - ageNorm;
            if (life <= 0.0f)
                continue;

            glm::vec3 forward = safeNormalize(proj.velocity, glm::vec3(0.0f, 0.2f, 1.0f));
            glm::vec3 side = safeNormalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::vec3 up = safeNormalize(glm::cross(side, forward), glm::vec3(0.0f, 1.0f, 0.0f));
            float pulse = 0.86f + 0.14f * std::sin(proj.age * 18.0f);

            for (int plume = 0; plume < 5; ++plume)
            {
                float lane = static_cast<float>(plume) / 4.0f;
                float back = 0.06f + 0.15f * static_cast<float>(plume);
                float sideOffset = (lane - 0.5f) * 0.16f;
                glm::vec3 pos = proj.worldPos - forward * back + side * sideOffset + up * (0.04f + 0.03f * std::sin(proj.age * 11.0f + lane * 8.0f));
                float seedA = glm::fract(0.19f + lane * 0.27f + proj.age * 0.31f);
                float seedB = glm::fract(0.53f + lane * 0.21f + proj.originPos.x * 0.07f + proj.originPos.z * 0.05f);
                float sizePx = (plume == 0 ? 118.0f : 86.0f - 8.0f * static_cast<float>(plume)) * pulse;
                float brightness = (plume == 0 ? 1.88f : 1.34f + 0.12f * static_cast<float>(4 - plume)) * (0.92f + 0.22f * life);
                float alphaScale = plume == 0 ? 1.0f : 0.84f;
                float lift = 1.02f + 0.20f * static_cast<float>(plume);
                float variant = 0.24f + lane * 0.48f;
                pushFireSprite(pos, sizePx, ageNorm, seedA, seedB, brightness, alphaScale, lift, variant);
            }

            for (int ember = 0; ember < 4; ++ember)
            {
                float lane = static_cast<float>(ember) / 3.0f;
                glm::vec3 pos = proj.worldPos - forward * (0.30f + 0.12f * static_cast<float>(ember)) + up * (0.05f * static_cast<float>(ember)) - side * ((lane - 0.5f) * 0.10f);
                float seedA = glm::fract(0.73f + lane * 0.17f + proj.age * 0.27f);
                float seedB = glm::fract(0.28f + lane * 0.39f + proj.targetPos.x * 0.04f);
                pushFireSprite(pos, 44.0f - 4.0f * static_cast<float>(ember), ageNorm, seedA, seedB, 1.02f, 0.52f, 0.92f, 0.14f + lane * 0.22f);
            }

            float shockSeedA = glm::fract(proj.originPos.x * 0.13f + proj.age * 0.41f);
            float shockSeedB = glm::fract(proj.originPos.z * 0.17f + proj.age * 0.29f);
            pushFireSprite(proj.worldPos, 136.0f * pulse, ageNorm, shockSeedA, shockSeedB, 1.95f, 0.92f, -1.0f, 0.62f);
        }

        for (const auto &particle : m_fireTrailParticles)
        {
            float ageNorm = particle.maxAge > 0.0f ? std::clamp(particle.age / particle.maxAge, 0.0f, 1.0f) : 1.0f;
            float life = 1.0f - ageNorm;
            if (life <= 0.0f)
                continue;

            float seedA = glm::fract(particle.worldPos.x * 0.17f + particle.age * 0.31f);
            float seedB = glm::fract(particle.worldPos.z * 0.23f + particle.age * 0.27f + particle.size * 1.9f);
            float sizePx = glm::mix(34.0f, 62.0f, particle.size * 8.0f) * (0.86f + life * 0.36f);
            float brightness = 0.94f + life * 0.44f;
            float alphaScale = 0.48f + life * 0.28f;
            float lift = 0.88f + life * 0.22f;
            float variant = 0.18f + seedA * 0.34f;
            pushFireSprite(particle.worldPos, sizePx, ageNorm, seedA, seedB, brightness, alphaScale, lift, variant);
        }

        for (const auto &vfx : m_skillVfxList)
        {
            if (vfx.type != game::skill::SkillEffect::FireBlast)
                continue;

            float ageNorm = vfx.maxAge > 0.0f ? std::clamp(vfx.age / vfx.maxAge, 0.0f, 1.0f) : 1.0f;
            float life = 1.0f - ageNorm;
            if (life <= 0.0f)
                continue;

            if (vfx.param < 0.0f)
            {
                for (int plume = 0; plume < 8; ++plume)
                {
                    float norm = static_cast<float>(plume) / 8.0f;
                    float angle = norm * static_cast<float>(M_PI) * 2.0f + vfx.age * 2.2f;
                    glm::vec3 pos = vfx.worldPos + glm::vec3(std::cos(angle) * 0.10f, 0.08f + 0.02f * std::sin(vfx.age * 10.0f + norm * 9.0f), std::sin(angle) * 0.10f);
                    float seedA = glm::fract(0.16f + norm * 0.37f);
                    float seedB = glm::fract(0.52f + norm * 0.29f);
                    pushFireSprite(pos, 68.0f + 10.0f * std::sin(norm * 11.0f), ageNorm, seedA, seedB, 1.34f, 0.92f, 1.02f, 0.18f + norm * 0.34f);
                }
                pushFireSprite(vfx.worldPos + glm::vec3(0.0f, 0.04f, 0.0f), 92.0f, ageNorm, 0.17f, 0.63f, 1.65f, 1.0f, 1.15f, 0.42f);
                pushFireSprite(vfx.worldPos + glm::vec3(0.0f, 0.03f, 0.0f), 128.0f, ageNorm, 0.26f, 0.72f, 2.05f, 1.0f, -1.0f, 0.58f);
                continue;
            }

            float scale = std::max(1.0f, vfx.param / 3.4f);
            int burstCount = 30;
            for (int plume = 0; plume < burstCount; ++plume)
            {
                float norm = static_cast<float>(plume) / static_cast<float>(burstCount);
                float seedA = glm::fract(norm * 2.73f + vfx.param * 0.07f);
                float seedB = glm::fract(norm * 5.19f + vfx.worldPos.x * 0.03f + vfx.worldPos.z * 0.08f);
                float angle = norm * static_cast<float>(M_PI) * 2.0f + ageNorm * 4.5f + seedA;
                float radius = (0.30f + 2.05f * ageNorm + 0.30f * seedA) * scale;
                glm::vec3 pos = vfx.worldPos + glm::vec3(std::cos(angle) * radius, 0.18f + 0.55f * ageNorm + 0.25f * seedB, std::sin(angle) * radius);
                float sizePx = glm::mix(72.0f, 132.0f, seedA) * (1.20f - ageNorm * 0.28f);
                float brightness = glm::mix(1.20f, 1.92f, seedB) * (0.94f + life * 0.42f);
                float alphaScale = glm::mix(0.72f, 1.0f, seedA) * life;
                float lift = glm::mix(0.92f, 1.68f, seedB);
                float variant = glm::mix(0.12f, 0.92f, seedA * 0.6f + seedB * 0.4f);
                pushFireSprite(pos, sizePx, ageNorm, seedA, seedB, brightness, alphaScale, lift, variant);
            }

            for (int core = 0; core < 10; ++core)
            {
                float norm = static_cast<float>(core) / 10.0f;
                float seedA = glm::fract(0.11f + norm * 0.41f + ageNorm * 0.13f);
                float seedB = glm::fract(0.67f + norm * 0.23f + vfx.param * 0.05f);
                glm::vec3 pos = vfx.worldPos + glm::vec3((seedA - 0.5f) * 0.55f * scale, 0.10f + 0.48f * norm, (seedB - 0.5f) * 0.55f * scale);
                pushFireSprite(pos, 118.0f - 7.0f * static_cast<float>(core), ageNorm, seedA, seedB, 1.72f, 1.0f * life, 1.48f, 0.32f + norm * 0.28f);
            }

            float shockSize = (170.0f + 90.0f * ageNorm) * scale;
            float shockAlpha = (1.0f - ageNorm * 0.42f) * life;
            pushFireSprite(vfx.worldPos + glm::vec3(0.0f, 0.08f + 0.12f * ageNorm, 0.0f), shockSize, ageNorm, 0.21f, 0.81f, 2.25f, shockAlpha, -1.0f, 0.74f);
            pushFireSprite(vfx.worldPos + glm::vec3(0.0f, 0.14f + 0.18f * ageNorm, 0.0f), shockSize * 0.72f, ageNorm, 0.37f, 0.64f, 2.45f, shockAlpha * 0.86f, -1.0f, 0.36f);
        }

        for (const auto &field : m_burnFields)
        {
            float ageNorm = field.maxAge > 0.0f ? std::clamp(field.age / field.maxAge, 0.0f, 1.0f) : 1.0f;
            float life = 1.0f - ageNorm;
            if (life <= 0.0f)
                continue;

            int plumeCount = 24;
            float swirl = field.age * (0.8f + field.radius * 0.12f);
            for (int plume = 0; plume < plumeCount; ++plume)
            {
                float norm = static_cast<float>(plume) / static_cast<float>(plumeCount);
                float seedA = glm::fract(norm * 3.17f + field.radius * 0.13f + field.age * 0.09f);
                float seedB = glm::fract(norm * 5.93f + field.center.x * 0.07f + field.center.z * 0.11f);
                float ringBias = plume < 12 ? 1.0f : 0.0f;
                float radius = ringBias > 0.5f
                    ? field.radius * (0.68f + 0.16f * std::sin(swirl + norm * 6.28318f))
                    : field.radius * (0.18f + 0.34f * glm::fract(seedA * 7.0f + seedB * 5.0f));
                float angle = norm * static_cast<float>(M_PI) * 2.0f + swirl + seedB * 1.7f;
                glm::vec3 pos = field.center + glm::vec3(std::cos(angle) * radius, 0.18f + 0.06f * std::sin(field.age * 3.5f + norm * 9.0f), std::sin(angle) * radius);
                float sizePx = glm::mix(52.0f, 104.0f, ringBias > 0.5f ? 0.7f + 0.3f * seedA : 0.45f + 0.35f * seedB);
                sizePx *= glm::mix(0.85f, 1.25f, life);
                float brightness = glm::mix(1.15f, 1.72f, seedA) * (0.92f + 0.20f * std::sin(field.age * 8.0f + seedB * 21.0f));
                float alphaScale = glm::mix(0.76f, 1.0f, ringBias > 0.5f ? 1.0f : 0.6f);
                float lift = glm::mix(0.8f, 1.55f, ringBias > 0.5f ? seedB : seedA);
                float variant = glm::mix(0.18f, 0.88f, seedA * 0.65f + seedB * 0.35f);

                vertices.push_back({
                    pos,
                    glm::vec4(sizePx, ageNorm, seedA, seedB),
                    glm::vec4(brightness, alphaScale, lift, variant)
                });
            }
        }

        if (vertices.empty())
            return;

        glEnable(GL_BLEND);
        if (m_glBlendFunc)
            m_glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_PROGRAM_POINT_SIZE);
        if (m_glDepthMask)
            m_glDepthMask(GL_FALSE);

        glUseProgram(m_fireFieldShader);
        glUniformMatrix4fv(glGetUniformLocation(m_fireFieldShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(m_fireFieldShader, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
        if (m_glUniform3fv)
        {
            m_glUniform3fv(glGetUniformLocation(m_fireFieldShader, "uFogColor"), 1, glm::value_ptr(fogColor));
            m_glUniform3fv(glGetUniformLocation(m_fireFieldShader, "uCameraPos"), 1, glm::value_ptr(renderCamera));
        }
        if (m_glUniform1f)
        {
            m_glUniform1f(glGetUniformLocation(m_fireFieldShader, "uFogNear"), fogNear);
            m_glUniform1f(glGetUniformLocation(m_fireFieldShader, "uFogFar"), fogFar);
            m_glUniform1f(glGetUniformLocation(m_fireFieldShader, "uViewportHeight"), static_cast<float>(viewportHeight));
            m_glUniform1f(glGetUniformLocation(m_fireFieldShader, "uGlobalTime"), m_timeOfDaySystem.getTimeOfDay() * 360.0f);
        }

        glBindVertexArray(m_dashStarVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_dashStarVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(DashStarVertex)), vertices.data(), GL_DYNAMIC_DRAW);
        if (m_glDrawArrays)
            m_glDrawArrays(GL_POINTS, 0, static_cast<int>(vertices.size()));
        glBindVertexArray(0);

        if (m_glDepthMask)
            m_glDepthMask(GL_TRUE);
        if (m_glBlendFunc)
            m_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void VoxelScene::renderFireScreenEffect(int viewportWidth, int viewportHeight)
    {
        if (!m_fireScreenShader || !m_dashScreenVao || m_fireScreenOverlay <= 0.001f)
            return;

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        if (m_glBlendFunc)
            m_glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        glUseProgram(m_fireScreenShader);
        if (m_glUniform2f)
            m_glUniform2f(glGetUniformLocation(m_fireScreenShader, "uResolution"), static_cast<float>(viewportWidth), static_cast<float>(viewportHeight));
        if (m_glUniform1f)
        {
            m_glUniform1f(glGetUniformLocation(m_fireScreenShader, "uTime"), m_timeOfDaySystem.getTimeOfDay() * 360.0f);
            m_glUniform1f(glGetUniformLocation(m_fireScreenShader, "uIntensity"), m_fireScreenOverlay);
        }

        glBindVertexArray(m_dashScreenVao);
        if (m_glDrawArrays)
            m_glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        if (m_glBlendFunc)
            m_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        if (m_glCullFace)
            m_glCullFace(GL_BACK);
    }

    void VoxelScene::renderDashScreenEffect(int viewportWidth, int viewportHeight)
    {
        if (!m_dashScreenShader || !m_dashScreenVao || !m_dashGradientATexture || !m_dashGradientBTexture || m_dashScreenOverlay <= 0.001f)
            return;

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        if (m_glBlendFunc)
            m_glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        glUseProgram(m_dashScreenShader);
        if (m_glUniform2f)
            m_glUniform2f(glGetUniformLocation(m_dashScreenShader, "uResolution"), static_cast<float>(viewportWidth), static_cast<float>(viewportHeight));
        if (m_glUniform1f)
        {
            m_glUniform1f(glGetUniformLocation(m_dashScreenShader, "uTime"), m_timeOfDaySystem.getTimeOfDay() * 360.0f);
            m_glUniform1f(glGetUniformLocation(m_dashScreenShader, "uIntensity"), m_dashScreenOverlay);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_dashGradientATexture);
        glUniform1i(glGetUniformLocation(m_dashScreenShader, "uGradientA"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_dashGradientBTexture);
        glUniform1i(glGetUniformLocation(m_dashScreenShader, "uGradientB"), 1);

        glBindVertexArray(m_dashScreenVao);
        if (m_glDrawArrays)
            m_glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        if (m_glBlendFunc)
            m_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        if (m_glCullFace)
            m_glCullFace(GL_BACK);
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

    void VoxelScene::renderMiniMap()
    {
        if (!m_showMiniMap || !m_routeData.isValid())
            return;

        constexpr float kMapSize = 212.0f;
        constexpr float kPadding = 14.0f;
        constexpr float kHeaderH = 26.0f;
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({display.x - kMapSize - 18.0f, 236.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({kMapSize + kPadding * 2.0f, kMapSize + kPadding * 2.0f + kHeaderH}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.56f);
        ImGui::Begin("小地图", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

        ImGui::TextDisabled("路线与当前位置");
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        float cellSize = kMapSize / static_cast<float>(kRouteMapSize);

        for (int y = 0; y < kRouteMapSize; ++y)
        {
            for (int x = 0; x < kRouteMapSize; ++x)
            {
                auto tc = game::route::RouteData::terrainColor(m_routeData.terrain[y][x]);
                ImU32 fill = IM_COL32(tc.r, tc.g, tc.b, 165);
                glm::ivec2 cell{x, y};
                if (cell == m_routeData.objectiveCell)
                    fill = IM_COL32(255, 200, 70, 220);
                else if (cell == m_routeData.evacCell())
                    fill = IM_COL32(80, 210, 255, 210);
                else if (cell == m_routeData.startCell())
                    fill = IM_COL32(90, 225, 120, 210);

                ImVec2 min{origin.x + x * cellSize, origin.y + y * cellSize};
                ImVec2 max{min.x + cellSize - 1.0f, min.y + cellSize - 1.0f};
                dl->AddRectFilled(min, max, fill, 2.0f);
            }
        }

        for (int i = 0; i + 1 < static_cast<int>(m_routeData.path.size()); ++i)
        {
            glm::ivec2 a = m_routeData.path[static_cast<size_t>(i)];
            glm::ivec2 b = m_routeData.path[static_cast<size_t>(i + 1)];
            ImVec2 pa{origin.x + (a.x + 0.5f) * cellSize, origin.y + (a.y + 0.5f) * cellSize};
            ImVec2 pb{origin.x + (b.x + 0.5f) * cellSize, origin.y + (b.y + 0.5f) * cellSize};
            dl->AddLine(pa, pb, IM_COL32(255, 245, 130, 220), 2.0f);
        }

        glm::ivec2 playerCell = worldToRouteCell(m_cameraPos);
        ImVec2 playerPos{origin.x + (playerCell.x + 0.5f) * cellSize, origin.y + (playerCell.y + 0.5f) * cellSize};
        glm::vec3 forward = getPlayerModelForward();
        ImVec2 facingTip{playerPos.x + forward.x * cellSize * 0.9f, playerPos.y + forward.z * cellSize * 0.9f};
        dl->AddCircleFilled(playerPos, 4.0f, IM_COL32(255, 255, 255, 245));
        dl->AddCircle(playerPos, 6.0f, IM_COL32(70, 190, 255, 220), 18, 1.5f);
        dl->AddLine(playerPos, facingTip, IM_COL32(255, 120, 80, 220), 2.0f);
        dl->AddRect(origin, {origin.x + kMapSize, origin.y + kMapSize}, IM_COL32(120, 150, 210, 180), 4.0f, 0, 1.2f);
        ImGui::Dummy({kMapSize, kMapSize + 4.0f});
        ImGui::TextDisabled("白点=玩家  黄=目标  蓝=撤离");
        ImGui::End();
    }

    void VoxelScene::renderIntegratedHUD(const TargetBlock &target, int interactModelIndex)
    {
        ImVec2 display = ImGui::GetIO().DisplaySize;

        ImGui::SetNextWindowPos({display.x - 338.0f, 18.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({320.0f, 430.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.58f);
        ImGui::Begin("作战面板", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

        ImGui::TextColored({0.82f, 0.90f, 1.0f, 1.0f}, "状态与星技");
        ImGui::Separator();
        ImGui::Text("HP %.0f / %.0f", m_hp, m_maxHp);
        ImGui::ProgressBar(m_hp / std::max(m_maxHp, 1.0f), {-1.0f, 10.0f}, "");
        ImGui::Text("SE %.0f / %.0f", m_starEnergy, m_maxStarEnergy);
        ImGui::ProgressBar(m_starEnergy / std::max(m_maxStarEnergy, 1.0f), {-1.0f, 10.0f}, "");
        ImGui::Text("攻击 %.0f   防御 %.0f", m_attack, m_defense);
        ImGui::Text("时刻 %02d:%02d   %s", m_timeOfDaySystem.getHour24(), m_timeOfDaySystem.getMinute(), m_timeOfDaySystem.getPhaseName());
        ImGui::Text("天气 %s   怪物 %d", m_weatherSystem.getCurrentWeatherName(), static_cast<int>(m_monsters.size()));

        ImGui::Spacing();
        ImGui::SeparatorText("星技");
        for (size_t i = 0; i < m_starSockets.size(); ++i)
        {
            const auto &slot = m_starSockets[i];
            if (slot.isEmpty())
                continue;
            const auto *def = game::skill::getStarSkillDef(slot.item->id);
            if (!def)
                continue;
            float cooldownRatio = def->cooldown > 0.0f ? 1.0f - (m_skillCooldowns[i] / def->cooldown) : 1.0f;
            cooldownRatio = std::clamp(cooldownRatio, 0.0f, 1.0f);
            ImGui::Text("%d. %s %s", static_cast<int>(i + 1), slot.item->name.c_str(), skillKeyHint(def->effect));
            ImGui::ProgressBar(cooldownRatio, {-1.0f, 7.0f}, "");
        }

        if (m_showMiniMap && m_routeData.isValid())
        {
            ImGui::Spacing();
            ImGui::SeparatorText("战术地图");
            const float mapSize = 176.0f;
            ImDrawList *dl = ImGui::GetWindowDrawList();
            ImVec2 origin = ImGui::GetCursorScreenPos();
            float cellSize = mapSize / static_cast<float>(kRouteMapSize);
            for (int y = 0; y < kRouteMapSize; ++y)
            {
                for (int x = 0; x < kRouteMapSize; ++x)
                {
                    auto tc = game::route::RouteData::terrainColor(m_routeData.terrain[y][x]);
                    ImU32 fill = IM_COL32(tc.r, tc.g, tc.b, 165);
                    glm::ivec2 cell{x, y};
                    if (cell == m_routeData.objectiveCell)
                        fill = IM_COL32(255, 200, 70, 220);
                    else if (cell == m_routeData.evacCell())
                        fill = IM_COL32(80, 210, 255, 210);
                    else if (cell == m_routeData.startCell())
                        fill = IM_COL32(90, 225, 120, 210);
                    ImVec2 min{origin.x + x * cellSize, origin.y + y * cellSize};
                    ImVec2 max{min.x + cellSize - 1.0f, min.y + cellSize - 1.0f};
                    dl->AddRectFilled(min, max, fill, 2.0f);
                }
            }
            for (int i = 0; i + 1 < static_cast<int>(m_routeData.path.size()); ++i)
            {
                glm::ivec2 a = m_routeData.path[static_cast<size_t>(i)];
                glm::ivec2 b = m_routeData.path[static_cast<size_t>(i + 1)];
                ImVec2 pa{origin.x + (a.x + 0.5f) * cellSize, origin.y + (a.y + 0.5f) * cellSize};
                ImVec2 pb{origin.x + (b.x + 0.5f) * cellSize, origin.y + (b.y + 0.5f) * cellSize};
                dl->AddLine(pa, pb, IM_COL32(255, 245, 130, 220), 2.0f);
            }
            glm::ivec2 playerCell = worldToRouteCell(m_cameraPos);
            ImVec2 playerPos{origin.x + (playerCell.x + 0.5f) * cellSize, origin.y + (playerCell.y + 0.5f) * cellSize};
            glm::vec3 forward = getPlayerModelForward();
            ImVec2 facingTip{playerPos.x + forward.x * cellSize * 0.8f, playerPos.y + forward.z * cellSize * 0.8f};
            dl->AddCircleFilled(playerPos, 4.0f, IM_COL32(255, 255, 255, 245));
            dl->AddLine(playerPos, facingTip, IM_COL32(255, 120, 80, 220), 2.0f);
            dl->AddRect(origin, {origin.x + mapSize, origin.y + mapSize}, IM_COL32(120, 150, 210, 180), 4.0f, 0, 1.2f);
            ImGui::Dummy({mapSize, mapSize + 2.0f});
            ImGui::TextDisabled("白点=玩家 黄=目标 蓝=撤离");
        }

        ImGui::End();

        ImGui::SetNextWindowPos({18.0f, 18.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({320.0f, 276.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.54f);
        ImGui::Begin("任务与交互", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
        ImGui::Text("位置 %.1f %.1f %.1f", m_cameraPos.x, m_cameraPos.y, m_cameraPos.z);
        ImGui::Text("视角 %.1f / %.1f", m_yaw, m_pitch);
        ImGui::Text("星球 %s", game::route::RouteData::planetName(m_routeData.selectedPlanet));
        if (target.hit && m_highlightTargetBlock)
            ImGui::Text("目标方块 %d %d %d", target.block.x, target.block.y, target.block.z);
        else
            ImGui::TextDisabled("目标方块 <none>");

        if (m_routeData.isValid())
        {
            ImGui::SeparatorText("任务");
            ImGui::Text("出发点: %s", game::route::RouteData::cellLabel(m_routeData.startCell()).c_str());
            ImGui::Text("目标点: %s", game::route::RouteData::cellLabel(m_routeData.objectiveCell).c_str());
            ImGui::Text("撤离点: %s", game::route::RouteData::cellLabel(m_routeData.evacCell()).c_str());
            ImGui::Text("路线进度: %d / %d", std::max(m_routeProgressIndex + 1, 1), static_cast<int>(m_routeData.path.size()));
            ImGui::TextColored(m_routeObjectiveReached ? ImVec4(0.45f, 1.0f, 0.55f, 1.0f) : ImVec4(1.0f, 0.82f, 0.28f, 1.0f),
                m_routeObjectiveReached ? "目标矿区已抵达" : "先前往目标矿区");
        }

        if (interactModelIndex >= 0)
        {
            const auto &model = m_worldModels[static_cast<size_t>(interactModelIndex)];
            ImGui::SeparatorText("附近交互");
            ImGui::Text("%s", model.label.c_str());
            ImGui::TextDisabled("%s", model.prompt.c_str());
        }

        if (m_showInputHints && !m_showPauseMenu && !m_showSettlement)
        {
            ImGui::SeparatorText("操作");
            ImGui::TextDisabled("WASD 移动  Space 跳跃  Shift 快速下坠");
            ImGui::TextDisabled("鼠标视角  F3 切视角  ESC 暂停");
            ImGui::TextDisabled("左键攻击  右键填补  Q 技能  B 撤离");
            ImGui::TextDisabled("I 背包  O 设置");
        }
        ImGui::End();

        ImGui::SetNextWindowPos({(display.x - 422.0f) * 0.5f, display.y - 108.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({422.0f, 84.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.46f);
        ImGui::Begin("快捷栏", nullptr,
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
            ImGui::Button(label, {74.0f, 48.0f});
            ImGui::PopStyleColor(2);
        }
        ImGui::End();
    }

    // ── 星技球图标 ────────────────────────────────────────────────────────────
    static void vxlDrawStarGemSphereIcon(ImDrawList* dl, ImVec2 center, float r,
                                         const std::string& id, float alpha = 1.0f)
    {
        int a = static_cast<int>(alpha * 255.0f);
        if (a <= 0) return;
        ImU32 baseCol, midCol, rimCol, glowCol;
        const char* symbol = "\xe2\x98\x86"; // ☆
        if (id == "star_fire")
        {
            baseCol = IM_COL32(140, 40,  5, a); midCol = IM_COL32(230,110,  0, a);
            rimCol  = IM_COL32(255, 80,  0, a); glowCol = IM_COL32(255,140,  0, static_cast<int>(60*alpha));
            symbol  = "\xe7\x81\xab"; // 火
        }
        else if (id == "star_ice")
        {
            baseCol = IM_COL32( 10, 60,140, a); midCol = IM_COL32( 50,130,200, a);
            rimCol  = IM_COL32( 80,200,255, a); glowCol = IM_COL32( 80,200,255, static_cast<int>(60*alpha));
            symbol  = "\xe5\x86\xb0"; // 冰
        }
        else if (id == "star_wind")
        {
            baseCol = IM_COL32( 20,100, 50, a); midCol = IM_COL32( 50,180, 80, a);
            rimCol  = IM_COL32( 80,230,100, a); glowCol = IM_COL32( 80,230,100, static_cast<int>(60*alpha));
            symbol  = "\xe9\xa3\x8e"; // 风
        }
        else if (id == "star_light")
        {
            baseCol = IM_COL32(120,100,  5, a); midCol = IM_COL32(210,180, 20, a);
            rimCol  = IM_COL32(255,240, 60, a); glowCol = IM_COL32(255,240, 60, static_cast<int>(60*alpha));
            symbol  = "\xe5\x85\x89"; // 光
        }
        else if (id == "star_jump")
        {
            baseCol = IM_COL32( 60, 20,130, a); midCol = IM_COL32(130, 80,220, a);
            rimCol  = IM_COL32(190,130,255, a); glowCol = IM_COL32(180,100,255, static_cast<int>(60*alpha));
            symbol  = "\xe8\xb7\xb3"; // 跳
        }
        else
        {
            baseCol = IM_COL32( 60, 60, 80, a); midCol = IM_COL32( 90, 90,120, a);
            rimCol  = IM_COL32(160,160,200, a); glowCol = IM_COL32(160,160,200, static_cast<int>(40*alpha));
        }
        dl->AddCircle(center, r * 1.3f, glowCol, 32, 2.0f);
        dl->AddCircleFilled(center, r, baseCol);
        dl->AddCircleFilled({center.x - r*0.10f, center.y - r*0.14f}, r*0.70f, midCol);
        dl->AddCircleFilled({center.x - r*0.26f, center.y - r*0.30f}, r*0.34f,
                            IM_COL32(255,255,255, static_cast<int>(80*alpha)));
        dl->AddCircle(center, r, rimCol, 32, 1.5f);
        ImVec2 ts = ImGui::CalcTextSize(symbol);
        dl->AddText({center.x - ts.x*0.5f, center.y - ts.y*0.5f},
                    IM_COL32(255,255,255, a), symbol);
    }

    // ── 普通物品图标 ──────────────────────────────────────────────────────────
    static void vxlDrawItemIcon(ImDrawList* dl, ImVec2 bmin, ImVec2 bmax,
                                game::inventory::ItemCategory cat, int count)
    {
        float cx = (bmin.x + bmax.x) * 0.5f;
        float cy = (bmin.y + bmax.y) * 0.5f;
        float r  = std::min(bmax.x - bmin.x, bmax.y - bmin.y) * 0.32f;
        const char* sym = nullptr;
        ImU32 col = IM_COL32(160,160,160,200);
        switch (cat)
        {
        case game::inventory::ItemCategory::Weapon:
            col = IM_COL32(200,120, 60,220); sym = "\xe5\x88\x80"; break; // 刀
        case game::inventory::ItemCategory::Consumable:
            col = IM_COL32( 80,200,100,220); sym = "+"; break;
        case game::inventory::ItemCategory::Material:
            col = IM_COL32(100,180,240,220); sym = "\xe2\x97\x86"; break; // ◆
        default: break;
        }
        dl->AddRectFilled(bmin, bmax, IM_COL32(20,20,35,180), 4.0f);
        dl->AddCircleFilled({cx, cy - r*0.3f}, r, col);
        dl->AddCircle({cx, cy - r*0.3f}, r, IM_COL32(255,255,255,60), 20, 1.0f);
        if (sym)
        {
            ImVec2 ts = ImGui::CalcTextSize(sym);
            dl->AddText({cx - ts.x*0.5f, cy - r*0.3f - ts.y*0.5f},
                        IM_COL32(255,255,255,200), sym);
        }
        if (count > 1)
        {
            char buf[8]; snprintf(buf, sizeof(buf), "%d", count);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText({bmax.x - ts.x - 2.0f, bmax.y - ts.y - 1.0f},
                        IM_COL32(220,220,220,230), buf);
        }
    }

    void VoxelScene::renderInventoryUI()
    {
        if (!m_showInventory)
            return;

        constexpr int   COLS      = game::inventory::Inventory::COLS;
        constexpr int   ROWS      = game::inventory::Inventory::ROWS;
        constexpr float SLOT      = 52.0f;
        constexpr float GAP       = 4.0f;
        constexpr int   SK_COLS   = 2;
        constexpr int   SK_ROWS   = 3;
        constexpr int   SK_COUNT  = SK_COLS * SK_ROWS;
        constexpr float PANEL_GAP = 12.0f;

        const float INV_W   = COLS * SLOT + (COLS - 1) * GAP;
        const float SKILL_W = SK_COLS * SLOT + (SK_COLS - 1) * GAP;
        const float WIN_W   = INV_W + PANEL_GAP + SKILL_W + 20.0f;
        const float WIN_H   = ROWS * SLOT + (ROWS - 1) * GAP + 82.0f;

        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({(display.x - WIN_W) * 0.5f, (display.y - WIN_H) * 0.5f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({WIN_W, WIN_H}, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  {GAP, GAP});
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2.0f, 2.0f});

        ImGui::Begin(locale::T("inventory.title").c_str(), &m_showInventory,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

        if (ImGui::BeginTabBar("##inv_tabs"))
        {
            // ── 背包 + 技能格子 标签 ──────────────────────────────────────
            if (ImGui::BeginTabItem("背包"))
            {
                // 左：背包网格
                ImGui::BeginChild("##inv_grid", {INV_W, 0.0f}, false,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                for (int row = 0; row < ROWS; ++row)
                {
                    for (int col = 0; col < COLS; ++col)
                    {
                        if (col > 0) ImGui::SameLine();
                        int idx = row * COLS + col;
                        auto& slotRef = m_inventory.getSlot(idx);
                        bool is_weapon = !slotRef.isEmpty() &&
                            slotRef.item->category == game::inventory::ItemCategory::Weapon;
                        bool is_star   = !slotRef.isEmpty() &&
                            slotRef.item->category == game::inventory::ItemCategory::StarSkill;

                        ImGui::PushID(idx);
                        if (is_weapon)
                            ImGui::PushStyleColor(ImGuiCol_Button, {0.30f, 0.18f, 0.10f, 1.0f});
                        else if (is_star)
                            ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.22f, 0.38f, 1.0f});
                        else
                            ImGui::PushStyleColor(ImGuiCol_Button, {0.18f, 0.18f, 0.28f, 1.0f});
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.38f, 0.35f, 0.55f, 1.0f});

                        char label[48];
                        if (slotRef.isEmpty())
                            snprintf(label, sizeof(label), "##s%d", idx);
                        else if (is_weapon)
                        {
                            const auto* def = game::weapon::getWeaponDef(slotRef.item->id);
                            snprintf(label, sizeof(label), "%s##s%d",
                                def ? def->icon_label.c_str() : "[W]", idx);
                        }
                        else
                            snprintf(label, sizeof(label), "##s%d", idx);

                        if (ImGui::Button(label, {SLOT, SLOT}))
                        {
                            m_selectedInventorySlot = idx;
                            if (is_weapon)
                                m_weaponBar.equipFromInventory(m_weaponBar.getActiveIndex(), idx, m_inventory);
                        }

                        // 图标
                        if (!slotRef.isEmpty() && !is_weapon)
                        {
                            auto* idl = ImGui::GetWindowDrawList();
                            ImVec2 imin = ImGui::GetItemRectMin();
                            ImVec2 imax = ImGui::GetItemRectMax();
                            if (is_star)
                                vxlDrawStarGemSphereIcon(idl,
                                    {(imin.x+imax.x)*0.5f, (imin.y+imax.y)*0.5f},
                                    SLOT * 0.38f, slotRef.item->id, 1.0f);
                            else
                                vxlDrawItemIcon(idl, imin, imax,
                                    slotRef.item->category, slotRef.count);
                        }

                        // 右键星技珠 → 放入第一个空槽
                        if (is_star && ImGui::IsItemClicked(ImGuiMouseButton_Right))
                        {
                            for (auto& sk : m_starSockets)
                            {
                                if (sk.isEmpty())
                                {
                                    sk.item  = slotRef.item;
                                    sk.count = 1;
                                    slotRef.item.reset();
                                    slotRef.count = 0;
                                    break;
                                }
                            }
                        }

                        // 拖放源
                        if (!slotRef.isEmpty() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                        {
                            ImGui::SetDragDropPayload("INV_SLOT", &idx, sizeof(int));
                            ImGui::TextUnformatted(slotRef.item->name.c_str());
                            if (is_star)
                                ImGui::TextDisabled("右键快速装备 | 拖入技能格子");
                            ImGui::EndDragDropSource();
                        }

                        // 拖放目标：接受从技能槽拖回背包
                        if (slotRef.isEmpty() && ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("STAR_SLOT"))
                            {
                                int srcSk = *static_cast<const int*>(p->Data);
                                auto& srcSocket = m_starSockets[srcSk];
                                if (!srcSocket.isEmpty())
                                {
                                    slotRef.item  = srcSocket.item;
                                    slotRef.count = srcSocket.count;
                                    srcSocket.item.reset();
                                    srcSocket.count = 0;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        // Tooltip
                        if (!slotRef.isEmpty() && ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(slotRef.item->name.c_str());
                            if (is_weapon)
                            {
                                const auto* def = game::weapon::getWeaponDef(slotRef.item->id);
                                if (def)
                                {
                                    ImGui::Separator();
                                    ImGui::TextDisabled("%s: %d", locale::T("weapon.damage").c_str(), def->damage);
                                    ImGui::TextDisabled("%s: %.1f/s", locale::T("weapon.speed").c_str(), def->attack_speed);
                                }
                            }
                            else if (is_star)
                            {
                                ImGui::TextDisabled("星技珠子");
                                ImGui::Separator();
                                ImGui::TextDisabled("右键 → 装入技能格子");
                                ImGui::TextDisabled("拖拽 → 放到技能格子指定槽");
                            }
                            else
                                ImGui::TextDisabled("%s: %d / %d",
                                    locale::T("inventory.quantity").c_str(),
                                    slotRef.count, slotRef.item->max_stack);
                            ImGui::EndTooltip();
                        }

                        ImGui::PopStyleColor(2);
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild(); // ##inv_grid

                // 右：技能格子面板
                ImGui::SameLine(0.0f, PANEL_GAP);
                ImGui::BeginChild("##skill_panel", {SKILL_W, 0.0f}, false,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.90f, 1.0f, 1.0f));
                ImGui::TextUnformatted("\xe2\x98\x85 \xe6\x8a\x80\xe8\x83\xbd\xe6\xa0\xbc\xe5\xad\x90"); // ★ 技能格子
                ImGui::PopStyleColor();
                ImGui::Separator();
                ImGui::Spacing();

                auto* skillDL = ImGui::GetWindowDrawList();
                for (int sk = 0; sk < SK_COUNT; ++sk)
                {
                    if (sk % SK_COLS != 0) ImGui::SameLine();
                    auto& skSlot  = m_starSockets[sk];
                    bool  occupied = !skSlot.isEmpty();

                    ImGui::PushID(1000 + sk);
                    ImGui::PushStyleColor(ImGuiCol_Button,
                        occupied ? ImVec4(0.15f, 0.30f, 0.55f, 1.0f)
                                 : ImVec4(0.10f, 0.12f, 0.22f, 0.9f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.48f, 0.75f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.20f, 0.38f, 0.65f, 1.0f));

                    char skId[16];
                    snprintf(skId, sizeof(skId), "##sk%d", sk);
                    ImGui::Button(skId, {SLOT, SLOT});

                    {
                        ImVec2 bmin = ImGui::GetItemRectMin();
                        ImVec2 bmax = ImGui::GetItemRectMax();
                        float  bcx  = (bmin.x + bmax.x) * 0.5f;
                        float  bcy  = (bmin.y + bmax.y) * 0.5f;
                        if (occupied)
                            vxlDrawStarGemSphereIcon(skillDL, {bcx, bcy},
                                SLOT * 0.38f, skSlot.item->id, 1.0f);
                        else
                        {
                            char numBuf[4];
                            snprintf(numBuf, sizeof(numBuf), "%d", sk + 1);
                            ImVec2 ns = ImGui::CalcTextSize(numBuf);
                            skillDL->AddText({bcx - ns.x*0.5f, bcy - ns.y*0.5f},
                                IM_COL32(80, 100, 150, 140), numBuf);
                        }
                    }

                    // 右键取下
                    if (occupied && ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    {
                        m_inventory.addItem(*skSlot.item, skSlot.count);
                        skSlot.item.reset();
                        skSlot.count = 0;
                    }

                    // 拖放源
                    if (occupied && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                    {
                        ImGui::SetDragDropPayload("STAR_SLOT", &sk, sizeof(int));
                        ImGui::TextUnformatted(skSlot.item->name.c_str());
                        ImGui::TextDisabled("右键卸下 | 拖至背包 | 拖至其他槽换位");
                        ImGui::EndDragDropSource();
                    }

                    // 拖放目标
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("INV_SLOT"))
                        {
                            int srcIdx = *static_cast<const int*>(p->Data);
                            auto& src  = m_inventory.getSlot(srcIdx);
                            if (!src.isEmpty() &&
                                src.item->category == game::inventory::ItemCategory::StarSkill)
                            {
                                if (occupied)
                                    m_inventory.addItem(*skSlot.item, skSlot.count);
                                skSlot.item  = src.item;
                                skSlot.count = 1;
                                src.item.reset();
                                src.count = 0;
                            }
                        }
                        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("STAR_SLOT"))
                        {
                            int srcSk = *static_cast<const int*>(p->Data);
                            if (srcSk != sk)
                                std::swap(m_starSockets[srcSk], m_starSockets[sk]);
                        }
                        ImGui::EndDragDropTarget();
                    }

                    // Tooltip
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        if (occupied)
                        {
                            ImGui::TextUnformatted(skSlot.item->name.c_str());
                            ImGui::Separator();
                            ImGui::TextDisabled("右键 → 取下归还背包");
                            ImGui::TextDisabled("拖拽 → 移至其他槽位");
                        }
                        else
                        {
                            ImGui::TextDisabled("空技能槽 #%d", sk + 1);
                            ImGui::Separator();
                            ImGui::TextDisabled("右键背包中的星技珠装备");
                            ImGui::TextDisabled("或拖拽星技珠到此处");
                        }
                        ImGui::EndTooltip();
                    }

                    ImGui::PopStyleColor(3);
                    ImGui::PopID();
                }
                ImGui::EndChild(); // ##skill_panel
                ImGui::EndTabItem();
            }

            // ── 星技 标签（圆环视图） ─────────────────────────────────────
            if (ImGui::BeginTabItem("\xe6\x98\x9f\xe6\x8a\x80")) // 星技
            {
                constexpr int   COUNT  = 6;
                constexpr float RING_R = 130.0f;
                constexpr float SLOT_R = 28.0f;
                constexpr float CHAR_R = 48.0f;
                constexpr float PI_F   = 3.14159265f;

                auto* dl   = ImGui::GetWindowDrawList();
                ImVec2 avail  = ImGui::GetContentRegionAvail();
                ImVec2 origin = ImGui::GetCursorScreenPos();
                float cx = origin.x + avail.x * 0.5f;
                float cy = origin.y + avail.y * 0.5f;

                float sx[COUNT], sy[COUNT];
                for (int i = 0; i < COUNT; ++i)
                {
                    float angle = i * (2.0f * PI_F / COUNT) - PI_F * 0.5f;
                    sx[i] = cx + RING_R * cosf(angle);
                    sy[i] = cy + RING_R * sinf(angle);
                }

                // 连接线
                for (int i = 0; i < COUNT; ++i)
                    dl->AddLine({cx, cy}, {sx[i], sy[i]}, IM_COL32(60,100,160,100), 1.5f);

                // 中央圆
                dl->AddCircleFilled({cx, cy}, CHAR_R, IM_COL32(30, 50, 80, 230));
                dl->AddCircle({cx, cy}, CHAR_R, IM_COL32(100,180,255,200), 48, 2.5f);
                {
                    const char* lbl = "\xe8\xa7\x92\xe8\x89\xb2"; // 角色
                    ImVec2 ls = ImGui::CalcTextSize(lbl);
                    dl->AddText({cx - ls.x*0.5f, cy - ls.y*0.5f},
                                IM_COL32(180,220,255,255), lbl);
                }

                for (int i = 0; i < COUNT; ++i)
                {
                    auto& slot    = m_starSockets[i];
                    bool occupied = !slot.isEmpty();

                    ImU32 bgCol   = occupied ? IM_COL32(40, 80,140,230) : IM_COL32(20, 30, 55,200);
                    ImU32 ringCol = occupied ? IM_COL32(100,200,255,255) : IM_COL32(80,120,200,150);
                    dl->AddCircleFilled({sx[i], sy[i]}, SLOT_R, bgCol);
                    dl->AddCircle({sx[i], sy[i]}, SLOT_R, ringCol, 32, 2.0f);

                    char numBuf[4];
                    snprintf(numBuf, sizeof(numBuf), "%d", i + 1);
                    ImVec2 numSz = ImGui::CalcTextSize(numBuf);
                    dl->AddText({sx[i] - numSz.x*0.5f, sy[i] - SLOT_R + 4.0f},
                                IM_COL32(120,160,200,140), numBuf);

                    char btnId[16];
                    snprintf(btnId, sizeof(btnId), "##ss%d", i);
                    ImGui::SetCursorScreenPos({sx[i] - SLOT_R, sy[i] - SLOT_R});
                    ImGui::InvisibleButton(btnId, {SLOT_R * 2.0f, SLOT_R * 2.0f});

                    if (occupied && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                    {
                        ImGui::SetDragDropPayload("STAR_SLOT", &i, sizeof(int));
                        ImGui::TextUnformatted(slot.item->name.c_str());
                        ImGui::TextDisabled("\xe6\x8b\x96\xe8\x87\xb3\xe8\x83\x8c\xe5\x8c\x85\xe5\x8f\x96\xe4\xb8\x8b"); // 拖至背包取下
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("INV_SLOT"))
                        {
                            int srcIdx = *static_cast<const int*>(p->Data);
                            auto& src  = m_inventory.getSlot(srcIdx);
                            if (!src.isEmpty() &&
                                src.item->category == game::inventory::ItemCategory::StarSkill)
                            {
                                if (occupied)
                                    m_inventory.addItem(*slot.item, slot.count);
                                slot.item  = src.item;
                                slot.count = 1;
                                src.item.reset();
                                src.count = 0;
                            }
                        }
                        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("STAR_SLOT"))
                        {
                            int srcSocket = *static_cast<const int*>(p->Data);
                            if (srcSocket != i)
                                std::swap(m_starSockets[srcSocket], m_starSockets[i]);
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (occupied && ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(slot.item->name.c_str());
                        ImGui::TextDisabled("\xe6\x8b\x96\xe6\x8b\xbd\xe8\x87\xb3\xe8\x83\x8c\xe5\x8c\x85\xe5\x8f\xaf\xe5\x8f\x96\xe4\xb8\x8b"); // 拖拽至背包可取下
                        ImGui::EndTooltip();
                    }

                    if (occupied)
                    {
                        vxlDrawStarGemSphereIcon(dl, {sx[i], sy[i]}, SLOT_R * 0.82f,
                                                 slot.item->id, 1.0f);
                    }
                    else
                    {
                        const char* plus = "+";
                        ImVec2 ps = ImGui::CalcTextSize(plus);
                        dl->AddText({sx[i] - ps.x*0.5f, sy[i] - ps.y*0.5f},
                                    IM_COL32(80,120,200,160), plus);
                    }
                }

                ImGui::SetCursorScreenPos({origin.x, origin.y + avail.y - 2.0f});
                ImGui::Dummy({avail.x, 1.0f});
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();
        ImGui::TextDisabled("I: \xe8\x83\x8c\xe5\x8c\x85  O: \xe8\xae\xbe\xe7\xbd\xae  1-5: \xe5\x88\x87\xe6\xad\xa6\xe5\x99\xa8  Q: \xe6\x8a\x80\xe8\x83\xbd");
        ImGui::End();
        ImGui::PopStyleVar(2);
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
            static int fpsOptionIndex = 0;
            const int fpsOptions[] = {60, 120, 144};
            const char *fpsLabels[] = {"60 FPS", "120 FPS", "144 FPS"};
            const int configuredTargetFps = loadConfiguredTargetFps();
            for (int i = 0; i < 3; ++i)
            {
                if (configuredTargetFps == fpsOptions[i])
                {
                    fpsOptionIndex = i;
                    break;
                }
            }
            int weatherIndex = static_cast<int>(m_weatherSystem.getCurrentWeather());
            const char *weatherNames[] = {"晴天", "小雨", "中雨", "大雨", "雷雨"};
            if (ImGui::Combo("天气", &weatherIndex, weatherNames, IM_ARRAYSIZE(weatherNames)))
                m_weatherSystem.setWeather(static_cast<game::weather::WeatherType>(weatherIndex), 1.2f);
            if (ImGui::Combo("目标帧率", &fpsOptionIndex, fpsLabels, IM_ARRAYSIZE(fpsLabels)))
                saveConfiguredTargetFps(fpsOptions[fpsOptionIndex]);
            ImGui::SliderFloat("第三人称距离", &m_thirdPersonDistance, 3.2f, 8.5f, "%.1f");
            ImGui::Checkbox("显示小地图", &m_showMiniMap);
            ImGui::Checkbox("显示输入提示", &m_showInputHints);
            ImGui::Checkbox("显示目标提示", &m_highlightTargetBlock);
            ImGui::TextDisabled("帧率修改会写入配置文件，重启后生效。");
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
            ImGui::BulletText("WASD: 平移    Space: 跳跃    Shift: 快速下坠");
            ImGui::BulletText("鼠标: 视角    F3: 第一/第三人称");
            ImGui::BulletText("左键: 巨剑攻击    右键按住: 技能瞄准");
            ImGui::BulletText("瞄准中左键: 释放技能    B: 撤离交互");
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
        ImGui::TextDisabled("WASD 移动  Space 跳跃  Shift 快速下坠");
        ImGui::TextDisabled("鼠标视角  F3 切视角  ESC 暂停");
        ImGui::TextDisabled("左键攻击  右键按住瞄准技能  左键释放  B 撤离");
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
        bool interactKey = keys[SDL_SCANCODE_E];

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
            m_skillAimActive = false;
            m_prevInventoryKey = inventoryKey;
            m_prevSettingsKey = settingsKey;
            m_prevSkillKey = skillKey;
            m_prevPerspectiveKey = perspectiveKey;
            m_prevPauseKey = pauseKey;
            m_prevInteractKey = interactKey;
            m_prevLeftMouse = false;
            m_prevRightMouse = false;
            return;
        }

        if (m_showSettlement || m_showPauseMenu)
        {
            m_skillAimActive = false;
            m_prevInventoryKey = inventoryKey;
            m_prevSettingsKey = settingsKey;
            m_prevSkillKey = skillKey;
            m_prevPerspectiveKey = perspectiveKey;
            m_prevPauseKey = pauseKey;
            m_prevInteractKey = interactKey;
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
        bool canAimSkill = canAimAttackStarSkill();
        m_skillAimActive = !m_showInventory && rightDown && canAimSkill;
        bool evacuateKey = keys[SDL_SCANCODE_B];
        int interactModelIndex = findNearbyInteractableModel();

        glm::vec3 evacCenter = getCellWorldCenter(m_routeData.evacCell());
        bool inEvacZone = glm::distance(glm::vec2(m_cameraPos.x, m_cameraPos.z), glm::vec2(evacCenter.x, evacCenter.z)) <= kEvacInteractRadius;
        if (evacuateKey && inEvacZone && m_routeObjectiveReached)
        {
            m_showSettlement = true;
            updateMouseCapture();
        }

        if (interactKey && !m_prevInteractKey && interactModelIndex >= 0)
            interactWithModel(static_cast<size_t>(interactModelIndex));

        if (!m_showInventory && m_skillAimActive && leftDown && !m_prevLeftMouse)
        {
            if (triggerAimedAttackStarSkill(target))
                m_skillAimActive = false;
        }
        else if (!m_showInventory && leftDown && !m_prevLeftMouse)
        {
            performWeaponAttack(target);
        }
        if (!m_showInventory && target.hit && rightDown && !m_prevRightMouse && !canAimSkill)
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
        m_prevInteractKey = interactKey;
        m_prevLeftMouse = leftDown;
        m_prevRightMouse = rightDown;
    }

    void VoxelScene::renderOverlay(const TargetBlock &target, const glm::mat4 &proj, const glm::mat4 &view)
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
        ImGui::Text("WASD 移动  Space 跳跃  Shift 快速下坠");
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
        int interactModelIndex = findNearbyInteractableModel();
        if (interactModelIndex >= 0)
        {
            const auto &model = m_worldModels[static_cast<size_t>(interactModelIndex)];
            ImGui::Separator();
            ImGui::Text("设施: %s", model.label.c_str());
            ImGui::TextDisabled("%s", model.prompt.c_str());
        }
        ImGui::End();

        renderIntegratedHUD(target, interactModelIndex);
        renderInventoryUI();
        renderSettingsUI();

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
        const bool aiming = m_skillAimActive || m_skillAimBlend > 0.01f;
        const int aimedMonsterIndex = findAimedMonsterIndex();
        const bool highlightHit = target.hit || aimedMonsterIndex >= 0;
        const float reticleGap = 7.0f + 6.0f * (1.0f - m_skillAimBlend);
        const float reticleLen = 8.0f + 4.0f * m_skillAimBlend;
        const ImU32 reticleColor = highlightHit
            ? IM_COL32(255, 138, 72, 245)
            : (aiming ? IM_COL32(255, 210, 120, 240) : IM_COL32(255, 255, 255, 220));
        dl->AddLine({center.x - reticleGap - reticleLen, center.y}, {center.x - reticleGap, center.y}, reticleColor, 2.0f);
        dl->AddLine({center.x + reticleGap, center.y}, {center.x + reticleGap + reticleLen, center.y}, reticleColor, 2.0f);
        dl->AddLine({center.x, center.y - reticleGap - reticleLen}, {center.x, center.y - reticleGap}, reticleColor, 2.0f);
        dl->AddLine({center.x, center.y + reticleGap}, {center.x, center.y + reticleGap + reticleLen}, reticleColor, 2.0f);
        dl->AddCircleFilled(center, aiming ? 2.6f : 1.8f, reticleColor);

        if (aiming)
        {
            float aimCharge = std::clamp(m_skillAimBlend, 0.0f, 1.0f);
            dl->AddCircle(center, 28.0f, IM_COL32(255, 255, 255, 36), 48, 3.0f);
            dl->PathClear();
            dl->PathArcTo(center, 28.0f, -static_cast<float>(M_PI) * 0.5f, -static_cast<float>(M_PI) * 0.5f + static_cast<float>(M_PI) * 2.0f * aimCharge, 48);
            dl->PathStroke(IM_COL32(255, 170, 60, 235), 0, 3.5f);

            int fireSkillIndex = -1;
            const game::skill::StarSkillDef *fireSkillDef = nullptr;
            for (size_t i = 0; i < m_starSockets.size(); ++i)
            {
                const auto &slot = m_starSockets[i];
                if (slot.isEmpty())
                    continue;
                const auto *def = game::skill::getStarSkillDef(slot.item->id);
                if (def && def->effect == game::skill::SkillEffect::FireBlast)
                {
                    fireSkillIndex = static_cast<int>(i);
                    fireSkillDef = def;
                    break;
                }
            }

            float cooldownLeft = fireSkillIndex >= 0 ? m_skillCooldowns[static_cast<size_t>(fireSkillIndex)] : 0.0f;
            float cooldownRatio = 1.0f;
            if (fireSkillDef && fireSkillDef->cooldown > 0.0f)
                cooldownRatio = std::clamp(1.0f - cooldownLeft / fireSkillDef->cooldown, 0.0f, 1.0f);
            float energyRatio = std::clamp(m_starEnergy / std::max(m_maxStarEnergy, 1.0f), 0.0f, 1.0f);

            ImVec2 panelMin{center.x - 118.0f, center.y + 42.0f};
            ImVec2 panelMax{center.x + 118.0f, center.y + 96.0f};
            dl->AddRectFilled(panelMin, panelMax, IM_COL32(8, 14, 22, 168), 10.0f);
            dl->AddRect(panelMin, panelMax, IM_COL32(255, 185, 90, 120), 10.0f, 0, 1.2f);
            dl->AddText({panelMin.x + 14.0f, panelMin.y + 8.0f}, IM_COL32(255, 222, 150, 240), "炎焰瞄准");
            dl->AddText({panelMin.x + 14.0f, panelMin.y + 28.0f}, IM_COL32(230, 236, 245, 220), "按左键释放火球");
            dl->AddRectFilled({panelMin.x + 14.0f, panelMin.y + 48.0f}, {panelMin.x + 102.0f, panelMin.y + 55.0f}, IM_COL32(60, 78, 96, 180), 4.0f);
            dl->AddRectFilled({panelMin.x + 14.0f, panelMin.y + 48.0f}, {panelMin.x + 14.0f + 88.0f * aimCharge, panelMin.y + 55.0f}, IM_COL32(255, 166, 60, 220), 4.0f);
            dl->AddText({panelMin.x + 110.0f, panelMin.y + 43.0f}, IM_COL32(255, 214, 140, 220), "切镜");
            dl->AddRectFilled({panelMin.x + 150.0f, panelMin.y + 48.0f}, {panelMin.x + 206.0f, panelMin.y + 55.0f}, IM_COL32(60, 78, 96, 180), 4.0f);
            dl->AddRectFilled({panelMin.x + 150.0f, panelMin.y + 48.0f}, {panelMin.x + 150.0f + 56.0f * cooldownRatio, panelMin.y + 55.0f}, IM_COL32(90, 210, 255, 210), 4.0f);
            dl->AddText({panelMin.x + 14.0f, panelMin.y + 64.0f}, IM_COL32(255, 202, 110, 220), (std::string("星能 ") + std::to_string(static_cast<int>(m_starEnergy)) + "/" + std::to_string(static_cast<int>(m_maxStarEnergy))).c_str());
            dl->AddText({panelMin.x + 134.0f, panelMin.y + 64.0f}, IM_COL32(170, 225, 255, 220), (std::string("冷却 ") + (cooldownLeft > 0.0f ? std::to_string(static_cast<int>(std::ceil(cooldownLeft * 10.0f)) / 10.0f) : std::string("OK"))).c_str());
            dl->AddRectFilled({panelMin.x + 14.0f, panelMin.y + 82.0f}, {panelMin.x + 206.0f, panelMin.y + 88.0f}, IM_COL32(60, 78, 96, 180), 4.0f);
            dl->AddRectFilled({panelMin.x + 14.0f, panelMin.y + 82.0f}, {panelMin.x + 14.0f + 192.0f * energyRatio, panelMin.y + 88.0f}, IM_COL32(255, 196, 70, 210), 4.0f);

            glm::vec3 origin = getSkillCastOrigin();
            glm::vec3 targetPoint = target.hit
                ? glm::vec3(target.block) + glm::vec3(0.5f)
                : (aimedMonsterIndex >= 0
                    ? m_monsters[static_cast<size_t>(aimedMonsterIndex)].pos + glm::vec3(0.0f, monsterHalfExtents(m_monsters[static_cast<size_t>(aimedMonsterIndex)].type).y, 0.0f)
                    : m_cameraPos + getForward() * 18.0f);
            glm::vec3 delta = targetPoint - origin;
            glm::vec2 horizontalDelta{delta.x, delta.z};
            float horizontalDistance = glm::length(horizontalDelta);
            glm::vec2 horizontalDir = horizontalDistance > 0.001f
                ? horizontalDelta / horizontalDistance
                : glm::vec2(getForward().x, getForward().z);
            float flightTime = std::clamp(horizontalDistance / kFireProjectileHorizontalSpeed,
                                          kFireProjectileMinFlightTime,
                                          kFireProjectileMaxFlightTime);
            glm::vec3 velocity{
                horizontalDir.x * (horizontalDistance / std::max(flightTime, 0.001f)),
                0.0f,
                horizontalDir.y * (horizontalDistance / std::max(flightTime, 0.001f))
            };
            velocity.y = (delta.y + 0.5f * kFireProjectileGravity * flightTime * flightTime) / std::max(flightTime, 0.001f);

            ImVec2 prevPoint{};
            bool hasPrevPoint = false;
            const glm::vec2 displaySize = {ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y};
            for (int step = 0; step <= 18; ++step)
            {
                float t = flightTime * static_cast<float>(step) / 18.0f;
                glm::vec3 sample = origin + velocity * t + glm::vec3(0.0f, -0.5f * kFireProjectileGravity * t * t, 0.0f);
                ImVec2 point;
                if (!projectWorldToScreen(sample, proj, view, displaySize, point))
                    continue;
                if (hasPrevPoint)
                {
                    dl->AddLine(prevPoint, point, IM_COL32(255, 164, 58, 210), 2.2f);
                    dl->AddLine(prevPoint, point, IM_COL32(255, 236, 170, 96), 5.0f);
                }
                dl->AddCircleFilled(point, step == 18 ? 3.8f : 2.2f, step == 18 ? IM_COL32(255, 96, 46, 240) : IM_COL32(255, 210, 120, 180));
                prevPoint = point;
                hasPrevPoint = true;
            }

            const float impactRadius = 3.4f;
            glm::vec3 impactCenter = targetPoint;
            if (!target.hit && aimedMonsterIndex < 0)
            {
                int impactGroundY = findGroundY(static_cast<int>(std::floor(targetPoint.x)), static_cast<int>(std::floor(targetPoint.z)));
                impactCenter.y = static_cast<float>(std::max(impactGroundY + 1, 1));
            }

            ImVec2 firstImpactPoint{};
            ImVec2 previousImpactPoint{};
            bool hasFirstImpactPoint = false;
            bool hasPreviousImpactPoint = false;
            for (int segment = 0; segment <= 28; ++segment)
            {
                float angle = static_cast<float>(segment) / 28.0f * static_cast<float>(M_PI) * 2.0f;
                glm::vec3 ringPoint = impactCenter + glm::vec3(std::cos(angle) * impactRadius, 0.15f, std::sin(angle) * impactRadius);
                ImVec2 screenPoint;
                if (!projectWorldToScreen(ringPoint, proj, view, displaySize, screenPoint))
                {
                    hasPreviousImpactPoint = false;
                    continue;
                }
                if (!hasFirstImpactPoint)
                {
                    firstImpactPoint = screenPoint;
                    hasFirstImpactPoint = true;
                }
                if (hasPreviousImpactPoint)
                {
                    dl->AddLine(previousImpactPoint, screenPoint, IM_COL32(255, 120, 52, 220), 2.4f);
                    dl->AddLine(previousImpactPoint, screenPoint, IM_COL32(255, 212, 160, 80), 6.0f);
                }
                previousImpactPoint = screenPoint;
                hasPreviousImpactPoint = true;
            }
            if (hasFirstImpactPoint && hasPreviousImpactPoint)
            {
                dl->AddLine(previousImpactPoint, firstImpactPoint, IM_COL32(255, 120, 52, 220), 2.4f);
                dl->AddLine(previousImpactPoint, firstImpactPoint, IM_COL32(255, 212, 160, 80), 6.0f);
            }
        }

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

        // HD-2D: Ensure post-processing buffers match screen size, then render scene to FBO
        resizeHD2DFBOs(width, height);
        if (m_hd2dFBO && m_glBindFramebuffer)
            m_glBindFramebuffer(GL_FRAMEBUFFER, m_hd2dFBO);

        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 eye = getCameraEyePosition();
        glm::vec3 renderCamera = getRenderCameraPosition();
        glm::vec3 thirdPersonTarget = eye + glm::vec3(0.0f, -0.2f, 0.0f);
        glm::vec3 firstPersonTarget = eye + getForward() * 4.0f;
        glm::vec3 lookTarget = m_thirdPersonView
            ? glm::mix(thirdPersonTarget, firstPersonTarget, m_skillAimBlend)
            : firstPersonTarget;
        // Octopath HD-2D narrower FOV (diorama / stage feel)
        float fovDegrees = 55.0f - 10.0f * m_skillAimBlend;
        glm::mat4 proj = glm::perspective(glm::radians(fovDegrees), static_cast<float>(width) / static_cast<float>(std::max(height, 1)), 0.05f, 400.0f);
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
        renderStaticModels(proj, view, renderCamera, lightDir, fogColor, ambientStrength, diffuseStrength, fogNear, fogFar, flash);
        renderSkillEffects3D(proj, view, renderCamera, lightDir, fogColor, ambientStrength, diffuseStrength, fogNear, fogFar, flash);
        renderFireFieldEffects3D(proj, view, renderCamera, fogColor, fogNear, fogFar, height);
        renderDashStarEffects3D(proj, view, renderCamera, fogColor, fogNear, fogFar, height);
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
        glBindVertexArray(m_monsterVao);
        if (m_glDrawArrays && m_monsterVertexCount > 0)
            m_glDrawArrays(GL_TRIANGLES, 0, m_monsterVertexCount);
        if (m_thirdPersonView && !isSkillAimFirstPerson())
        {
            // HD-2D: replace 3D box geometry with 2D billboard sprite
            float spriteFlash = flash + (hasActiveSkillVisuals() ? 0.45f : 0.0f);
            renderPlayerSprite(proj, view, fogColor, fogNear, fogFar,
                               ambientStrength, diffuseStrength, lightDir, spriteFlash);
            glEnable(GL_CULL_FACE);
            if (m_glCullFace)
                m_glCullFace(GL_BACK);
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

        // HD-2D post-processing: bloom + tilt-shift DOF → composited to default framebuffer
        renderHD2DPostProcess(width, height);

        // Screen-space overlays applied on top of the composited image
        renderFireScreenEffect(width, height);
        renderDashScreenEffect(width, height);

        renderOverlay(raycastBlock(), proj, view);
    }

    void VoxelScene::clean()
    {
        if (m_shader)
        {
            glDeleteProgram(m_shader);
            m_shader = 0;
        }
        if (m_modelShader)
        {
            glDeleteProgram(m_modelShader);
            m_modelShader = 0;
        }
        if (m_dashStarShader)
        {
            glDeleteProgram(m_dashStarShader);
            m_dashStarShader = 0;
        }
        if (m_dashScreenShader)
        {
            glDeleteProgram(m_dashScreenShader);
            m_dashScreenShader = 0;
        }
        if (m_fireFieldShader)
        {
            glDeleteProgram(m_fireFieldShader);
            m_fireFieldShader = 0;
        }
        if (m_fireScreenShader)
        {
            glDeleteProgram(m_fireScreenShader);
            m_fireScreenShader = 0;
        }
        for (auto &[key, chunk] : m_chunkMeshes)
            releaseChunk(chunk);
        m_chunkMeshes.clear();
        for (auto &mesh : m_staticModelLibrary)
            releaseStaticModelMesh(mesh);
        m_staticModelLibrary.clear();
        for (auto &[path, texture] : m_modelTextures)
        {
            if (texture)
                glDeleteTextures(1, &texture);
        }
        m_modelTextures.clear();
        if (m_dashGradientATexture)
        {
            glDeleteTextures(1, &m_dashGradientATexture);
            m_dashGradientATexture = 0;
        }
        if (m_dashGradientBTexture)
        {
            glDeleteTextures(1, &m_dashGradientBTexture);
            m_dashGradientBTexture = 0;
        }
        m_worldModels.clear();
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
        if (m_effectVbo)
        {
            glDeleteBuffers(1, &m_effectVbo);
            m_effectVbo = 0;
        }
        if (m_dashStarVbo)
        {
            glDeleteBuffers(1, &m_dashStarVbo);
            m_dashStarVbo = 0;
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
        if (m_effectVao)
        {
            glDeleteVertexArrays(1, &m_effectVao);
            m_effectVao = 0;
        }
        if (m_dashStarVao)
        {
            glDeleteVertexArrays(1, &m_dashStarVao);
            m_dashStarVao = 0;
        }
        if (m_dashScreenVao)
        {
            glDeleteVertexArrays(1, &m_dashScreenVao);
            m_dashScreenVao = 0;
        }

        // HD-2D: sprite and post-processing GL resources
        if (m_playerSpriteTex)     { glDeleteTextures(1, &m_playerSpriteTex); m_playerSpriteTex = 0; }
        if (m_spriteShader)        { glDeleteProgram(m_spriteShader);       m_spriteShader = 0; }
        if (m_bloomExtractShader)  { glDeleteProgram(m_bloomExtractShader);  m_bloomExtractShader = 0; }
        if (m_bloomBlurShader)     { glDeleteProgram(m_bloomBlurShader);     m_bloomBlurShader = 0; }
        if (m_hd2dCompositeShader) { glDeleteProgram(m_hd2dCompositeShader); m_hd2dCompositeShader = 0; }
        if (m_spriteQuadVbo)       { glDeleteBuffers(1, &m_spriteQuadVbo);   m_spriteQuadVbo = 0; }
        if (m_spriteQuadVao)       { glDeleteVertexArrays(1, &m_spriteQuadVao); m_spriteQuadVao = 0; }
        if (m_fullQuadVao)         { glDeleteVertexArrays(1, &m_fullQuadVao); m_fullQuadVao = 0; }
        if (m_hd2dFBO)             { if (m_glDeleteFramebuffers)  m_glDeleteFramebuffers(1, &m_hd2dFBO);   m_hd2dFBO = 0; }
        if (m_hd2dColorTex)        { glDeleteTextures(1, &m_hd2dColorTex);  m_hd2dColorTex = 0; }
        if (m_hd2dDepthRbo)        { if (m_glDeleteRenderbuffers) m_glDeleteRenderbuffers(1, &m_hd2dDepthRbo); m_hd2dDepthRbo = 0; }
        if (m_bloomFBO[0])         { if (m_glDeleteFramebuffers)  m_glDeleteFramebuffers(2, m_bloomFBO);    m_bloomFBO[0] = m_bloomFBO[1] = 0; }
        if (m_bloomTex[0])         { glDeleteTextures(2, m_bloomTex);        m_bloomTex[0] = m_bloomTex[1] = 0; }

        if (m_glContext)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            m_glContext = nullptr;
        }
    }
}