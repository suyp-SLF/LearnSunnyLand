#pragma once
#include <string>

namespace game::skill
{
    // ──────────────────────────────────────────────────────────────────────────
    //  星技效果类型
    // ──────────────────────────────────────────────────────────────────────────
    enum class SkillEffect
    {
        FireBlast,   // 炎焰：攻击时在光标位置爆炸，范围破坏瓦片 + 击杀怪物
        IceAura,     // 寒冰：被动光环，自动冻结并消灭近身怪物
        WindBoost,   // 疾风：被动加速，提升移速与喷气包性能
        LightDash,   // 闪光：主动冲刺（Q键），向面朝方向瞬间加速
    };

    // ──────────────────────────────────────────────────────────────────────────
    //  星技定义
    //    cooldown  激活/触发间隔（秒）；被动技能为 0
    //    range     效果范围（像素）；无范围类技能为 0
    //    param     效果参数（速度倍率、冲刺速度等，含义依 effect 而定）
    // ──────────────────────────────────────────────────────────────────────────
    struct StarSkillDef
    {
        const char*  id;
        SkillEffect  effect;
        float        cooldown;
        float        range;
        float        param;
    };

    inline const StarSkillDef* getStarSkillDef(const std::string& id)
    {
        static const StarSkillDef defs[] = {
            // id            effect                cd     range   param
            { "star_fire",  SkillEffect::FireBlast, 1.4f,  80.0f,  0.0f  },
            { "star_ice",   SkillEffect::IceAura,   1.2f,  52.0f,  0.0f  },
            { "star_wind",  SkillEffect::WindBoost,  0.0f,   0.0f,  1.35f },
            { "star_light", SkillEffect::LightDash,  0.8f,   0.0f, 22.0f  },
        };
        for (const auto& d : defs)
            if (id == d.id) return &d;
        return nullptr;
    }

} // namespace game::skill
