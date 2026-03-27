#include "controller_component.h"
#include "transform_component.h"
#include "physics_component.h"
#include "../object/game_object.h"
#include "../core/context.h"
#include "../input/input_manager.h"
#include <algorithm>
#include <cmath>

namespace engine::component
{
    ControllerComponent::ControllerComponent(float speed, float jetpackForce)
        : m_speed(speed), m_jetpackForce(jetpackForce)
    {
    }

    void ControllerComponent::handleInput()
    {
        if (!_context || !m_enabled)
        {
            m_inputDir = {0.0f, 0.0f};
            return;
        }

        auto &input = _context->getInputManager();
        m_inputDir = {0.0f, 0.0f};

        if (input.isActionDown("move_left"))
            m_inputDir.x -= 1.0f;
        if (input.isActionDown("move_right"))
            m_inputDir.x += 1.0f;

        // DNF Y\u8f74\u6df1\u5ea6\u79fb\u52a8\uff1aW/\u4e0a\u2192\u5411\u540e\u65b9\uff08\u8fdc\u7aef\uff09\uff0cS/\u4e0b\u2192\u5411\u524d\u65b9\uff08\u8fd1\u7aef\uff09
        if (input.isActionDown("move_up"))
            m_inputDir.y -= 1.0f;
        if (input.isActionDown("move_down"))
            m_inputDir.y += 1.0f;

        if (m_inputDir.x != 0.0f)
        {
            m_inputDir.x = m_inputDir.x > 0 ? 1.0f : -1.0f;
        }
    }

    float ControllerComponent::approach(float current, float target, float delta) const
    {
        if (current < target)
            return std::min(current + delta, target);
        return std::max(current - delta, target);
    }

    bool ControllerComponent::isGrounded(const PhysicsComponent& physics) const
    {
        return std::abs(physics.getVelocity().y) <= m_groundedThreshold;
    }

    const char* ControllerComponent::getMovementStateName() const
    {
        switch (m_state)
        {
        case MovementState::Idle: return "待机";
        case MovementState::Run: return "奔跑";
        case MovementState::Jump: return "起跳";
        case MovementState::Fall: return "下落";
        case MovementState::Jetpack: return "喷气";
        }
        return "未知";
    }

    const char* ControllerComponent::getAnimationStateKey() const
    {
        switch (m_state)
        {
        case MovementState::Idle: return "idle";
        case MovementState::Run: return "run";
        case MovementState::Jump: return "jump";
        case MovementState::Fall: return "fall";
        case MovementState::Jetpack: return "jetpack";
        }
        return "idle";
    }

    float ControllerComponent::getJetpackFuelRatio() const
    {
        if (m_jetpackFuelMax <= 0.0f)
            return 0.0f;
        return std::clamp(m_jetpackFuel / m_jetpackFuelMax, 0.0f, 1.0f);
    }

    void ControllerComponent::updateMovementState(const glm::vec2& velocity, bool grounded, bool jetpacking, float velZ)
    {
        if (grounded)
        {
            m_state = std::abs(velocity.x) > 0.6f ? MovementState::Run : MovementState::Idle;
            return;
        }

        if (jetpacking)
        {
            m_state = MovementState::Jetpack;
            return;
        }

        m_state = velZ > 0.0f ? MovementState::Jump : MovementState::Fall;
    }

    void ControllerComponent::update(float delta_time)
    {
        if (!_owner)
            return;

        auto* physics = _owner->getComponent<PhysicsComponent>();
        if (!physics)
            return;

        if (!_context || !m_enabled)
        {
            glm::vec2 vel = physics->getVelocity();
            vel.x = 0.0f;
            vel.y = 0.0f;
            physics->setVelocity(vel);
            updateMovementState(vel, true, false, m_velZ);
            return;
        }

        auto& input = _context->getInputManager();
        glm::vec2 vel = physics->getVelocity();

        // ── Z轴物理（视觉跳跃，纯逻辑，与 Box2D 无关）── 先算，供深度移动判断
        bool zGrounded = (m_posZ <= 0.0f && m_velZ <= 0.0f);

        if (zGrounded)
        {
            m_posZ = 0.0f;
            m_velZ = 0.0f;
            m_coyoteTimer = m_coyoteTime;
            m_jetpackFuel = m_jetpackEnabled ? m_jetpackFuelMax : 0.0f;
            m_hasReleasedJumpSinceTakeoff = false;
            m_jetpackUnlockedThisAir = false;
        }
        else
        {
            m_coyoteTimer = std::max(0.0f, m_coyoteTimer - delta_time);
            if (input.isActionReleased("jump"))
                m_hasReleasedJumpSinceTakeoff = true;

            // Z轴重力（下落时增大倍率消除飘浮感）
            float gravMult = (m_velZ < 0.0f) ? m_fallGravityMultiplier : 1.0f;
            m_velZ -= kZGravity * gravMult * delta_time;
            m_posZ += m_velZ * delta_time;

            if (m_posZ <= 0.0f)
            {
                m_posZ = 0.0f;
                m_velZ = 0.0f;
                zGrounded = true;
            }
        }

        // ── Y轴深度移动：W/S 在走廊内前后移动（每帧覆盖 Box2D Y 速度）──
        {
            float posY = 0.0f;
            if (auto* transform = _owner->getComponent<TransformComponent>())
                posY = transform->getPosition().y + m_posZ;  // 还原 Z 偏移得到地面 Y

            float velY = m_inputDir.y * m_depthSpeed;
            // 跳跃空中时深度移动减半（保留轻微空中深度控制）
            if (!zGrounded) velY *= 0.3f;
            // 硬限位：触及边界时清零对应方向速度
            if (velY < 0.0f && posY <= m_groundYMin) velY = 0.0f;
            if (velY > 0.0f && posY >= m_groundYMax) velY = 0.0f;
            vel.y = velY;
        }

        // ── 水平移动（含跑步倍率）──
        float speedMult = m_isRunMode ? 1.5f : 1.0f;
        float targetSpeed = m_inputDir.x * m_speed * speedMult;
        float accel = zGrounded ? m_groundAccel : m_airAccel;
        vel.x = approach(vel.x, targetSpeed, accel * delta_time);

        // ── 惯性：松开方向键时地面滑行衰减 ──
        if (std::abs(m_inputDir.x) < 0.1f && zGrounded)
            vel.x *= std::max(0.0f, 1.0f - 15.0f * delta_time);

        if (m_inputDir.x < 0.0f)
            m_facing = FacingDirection::Left;
        else if (m_inputDir.x > 0.0f)
            m_facing = FacingDirection::Right;

        // ── Z轴跳跃 ──
        bool jumpedThisFrame = false;
        if (input.isActionPressed("jump") && m_coyoteTimer > 0.0f)
        {
            m_velZ = kZJumpSpeed;
            m_posZ = 1.0f;   // 离地一像素，避免立即判定为落地
            m_coyoteTimer = 0.0f;
            zGrounded = false;
            jumpedThisFrame = true;
        }

        // 松开跳跃键时缩减上升速度（可变跳高）
        if (!zGrounded && input.isActionReleased("jump") && m_velZ > 0.0f)
            m_velZ *= m_jumpCutFactor;

        // ── 喷气背包（Z轴悬停）──
        if (m_jetpackEnabled && !zGrounded && !jumpedThisFrame && m_hasReleasedJumpSinceTakeoff &&
            input.isActionPressed("jump") && m_jetpackFuel > 0.0f)
        {
            m_jetpackUnlockedThisAir = true;
        }

        bool jetpacking = false;
        if (m_jetpackEnabled && !zGrounded && m_jetpackUnlockedThisAir &&
            input.isActionDown("jump") && m_jetpackFuel > 0.0f)
        {
            // 喷气维持 Z速度
            m_velZ = std::max(m_velZ, m_jetpackRiseSpeed * 0.5f);
            m_jetpackFuel = std::max(0.0f, m_jetpackFuel - delta_time);
            jetpacking = true;
        }

        physics->setVelocity(vel);
        updateMovementState(vel, zGrounded, jetpacking, m_velZ);
    }
}
