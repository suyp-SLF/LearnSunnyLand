#include "inventory.h"
#include <algorithm>

namespace game::inventory
{
    Inventory::Inventory() : _slots(CAPACITY) {}

    bool Inventory::addItem(const Item &item, int count)
    {
        // 优先堆叠到已有同类物品的格子
        for (auto &slot : _slots)
        {
            if (!slot.isEmpty() && slot.item->id == item.id)
            {
                int space = slot.item->max_stack - slot.count;
                if (space > 0)
                {
                    int add = std::min(count, space);
                    slot.count += add;
                    count -= add;
                    if (count <= 0) return true;
                }
            }
        }

        // 再找空格子
        for (auto &slot : _slots)
        {
            if (slot.isEmpty())
            {
                slot.item = item;
                slot.count = std::min(count, item.max_stack);
                count -= slot.count;
                if (count <= 0) return true;
            }
        }

        return count <= 0;
    }

    bool Inventory::removeItem(const std::string &id, int count)
    {
        for (auto &slot : _slots)
        {
            if (!slot.isEmpty() && slot.item->id == id)
            {
                int remove = std::min(count, slot.count);
                slot.count -= remove;
                if (slot.count == 0) slot.item.reset();
                count -= remove;
                if (count <= 0) return true;
            }
        }
        return count <= 0;
    }

    void Inventory::swapSlots(int a, int b)
    {
        std::swap(_slots[a], _slots[b]);
    }

    int Inventory::countItem(const std::string &id) const
    {
        int total = 0;
        for (const auto &slot : _slots)
            if (!slot.isEmpty() && slot.item->id == id)
                total += slot.count;
        return total;
    }

    EquipmentLoadout::EquipmentLoadout()
    {
        _slotTypes = {
            EquipmentSlotType::MechEngine,
            EquipmentSlotType::Armor,
            EquipmentSlotType::AccessoryA,
            EquipmentSlotType::AccessoryB,
        };
    }

    const char *EquipmentLoadout::slotTypeLabel(EquipmentSlotType type)
    {
        switch (type)
        {
        case EquipmentSlotType::MechEngine:
            return "机甲引擎";
        case EquipmentSlotType::Armor:
            return "护甲";
        case EquipmentSlotType::AccessoryA:
            return "饰品A";
        case EquipmentSlotType::AccessoryB:
            return "饰品B";
        }
        return "未知";
    }

    bool EquipmentLoadout::canEquipInSlot(const Item &item, int slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= SLOT_COUNT)
            return false;

        if (item.category != ItemCategory::Equipment)
            return false;

        return item.equip_slot == _slotTypes[slotIndex];
    }

    bool EquipmentLoadout::equipFromInventory(int slotIndex, int invIndex, Inventory &inv)
    {
        if (slotIndex < 0 || slotIndex >= SLOT_COUNT)
            return false;
        if (invIndex < 0 || invIndex >= inv.getSlotCount())
            return false;

        auto &invSlot = inv.getSlot(invIndex);
        auto &eqSlot = _slots[slotIndex];
        if (invSlot.isEmpty())
            return false;
        if (!canEquipInSlot(*invSlot.item, slotIndex))
            return false;

        // 标准装备行为：支持与当前槽位物品交换。
        std::swap(eqSlot, invSlot);
        if (!invSlot.isEmpty())
            invSlot.count = 1;
        if (!eqSlot.isEmpty())
            eqSlot.count = 1;
        return true;
    }

    bool EquipmentLoadout::unequipToInventory(int slotIndex, Inventory &inv)
    {
        if (slotIndex < 0 || slotIndex >= SLOT_COUNT)
            return false;

        auto &eqSlot = _slots[slotIndex];
        if (eqSlot.isEmpty())
            return false;

        if (inv.addItem(*eqSlot.item, eqSlot.count))
        {
            eqSlot = {};
            return true;
        }
        return false;
    }

    bool EquipmentLoadout::hasItemId(const std::string &itemId) const
    {
        for (const auto &slot : _slots)
        {
            if (!slot.isEmpty() && slot.item->id == itemId)
                return true;
        }
        return false;
    }

} // namespace game::inventory
