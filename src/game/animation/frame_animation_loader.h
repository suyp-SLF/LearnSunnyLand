#pragma once

#include "../../engine/component/animation_component.h"
#include <optional>
#include <string>
#include <unordered_map>

namespace game::animation
{
    struct FrameAnimationSet
    {
        std::string texturePath;
        std::unordered_map<std::string, engine::component::AnimationClip> clips;

        std::optional<engine::utils::FRect> initialSourceRect(const std::string& preferredClip = "idle") const;
    };

    bool loadFrameAnimationSet(const std::string& jsonPath, FrameAnimationSet& outSet);
    FrameAnimationSet makeDefaultGundomAnimationSet();
}