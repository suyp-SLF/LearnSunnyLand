#pragma once
#include "frame_editor.h"
#include "state_machine_editor.h"
#include "../statemachine/state_controller.h"
#include "../../engine/scene/scene.h"
#include "../../engine/world/chunk_manager.h"
#include "../../engine/world/world_config.h"
#include "../../engine/physics/physics_manager.h"
#include "../../engine/actor/actor_manager.h"
#include "../../engine/render/text_renderer.h"
#include "../../engine/ecs/registry.h"
#include "../inventory/inventory.h"
#include "../weapon/weapon.h"
#include "../monster/monster_manager.h"
#include "../world/tree_manager.h"
#include "../world/time_of_day_system.h"
#include "../weather/weather_system.h"
#include "../mission/planet_mission_ui.h"
#include "../route/route_data.h"
#include "../skill/star_skill.h"
#include "../component/attribute_component.h"
#include <array>
#include <string>
#include <vector>

namespace engine::object
{
    class GameObject;
}

namespace game::scene
{
    class GameScene : public engine::scene::Scene
    {
    public:
        GameScene(const std::string &name,
                  engine::core::Context &context,
                  engine::scene::SceneManager &sceneManager,
                  game::route::RouteData routeData = {});

        void init() override;
        void update(float delta_time) override;
        void render() override;
        void handleInput() override;
        void clean() override;

    private:
        std::unique_ptr<engine::world::ChunkManager> chunk_manager;
        std::unique_ptr<engine::physics::PhysicsManager> physics_manager;
        std::unique_ptr<engine::actor::ActorManager> actor_manager;
        engine::render::TextRenderer* text_renderer = nullptr;
        engine::ecs::Registry ecs_registry;

        engine::object::GameObject* m_player = nullptr;

        SDL_GPUTexture* m_textTexture = nullptr;
        SDL_GLContext m_glContext = nullptr;

        // 缩放滑块
        float m_zoomSliderValue = 9.0f;   // DNF 默认缩放

        // ── 设置界面：内存历史折线图 ──────────────────────────────────────────
        static constexpr int kMemHistoryLen = 128;   // 环形缓冲长度（帧/采样点）
        std::array<float, kMemHistoryLen> m_rssHistory{};   // RSS（MB）历史
        int   m_rssHistoryIdx   = 0;         // 下一写入位置
        float m_rssHistoryTimer = 0.0f;      // 距上次采样经过秒数
        float m_rssPeakMB       = 0.0f;      // 历史峰值

        // ── 设置界面：FPS 历史折线图 ──────────────────────────────────────────
        std::array<float, kMemHistoryLen> m_fpsHistory{};  // 实时 FPS 历史
        int   m_fpsHistoryIdx   = 0;         // 下一写入位置
        float m_fpsHistoryTimer = 0.0f;      // 距上次采样经过秒数（每 0.25s 一次）
        float m_fpsPeak         = 0.0f;      // 历史峰值
        int   m_maxFpsSlider    = 60;        // 目标帧率设置（0 = 不限）

        // ── 设置界面：粒子效果档位 ────────────────────────────────────────────
        enum class UiParticleLevel { None = 0, Low, Medium, High };
        UiParticleLevel m_uiParticleLevel = UiParticleLevel::Low;

        // 设置界面背景漂浮尘埃粒子
        struct UiDust {
            float x, y;           // 屏幕归一化坐标 [0,1]
            float vy;             // 上升速度（归一化/秒）
            float alpha;          // 当前透明度
            float size;           // 像素大小
            uint8_t r, g, b;      // 颜色
        };
        std::vector<UiDust> m_uiDusts;
        glm::vec2 m_lastChunkUpdatePos = {-99999.0f, -99999.0f};
        glm::vec2 m_lastMouseLogicalPos = {0.0f, 0.0f};
        glm::vec2 m_lastMouseWorldPos = {0.0f, 0.0f};
        glm::vec2 m_lastHoveredTileCenter = {0.0f, 0.0f};
        glm::vec2 m_lastAttackSkillTarget = {0.0f, 0.0f};
        glm::ivec2 m_hoveredTile = {0, 0};
        bool m_hasHoveredTile = false;
        bool m_hasLastAttackSkillTarget = false;
        bool m_showActiveChunkHighlights = false;
        bool m_showSkillDebugOverlay = false;
        bool m_showPhysicsDebug = false;
        bool m_showFpsOverlay = true;   // 由 config.json performance.show_fps 控制
        bool m_invertPlayerFacing = false;
        bool m_showSettings = false;
        bool m_devMode = false;         // 开发模式：显示地形/物理调试覆盖层

        // 背包系统
        game::inventory::Inventory m_inventory;
        bool m_showInventory = false;

        // 星技槽：6 个圆形槽，只接受 StarSkill 类型物品
        std::array<game::inventory::InventorySlot, 6> m_starSockets;

        // 星技技能状态
        std::array<float, 6> m_skillCooldowns{};   // 各槽冷却计时（秒）
        bool m_windStarEquipped = false;            // 上帧疾风星技装备状态（防止每帧重设）

        // 技能特效粒子列表
        struct SkillVFX
        {
            game::skill::SkillEffect type;
            glm::vec2 worldPos;
            float age    = 0.0f;   // 当前已存在时间（秒）
            float maxAge = 0.6f;   // 总持续时间
            float param  = 0.0f;   // 附加参数（如冲刺方向）
        };
        std::vector<SkillVFX> m_skillVfxList;

        struct SkillProjectile
        {
            game::skill::SkillEffect type;
            glm::vec2 originPos;
            glm::vec2 worldPos;
            glm::vec2 lastWorldPos;
            glm::vec2 targetPos;
            glm::vec2 velocity;
            float age = 0.0f;
            float maxAge = 0.0f;
            float radius = 0.0f;
        };
        std::vector<SkillProjectile> m_skillProjectiles;

        struct SlashVFX
        {
            glm::vec2 worldPos;
            float facing = 1.0f;
            float age = 0.0f;
            float maxAge = 0.18f;
            float radius = 0.0f;
        };
        std::vector<SlashVFX> m_slashVfxList;

        struct CombatFragment
        {
            glm::vec2 worldPos;
            glm::vec2 velocity;
            float age = 0.0f;
            float maxAge = 0.0f;
            float size = 0.0f;
        };
        std::vector<CombatFragment> m_combatFragments;

        // 武器栏
        game::weapon::WeaponBar m_weaponBar;
        float m_weaponAttackCooldown = 0.0f;

        // 怪物系统
        std::unique_ptr<game::monster::MonsterManager> m_monsterManager;

        // 树木系统
        game::world::TreeManager m_treeManager;

        // 天气系统
        game::weather::WeatherSystem m_weatherSystem;

        // 昼夜与天空系统
        game::world::TimeOfDaySystem m_timeOfDaySystem;

        // 星球任务规划 UI
        game::mission::PlanetMissionUI m_missionUI;

        // 路线数据
        game::route::RouteData m_routeData;
        int                    m_currentZone = 0;

        // 星球（世界）参数
        engine::world::WorldConfig m_worldConfig;

        void createTestObject();
        void testCamera();
        void createPlayer();
        void renderUI();
        void updateTextTexture();
        void renderInventoryUI();
        void renderStarSocketPage();
        void renderSkillHUD();           // 屏幕底部技能冷却 HUD
        void renderPlayerStatusHUD();    // 左上角 HP / 星能 / 属性面板
        void tickSkillVFX(float dt);     // 推进技能特效寿命
        void tickSkillProjectiles(float dt);
        void tickCombatEffects(float dt);
        void renderSkillVFX();           // 渲染技能特效（前景层）
        void renderSkillProjectiles();
        void renderCombatEffects();
        void renderSkillDebugOverlay();  // 临时调试：技能目标与特效对齐
        void renderWeaponBar();
        void renderDropItems();
        void renderPlayerStateTag();
        void renderActorGroundShadows();
        void syncPlayerPresentation();
        void renderPerformanceOverlay() const;
        void renderCommandTerminal();
        void renderSettingsPage();
        void updateSettingsParticles(float dt);  // 设置界面粒子更新（每帧调用）
        void renderMechPrompt();
        void renderRouteHUD();    // 左下角路线 HUD
        void renderSettlementUI(); // 撤离结算界面
        void injectOreVeins();    // 在目标格区域注入矿脉
        void initTestItems();
        void executeCommand();
        void spawnMechDrop();
        void tryEnterMech();
        void exitMech();
        void performMeleeAttack(glm::vec2 targetPos);
        void performMechAttack();
        void tickStarSkillPassives(float dt);          // 被动星技每帧更新
        void triggerAttackStarSkills(glm::vec2 pos);  // 攻击触发型星技
        void triggerActiveStarSkills();               // 主动技能（Q键）
        void explodeFireBlast(glm::vec2 pos, float radius);
        engine::object::GameObject* getControlledActor() const;
        glm::vec2 getActorWorldPosition(const engine::object::GameObject* actor) const;
        glm::vec2 getPlayerCastOrigin(glm::vec2 targetPos) const;
        glm::vec2 findSafeDisembarkPosition() const;

        // 撤离结算状态
        bool m_showSettlement = false;
        bool m_showCommandInput = false;
        bool m_focusCommandInput = false;
        bool m_isPlayerInMech = false;
        std::array<char, 16> m_commandBuffer{};
        engine::object::GameObject* m_mech = nullptr;
        float m_mechAttackCooldown = 0.0f;
        float m_mechAttackFlashTimer = 0.0f;
        int m_mechLastAttackHits = 0;
        // 玩家帧动画由 AnimationComponent 管理，此处不再保留手动计时变量

        // 机甲精灵表动画
        int   m_mechAnimFrame  = 0;
        float m_mechAnimTimer  = 0.0f;
        int   m_mechAnimRow    = 1;   // 0=步行 1=待机/瞄准 2=射击
        float m_mechShootTimer = 0.0f;

        // DNF 卷轴战斗：冲刺系统
        bool  m_isDashing       = false;
        float m_dashTimer       = 0.0f;   // 当前冲刺剩余时长
        float m_dashCooldown    = 0.0f;   // 冲刺冷却剩余时长
        float m_dashFacing      = 1.0f;   // 冲刺方向 (+1=右 -1=左)
        bool  m_dashKeyWas      = false;  // 上帧 Shift 状态（边沿检测）

        // DNF 卷轴战斗：连击系统
        int   m_comboCount      = 0;      // 当前连击数 (0‑2)
        float m_comboResetTimer = 0.0f;   // 连击窗口倒计时

        // DNF 双击跑步检测
        float m_doubleTapTimer   = 0.0f;  // 200ms 窗口倒计时
        int   m_doubleTapLastDir = 0;     // 上次单击方向 (-1/0/1)

        // ── Gundam 攻击动画状态机 ──────────────────────────────
        // 连招动画队列：Z键/左键鼠标 触发，每轮 attack_a → attack_b → attack_c 循环
        int   m_attackComboStep   = 0;    // 0/1/2 对应 a/b/c 连招
        float m_attackAnimTimer   = 0.0f; // 当前攻击动画剩余时间（>0 表示正在播放攻击动画）
        bool  m_attackInputQueued = false;// 攻击动画播放中按下攻击，等待连招

        // 重击/大招变量
        float m_heavyAttackTimer  = 0.0f; // >0 = 重击动画播放中
        float m_ultimateTimer     = 0.0f; // >0 = 大招动画播放中
        float m_cannonTimer       = 0.0f; // >0 = 炮击动画播放中

        // 攻击动画持续时间常量（秒）
        static constexpr float kAttackADur  = 8 * 0.07f; // attack_a: 8帧 @ 70ms
        static constexpr float kAttackBDur  = 8 * 0.07f;
        static constexpr float kAttackCDur  = 8 * 0.07f;
        static constexpr float kAttackDDur  = 8 * 0.06f;
        static constexpr float kCannonDur   = 8 * 0.08f;
        static constexpr float kUltimateDur = 8 * 0.10f;

        // ── 帧编辑器 ──────────────────────────────────────────────────
        FrameEditor m_frameEditor;

        // ── 状态机编辑器 ──────────────────────────────────────────────
        StateMachineEditor m_smEditor;

        // ── 开发模式：角色选择器 ─────────────────────────────────────────
        struct CharacterEntry {
            std::string id;           // e.g. "gundom"
            std::string displayName;  // e.g. "冈达姆"
            std::string smPath;       // e.g. "assets/textures/Characters/gundom.sm.json"
        };
        std::vector<CharacterEntry> m_characters;
        int m_selectedCharacter = 0;

        // ── 玩家状态机控制器 ─────────────────────────────────────────────
        // 用法：
        //  1. 在 init() 或切换角色时调用 loadPlayerSM("xxx.sm.json")
        //  2. 在 update() 中调用 tickPlayerSM(dt)：
        //     - 根据按键/物理状态构建 activeInputs
        //     - 调用 m_playerSM.update() 并应用根位移/帧事件
        game::statemachine::StateController m_playerSM;
        game::statemachine::StateMachineData m_playerSMData;
        std::string m_playerSMPath;
        bool   m_playerSMLoaded  = false;
        bool   m_prevGrounded    = true;   // 上帧落地状态（用于检测 LAND 事件）

        void loadPlayerSM(const std::string& smJsonPath);  // 加载并 init
        void tickPlayerSM(float dt);                        // 每帧驱动状态机

        void scanCharacters();        // 扫描 *.sm.json 填充 m_characters
        void renderDevModeOverlay();  // 右上角角色选择覆盖层（仅 dev 模式）
    }; // class GameScene
} // namespace game::scene