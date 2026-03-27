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
        p.x      = randFloat() * (displayW + 120.0f) - 60.0f;
        p.y      = randFloat() * -displayH;  // 从屏幕上方随机位置开始
        p.speed  = particleSpeed()  * (0.8f + randFloat() * 0.4f);
        p.length = particleLength() * (0.7f + randFloat() * 0.6f);
        p.alpha  = particleAlpha()  * (0.5f + randFloat() * 0.5f) * m_intensity;
        m_particles.push_back(p);
    }

    void WeatherSystem::respawnParticle(RainParticle &p, float displayW, float /*displayH*/)
    {
        p.x      = randFloat() * (displayW + 120.0f) - 60.0f;
        p.y      = -p.length - randFloat() * 30.0f;
        p.speed  = particleSpeed()  * (0.8f + randFloat() * 0.4f);
        p.length = particleLength() * (0.7f + randFloat() * 0.6f);
        p.alpha  = particleAlpha()  * (0.5f + randFloat() * 0.5f) * m_intensity;
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

        // ── 粒子物理 ──
        for (auto &p : m_particles)
        {
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

            // 主雨线：淡蓝白色
            bg->AddLine(
                ImVec2(p.x,        p.y),
                ImVec2(p.x + dx,   p.y + dy),
                IM_COL32(175, 215, 255, a), 1.3f);

            // 高光细线（仅中雨以上可见），模拟雨滴反光
            if (alpha > 0.45f)
            {
                bg->AddLine(
                    ImVec2(p.x - 0.6f,      p.y),
                    ImVec2(p.x + dx - 0.6f, p.y + dy),
                    IM_COL32(220, 240, 255, static_cast<int>(a * 0.38f)), 0.5f);
            }
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

} // namespace game::weather
