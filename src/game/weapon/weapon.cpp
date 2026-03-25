#include "weapon.h"
#include <unordered_map>
#include <algorithm>

namespace game::weapon
{
    // ------------------------------------------------------------------
    // 全武器定义表（可随时扩展）
    // 格式：id, name_key, attack_type, damage, attack_speed, range, ammo, icon, desc_key
    // ------------------------------------------------------------------
    static const std::unordered_map<std::string, WeaponDef> s_defs = {
        {"alloy_greatsword",
         {"alloy_greatsword", "weapon.alloy_greatsword", AttackType::Melee,
          160, 1.15f, 118.f, 0, "[巨剑]", "weapon.alloy_greatsword.desc"}},
    };

    const WeaponDef* getWeaponDef(const std::string& id)
    {
        auto it = s_defs.find(id);
        return it != s_defs.end() ? &it->second : nullptr;
    }

    bool isWeaponId(const std::string& id)
    {
        return s_defs.find(id) != s_defs.end();
    }

    // ------------------------------------------------------------------
    //  WeaponBar 实现
    // ------------------------------------------------------------------
    void WeaponBar::equipFromInventory(int bar_slot, int inv_slot, inventory::Inventory& inv)
    {
        auto& inv_s  = inv.getSlot(inv_slot);
        auto& bar_s  = _slots[bar_slot];

        // 若背包格是空的，什么都不做
        if (inv_s.isEmpty()) return;

        // 只允许武器进武器栏
        if (inv_s.item->category != inventory::ItemCategory::Weapon) return;

        // 武器栏有武器，交换
        std::swap(bar_s, inv_s);
    }

    void WeaponBar::unequipToInventory(int bar_slot, inventory::Inventory& inv)
    {
        auto& bar_s = _slots[bar_slot];
        if (bar_s.isEmpty()) return;

        if (inv.addItem(*bar_s.item, bar_s.count))
            bar_s = {};
    }

} // namespace game::weapon
