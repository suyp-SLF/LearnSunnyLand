#pragma once
#include <vector>
#include <cstdint>

namespace game::weather
{
    // ──────────────────────────────────────────────
    // 天气类型
    // ──────────────────────────────────────────────
    enum class WeatherType : uint8_t
    {
        Clear        = 0,  // 晴天
        LightRain    = 1,  // 小雨
        MediumRain   = 2,  // 中雨
        HeavyRain    = 3,  // 大雨
        Thunderstorm = 4,  // 雷雨
    };

    // ──────────────────────────────────────────────
    // 单个雨滴粒子（屏幕坐标）
    // ──────────────────────────────────────────────
    struct RainParticle
    {
        float x, y;      // 屏幕坐标（像素）
        float speed;     // 垂直速度（像素/秒）
        float length;    // 条纹长度（像素）
        float alpha;     // 不透明度 0..1
        float depth;     // 0=远景 1=近景
    };

    // ──────────────────────────────────────────────
    // 雨水落地水花/涟漪粒子
    // ──────────────────────────────────────────────
    struct RainSplash
    {
        float x, y;        // 屏幕坐标
        float radius;      // 当前半径
        float maxRadius;   // 最大半径
        float age;         // 已存活时间
        float maxAge;      // 最大寿命
        float alpha;       // 当前透明度
        bool active = false;
    };

    struct ScreenRainDrop
    {
        float x, y;      // 屏幕坐标
        float vx, vy;    // 屏幕空间速度
        float length;    // 条纹长度
        float alpha;     // 不透明度 0..1
        float width;     // 线宽
    };

    struct LensRainDrop
    {
        float x, y;        // 屏幕坐标
        float radius;      // 主水珠半径
        float alpha;       // 不透明度 0..1
        float vy;          // 缓慢下滑速度
        float smear;       // 拖尾长度
        float age;         // 已存在时间
        float maxAge;      // 生命周期
    };

    // ──────────────────────────────────────────────
    // 天气系统
    //   update()  — 每帧调用（粒子物理、闪电计时等）
    //   render()  — 在 ImGui::NewFrame() 之后调用
    //               使用 GetBackgroundDrawList() 绘制在ImGui窗口之下
    // ──────────────────────────────────────────────
    class WeatherSystem
    {
    public:
        struct RuntimeState
        {
            WeatherType current = WeatherType::Clear;
            float intensity = 1.0f;
            float transitionTimer = 0.0f;
            float transitionDuration = 3.0f;
            bool isTransitioning = false;
            float lightningFlash = 0.0f;
            float lightningNextTime = 8.0f;
            float autoChangeTimer = 0.0f;
        };

        WeatherSystem();

        /** 每帧更新粒子位置、闪电计时、自动切换天气 */
        void update(float dt, float displayW, float displayH);

        /** 在 ImGui 帧内调用，通过背景 DrawList 绘制天气效果 */
        void render(float displayW, float displayH);
        void renderForeground(float displayW, float displayH);

        /**
         * @brief 切换天气
         * @param type          目标天气类型
         * @param transitionSec 过渡时间（秒）
         */
        void setWeather(WeatherType type, float transitionSec = 3.0f);

        void setScreenRainOverlayEnabled(bool enabled) { m_screenRainOverlayEnabled = enabled; }
        bool isScreenRainOverlayEnabled() const { return m_screenRainOverlayEnabled; }
        void setScreenRainOverlayStrength(float strength);
        float getScreenRainOverlayStrength() const { return m_screenRainOverlayStrength; }
        void setScreenRainMotionScale(float scale);
        float getScreenRainMotionScale() const { return m_screenRainMotionScale; }
        void setViewMotion(float vx, float vy)
        {
            m_viewMotionX = vx;
            m_viewMotionY = vy;
        }
        void setCameraState(float worldX, float worldY, float zoom, float verticalScale);

        WeatherType getCurrentWeather() const { return m_current; }
        float getSkyVisibility() const;

        /** 返回天气中文名 */
        static const char* getWeatherName(WeatherType t);
        const char* getCurrentWeatherName() const { return getWeatherName(m_current); }
        RuntimeState captureRuntimeState() const;
        void restoreRuntimeState(const RuntimeState& state);

        /** 设置地面在屏幕上的范围（ImGui 显示坐标，像素）
         *  minY = 走廊远端（背景侧），maxY = 走廊前沿（屏幕下方）
         *  雨水涟漪将在整个地面走廊范围内出现 */
        void setGroundScreenBand(float minY, float maxY)
        {
            m_groundMinScreenY = minY;
            m_groundScreenY    = maxY;
        }
        /** 向后兼容：仅设置前沿 Y（等同于只更新 maxY） */
        void setGroundScreenY(float y) { m_groundScreenY = y; }

        /** 自动天气切换周期（秒）；设为 0 则禁用自动切换 */
        float autoChangePeriod = 50.0f;

    private:
        WeatherType m_current  = WeatherType::Clear;

        // 过渡：从 0 淡入到 1.0
        float m_intensity        = 1.0f;  // 当前显示强度
        float m_transitionTimer  = 0.0f;
        float m_transitionDuration = 3.0f;
        bool  m_isTransitioning  = false;

        std::vector<RainParticle> m_particles;
        std::vector<RainSplash>   m_splashes;    // 地面水花涟漪
        std::vector<ScreenRainDrop> m_screenDrops;
        std::vector<LensRainDrop> m_lensDrops;
        float                     m_fogTime = 0.0f; // 雾气动画累计时间
        uint64_t m_rng;

        // 雷电
        float m_lightningFlash    = 0.0f;   // 0..1 闪光强度
        float m_lightningNextTime = 8.0f;   // 到下次雷电的倒计时

        // 自动切换计时
        float m_autoChangeTimer = 0.0f;

        // 雨斜度（水平移动 / 垂直移动 比值，负值向左倾斜）
        static constexpr float WIND_DX = -0.22f;
        bool  m_screenRainOverlayEnabled = false;
        float m_screenRainOverlayStrength = 1.0f;
        float m_screenRainMotionScale = 1.0f;
        float m_viewMotionX      = 0.0f;
        float m_viewMotionY      = 0.0f;
        float m_cameraWorldX     = 0.0f;
        float m_cameraWorldY     = 0.0f;
        float m_prevCameraWorldX = 0.0f;
        float m_prevCameraWorldY = 0.0f;
        float m_cameraZoom       = 1.0f;
        float m_cameraVerticalScale = 1.0f;
        bool  m_hasCameraState   = false;
        float m_groundScreenY    = -1.0f;  // 地面走廊前沿屏幕 Y（最大 Y，靠近玩家）
        float m_groundMinScreenY = -1.0f;  // 地面走廊远端屏幕 Y（最小 Y，靠近背景）
        // ── 内部查询 ──
        int   targetParticleCount() const;
        int   targetScreenDropCount() const;
        int   targetLensDropCount() const;
        float particleSpeed()  const;
        float particleLength() const;
        float particleAlpha()  const;
        float getDimAlpha()    const;

        // ── RNG ──
        uint64_t nextRand();
        float    randFloat();  // [0, 1)

        // ── 粒子生命周期 ──
        void spawnParticle(float displayW, float displayH);
        void respawnParticle(RainParticle &p, float displayW, float displayH);
        void spawnScreenDrop(float displayW, float displayH);
        void respawnScreenDrop(ScreenRainDrop &drop, float displayW, float displayH, bool topOnly);
        void spawnLensDrop(float displayW, float displayH);
        void respawnLensDrop(LensRainDrop &drop, float displayW, float displayH, bool topOnly);
        void emitSplash(float x, float y);
    };

} // namespace game::weather
