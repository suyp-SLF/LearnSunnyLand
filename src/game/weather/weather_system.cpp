#include "weather_system.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace game::weather
{
    WeatherSystem::WeatherSystem()
        : m_rng(0xDEADBEEF12345678ULL)
    {
        m_autoChangeTimer = autoChangePeriod;
        // 预留容量避免运行时反复内存分配
        m_particles.reserve(500);
        m_splashes.reserve(80);
        m_screenDrops.reserve(64);
        m_lensDrops.reserve(24);
    }

    void WeatherSystem::setScreenRainOverlayStrength(float strength)
    {
        m_screenRainOverlayStrength = std::clamp(strength, 0.0f, 2.0f);
    }

    void WeatherSystem::setScreenRainMotionScale(float scale)
    {
        m_screenRainMotionScale = std::clamp(scale, 0.0f, 3.0f);
    }

    void WeatherSystem::setCameraState(float worldX, float worldY, float zoom, float verticalScale)
    {
        if (!m_hasCameraState)
        {
            m_prevCameraWorldX = worldX;
            m_prevCameraWorldY = worldY;
            m_hasCameraState = true;
        }

        m_cameraWorldX = worldX;
        m_cameraWorldY = worldY;
        m_cameraZoom = std::max(zoom, 0.01f);
        m_cameraVerticalScale = std::max(verticalScale, 0.01f);
    }

    // ──────────────────────────────────────────────
    // RNG
    // ──────────────────────────────────────────────
    uint64_t WeatherSystem::nextRand()
    {
        m_rng ^= m_rng << 13;
        m_rng ^= m_rng >> 7;
        m_rng ^= m_rng << 17;
        return m_rng;
    }

    float WeatherSystem::randFloat()
    {
        return static_cast<float>(nextRand() & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    // ──────────────────────────────────────────────
    // 天气参数查询
    // ──────────────────────────────────────────────
    int WeatherSystem::targetParticleCount() const
    {
        switch (m_current)
        {
            case WeatherType::Clear:        return 0;
            case WeatherType::LightRain:    return 100;
            case WeatherType::MediumRain:   return 240;
            case WeatherType::HeavyRain:    return 380;
            case WeatherType::Thunderstorm: return 500;
        }
        return 0;
    }

    float WeatherSystem::particleSpeed() const
    {
        switch (m_current)
        {
            case WeatherType::LightRain:    return 340.0f;
            case WeatherType::MediumRain:   return 560.0f;
            case WeatherType::HeavyRain:    return 780.0f;
            case WeatherType::Thunderstorm: return 980.0f;
            default: return 0.0f;
        }
    }

    float WeatherSystem::particleLength() const
    {
        switch (m_current)
        {
            case WeatherType::LightRain:    return 8.0f;
            case WeatherType::MediumRain:   return 14.0f;
            case WeatherType::HeavyRain:    return 20.0f;
            case WeatherType::Thunderstorm: return 25.0f;
            default: return 0.0f;
        }
    }

    float WeatherSystem::particleAlpha() const
    {
        switch (m_current)
        {
            case WeatherType::LightRain:    return 0.35f;
            case WeatherType::MediumRain:   return 0.55f;
            case WeatherType::HeavyRain:    return 0.72f;
            case WeatherType::Thunderstorm: return 0.85f;
            default: return 0.0f;
        }
    }

    // 天空暗化 alpha（阴天效果）
    float WeatherSystem::getDimAlpha() const
    {
        switch (m_current)
        {
            case WeatherType::Clear:        return 0.0f;
            case WeatherType::LightRain:    return 0.12f;
            case WeatherType::MediumRain:   return 0.25f;
            case WeatherType::HeavyRain:    return 0.40f;
            case WeatherType::Thunderstorm: return 0.55f;
        }
        return 0.0f;
    }

    int WeatherSystem::targetScreenDropCount() const
    {
        if (!m_screenRainOverlayEnabled)
            return 0;

        const float density = std::max(0.1f, m_screenRainOverlayStrength);
        switch (m_current)
        {
            case WeatherType::Clear:        return 0;
            case WeatherType::LightRain:    return static_cast<int>(10.0f * density);
            case WeatherType::MediumRain:   return static_cast<int>(18.0f * density);
            case WeatherType::HeavyRain:    return static_cast<int>(28.0f * density);
            case WeatherType::Thunderstorm: return static_cast<int>(40.0f * density);
        }
        return 0;
    }

    int WeatherSystem::targetLensDropCount() const
    {
        if (!m_screenRainOverlayEnabled)
            return 0;

        const float density = std::max(0.1f, m_screenRainOverlayStrength);
        switch (m_current)
        {
            case WeatherType::Clear:        return 0;
            case WeatherType::LightRain:    return static_cast<int>(4.0f * density);
            case WeatherType::MediumRain:   return static_cast<int>(7.0f * density);
            case WeatherType::HeavyRain:    return static_cast<int>(10.0f * density);
            case WeatherType::Thunderstorm: return static_cast<int>(14.0f * density);
        }
        return 0;
    }

    float WeatherSystem::getSkyVisibility() const
    {
        float baseVisibility = 1.0f;
        switch (m_current)
        {
            case WeatherType::Clear:        baseVisibility = 1.0f; break;
            case WeatherType::LightRain:    baseVisibility = 0.82f; break;
            case WeatherType::MediumRain:   baseVisibility = 0.62f; break;
            case WeatherType::HeavyRain:    baseVisibility = 0.42f; break;
            case WeatherType::Thunderstorm: baseVisibility = 0.28f; break;
        }
        return 1.0f - (1.0f - baseVisibility) * m_intensity;
    }

    // ──────────────────────────────────────────────
    // 粒子生成
    // ──────────────────────────────────────────────
    void WeatherSystem::spawnParticle(float displayW, float displayH)
    {
        RainParticle p;
        p.depth  = randFloat();
        p.x      = randFloat() * (displayW + 120.0f) - 60.0f;
        p.y      = randFloat() * -displayH;  // 从屏幕上方随机位置开始
        const float depthScale = 0.55f + p.depth * 0.85f;
        p.speed  = particleSpeed()  * depthScale * (0.78f + randFloat() * 0.36f);
        p.length = particleLength() * (0.60f + p.depth * 0.95f) * (0.72f + randFloat() * 0.50f);
        p.alpha  = particleAlpha()  * (0.32f + p.depth * 0.68f) * (0.45f + randFloat() * 0.45f) * m_intensity;
        m_particles.push_back(p);
    }

    void WeatherSystem::respawnParticle(RainParticle &p, float displayW, float /*displayH*/)
    {
        p.depth  = randFloat();
        p.x      = randFloat() * (displayW + 120.0f) - 60.0f;
        const float depthScale = 0.55f + p.depth * 0.85f;
        p.speed  = particleSpeed()  * depthScale * (0.78f + randFloat() * 0.36f);
        p.length = particleLength() * (0.60f + p.depth * 0.95f) * (0.72f + randFloat() * 0.50f);
        p.y      = -p.length - randFloat() * 30.0f;
        p.alpha  = particleAlpha()  * (0.32f + p.depth * 0.68f) * (0.45f + randFloat() * 0.45f) * m_intensity;
    }

    void WeatherSystem::spawnScreenDrop(float displayW, float displayH)
    {
        ScreenRainDrop drop;
        respawnScreenDrop(drop, displayW, displayH, false);
        m_screenDrops.push_back(drop);
    }

    void WeatherSystem::respawnScreenDrop(ScreenRainDrop &drop, float displayW, float displayH, bool topOnly)
    {
        const float baseSpeed = particleSpeed() * (1.35f + randFloat() * 0.55f);
        const float lateralFromMotion = -m_viewMotionX * m_screenRainMotionScale * (0.85f + randFloat() * 0.45f);

        drop.x = randFloat() * (displayW + 180.0f) - 90.0f;
        drop.y = topOnly ? (-40.0f - randFloat() * 80.0f) : (randFloat() * displayH);
        drop.vy = baseSpeed;
        drop.vx = WIND_DX * baseSpeed * (0.9f + randFloat() * 0.45f) + lateralFromMotion;
        drop.length = particleLength() * (1.3f + 0.8f * m_screenRainOverlayStrength + randFloat() * 1.5f)
            + std::abs(m_viewMotionX) * 0.015f * m_screenRainMotionScale;
        drop.alpha = std::min(0.95f, particleAlpha() * (0.22f + 0.14f * m_screenRainOverlayStrength + randFloat() * 0.24f));
        drop.width = 1.1f + randFloat() * 1.4f;
    }

    void WeatherSystem::spawnLensDrop(float displayW, float displayH)
    {
        LensRainDrop drop;
        respawnLensDrop(drop, displayW, displayH, false);
        m_lensDrops.push_back(drop);
    }

    void WeatherSystem::respawnLensDrop(LensRainDrop &drop, float displayW, float displayH, bool topOnly)
    {
        drop.x = randFloat() * (displayW * 0.94f) + displayW * 0.03f;
        drop.y = topOnly ? (-20.0f - randFloat() * 50.0f) : (randFloat() * displayH * 0.74f);
        drop.radius = (4.5f + randFloat() * 7.0f) * (0.85f + m_screenRainOverlayStrength * 0.35f);
        drop.alpha = std::min(0.34f, particleAlpha() * (0.10f + randFloat() * 0.10f) * (0.75f + 0.35f * m_screenRainOverlayStrength));
        drop.vy = 8.0f + randFloat() * 20.0f + std::abs(m_viewMotionY) * 0.015f;
        drop.smear = drop.radius * (1.6f + randFloat() * 2.8f) + std::abs(m_viewMotionX) * 0.020f * m_screenRainMotionScale;
        drop.age = 0.0f;
        drop.maxAge = 1.6f + randFloat() * 3.0f;
    }

    // ──────────────────────────────────────────────
    // 公共接口
    // ──────────────────────────────────────────────
    void WeatherSystem::setWeather(WeatherType type, float transitionSec)
    {
        if (type == m_current && !m_isTransitioning) return;

        m_current            = type;
        m_transitionDuration = transitionSec;
        m_transitionTimer    = 0.0f;
        m_intensity          = 0.0f;     // 从零开始淡入
        m_isTransitioning    = true;
        m_particles.clear();             // 清除旧粒子，让新天气自然生成
        m_screenDrops.clear();
        m_lensDrops.clear();

        // 重置雷电计时（切换到雷雨时稍后触发）
        m_lightningFlash    = 0.0f;
        m_lightningNextTime = 3.0f + static_cast<float>(nextRand() % 8);
    }

    const char* WeatherSystem::getWeatherName(WeatherType t)
    {
        switch (t)
        {
            case WeatherType::Clear:        return "晴天";
            case WeatherType::LightRain:    return "小雨";
            case WeatherType::MediumRain:   return "中雨";
            case WeatherType::HeavyRain:    return "大雨";
            case WeatherType::Thunderstorm: return "雷雨";
        }
        return "未知";
    }

    // ──────────────────────────────────────────────
    // update()
    // ──────────────────────────────────────────────
    void WeatherSystem::update(float dt, float displayW, float displayH)
    {
        float cameraDriftX = 0.0f;
        float cameraDriftY = 0.0f;
        if (m_hasCameraState)
        {
            cameraDriftX = (m_cameraWorldX - m_prevCameraWorldX) * m_cameraZoom;
            cameraDriftY = (m_cameraWorldY - m_prevCameraWorldY) * m_cameraZoom * m_cameraVerticalScale;
            m_prevCameraWorldX = m_cameraWorldX;
            m_prevCameraWorldY = m_cameraWorldY;
        }

        // ── 过渡淡入 ──
        if (m_isTransitioning)
        {
            m_transitionTimer += dt;
            m_intensity = std::min(1.0f, m_transitionTimer / m_transitionDuration);
            if (m_intensity >= 1.0f)
                m_isTransitioning = false;
        }

        // ── 粒子池管理 ──
        int target = targetParticleCount();
        int deficit = target - static_cast<int>(m_particles.size());
        int spawnBudget = std::min(deficit, 36);
        while (spawnBudget-- > 0)
            spawnParticle(displayW, displayH);
        if (static_cast<int>(m_particles.size()) > target)
            m_particles.resize(static_cast<size_t>(target));

        const int screenTarget = targetScreenDropCount();
        int screenDeficit = screenTarget - static_cast<int>(m_screenDrops.size());
        int screenSpawnBudget = std::min(screenDeficit, 8);
        while (screenSpawnBudget-- > 0)
            spawnScreenDrop(displayW, displayH);
        if (static_cast<int>(m_screenDrops.size()) > screenTarget)
            m_screenDrops.resize(static_cast<size_t>(screenTarget));

        const int lensTarget = targetLensDropCount();
        int lensDeficit = lensTarget - static_cast<int>(m_lensDrops.size());
        int lensSpawnBudget = std::min(lensDeficit, 3);
        while (lensSpawnBudget-- > 0)
            spawnLensDrop(displayW, displayH);
        if (static_cast<int>(m_lensDrops.size()) > lensTarget)
            m_lensDrops.resize(static_cast<size_t>(lensTarget));

        // ── 粒子物理 ──
        for (auto &p : m_particles)
        {
            const float parallax = 0.68f + p.depth * 0.52f;
            p.x -= cameraDriftX * parallax;
            p.y -= cameraDriftY * parallax;
            p.y += p.speed * dt;
            p.x += WIND_DX * p.speed * dt;

            // 雨水落在地面走廊范围内时产生水花
            // gMin = 走廊远端屏幕Y（背景侧），gMax = 走廊前沿屏幕Y（玩家侧）
            float gMax = (m_groundScreenY    > 0.0f) ? m_groundScreenY    : displayH * 0.88f;
            float gMin = (m_groundMinScreenY > 0.0f) ? m_groundMinScreenY : displayH * 0.33f;
            if (p.y >= gMin && p.y <= gMax + displayH * 0.03f
                && m_current != WeatherType::Clear && m_intensity > 0.3f)
            {
                // 按雨强决定生成概率
                float spawnProb = particleAlpha() * m_intensity;
                if ((nextRand() & 0x7F) < static_cast<uint32_t>(spawnProb * 24))
                {
                    RainSplash s;
                    s.x        = p.x + (static_cast<float>(nextRand() & 0x7) - 3.5f);
                    s.y        = p.y  + (static_cast<float>(nextRand() & 0x7) - 3.5f) * 0.4f;  // 落点即溅起位置
                    s.radius   = 0.0f;
                    s.maxRadius = 4.0f + randFloat() * 9.0f;
                    s.age      = 0.0f;
                    s.maxAge   = 0.22f + randFloat() * 0.20f;
                    s.alpha    = 0.55f + randFloat() * 0.35f;
                    m_splashes.push_back(s);
                }
            }

            // 到达地面前沿（gMax）或离开屏幕 → 重新生成
            if (p.y > gMax + 5.0f ||
                p.x < -100.0f     ||
                p.x > displayW + 100.0f)
            {
                respawnParticle(p, displayW, displayH);
            }
        }

        for (auto &s : m_splashes)
        {
            s.x -= cameraDriftX;
            s.y -= cameraDriftY;
        }

        for (auto &drop : m_screenDrops)
        {
            const float motionBoostX = -m_viewMotionX * 1.2f * m_screenRainMotionScale;
            const float motionBoostY = std::abs(m_viewMotionY) * 0.10f;
            drop.x += (drop.vx + motionBoostX) * dt;
            drop.y += (drop.vy + motionBoostY) * dt;

            if (drop.y > displayH + drop.length + 12.0f ||
                drop.x < -120.0f ||
                drop.x > displayW + 120.0f)
            {
                respawnScreenDrop(drop, displayW, displayH, true);
            }
        }

        for (auto &drop : m_lensDrops)
        {
            drop.age += dt;
            drop.y += drop.vy * dt;
            drop.x += -m_viewMotionX * 0.04f * m_screenRainMotionScale * dt;
            drop.smear = std::max(drop.radius * 1.1f,
                                  drop.smear * (1.0f - dt * 0.8f)
                                      + std::abs(m_viewMotionX) * 0.018f * m_screenRainMotionScale);

            if (drop.age >= drop.maxAge ||
                drop.y > displayH + drop.smear + drop.radius)
            {
                respawnLensDrop(drop, displayW, displayH, true);
            }
        }

        // ── 水花涟漪更新 ──
        for (auto &s : m_splashes)
        {
            s.age += dt;
            float t  = s.age / s.maxAge;
            s.radius = s.maxRadius * t;
            s.alpha  = (1.0f - t * t) * 0.85f;
        }
        m_splashes.erase(
            std::remove_if(m_splashes.begin(), m_splashes.end(),
                           [](const RainSplash &s){ return s.age >= s.maxAge; }),
            m_splashes.end());

        // ── 雾气时间累计 ──
        m_fogTime += dt;

        // ── 晴天：淡出并清理粒子 ──
        if (m_current == WeatherType::Clear)
        {
            for (auto &p : m_particles)
                p.alpha -= dt * 0.8f;
            m_particles.erase(
                std::remove_if(m_particles.begin(), m_particles.end(),
                               [](const RainParticle &p){ return p.alpha <= 0.0f; }),
                m_particles.end());

            for (auto &drop : m_screenDrops)
                drop.alpha -= dt * 0.9f;
            m_screenDrops.erase(
                std::remove_if(m_screenDrops.begin(), m_screenDrops.end(),
                               [](const ScreenRainDrop &drop){ return drop.alpha <= 0.0f; }),
                m_screenDrops.end());

            for (auto &drop : m_lensDrops)
                drop.alpha -= dt * 0.22f;
            m_lensDrops.erase(
                std::remove_if(m_lensDrops.begin(), m_lensDrops.end(),
                               [](const LensRainDrop &drop){ return drop.alpha <= 0.0f; }),
                m_lensDrops.end());
        }

        // ── 雷电（仅雷雨天气）──
        if (m_current == WeatherType::Thunderstorm && m_intensity > 0.5f)
        {
            m_lightningNextTime -= dt;
            if (m_lightningNextTime <= 0.0f)
            {
                m_lightningFlash    = 0.85f + randFloat() * 0.15f;
                m_lightningNextTime = 4.0f + randFloat() * 10.0f;
            }
        }
        if (m_lightningFlash > 0.0f)
        {
            m_lightningFlash -= dt * 3.5f;
            if (m_lightningFlash < 0.0f) m_lightningFlash = 0.0f;
        }

        // ── 自动天气切换 ──
        if (autoChangePeriod > 0.0f)
        {
            m_autoChangeTimer -= dt;
            if (m_autoChangeTimer <= 0.0f)
            {
                // 随机下一种天气（带权重偏向晴天和小雨）
                uint32_t r = static_cast<uint32_t>(nextRand() % 12);
                WeatherType next;
                if      (r < 4)  next = WeatherType::Clear;
                else if (r < 7)  next = WeatherType::LightRain;
                else if (r < 9)  next = WeatherType::MediumRain;
                else if (r < 11) next = WeatherType::HeavyRain;
                else             next = WeatherType::Thunderstorm;

                setWeather(next, 4.0f);
                m_autoChangeTimer = autoChangePeriod * (0.6f + randFloat() * 0.8f);
            }
        }
    }

    // ──────────────────────────────────────────────
    // render()  —  在 ImGui::NewFrame() 后调用
    // ──────────────────────────────────────────────
    void WeatherSystem::render(float displayW, float displayH)
    {
        ImDrawList *bg = ImGui::GetBackgroundDrawList();

        // ── 天空暗化（阴云效果）��─
        float dim = getDimAlpha() * m_intensity;
        if (dim > 0.0f)
        {
            bg->AddRectFilled(
                ImVec2(0.0f, 0.0f),
                ImVec2(displayW, displayH),
                IM_COL32(8, 16, 40, static_cast<int>(dim * 230)));
        }

        // ── 雨滴条纹（增强：主线 + 高光细线）──
        for (const auto &p : m_particles)
        {
            float alpha = p.alpha * m_intensity;
            if (alpha <= 0.01f) continue;

            uint8_t a  = static_cast<uint8_t>(std::min(alpha * 255.0f, 255.0f));
            float   dx = WIND_DX * p.length;
            float   dy = p.length;
            const float thickness = 0.75f + p.depth * 0.95f;

            // 主雨线：淡蓝白色
            bg->AddLine(
                ImVec2(p.x,        p.y),
                ImVec2(p.x + dx,   p.y + dy),
                IM_COL32(165 + static_cast<int>(p.depth * 20.0f), 205 + static_cast<int>(p.depth * 15.0f), 255, a),
                thickness);

            // 高光细线（仅中雨以上可见），模拟雨滴反光
            if (alpha > 0.32f && p.depth > 0.35f)
            {
                bg->AddLine(
                    ImVec2(p.x - 0.6f,      p.y),
                    ImVec2(p.x + dx - 0.6f, p.y + dy),
                    IM_COL32(220, 240, 255, static_cast<int>(a * (0.18f + p.depth * 0.22f))), 0.5f + p.depth * 0.25f);
            }
        }

        for (const auto &drop : m_screenDrops)
        {
            float alpha = drop.alpha * m_intensity;
            if (alpha <= 0.01f) continue;

            const float dx = drop.vx * 0.020f;
            const float dy = drop.length;
            const uint8_t a = static_cast<uint8_t>(std::min(alpha * 255.0f, 255.0f));
            bg->AddLine(
                ImVec2(drop.x, drop.y),
                ImVec2(drop.x + dx, drop.y + dy),
                IM_COL32(210, 232, 255, a), drop.width);

            bg->AddLine(
                ImVec2(drop.x - 0.8f, drop.y - 2.0f),
                ImVec2(drop.x + dx - 0.8f, drop.y + dy - 2.0f),
                IM_COL32(255, 255, 255, static_cast<int>(a * 0.22f)), 0.7f);
        }

        // ── 地面流动雾气 ──
        if (m_current != WeatherType::Clear && m_intensity > 0.15f)
        {
            float fogStrength = getDimAlpha() * m_intensity;

            // 底部上升渐变雾
            float fogH = displayH * 0.20f;
            for (int f = 0; f < 7; ++f)
            {
                float t     = static_cast<float>(f) / 6.0f;
                float y     = displayH - fogH * (1.0f - t * t);
                float aVal  = (1.0f - t) * fogStrength * 95.0f;
                if (aVal < 1.0f) continue;
                float xOff  = std::sin(m_fogTime * 0.25f + t * 1.8f) * 35.0f;
                bg->AddRectFilled(
                    ImVec2(-50.0f + xOff, y),
                    ImVec2(displayW + 50.0f, displayH),
                    IM_COL32(12, 22, 50, static_cast<int>(aVal)));
            }

            // 水平漂移雾带（3 条，速度各异）
            for (int band = 0; band < 3; ++band)
            {
                float speed  = 14.0f + band * 8.0f;
                float period = displayW * 1.8f;
                float offset = std::fmod(m_fogTime * speed + band * period * 0.35f, period);
                float bandY  = displayH * (0.72f + band * 0.07f);
                float bAlpha = fogStrength * (18.0f + band * 14.0f);
                if (bAlpha < 1.0f) continue;
                // 两段平铺
                for (int rep = -1; rep <= 1; ++rep)
                {
                    float bx = offset + rep * period - displayW * 0.3f;
                    bg->AddRectFilled(
                        ImVec2(bx,  bandY - 22.0f),
                        ImVec2(bx + displayW * 0.65f, bandY + 22.0f),
                        IM_COL32(14, 28, 58, static_cast<int>(bAlpha)), 22.0f);
                }
            }
        }

        // ── 水花涟漪 ──
        for (const auto &s : m_splashes)
        {
            float effAlpha = s.alpha * m_intensity;
            if (effAlpha <= 0.01f) continue;
            uint8_t a = static_cast<uint8_t>(std::min(effAlpha * 210.0f, 210.0f));
            ImU32  sc = IM_COL32(160, 215, 255, a);
            // 外椭圆涟漪
            bg->AddEllipse(ImVec2(s.x, s.y),
                ImVec2{s.radius * 2.2f, s.radius * 0.75f}, sc, 0.0f, 22, 1.0f);
            // 内小圆
            if (s.radius > 1.5f)
                bg->AddEllipse(ImVec2(s.x, s.y),
                    ImVec2{s.radius * 0.9f, s.radius * 0.32f},
                    IM_COL32(200, 235, 255, static_cast<int>(a * 0.6f)), 0.0f, 16, 0.8f);
        }

        // ── 闪电全屏白光 ──
        if (m_lightningFlash > 0.0f)
        {
            uint8_t fa = static_cast<uint8_t>(m_lightningFlash * 195.0f);
            bg->AddRectFilled(
                ImVec2(0.0f, 0.0f),
                ImVec2(displayW, displayH),
                IM_COL32(255, 255, 245, fa));
        }
    }

    void WeatherSystem::renderForeground(float /*displayW*/, float /*displayH*/)
    {
        ImDrawList *fg = ImGui::GetForegroundDrawList();

        for (const auto &drop : m_lensDrops)
        {
            const float lifeT = std::clamp(drop.age / std::max(drop.maxAge, 0.0001f), 0.0f, 1.0f);
            const float alpha = drop.alpha * m_intensity * (1.0f - lifeT * 0.35f);
            if (alpha <= 0.008f) continue;

            const float tailLen = drop.smear * (0.65f + 0.35f * lifeT);
            const int coreA = static_cast<int>(std::min(alpha * 255.0f, 255.0f));
            const int glowA = static_cast<int>(coreA * 0.28f);
            const int tailA = static_cast<int>(coreA * 0.18f);

            fg->AddCircleFilled(
                ImVec2(drop.x, drop.y),
                drop.radius * 1.85f,
                IM_COL32(160, 205, 255, glowA), 20);
            fg->AddCircleFilled(
                ImVec2(drop.x, drop.y),
                drop.radius,
                IM_COL32(185, 220, 255, static_cast<int>(coreA * 0.45f)), 20);
            fg->AddCircleFilled(
                ImVec2(drop.x - drop.radius * 0.22f, drop.y - drop.radius * 0.28f),
                drop.radius * 0.32f,
                IM_COL32(255, 255, 255, static_cast<int>(coreA * 0.75f)), 12);
            fg->AddCircle(
                ImVec2(drop.x + drop.radius * 0.12f, drop.y + drop.radius * 0.10f),
                drop.radius * 0.82f,
                IM_COL32(140, 190, 245, static_cast<int>(coreA * 0.22f)), 18, 1.0f);

            fg->AddLine(
                ImVec2(drop.x, drop.y + drop.radius * 0.35f),
                ImVec2(drop.x - m_viewMotionX * 0.02f * m_screenRainMotionScale,
                       drop.y + tailLen),
                IM_COL32(185, 220, 255, tailA),
                std::max(1.0f, drop.radius * 0.42f));
            fg->AddLine(
                ImVec2(drop.x - drop.radius * 0.18f, drop.y + drop.radius * 0.10f),
                ImVec2(drop.x - m_viewMotionX * 0.028f * m_screenRainMotionScale,
                       drop.y + tailLen * 0.92f),
                IM_COL32(255, 255, 255, static_cast<int>(tailA * 0.55f)),
                std::max(0.8f, drop.radius * 0.18f));
        }
    }

} // namespace game::weather
