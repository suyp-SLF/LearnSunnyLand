#include "controller_component.h"
#include "transform_component.h"
#include "physics_component.h"
#include "../object/game_object.h"
#include "../core/context.h"
#include "../input/input_manager.h"
#include <glm/geometric.hpp>

namespace engine::component
{
    ControllerComponent::ControllerComponent(float speed, float jetpackForce)
        : m_speed(speed), m_jetpackForce(jetpackForce)
    {
    }

    void ControllerComponent::handleInput()
    {
        if (!_context)
            return;

        auto &input = _context->getInputManager();
        m_inputDir = {0.0f, 0.0f};

        if (input.isActionDown("move_left"))
            m_inputDir.x -= 1.0f;
        if (input.isActionDown("move_right"))
            m_inputDir.x += 1.0f;

        if (m_inputDir.x != 0.0f)
        {
            m_inputDir.x = m_inputDir.x > 0 ? 1.0f : -1.0f;
        }
    }

    void ControllerComponent::update(float /*delta_time*/)
    {
        if (!_owner)
            return;

        auto* physics = _owner->getComponent<PhysicsComponent>();
        if (!physics)
            return;

        auto vel = physics->getVelocity();

        // 立即响应输入，无输入时立即停止X轴移动
        if (m_inputDir.x != 0.0f)
        {
            vel.x = m_inputDir.x * m_speed;
        }
        else
        {
            vel.x = 0.0f; // 立即停止
        }

        physics->setVelocity(vel);

        if (_context && _context->getInputManager().isActionDown("jump"))
        {
            physics->applyForce({0.0f, -m_jetpackForce});
        }
    }
}
