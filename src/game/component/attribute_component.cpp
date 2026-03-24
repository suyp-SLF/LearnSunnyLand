#include "attribute_component.h"
#include <algorithm>
#include <cmath>

namespace game::component
{
    uint32_t AttributeComponent::s_rng = 0xDEADBEEFu;

    float AttributeComponent::nextRand()
    {
        s_rng = s_rng * 1664525u + 1013904223u;
        return static_cast<float>(s_rng >> 16) / 65535.0f; // [0, 1)
    }

    // ────────────────────────────────────────────────────────────────────────
    AttributeComponent::AttributeComponent(BaseStats base)
        : m_base(base)
        , m_currentHp(base.maxHp)
        , m_currentStarEnergy(base.maxStarEnergy * 0.6f)  // 初始 60% 星能
    {}

    // ── 基础属性映射 ──────────────────────────────────────────────────────────
    float AttributeComponent::baseOf(StatType stat) const
    {
        switch (stat)
        {
        case StatType::MaxHp:           return m_base.maxHp;
        case StatType::MaxStarEnergy:   return m_base.maxStarEnergy;
        case StatType::Attack:          return m_base.attack;
        case StatType::Defense:         return m_base.defense;
        case StatType::Speed:           return m_base.speed;
        case StatType::JumpPower:       return m_base.jumpPower;
        case StatType::CritRate:        return m_base.critRate;
        case StatType::CritMultiplier:  return m_base.critMultiplier;
        case StatType::HpRegen:         return m_base.hpRegen;
        case StatType::StarEnergyRegen: return m_base.starEnergyRegen;
        case StatType::Dodge:           return m_base.dodge;
        }
        return 0.0f;
    }

    // ── 有效属性计算 ──────────────────────────────────────────────────────────
    float AttributeComponent::get(StatType stat) const
    {
        float base      = baseOf(stat);
        float sumFlat   = 0.0f;
        float sumMult   = 0.0f;
        for (const auto& m : m_modifiers)
        {
            if (m.stat != stat) continue;
            sumFlat += m.flat;
            sumMult += m.mult;
        }
        float result = (base + sumFlat) * (1.0f + sumMult);
        if (stat == StatType::CritRate || stat == StatType::Dodge)
            result = std::clamp(result, 0.0f, 1.0f);
        return result;
    }

    float AttributeComponent::getBase(StatType stat) const { return baseOf(stat); }

    float AttributeComponent::getHpRatio() const
    {
        float maxHp = get(StatType::MaxHp);
        return maxHp > 0.0f ? std::clamp(m_currentHp / maxHp, 0.0f, 1.0f) : 0.0f;
    }

    float AttributeComponent::getStarEnergyRatio() const
    {
        float maxSe = get(StatType::MaxStarEnergy);
        return maxSe > 0.0f ? std::clamp(m_currentStarEnergy / maxSe, 0.0f, 1.0f) : 0.0f;
    }

    // ── 血量 ──────────────────────────────────────────────────────────────────
    float AttributeComponent::applyDamage(float rawDamage, bool ignoreDodge)
    {
        if (!ignoreDodge && nextRand() < get(StatType::Dodge))
            return 0.0f;

        float def     = get(StatType::Defense);
        float reduced = std::max(1.0f, rawDamage - def);
        m_currentHp   = std::max(0.0f, m_currentHp - reduced);
        return reduced;
    }

    void AttributeComponent::heal(float amount)
    {
        if (amount <= 0.0f) return;
        m_currentHp = std::min(m_currentHp + amount, get(StatType::MaxHp));
    }

    void AttributeComponent::setHp(float hp)
    {
        m_currentHp = std::clamp(hp, 0.0f, get(StatType::MaxHp));
    }

    // ── 星能 ──────────────────────────────────────────────────────────────────
    bool AttributeComponent::consumeStarEnergy(float amount)
    {
        if (m_currentStarEnergy < amount) return false;
        m_currentStarEnergy -= amount;
        return true;
    }

    void AttributeComponent::restoreStarEnergy(float amount)
    {
        if (amount <= 0.0f) return;
        m_currentStarEnergy = std::min(m_currentStarEnergy + amount, get(StatType::MaxStarEnergy));
    }

    void AttributeComponent::setStarEnergy(float se)
    {
        m_currentStarEnergy = std::clamp(se, 0.0f, get(StatType::MaxStarEnergy));
    }

    // ── 修改器 ────────────────────────────────────────────────────────────────
    void AttributeComponent::addModifier(StatModifier mod)
    {
        // 同 source + stat 覆盖（刷新 buff）
        for (auto& m : m_modifiers)
        {
            if (m.source == mod.source && m.stat == mod.stat)
            {
                m = std::move(mod);
                return;
            }
        }
        m_modifiers.push_back(std::move(mod));
    }

    void AttributeComponent::removeModifier(const std::string& source, StatType stat)
    {
        m_modifiers.erase(
            std::remove_if(m_modifiers.begin(), m_modifiers.end(),
                [&](const StatModifier& m) {
                    return m.source == source && m.stat == stat;
                }),
            m_modifiers.end());
    }

    void AttributeComponent::removeAllModifiers(const std::string& source)
    {
        m_modifiers.erase(
            std::remove_if(m_modifiers.begin(), m_modifiers.end(),
                [&](const StatModifier& m) { return m.source == source; }),
            m_modifiers.end());
    }

    bool AttributeComponent::hasModifier(const std::string& source, StatType stat) const
    {
        for (const auto& m : m_modifiers)
            if (m.source == source && m.stat == stat) return true;
        return false;
    }

    // ── Update ────────────────────────────────────────────────────────────────
    void AttributeComponent::update(float dt)
    {
        // 1. 修改器计时
        for (auto& m : m_modifiers)
            if (m.duration > 0.0f) m.duration -= dt;

        // 2. 移除过期修改器（duration != -1 且 <= 0）
        m_modifiers.erase(
            std::remove_if(m_modifiers.begin(), m_modifiers.end(),
                [](const StatModifier& m) {
                    return m.duration != -1.0f && m.duration <= 0.0f;
                }),
            m_modifiers.end());

        // 3. 生命回复
        if (float r = get(StatType::HpRegen); r > 0.0f)
            heal(r * dt);

        // 4. 星能回复
        if (float r = get(StatType::StarEnergyRegen); r > 0.0f)
            restoreStarEnergy(r * dt);

        // 5. 截断当前值不超过有效上限（上限被减少时同步）
        float maxHp = get(StatType::MaxHp);
        float maxSe = get(StatType::MaxStarEnergy);
        m_currentHp         = std::min(m_currentHp,         maxHp);
        m_currentStarEnergy = std::min(m_currentStarEnergy, maxSe);
    }

} // namespace game::component
