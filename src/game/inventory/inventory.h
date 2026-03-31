#pragma once
#include <string>
#include <vector>
#include <optional>
#include <array>

namespace game::inventory
{
    enum class ItemCategory { Misc, Weapon, Consumable, Material, StarSkill, Equipment };

    enum class EquipmentSlotType
    {
        MechEngine,
        Armor,
        AccessoryA,
        AccessoryB,
    };

    struct Item
    {
        std::string id;
        std::string name;
        int max_stack = 99;
        ItemCategory category = ItemCategory::Misc;
        EquipmentSlotType equip_slot = EquipmentSlotType::AccessoryA;
    };

    struct InventorySlot
    {
        std::optional<Item> item;
        int count = 0;

        bool isEmpty() const { return !item.has_value() || count == 0; }
    };

    class Inventory
    {
    public:
        static constexpr int COLS = 10;
        static constexpr int ROWS = 10;
        static constexpr int CAPACITY = COLS * ROWS;

        Inventory();

        // 添加物品，优先堆叠同类格子，返回是否完全放入
        bool addItem(const Item &item, int count = 1);

        // 移除指定ID物品，返回是否完全移除
        bool removeItem(const std::string &id, int count = 1);

        // 统计指定ID物品的总数量
        int countItem(const std::string &id) const;

        // 交换两个格子
        void swapSlots(int a, int b);

        InventorySlot &getSlot(int index) { return _slots[index]; }
        const InventorySlot &getSlot(int index) const { return _slots[index]; }
        int getSlotCount() const { return CAPACITY; }

    private:
        std::vector<InventorySlot> _slots;
    };

    class EquipmentLoadout
    {
    public:
        static constexpr int SLOT_COUNT = 4;

        EquipmentLoadout();

        InventorySlot &getSlot(int index) { return _slots[index]; }
        const InventorySlot &getSlot(int index) const { return _slots[index]; }

        EquipmentSlotType getSlotType(int index) const { return _slotTypes[index]; }
        static const char *slotTypeLabel(EquipmentSlotType type);

        bool canEquipInSlot(const Item &item, int slotIndex) const;
        bool equipFromInventory(int slotIndex, int invIndex, Inventory &inv);
        bool unequipToInventory(int slotIndex, Inventory &inv);

        bool hasItemId(const std::string &itemId) const;

    private:
        std::array<EquipmentSlotType, SLOT_COUNT> _slotTypes{};
        std::array<InventorySlot, SLOT_COUNT> _slots{};
    };

} // namespace game::inventory
