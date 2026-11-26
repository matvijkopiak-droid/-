#include "items.h"

static const char* SK_NAMES[SK_COUNT] = {"Фітнес", "Сила", "Спринт", "Легка хода", "Скритність", "Тупа зброя", "Холодна зброя",
                                         "Вогнепальна", "Теслярство", "Кулінарія", "Землеробство", "Перша допомога", "Збирання"};
static const char* SK_DESC[SK_COUNT] = {
    "Більше витривалості, швидше її відновлення.", "Більша вантажопідйомність і шкода в ближньому бою.",
    "Швидший біг на спринті, менша витрата сил.", "Тихіші кроки.", "Зомбі помічають вас пізніше, коли ви крадетесь.",
    "Бита та інша тупа зброя: шкода, швидкість, влучність.", "Ножі та сокири: шкода, швидкість, влучність.",
    "Влучність, швидша перезарядка, більша шкода.", "Швидше будуєте, міцніші барикади.",
    "Страви дають більше поживності.", "Більший урожай із грядок.", "Бинти лікують краще і швидше.",
    "Швидший обшук, більше здобичі."};
const char* skillName(int s) { return SK_NAMES[s]; }
const char* skillDesc(int s) { return SK_DESC[s]; }

float Skills::needed(int level) {
    static const float T[10] = {75, 150, 300, 500, 750, 1000, 1500, 2000, 3000, 4000};
    return T[clampv(level, 0, 9)];
}
bool Skills::addXp(int s, float amount) {
    if (s < 0 || s >= SK_COUNT || lvl[s] >= 10) return false;
    xp[s] += amount;
    bool up = false;
    while (lvl[s] < 10 && xp[s] >= needed(lvl[s])) { xp[s] -= needed(lvl[s]); lvl[s]++; up = true; }
    if (lvl[s] >= 10) xp[s] = 0;
    return up;
}

const char* occName(int o) {
    static const char* n[OCC_COUNT] = {"Поліцейський", "Тесля", "Медсестра", "Фермер", "Злодій"};
    return n[o];
}
const char* occDesc(int o) {
    static const char* d[OCC_COUNT] = {"Пістолет і патрони. Вогнепальна 2, Сила 1.", "Молоток, дошки, цвяхи. Теслярство 3, Сила 1.",
                                       "Аптечка й бинти. Перша допомога 3, Кулінарія 1.", "Насіння та сокира. Землеробство 3, Збирання 1.",
                                       "Ніж і ліхтарик. Легка хода 3, Скритність 2."};
    return d[o];
}

// name, type, weight, food, water, heal, dmgMin, dmgMax, range, cd, noise, skill, ammoCap, ammoItem, stack
static const ItemDef DEFS[I_COUNT] = {
    {"—", IT_MISC, 0, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, false},
    {"Консерви з квасолею", IT_FOOD, .4f, 28, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Хліб", IT_FOOD, .3f, 20, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Яблуко", IT_FOOD, .2f, 9, 4, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Чіпси", IT_FOOD, .1f, 11, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Готова страва", IT_FOOD, .5f, 42, 6, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Овочі", IT_FOOD, .2f, 12, 2, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Вода в пляшці", IT_DRINK, .5f, 0, 45, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Газований напій", IT_DRINK, .3f, 6, 18, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Порожня пляшка", IT_MISC, .1f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Бинт", IT_MED, .1f, 0, 0, 12, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Знеболювальне", IT_MED, .1f, 0, 0, 8, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Аптечка", IT_MED, .8f, 0, 0, 45, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Ганчірка", IT_MATERIAL, .05f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Дошка", IT_MATERIAL, 1.0f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Цвяхи", IT_MATERIAL, .02f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Колода", IT_MATERIAL, 3.0f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Батарейка", IT_MISC, .1f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Молоток", IT_TOOL, 1.0f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, false},
    {"Пилка", IT_TOOL, 1.0f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, false},
    {"Ліхтарик", IT_TOOL, .3f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, false},
    {"Сірники", IT_TOOL, .05f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Насіння", IT_SEED, .05f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Рюкзак", IT_MISC, .6f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, false},
    {"Бейсбольна бита", IT_MELEE, 1.5f, 0, 0, 0, 14, 22, 1.7f, .75f, 9, SK_BLUNT, 0, I_NONE, false},
    {"Ніж", IT_MELEE, .4f, 0, 0, 0, 9, 15, 1.4f, .45f, 4, SK_BLADE, 0, I_NONE, false},
    {"Сокира", IT_MELEE, 2.5f, 0, 0, 0, 22, 36, 1.8f, 1.0f, 11, SK_BLADE, 0, I_NONE, false},
    {"Пістолет", IT_GUN, 1.0f, 0, 0, 0, 30, 44, 15, .5f, 42, SK_FIREARM, 15, I_AMMO9, false},
    {"Рушниця", IT_GUN, 3.0f, 0, 0, 0, 70, 100, 10, 1.2f, 52, SK_FIREARM, 6, I_SHELLS, false},
    {"Мисливська гвинтівка", IT_GUN, 3.5f, 0, 0, 0, 85, 115, 22, 1.4f, 56, SK_FIREARM, 5, I_AMMOR, false},
    {"Патрони 9 мм", IT_AMMO, .03f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Патрони для рушниці", IT_AMMO, .06f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
    {"Гвинтівкові набої", IT_AMMO, .05f, 0, 0, 0, 0, 0, 0, 0, 0, SK_NONE, 0, I_NONE, true},
};
const ItemDef& idef(ItemId id) { return DEFS[id]; }

float Inventory::weight() const {
    float w = 0;
    for (auto& i : items) w += idef(i.id).weight * i.n;
    return w;
}
int Inventory::count(ItemId id) const {
    int c = 0;
    for (auto& i : items) if (i.id == id) c += i.n;
    return c;
}
void Inventory::add(ItemId id, int n, int ammo) {
    if (id == I_NONE || n <= 0) return;
    if (idef(id).stack) {
        for (auto& i : items) if (i.id == id) { i.n += n; return; }
    }
    if (idef(id).stack) items.push_back({id, n, 0});
    else for (int k = 0; k < n; k++) items.push_back({id, 1, ammo});
}
bool Inventory::take(ItemId id, int n) {
    if (count(id) < n) return false;
    for (size_t k = 0; k < items.size() && n > 0;) {
        if (items[k].id == id) {
            int t = std::min(n, items[k].n);
            items[k].n -= t; n -= t;
            if (items[k].n <= 0) { items.erase(items.begin() + k); continue; }
        }
        k++;
    }
    return true;
}
void Inventory::removeAt(int idx, int n) {
    if (idx < 0 || idx >= (int)items.size()) return;
    items[idx].n -= n;
    if (items[idx].n <= 0) items.erase(items.begin() + idx);
}
