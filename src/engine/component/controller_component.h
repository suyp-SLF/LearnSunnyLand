#pragma once
#include "component.h"
#include <glm/vec2.hpp>

namespace engine::component
{
    class PhysicsComponent;

    class ControllerComponent final : public Component
    {
    public:
        enum class MovementState
        {
            Idle,
            Run,
            Jump,
            Fall,
            Jetpack,
        };

        enum class FacingDirection
        {
            Left,
            Right,
        };

        ControllerComponent(float speed = 15.0f, float jetpackForce = 20.0f);
        ~ControllerComponent() = default;

        void setSpeed(float speed) { m_speed = speed; }
        void setEnabled(bool enabled) { m_enabled = enabled; }
        void setJumpSpeed(float jumpSpeed) { m_jumpSpeed = jumpSpeed; }
        void setGroundAcceleration(float accel) { m_groundAccel = accel; }
        void setAirAcceleration(float accel) { m_airAccel = accel; }
        void setJumpCutFactor(float factor) { m_jumpCutFactor = factor; }
        void setCoyoteTime(float coyoteTime) { m_coyoteTime = coyoteTime; }
        void setGroundedThreshold(float threshold) { m_groundedThreshold = threshold; }
        void setJetpackEnabled(bool enabled) { m_jetpackEnabled = enabled; }
        void setJetpackProfile(float fuelMax, float accel, float riseSpeed, float force)
        {
            m_jetpackFuelMax = fuelMax;
            m_jetpackFuel = fuelMax;
            m_jetpackAccel = accel;
            m_jetpackRiseSpeed = riseSpeed;
            m_jetpackForce = force;
        }
        bool isEnabled() const { return m_enabled; }
        float getSpeed() const { return m_speed; }
        MovementState getMovementState() const { return m_state; }
        FacingDirection getFacingDirection() const { return m_facing; }
        const char* getMovementStateName() const;
        const char* getAnimationStateKey() const;
        float getJetpackFuelRatio() const;

    private:
        float m_speed;
        float m_jetpackForce;
        glm::vec2 m_inputDir{0.0f, 0.0f};
        MovementState m_state = MovementState::Idle;
        FacingDirection m_facing = FacingDirection::Right;
        bool m_enabled = true;

        float m_groundAccel = 90.0f;
        float m_airAccel = 20.0f;          // 空中横向加速（比地面慢，减少飘感）
        float m_jumpSpeed = 8.0f;
        float m_jumpCutFactor = 0.45f;
        float m_coyoteTime = 0.1f;
        float m_coyoteTimer = 0.0f;
        float m_groundedThreshold = 0.12f;

        // 下落重力倍率：当 vel.y > 0（下落）时额外施加向下加速，消除飘浮感
        float m_fallGravityMultiplier = 2.8f;

        float m_jetpackFuelMax = 0.75f;
        float m_jetpackFuel = 0.75f;
        float m_jetpackAccel = 20.0f;
        float m_jetpackRiseSpeed = 5.5f;
        bool m_jetpackEnabled = true;
        bool m_hasReleasedJumpSinceTakeoff = false;
        bool m_jetpackUnlockedThisAir = false;

        float approach(float current, float target, float delta) const;
        bool isGrounded(const PhysicsComponent& physics) const;
        void updateMovementState(const glm::vec2& velocity, bool grounded, bool jetpacking);

        void handleInput() override;
        void update(float delta_time) override;
        void render() override {}
    };
}
