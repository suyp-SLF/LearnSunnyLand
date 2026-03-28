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

        const int frameCount = !clip.frames.empty()
            ? static_cast<int>(clip.frames.size())
            : clip.frame_count;

        if (frameCount <= 1)
        {
            applyFrame();
            return;
        }

        m_timer += dt;
        const auto currentFrameDuration = [&]() -> float {
            if (!clip.frames.empty())
            {
                const int frameIndex = std::clamp(m_frame, 0, frameCount - 1);
                return std::max(clip.frames[frameIndex].duration, 0.0001f);
            }
            return std::max(clip.frame_duration, 0.0001f);
        };

        while (m_timer >= currentFrameDuration())
        {
            m_timer -= currentFrameDuration();
            if (clip.loop)
                m_frame = (m_frame + 1) % frameCount;
            else
                m_frame = std::min(m_frame + 1, frameCount - 1);
        }
        applyFrame();
    }

    void AnimationComponent::forcePlay(const std::string& name)
    {
        if (!m_clips.count(name)) return;
        m_current = name;
        m_frame   = 0;
        m_timer   = 0.0f;
        applyFrame();
    }

    bool AnimationComponent::isFinished() const
    {
        auto it = m_clips.find(m_current);
        if (it == m_clips.end()) return true;
        const auto& clip = it->second;
        if (clip.loop) return false;
        return m_frame >= clip.frame_count - 1;
    }

    void AnimationComponent::applyFrame()
    {
        if (!m_sprite || m_current.empty()) return;

        auto it = m_clips.find(m_current);
        if (it == m_clips.end()) return;

        const auto& clip = it->second;
        if (!clip.frames.empty())
        {
            const int frameIndex = std::clamp(m_frame, 0, static_cast<int>(clip.frames.size()) - 1);
            const auto& frame = clip.frames[frameIndex];
            m_sprite->setSourceRect(frame.sourceRect);
            m_sprite->setFrameFlipped(frame.flipX);
            return;
        }

        m_sprite->setFrameFlipped(false);
        // 支持可变帧高：优先使用每条 clip 的 override 值
        const float fh = (clip.frame_h_override > 0.0f) ? clip.frame_h_override : m_frame_h;
        const float fy = (clip.y_origin >= 0.0f)
                             ? clip.y_origin
                             : static_cast<float>(clip.row) * m_frame_h;
        const float x = static_cast<float>(clip.col_start + m_frame) * m_frame_w;
        m_sprite->setSourceRect(engine::utils::FRect{{x, fy}, {m_frame_w, fh}});
    }
} // namespace engine::component
