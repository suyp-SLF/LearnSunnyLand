#include "time_of_day_system.h"

#include "../../engine/core/context.h"
#include "../../engine/render/camera.h"
#include "../../engine/render/renderer.h"
#include <algorithm>
#include <cmath>

namespace game::world
{
    namespace
    {
        constexpr float kTau = 6.28318530718f;
    }

    TimeOfDaySystem::TimeOfDaySystem()
    {
        buildStarField();
    }

    void TimeOfDaySystem::update(float deltaTime)
    {
        if (dayLengthSeconds <= 0.0f)
            return;

        m_timeOfDay += deltaTime / dayLengthSeconds;
        while (m_timeOfDay >= 1.0f)
            m_timeOfDay -= 1.0f;
        while (m_timeOfDay < 0.0f)
            m_timeOfDay += 1.0f;
    }

    void TimeOfDaySystem::renderBackground(engine::core::Context &context) const
    {
        auto &camera = context.getCamera();
        auto &renderer = context.getRenderer();

        const glm::vec2 cameraPos = camera.getPosition();
        const glm::vec2 viewport = camera.getViewportSize() / std::max(camera.getZoom(), 0.01f);
        const float left = cameraPos.x;
        const float top = cameraPos.y;
        const float width = viewport.x;
        const float height = viewport.y;

        const glm::vec4 topColor = sampleTopSkyColor();
        const glm::vec4 bottomColor = sampleBottomSkyColor();

        constexpr int kBands = 18;
        for (int i = 0; i < kBands; ++i)
        {
            float t0 = static_cast<float>(i) / static_cast<float>(kBands);
            float t1 = static_cast<float>(i + 1) / static_cast<float>(kBands);
            glm::vec4 bandColor = lerpColor(topColor, bottomColor, (t0 + t1) * 0.5f);
            renderer.drawRect(camera, left, top + height * t0, width, height * (t1 - t0) + 2.0f, bandColor);
        }

        float daylight = getDaylightFactor();
        float starAlpha = saturate(1.15f - daylight * 1.25f);
        for (const auto &star : m_stars)
        {
            glm::vec2 pos{
                left + width * (star.normalizedPos.x + cameraPos.x * 0.00003f * star.depth),
                top + height * (star.normalizedPos.y + cameraPos.y * 0.00002f * star.depth)};
            pos.x = left + std::fmod(pos.x - left + width, width);
            pos.y = top + std::fmod(pos.y - top + height, height);

            glm::vec4 color = star.color;
            color.a *= starAlpha;
            renderer.drawRect(camera, pos.x, pos.y, star.size, star.size, color);
        }

        const glm::vec2 sunPosNorm = sunNormalizedPosition();
        const glm::vec2 sunPos = celestialWorldPosition(
            cameraPos,
            left,
            top,
            width,
            height,
            sunPosNorm,
            {0.018f, 0.010f},
            {24.0f, 10.0f},
            0.55f,
            0.12f);
        const glm::vec4 sunGlow{1.0f, 0.76f, 0.34f, 0.16f + daylight * 0.18f};
        const glm::vec4 sunCore{1.0f, 0.88f, 0.52f, 0.95f};
        renderer.drawRect(camera, sunPos.x - 36.0f, sunPos.y - 36.0f, 72.0f, 72.0f, sunGlow);
        renderer.drawRect(camera, sunPos.x - 14.0f, sunPos.y - 14.0f, 28.0f, 28.0f, sunCore);

        const glm::vec2 farStarNorm = farStarNormalizedPosition();
        const glm::vec2 farStarPos = celestialWorldPosition(
            cameraPos,
            left,
            top,
            width,
            height,
            farStarNorm,
            {0.006f, 0.003f},
            {12.0f, 7.0f},
            0.33f,
            1.7f);
        glm::vec4 farStarColor{0.9f, 0.95f, 1.0f, 0.15f + (1.0f - daylight) * 0.55f};
        renderer.drawRect(camera, farStarPos.x - 18.0f, farStarPos.y - 18.0f, 36.0f, 36.0f, farStarColor);
        renderer.drawRect(camera, farStarPos.x - 5.0f, farStarPos.y - 5.0f, 10.0f, 10.0f, {0.82f, 0.92f, 1.0f, farStarColor.a + 0.15f});

        const float night = getNightFactor();
        const glm::vec2 planetA = celestialWorldPosition(
            cameraPos,
            left,
            top,
            width,
            height,
            {0.77f, 0.26f},
            {0.011f, 0.006f},
            {18.0f, 9.0f},
            0.27f,
            0.85f);
        const glm::vec2 planetB = celestialWorldPosition(
            cameraPos,
            left,
            top,
            width,
            height,
            {0.14f, 0.18f},
            {0.024f, 0.012f},
            {28.0f, 13.0f},
            0.41f,
            2.6f);
        renderer.drawRect(camera, planetA.x - 30.0f, planetA.y - 30.0f, 60.0f, 60.0f, {0.42f, 0.55f, 0.82f, 0.22f + night * 0.18f});
        renderer.drawRect(camera, planetA.x - 38.0f, planetA.y - 2.0f, 76.0f, 4.0f, {0.7f, 0.78f, 0.94f, 0.18f + night * 0.16f});
        renderer.drawRect(camera, planetA.x - 12.0f, planetA.y - 20.0f, 18.0f, 8.0f, {0.78f, 0.88f, 1.0f, 0.10f + night * 0.10f});
        renderer.drawRect(camera, planetB.x - 18.0f, planetB.y - 18.0f, 36.0f, 36.0f, {0.76f, 0.42f, 0.6f, 0.12f + night * 0.15f});
        renderer.drawRect(camera, planetB.x - 6.0f, planetB.y - 24.0f, 24.0f, 3.0f, {0.96f, 0.72f, 0.84f, 0.08f + night * 0.08f});
    }

    void TimeOfDaySystem::renderLighting(engine::core::Context &context) const
    {
        auto &camera = context.getCamera();
        auto &renderer = context.getRenderer();

        const glm::vec2 cameraPos = camera.getPosition();
        const glm::vec2 viewport = camera.getViewportSize() / std::max(camera.getZoom(), 0.01f);
        const glm::vec4 overlay = sampleAmbientOverlayColor();
        if (overlay.a <= 0.0f)
            return;

        renderer.drawRect(camera, cameraPos.x, cameraPos.y, viewport.x, viewport.y, overlay);
    }

    float TimeOfDaySystem::getDaylightFactor() const
    {
        float sunArc = std::sin((m_timeOfDay - 0.25f) * kTau);
        return saturate((sunArc + 0.22f) / 1.05f);
    }

    float TimeOfDaySystem::getNightFactor() const
    {
        return 1.0f - getDaylightFactor();
    }

    const char* TimeOfDaySystem::getPhaseName() const
    {
        float t = m_timeOfDay;
        if (t < 0.20f) return "深夜";
        if (t < 0.28f) return "黎明";
        if (t < 0.52f) return "白昼";
        if (t < 0.65f) return "黄昏";
        return "夜晚";
    }

    int TimeOfDaySystem::getHour24() const
    {
        int totalMinutes = static_cast<int>(m_timeOfDay * 24.0f * 60.0f) % (24 * 60);
        return totalMinutes / 60;
    }

    int TimeOfDaySystem::getMinute() const
    {
        int totalMinutes = static_cast<int>(m_timeOfDay * 24.0f * 60.0f) % (24 * 60);
        return totalMinutes % 60;
    }

    unsigned long long TimeOfDaySystem::nextRand()
    {
        m_rngState ^= m_rngState << 13;
        m_rngState ^= m_rngState >> 7;
        m_rngState ^= m_rngState << 17;
        return m_rngState;
    }

    float TimeOfDaySystem::randFloat()
    {
        return static_cast<float>(nextRand() & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    void TimeOfDaySystem::buildStarField()
    {
        m_stars.clear();
        m_stars.reserve(84);
        for (int i = 0; i < 84; ++i)
        {
            StarSprite star;
            star.normalizedPos = {randFloat(), randFloat() * 0.72f};
            star.size = 1.0f + std::floor(randFloat() * 3.0f);
            star.depth = 0.5f + randFloat() * 1.4f;
            float tint = randFloat();
            if (tint < 0.2f)
                star.color = {0.95f, 0.84f, 0.75f, 0.72f};
            else if (tint < 0.45f)
                star.color = {0.75f, 0.85f, 1.0f, 0.78f};
            else
                star.color = {0.95f, 0.97f, 1.0f, 0.85f};
            m_stars.push_back(star);
        }
    }

    glm::vec4 TimeOfDaySystem::sampleTopSkyColor() const
    {
        const glm::vec4 midnight{0.02f, 0.03f, 0.09f, 1.0f};
        const glm::vec4 dawn{0.34f, 0.19f, 0.34f, 1.0f};
        const glm::vec4 noon{0.18f, 0.32f, 0.54f, 1.0f};
        const glm::vec4 dusk{0.18f, 0.09f, 0.23f, 1.0f};

        float daylight = getDaylightFactor();
        float dawnMix = saturate(1.0f - std::abs(m_timeOfDay - 0.25f) / 0.10f);
        float duskMix = saturate(1.0f - std::abs(m_timeOfDay - 0.58f) / 0.10f);

        glm::vec4 color = lerpColor(midnight, noon, daylight);
        color = lerpColor(color, dawn, dawnMix * 0.85f);
        color = lerpColor(color, dusk, duskMix * 0.80f);
        return color;
    }

    glm::vec4 TimeOfDaySystem::sampleBottomSkyColor() const
    {
        const glm::vec4 midnight{0.06f, 0.07f, 0.16f, 1.0f};
        const glm::vec4 dawn{0.92f, 0.46f, 0.34f, 1.0f};
        const glm::vec4 noon{0.64f, 0.78f, 0.95f, 1.0f};
        const glm::vec4 dusk{0.90f, 0.36f, 0.26f, 1.0f};

        float daylight = getDaylightFactor();
        float dawnMix = saturate(1.0f - std::abs(m_timeOfDay - 0.25f) / 0.12f);
        float duskMix = saturate(1.0f - std::abs(m_timeOfDay - 0.58f) / 0.12f);

        glm::vec4 color = lerpColor(midnight, noon, daylight);
        color = lerpColor(color, dawn, dawnMix);
        color = lerpColor(color, dusk, duskMix);
        return color;
    }

    glm::vec4 TimeOfDaySystem::sampleAmbientOverlayColor() const
    {
        float night = getNightFactor();
        float duskMix = saturate(1.0f - std::abs(m_timeOfDay - 0.58f) / 0.12f);
        float dawnMix = saturate(1.0f - std::abs(m_timeOfDay - 0.25f) / 0.12f);

        glm::vec4 color{0.03f, 0.05f, 0.10f, night * 0.58f};
        if (duskMix > 0.0f || dawnMix > 0.0f)
        {
            glm::vec4 warm{0.22f, 0.11f, 0.08f, std::max(duskMix, dawnMix) * 0.18f};
            color = lerpColor(color, warm, std::max(duskMix, dawnMix) * 0.7f);
        }
        return color;
    }

    glm::vec2 TimeOfDaySystem::sunNormalizedPosition() const
    {
        float x = std::fmod(m_timeOfDay + 0.76f, 1.0f);
        float arc = std::sin((m_timeOfDay - 0.25f) * 3.14159265359f);
        float y = 0.78f - std::max(0.0f, arc) * 0.52f;
        return {x, y};
    }

    glm::vec2 TimeOfDaySystem::farStarNormalizedPosition() const
    {
        float x = std::fmod(m_timeOfDay * 0.35f + 0.18f, 1.0f);
        float y = 0.12f + 0.04f * std::sin(m_timeOfDay * kTau);
        return {x, y};
    }

    glm::vec2 TimeOfDaySystem::celestialWorldPosition(const glm::vec2 &cameraPos,
                                                      float left,
                                                      float top,
                                                      float width,
                                                      float height,
                                                      const glm::vec2 &anchorNorm,
                                                      const glm::vec2 &cameraParallax,
                                                      const glm::vec2 &driftAmplitude,
                                                      float driftSpeed,
                                                      float driftPhase) const
    {
        float driftTime = m_timeOfDay * kTau * driftSpeed + driftPhase;
        glm::vec2 normalized{
            wrap01(anchorNorm.x + std::sin(driftTime) * (driftAmplitude.x / std::max(width, 1.0f))),
            wrap01(anchorNorm.y + std::cos(driftTime * 0.85f) * (driftAmplitude.y / std::max(height, 1.0f)))};

        return {
            left + width * normalized.x + cameraPos.x * cameraParallax.x,
            top + height * normalized.y + cameraPos.y * cameraParallax.y};
    }

    glm::vec4 TimeOfDaySystem::lerpColor(const glm::vec4 &a, const glm::vec4 &b, float t)
    {
        float clamped = saturate(t);
        return a + (b - a) * clamped;
    }

    float TimeOfDaySystem::saturate(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    float TimeOfDaySystem::wrap01(float value)
    {
        float wrapped = std::fmod(value, 1.0f);
        if (wrapped < 0.0f)
            wrapped += 1.0f;
        return wrapped;
    }
}