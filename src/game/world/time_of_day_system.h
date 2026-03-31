#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <vector>

namespace engine::core
{
    class Context;
}

namespace game::world
{
    class TimeOfDaySystem
    {
    public:
        struct RuntimeState
        {
            float timeOfDay = 0.22f;
            float dayLengthSeconds = 300.0f;
        };

        TimeOfDaySystem();

        void update(float deltaTime);
        void renderBackground(engine::core::Context &context, float skyVisibility = 1.0f) const;
        void renderLighting(engine::core::Context &context) const;

        float getTimeOfDay() const { return m_timeOfDay; }
        float getDaylightFactor() const;
        float getNightFactor() const;
        const char* getPhaseName() const;
        int getHour24() const;
        int getMinute() const;
        RuntimeState captureRuntimeState() const;
        void restoreRuntimeState(const RuntimeState& state);

        float dayLengthSeconds = 300.0f;

    private:
        struct StarSprite
        {
            glm::vec2 normalizedPos;
            float size = 1.0f;
            float depth = 1.0f;
            glm::vec4 color{1.0f};
        };

        float m_timeOfDay = 0.22f;
        unsigned long long m_rngState = 0x9E3779B97F4A7C15ULL;
        std::vector<StarSprite> m_stars;

        unsigned long long nextRand();
        float randFloat();
        void buildStarField();

        glm::vec4 sampleTopSkyColor() const;
        glm::vec4 sampleBottomSkyColor() const;
        glm::vec4 sampleAmbientOverlayColor() const;
        float sampleSkyDetailFactor(float skyVisibility) const;
        glm::vec2 sunNormalizedPosition() const;
        glm::vec2 farStarNormalizedPosition() const;
        glm::vec2 celestialWorldPosition(const glm::vec2 &cameraPos,
                                         float left,
                                         float top,
                                         float width,
                                         float height,
                                         const glm::vec2 &anchorNorm,
                                         const glm::vec2 &cameraParallax,
                                         const glm::vec2 &driftAmplitude,
                                         float driftSpeed,
                                         float driftPhase) const;
        static glm::vec4 lerpColor(const glm::vec4 &a, const glm::vec4 &b, float t);
        static float saturate(float value);
        static float wrap01(float value);
    };
}