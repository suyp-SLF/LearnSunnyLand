#include "frame_animation_loader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <vector>

namespace game::animation
{
    namespace
    {
        using engine::component::AnimationClip;
        using engine::component::AnimationFrame;

        std::string normalizeActionName(const std::string& name)
        {
            std::string normalized;
            normalized.reserve(name.size());
            for (char ch : name)
            {
                if (ch == ' ' || ch == '-')
                {
                    normalized.push_back('_');
                }
                else
                {
                    normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                }
            }
            return normalized;
        }

        std::vector<std::string> clipAliasesForAction(const std::string& normalized)
        {
            if (normalized == "idle") return {"idle"};
            if (normalized == "move") return {"move", "walk", "run"};
            if (normalized == "walk") return {"walk", "move"};
            if (normalized == "run") return {"run", "walk", "move"};
            if (normalized == "turn") return {"turn"};
            if (normalized == "fly") return {"fly", "jetpack"};
            if (normalized == "jetpack") return {"jetpack", "fly"};
            if (normalized == "jump") return {"jump"};
            if (normalized == "fall") return {"fall"};
            if (normalized == "attack_1" || normalized == "attack_a") return {"attack_a", "attack_1"};
            if (normalized == "attack_2" || normalized == "attack_b") return {"attack_b", "attack_2"};
            if (normalized == "attack_3" || normalized == "attack_c") return {"attack_c", "attack_3"};
            if (normalized == "attack_4" || normalized == "attack_d") return {"attack_d", "attack_4"};
            if (normalized == "cannon") return {"cannon"};
            if (normalized == "ultimate") return {"ultimate"};
            return {normalized};
        }

        AnimationClip makeGridClip(int row, int colStart, int frameCount, float duration,
                                   bool loop, float yOrigin, float frameHeight)
        {
            AnimationClip clip{};
            clip.row = row;
            clip.col_start = colStart;
            clip.frame_count = frameCount;
            clip.frame_duration = duration;
            clip.loop = loop;
            clip.y_origin = yOrigin;
            clip.frame_h_override = frameHeight;
            return clip;
        }

        void addAliasedClip(FrameAnimationSet& set, const std::string& actionName, const AnimationClip& clip)
        {
            for (const auto& alias : clipAliasesForAction(actionName))
            {
                set.clips.insert_or_assign(alias, clip);
            }
        }
    }

    std::optional<engine::utils::FRect> FrameAnimationSet::initialSourceRect(const std::string& preferredClip) const
    {
        const auto findClip = [&](const std::string& name) -> const AnimationClip* {
            const auto it = clips.find(name);
            return it != clips.end() ? &it->second : nullptr;
        };

        const AnimationClip* clip = findClip(preferredClip);
        if (!clip && !clips.empty())
            clip = &clips.begin()->second;
        if (!clip)
            return std::nullopt;

        if (!clip->frames.empty())
            return clip->frames.front().sourceRect;

        return std::nullopt;
    }

    bool loadFrameAnimationSet(const std::string& jsonPath, FrameAnimationSet& outSet)
    {
        std::ifstream file(jsonPath);
        if (!file.is_open())
            return false;

        nlohmann::json root;
        try
        {
            file >> root;
        }
        catch (const std::exception& e)
        {
            spdlog::warn("帧动画 JSON 解析失败: {} ({})", jsonPath, e.what());
            return false;
        }

        FrameAnimationSet loadedSet;
        loadedSet.texturePath = root.value("texture", std::string{});

        for (const auto& action : root.value("actions", nlohmann::json::array()))
        {
            const std::string actionName = normalizeActionName(action.value("name", std::string{}));
            const auto& framesJson = action.value("frames", nlohmann::json::array());
            if (actionName.empty() || framesJson.empty())
                continue;

            AnimationClip clip{};
            clip.loop = action.value("is_loop", true);
            clip.frame_count = static_cast<int>(framesJson.size());
            clip.frame_duration = 0.1f;
            clip.frames.reserve(framesJson.size());

            for (const auto& frameJson : framesJson)
            {
                const float sx = frameJson.value("sx", 0.0f);
                const float sy = frameJson.value("sy", 0.0f);
                const float sw = std::max(frameJson.value("sw", 0.0f), 1.0f);
                const float sh = std::max(frameJson.value("sh", 0.0f), 1.0f);
                const float duration = std::max(frameJson.value("duration_ms", 100) / 1000.0f, 0.0001f);

                clip.frames.push_back(AnimationFrame{
                    engine::utils::FRect{{sx, sy}, {sw, sh}},
                    duration,
                    frameJson.value("flip_x", false)
                });
            }

            clip.frame_duration = clip.frames.front().duration;
            addAliasedClip(loadedSet, actionName, clip);
        }

        if (loadedSet.clips.empty())
            return false;

        outSet = std::move(loadedSet);
        return true;
    }

    FrameAnimationSet makeDefaultGundomAnimationSet()
    {
        FrameAnimationSet set;
        set.texturePath = "assets/textures/Characters/gundom.png";

        addAliasedClip(set, "idle",    makeGridClip(0, 1, 1, 0.12f,  true,  1625.0f, 125.0f));
        addAliasedClip(set, "move",    makeGridClip(0, 1, 2, 0.10f,  true,  2750.0f, 125.0f));
        addAliasedClip(set, "run",     makeGridClip(0, 1, 2, 0.075f, true,  2750.0f, 125.0f));
        addAliasedClip(set, "turn",    makeGridClip(0, 1, 1, 0.10f,  false, 1625.0f, 125.0f));
        addAliasedClip(set, "fly",     makeGridClip(0, 1, 2, 0.10f,  true,  2750.0f, 125.0f));
        addAliasedClip(set, "jetpack", makeGridClip(0, 1, 2, 0.10f,  true,  2750.0f, 125.0f));
        addAliasedClip(set, "jump",    makeGridClip(0, 1, 1, 0.10f,  false, 1625.0f, 125.0f));
        addAliasedClip(set, "fall",    makeGridClip(0, 1, 1, 0.12f,  true,  1625.0f, 125.0f));
        addAliasedClip(set, "attack_a", makeGridClip(0, 1, 2, 0.08f, false, 2750.0f, 125.0f));
        addAliasedClip(set, "attack_b", makeGridClip(0, 1, 2, 0.08f, false, 2750.0f, 125.0f));
        addAliasedClip(set, "attack_c", makeGridClip(0, 1, 2, 0.08f, false, 2750.0f, 125.0f));
        addAliasedClip(set, "attack_d", makeGridClip(0, 1, 2, 0.08f, false, 2750.0f, 125.0f));
        addAliasedClip(set, "cannon",   makeGridClip(0, 1, 2, 0.08f, false, 2750.0f, 125.0f));
        addAliasedClip(set, "ultimate", makeGridClip(0, 1, 2, 0.10f, false, 2750.0f, 125.0f));

        return set;
    }
}