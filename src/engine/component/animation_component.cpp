#include "animation_component.h"
#include "sprite_component.h"
#include "../object/game_object.h"
#include <algorithm>

namespace engine::component
{
    AnimationComponent::AnimationComponent(float frame_w, float frame_h)
        : m_frame_w(frame_w), m_frame_h(frame_h)
    {}

    void AnimationComponent::addClip(const std::string& name, AnimationClip clip)
    {
        m_clips.insert_or_assign(name, clip);
    }

    void AnimationComponent::play(const std::string& name)
    {
        if (name == m_current) return;
        if (!m_clips.count(name)) return;

        m_current = name;
        m_frame   = 0;
        m_timer   = 0.0f;
        applyFrame();
    }

    void AnimationComponent::init()
    {
        m_sprite = _owner->getComponent<SpriteComponent>();
    }

    void AnimationComponent::update(float dt)
    {
        if (m_current.empty() || !m_sprite) return;

        auto it = m_clips.find(m_current);
        if (it == m_clips.end()) return;
        const auto& clip = it->second;

        if (clip.frame_count <= 1)
        {
            applyFrame();
            return;
        }

        m_timer += dt;
        while (m_timer >= clip.frame_duration)
        {
            m_timer -= clip.frame_duration;
            if (clip.loop)
                m_frame = (m_frame + 1) % clip.frame_count;
            else
                m_frame = std::min(m_frame + 1, clip.frame_count - 1);
        }
        applyFrame();
    }

    void AnimationComponent::applyFrame()
    {
        if (!m_sprite || m_current.empty()) return;

        auto it = m_clips.find(m_current);
        if (it == m_clips.end()) return;

        const auto& clip = it->second;
        const float x = static_cast<float>(clip.col_start + m_frame) * m_frame_w;
        const float y = static_cast<float>(clip.row)                  * m_frame_h;
        m_sprite->setSourceRect(engine::utils::FRect{{x, y}, {m_frame_w, m_frame_h}});
    }
} // namespace engine::component
