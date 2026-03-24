#pragma once
#include "../../engine/component/component.h"
#include <string>
#include <vector>

namespace game::component
{
    // ──────────────────────────────────────────────────────────────────────────
    //  属性枚举
    // ──────────────────────────────────────────────────────────────────────────
    enum class StatType
    {
        MaxHp,           // 最大血量
        MaxStarEnergy,   // 最大星能
        Attack,          // 攻击力（武器/技能伤害基础值）
        Defense,         // 防御力（减少受到的伤害，固定值）
        Speed,           // 移速倍率（乘以 ControllerComponent 基础速度）
        JumpPower,       // 跳跃力倍率
        CritRate,        // 暴击率 [0, 1]
        CritMultiplier,  // 暴击倍率（默认 1.5）
        HpRegen,         // 生命回复速率（HP/s）
        StarEnergyRegen, // 星能回复速率（SE/s）
        Dodge,           // 闪避率 [0, 1]
    };

    // ──────────────────────────────────────────────────────────────────────────
    //  属性修改器（buff / debuff）
    //    source   唯一来源 ID，相同 source + stat 的修改器会互相覆盖（刷新）
    //    flat     加性加成（直接加到基础值）
    //    mult     乘性加成（汇总后：effective = (base + flatSum) * (1 + multSum)）
    //    duration 持续秒数；-1.0f 为永久（卸装时需手动移除）
    // ──────────────────────────────────────────────────────────────────────────
    struct StatModifier
    {
        std::string source;
        StatType    stat;
        float       flat     = 0.0f;
        float       mult     = 0.0f;
        float       duration = -1.0f;
    };

    // ──────────────────────────────────────────────────────────────────────────
    //  基础属性集
    // ──────────────────────────────────────────────────────────────────────────
    struct BaseStats
    {
        float maxHp           = 100.0f;
        float maxStarEnergy   = 100.0f;
        float attack          = 12.0f;
        float defense         = 3.0f;
        float speed           = 1.0f;   // 倍率；syncPlayerPresentation 乘以控制器基础速度
        float jumpPower       = 1.0f;   // 倍率
        float critRate        = 0.05f;
        float critMultiplier  = 1.5f;
        float hpRegen         = 0.0f;
        float starEnergyRegen = 8.0f;   // 8 SE/s
        float dodge           = 0.0f;
    };

    // ──────────────────────────────────────────────────────────────────────────
    //  AttributeComponent
    //    组件方式挂载到 GameObject 上，自动在 update() 中处理：
    //      - 修改器倒计时 & 过期移除
    //      - HP / 星能被动回复
    //      - 当前值不超过有效上限（上限减少时同步截断）
    // ──────────────────────────────────────────────────────────────────────────
    class AttributeComponent final : public engine::component::Component
    {
    public:
        explicit AttributeComponent(BaseStats base = {});

        // ── 当前值读取 ─────────────────────────────────────────────────────────
        float getHp()              const { return m_currentHp; }
        float getStarEnergy()      const { return m_currentStarEnergy; }
        float getHpRatio()         const;
        float getStarEnergyRatio() const;
        bool  isDead()             const { return m_currentHp <= 0.0f; }

        // ── 有效属性（含修改器） ────────────────────────────────────────────────
        float get(StatType stat)     const;
        float getBase(StatType stat) const;

        // ── 血量操作 ───────────────────────────────────────────────────────────
        // 返回实际扣除量（含闪避判定）
        float applyDamage(float rawDamage, bool ignoreDodge = false);
        void  heal(float amount);
        void  setHp(float hp);

        // ── 星能操作 ───────────────────────────────────────────────────────────
        // consumeStarEnergy: 不足时返回 false，不扣除
        bool consumeStarEnergy(float amount);
        void restoreStarEnergy(float amount);
        void setStarEnergy(float se);

        // ── 修改器管理 ─────────────────────────────────────────────────────────
        void addModifier(StatModifier mod);
        void removeModifier(const std::string& source, StatType stat);
        void removeAllModifiers(const std::string& source);
        bool hasModifier(const std::string& source, StatType stat) const;

        // ── 基础属性直接修改（用于升级/装备奖励） ─────────────────────────────
        BaseStats&       baseStats()       { return m_base; }
        const BaseStats& baseStats() const { return m_base; }

    protected:
        void update(float dt) override;
        void render()         override {}

    private:
        BaseStats              m_base;
        float                  m_currentHp;
        float                  m_currentStarEnergy;
        std::vector<StatModifier> m_modifiers;

        float baseOf(StatType stat) const;

        // 简单 LCG 随机（用于闪避判定，避免引入重量级 <random>）
        static uint32_t s_rng;
        static float nextRand();
    };

} // namespace game::component
