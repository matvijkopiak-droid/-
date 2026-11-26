// Предмети, інвентар, навички, професії.
#pragma once
#include "common.h"

enum SkillId : uint8_t {
    SK_FITNESS, SK_STRENGTH, SK_SPRINT, SK_LIGHTFOOT, SK_SNEAK, SK_BLUNT, SK_BLADE, SK_FIREARM,
    SK_CARPENTRY, SK_COOKING, SK_FARMING, SK_FIRSTAID, SK_FORAGING, SK_COUNT, SK_NONE = 255
};
const char* skillName(int s);
const char* skillDesc(int s);

struct Skills {
    std::array<int, SK_COUNT> lvl{};
    std::array<float, SK_COUNT> xp{};
    static float needed(int level);
    bool addXp(int s, float amount);  // true якщо підвищився рівень
    int operator[](int s) const { return lvl[s]; }
};

enum Occupation : uint8_t { OCC_POLICE, OCC_CARPENTER, OCC_NURSE, OCC_FARMER, OCC_BURGLAR, OCC_COUNT };
const char* occName(int o);
const char* occDesc(int o);

enum ItemType : uint8_t { IT_FOOD, IT_DRINK, IT_MED, IT_MELEE, IT_GUN, IT_AMMO, IT_TOOL, IT_MATERIAL, IT_MISC, IT_SEED };
enum ItemId : uint8_t {
    I_NONE, I_BEANS, I_BREAD, I_APPLE, I_CHIPS, I_MEAL, I_VEG, I_WATER, I_SODA, I_BOTTLE,
    I_BANDAGE, I_PILLS, I_FIRSTAID, I_RAG, I_PLANK, I_NAILS, I_LOG, I_BATTERY,
    I_HAMMER, I_SAW, I_FLASHLIGHT, I_MATCHES, I_SEEDS, I_BACKPACK,
    I_BAT, I_KNIFE, I_AXE, I_PISTOL, I_SHOTGUN, I_RIFLE, I_AMMO9, I_SHELLS, I_AMMOR, I_COUNT
};

struct ItemDef {
    const char* name;
    ItemType type;
    float weight;
    float food, water, heal;     // їжа/вода/лікування
    float dmgMin, dmgMax, range;  // зброя (range у клітинках)
    float cd, noise;              // перезарядка між ударами, шум (радіус у клітинках)
    SkillId skill;
    int ammoCap;
    ItemId ammoItem;
    bool stack;
};
const ItemDef& idef(ItemId id);

struct Item {
    ItemId id = I_NONE;
    int n = 1;
    int ammo = 0;   // для зброї: набої в обоймі
};

struct Inventory {
    std::vector<Item> items;
    float weight() const;
    int count(ItemId id) const;
    void add(ItemId id, int n = 1, int ammo = 0);
    bool take(ItemId id, int n = 1);   // видалити n штук
    void removeAt(int idx, int n = 1);
    bool empty() const { return items.empty(); }
};
