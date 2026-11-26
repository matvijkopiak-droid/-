#include "world.h"
#include <queue>

const char* roomName(int r) {
    static const char* n[] = {"", "Вітальня", "Кухня", "Спальня", "Ванна", "Торговий зал", "Склад"};
    return n[clampv(r, 0, 6)];
}
const char* furnName(int k) {
    static const char* n[] = {"Холодильник", "Шафа", "Гардероб", "Полиця", "Прилавок", "Ящик", "Ліжко", "Стіл", "Диван", "Раковина", "Авто", "Грядка", "Вогнище", "Бочка", "Стійка зі зброєю", "Купа речей"};
    return n[k];
}
bool furnIsContainer(int k) { return k == F_PILE || k == F_FRIDGE || k == F_CUPBOARD || k == F_WARDROBE || k == F_SHELF || k == F_COUNTER || k == F_CRATE || k == F_CAR || k == F_GUNRACK; }
bool furnSolid(int k) { return k != F_FARM && k != F_FIRE && k != F_PILE; }

int World::addFurn(FurnKind k, int x, int y, int z, bool built) {
    Furn f; f.kind = k; f.x = (int16_t)x; f.y = (int16_t)y; f.z = (int8_t)z; f.built = built;
    furn.push_back(f);
    int idx = (int)furn.size() - 1;
    furnAt[tileIdx(x, y, z)] = idx;
    return idx;
}
void World::removeFurn(int idx) {
    Furn& f = furn[idx];
    if (!f.alive) return;
    f.alive = false;
    int ti = tileIdx(f.x, f.y, f.z);
    if (furnAt[ti] == idx) furnAt[ti] = -1;
}

bool World::isFloor(int x, int y, int z) const {
    if (!inMap(x, y, z)) return false;
    return at(x, y, z).kind != TK_AIR;
}
bool World::blocks(int x, int y, int z) const {
    if (!inMap(x, y, z)) return true;
    const Tile& t = at(x, y, z);
    switch (t.kind) {
        case TK_AIR: case TK_WALL: case TK_TREE: case TK_FENCE: return true;
        case TK_DOOR: case TK_WINDOW:
            if (!(t.flags & (TF_OPEN | TF_BROKEN)) || t.barr > 0) return true;
            break;
        default: break;
    }
    int fi = furnAt[tileIdx(x, y, z)];
    if (fi >= 0 && furnSolid(furn[fi].kind)) return true;
    return false;
}
bool World::opaque(int x, int y, int z) const {
    if (!inMap(x, y, z)) return true;
    const Tile& t = at(x, y, z);
    if (t.kind == TK_WALL || t.kind == TK_TREE) return true;
    if (t.kind == TK_DOOR && !(t.flags & (TF_OPEN | TF_BROKEN))) return true;
    if (t.kind == TK_DOOR && t.barr >= 2) return true;
    if (t.kind == TK_WINDOW && t.barr >= 3) return true;
    return false;
}

int World::pathCost(int x, int y, int z, int mode) const {
    if (!inMap(x, y, z)) return 0;
    const Tile& t = at(x, y, z);
    int c = 1;
    switch (t.kind) {
        case TK_WALL: if (mode == PM_ZOMBIE && (t.flags & TF_BUILT)) return 12; return 0;
        case TK_AIR: case TK_TREE: case TK_FENCE: return 0;
        case TK_DOOR:
            if (t.flags & (TF_OPEN | TF_BROKEN)) { if (t.barr > 0) return mode == PM_ZOMBIE ? 8 : 0; c = 1; }
            else if ((t.flags & TF_LOCKED) || t.barr > 0) { if (mode == PM_HUMAN) return 0; c = 8; }
            else c = 3;
            break;
        case TK_WINDOW:
            if (t.flags & (TF_OPEN | TF_BROKEN)) { if (t.barr > 0) return mode == PM_ZOMBIE ? 8 : 0; c = 3; }
            else { if (mode == PM_HUMAN) return 0; c = 10; }
            break;
        case TK_ROAD: c = 1; break;
        default: break;
    }
    int fi = furnAt[tileIdx(x, y, z)];
    if (fi >= 0 && furnSolid(furn[fi].kind)) return 0;
    return c;
}

bool World::findPath(PNode from, PNode to, std::vector<PNode>& out, int maxNodes, int mode) const {
    out.clear();
    if (!inMap(from.x, from.y, from.z) || !inMap(to.x, to.y, to.z)) return false;
    if (pathCost(to.x, to.y, to.z, mode) == 0) return false;
    const int N = MAPW * MAPH * LEVELS;
    if ((int)gcost_.size() != N) { gcost_.assign(N, 0); parent_.assign(N, -1); stamp_.assign(N, 0); curStamp_ = 0; }
    ++curStamp_;
    typedef std::pair<float, int> QE;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> open;
    auto H = [&](int x, int y, int z) {
        float dx = (float)std::abs(x - to.x), dy = (float)std::abs(y - to.y);
        return std::max(dx, dy) + 0.414f * std::min(dx, dy) + std::abs(z - to.z) * 6.f;
    };
    int s = tileIdx(from.x, from.y, from.z), t = tileIdx(to.x, to.y, to.z);
    gcost_[s] = 0; parent_[s] = -1; stamp_[s] = curStamp_;
    open.push({H(from.x, from.y, from.z), s});
    std::vector<uint8_t> closed(N, 0);
    int expanded = 0; bool found = false;
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!open.empty() && expanded < maxNodes) {
        int cur = open.top().second; open.pop();
        if (closed[cur]) continue;
        closed[cur] = 1; ++expanded;
        if (cur == t) { found = true; break; }
        int z = cur / (MAPW * MAPH), rem = cur % (MAPW * MAPH), y = rem / MAPW, x = rem % MAPW;
        auto relax = [&](int nx, int ny, int nz, float step) {
            int ni = tileIdx(nx, ny, nz);
            if (closed[ni]) return;
            int pc = pathCost(nx, ny, nz, mode); if (pc == 0) return;
            float ng = gcost_[cur] + step * pc;
            if (stamp_[ni] != curStamp_ || ng < gcost_[ni]) {
                stamp_[ni] = curStamp_; gcost_[ni] = ng; parent_[ni] = cur;
                open.push({ng + H(nx, ny, nz), ni});
            }
        };
        for (int d = 0; d < 8; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!inMap(nx, ny, z)) continue;
            if (d >= 4) {
                if (pathCost(x + DX[d], y, z, mode) == 0 || pathCost(x, y + DY[d], z, mode) == 0) continue;
                auto k = [&](int ax, int ay) { int kk = at(ax, ay, z).kind; return kk == TK_DOOR || kk == TK_WINDOW; };
                if (k(nx, ny) || k(x + DX[d], y) || k(x, y + DY[d]) || k(x, y)) continue;
                relax(nx, ny, z, 1.414f);
            } else relax(nx, ny, z, 1.f);
        }
        uint8_t kd = at(x, y, z).kind;
        if (kd == TK_STAIR_UP && z + 1 < LEVELS) relax(x, y, z + 1, 2.f);
        if (kd == TK_STAIR_DOWN && z > 0) relax(x, y, z - 1, 2.f);
    }
    if (!found) return false;
    for (int c = t; c != s && c >= 0; c = parent_[c]) {
        int z = c / (MAPW * MAPH), rem = c % (MAPW * MAPH);
        out.push_back({(int16_t)(rem % MAPW), (int16_t)(rem / MAPW), (int8_t)z});
    }
    std::reverse(out.begin(), out.end());
    return true;
}

bool World::los(float x0, float y0, float x1, float y1, int z) const {
    int ax = ftile(x0), ay = ftile(y0), bx = ftile(x1), by = ftile(y1);
    int dx = std::abs(bx - ax), dy = std::abs(by - ay), sx = ax < bx ? 1 : -1, sy = ay < by ? 1 : -1, err = dx - dy;
    int x = ax, y = ay;
    while (!(x == bx && y == by)) {
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
        if (x == bx && y == by) break;
        if (opaque(x, y, z)) return false;
    }
    return true;
}

void World::computeVisible(int px, int py, int pz, int radius, std::vector<uint8_t>& vis) {
    vis.assign(MAPW * MAPH, 0);
    if ((int)seen.size() != MAPW * MAPH * LEVELS) seen.assign(MAPW * MAPH * LEVELS, 0);
    for (int y = py - radius; y <= py + radius; y++)
        for (int x = px - radius; x <= px + radius; x++) {
            if (!inMap(x, y, pz)) continue;
            int dx = x - px, dy = y - py;
            if (dx * dx + dy * dy > radius * radius) continue;
            if (los(px + .5f, py + .5f, x + .5f, y + .5f, pz)) { vis[y * MAPW + x] = 1; seen[tileIdx(x, y, pz)] = 1; }
        }
}

// ----------------------------------------------------------------------------------------------
// Генерація
// ----------------------------------------------------------------------------------------------
struct Rect { int x0, y0, x1, y1; int area() const { return (x1 - x0 + 1) * (y1 - y0 + 1); } };

template <class T> static void shuffleVec(std::vector<T>& v, Rng& r) { std::shuffle(v.begin(), v.end(), r.g); }

void World::fillLoot(Furn& f, BType bt, int room, Rng& r) {
    typedef std::pair<ItemId, float> W;
    std::vector<W> tab; int lo = 0, hi = 3; float emptyP = .25f;
    auto F = [&](std::initializer_list<W> l) { tab.assign(l.begin(), l.end()); };
    switch (f.kind) {
        case F_FRIDGE: F({{I_APPLE, 3}, {I_VEG, 2}, {I_SODA, 3}, {I_WATER, 3}, {I_MEAL, 1}, {I_BREAD, 2}}); hi = 3; emptyP = .3f; break;
        case F_CUPBOARD:
            if (room == R_BATH) F({{I_BANDAGE, 3}, {I_PILLS, 3}, {I_RAG, 2}, {I_FIRSTAID, .6f}});
            else F({{I_BEANS, 4}, {I_BREAD, 2}, {I_CHIPS, 3}, {I_SODA, 2}, {I_WATER, 2}, {I_MATCHES, 1}, {I_BOTTLE, 1}});
            hi = 3; break;
        case F_WARDROBE: F({{I_RAG, 5}, {I_BATTERY, 1.5f}, {I_FLASHLIGHT, .7f}, {I_BACKPACK, .5f}, {I_MATCHES, 1}, {I_KNIFE, .4f}}); hi = 3; emptyP = .35f; break;
        case F_COUNTER:
            if (bt == B_HOUSE || bt == B_APARTMENT) F({{I_KNIFE, 1}, {I_BEANS, 2}, {I_BREAD, 2}, {I_BOTTLE, 2}, {I_RAG, 2}});
            else F({{I_CHIPS, 2}, {I_SODA, 2}, {I_BATTERY, 2}, {I_MATCHES, 2}, {I_BANDAGE, 1}});
            hi = 2; break;
        case F_GUNRACK:
            F({{I_AMMO9, 4}, {I_SHELLS, 3}, {I_AMMOR, 2}, {I_PISTOL, 2}, {I_SHOTGUN, 1.2f}, {I_RIFLE, 1}, {I_KNIFE, 1.5f}, {I_BAT, 1}});
            lo = 1; hi = 3; emptyP = .08f; break;
        case F_CRATE:
            F({{I_PLANK, 4}, {I_NAILS, 3}, {I_HAMMER, 1}, {I_SAW, 1}, {I_AXE, .6f}, {I_LOG, 1}, {I_BEANS, 1}, {I_RAG, 1}, {I_BATTERY, 1}});
            lo = 1; hi = 4; emptyP = .2f; break;
        case F_CAR: F({{I_RAG, 2}, {I_BATTERY, 2}, {I_WATER, 2}, {I_FIRSTAID, .8f}, {I_HAMMER, 1}, {I_BAT, .6f}, {I_CHIPS, 2}, {I_AMMO9, .6f}}); hi = 2; emptyP = .4f; break;
        case F_SHELF:
        default:
            switch (bt) {
                case B_SUPER: F({{I_BEANS, 5}, {I_BREAD, 3}, {I_CHIPS, 3}, {I_SODA, 3}, {I_WATER, 3}, {I_APPLE, 2}, {I_BOTTLE, 1}, {I_SEEDS, .6f}}); hi = 4; emptyP = .15f; break;
                case B_SHOP: F({{I_CHIPS, 3}, {I_SODA, 3}, {I_BATTERY, 2}, {I_RAG, 2}, {I_MATCHES, 2}, {I_BEANS, 2}, {I_FLASHLIGHT, .7f}, {I_HAMMER, .6f}, {I_SEEDS, .6f}}); hi = 3; break;
                case B_PHARMACY: F({{I_BANDAGE, 5}, {I_PILLS, 4}, {I_FIRSTAID, 1.5f}, {I_RAG, 1}}); hi = 4; emptyP = .12f; break;
                case B_GAS: F({{I_CHIPS, 4}, {I_SODA, 4}, {I_WATER, 3}, {I_MATCHES, 2}, {I_BATTERY, 2}}); hi = 3; break;
                case B_WAREHOUSE: F({{I_PLANK, 4}, {I_NAILS, 3}, {I_HAMMER, 1}, {I_SAW, 1}, {I_LOG, 1.5f}, {I_BATTERY, 1}, {I_RAG, 1}}); hi = 4; break;
                case B_GUN: F({{I_AMMO9, 3}, {I_SHELLS, 2}, {I_KNIFE, 1}, {I_BAT, .6f}, {I_FLASHLIGHT, .6f}}); hi = 3; break;
                default: F({{I_BATTERY, 2}, {I_NAILS, 2}, {I_RAG, 3}, {I_CHIPS, 2}, {I_BEANS, 1}, {I_BOTTLE, 1}, {I_HAMMER, .3f}, {I_PILLS, .5f}}); hi = 2; emptyP = .4f; break;
            }
            break;
    }
    if (r.chance(emptyP)) return;
    float total = 0; for (auto& w : tab) total += w.second;
    int n = r.range(std::max(lo, 1), std::max(hi, 1));
    for (int i = 0; i < n; i++) {
        float p = r.f() * total; ItemId pick = tab[0].first;
        for (auto& w : tab) { if (p < w.second) { pick = w.first; break; } p -= w.second; }
        const ItemDef& d = idef(pick);
        int cnt = 1;
        if (pick == I_NAILS) cnt = r.range(6, 20);
        else if (pick == I_AMMO9 || pick == I_AMMOR) cnt = r.range(5, 16);
        else if (pick == I_SHELLS) cnt = r.range(4, 10);
        else if (pick == I_PLANK) cnt = r.range(1, 3);
        else if (d.stack && d.type == IT_FOOD) cnt = r.chance(.25f) ? 2 : 1;
        else if (pick == I_RAG) cnt = r.range(1, 3);
        int ammo = d.type == IT_GUN ? r.range(0, d.ammoCap / 2) : 0;
        f.inv.add(pick, cnt, ammo);
    }
}

int World::addBuilding(BType type, int bx, int by, int bw, int bh, int levels, int doorSide, Rng& r) {
    static const char* NAMES[] = {"Будинок", "Магазин", "Супермаркет", "Аптека", "Мисливський магазин", "АЗС", "Склад", "Багатоповерхівка"};
    Building b; b.x = bx; b.y = by; b.w = bw; b.h = bh; b.levels = levels; b.type = type; b.name = NAMES[type];
    int id = (int)buildings.size();
    bool resid = (type == B_HOUSE || type == B_APARTMENT);
    uint8_t fk = resid ? TK_FLOOR_W : (type == B_WAREHOUSE ? TK_FLOOR_C : TK_FLOOR_T);
    for (int z = 0; z < levels; z++)
        for (int y = by; y < by + bh; y++)
            for (int x = bx; x < bx + bw; x++) {
                Tile& t = at(x, y, z); t.bld = (int16_t)id; t.hp = 0;
                if (x == bx || x == bx + bw - 1 || y == by || y == by + bh - 1) { t.kind = TK_WALL; t.hp = 250; t.flags = 0; }
                else { t.kind = fk; t.flags = TF_INDOOR; }
            }
    // перегородки (BSP)
    std::vector<Rect> rooms;
    std::function<void(int, int, int, int, int)> split = [&](int x0, int y0, int x1, int y1, int depth) {
        int w = x1 - x0 + 1, h = y1 - y0 + 1;
        bool openType = (type == B_SUPER || type == B_WAREHOUSE);
        bool shopType = (type == B_SHOP || type == B_PHARMACY || type == B_GUN || type == B_GAS);
        bool canV = w >= 7, canH = h >= 7;
        if (openType || (shopType && depth >= 1)) canV = canH = false;
        if (shopType && depth == 0) { canV = w >= 11; canH = false; }
        if ((!canV && !canH) || (depth > 0 && (w * h <= 22 || r.chance(.2f)))) { rooms.push_back({x0, y0, x1, y1}); return; }
        bool vertical = canV && (!canH || w > h || (w == h && r.chance(.5f)));
        if (vertical) {
            int s = r.range(x0 + 3, x1 - 3), dy = r.range(y0, y1);
            for (int z = 0; z < levels; z++)
                for (int y = y0; y <= y1; y++) {
                    Tile& t = at(s, y, z);
                    if (y == dy) { t.kind = TK_DOOR; t.hp = 60; t.flags = TF_INDOOR; }
                    else { t.kind = TK_WALL; t.hp = 200; t.flags = TF_INDOOR; }
                }
            split(x0, y0, s - 1, y1, depth + 1); split(s + 1, y0, x1, y1, depth + 1);
        } else {
            int s = r.range(y0 + 3, y1 - 3), dx = r.range(x0, x1);
            for (int z = 0; z < levels; z++)
                for (int x = x0; x <= x1; x++) {
                    Tile& t = at(x, s, z);
                    if (x == dx) { t.kind = TK_DOOR; t.hp = 60; t.flags = TF_INDOOR; }
                    else { t.kind = TK_WALL; t.hp = 200; t.flags = TF_INDOOR; }
                }
            split(x0, y0, x1, s - 1, depth + 1); split(x0, s + 1, x1, y1, depth + 1);
        }
    };
    split(bx + 1, by + 1, bx + bw - 2, by + bh - 2, 0);

    // типи кімнат
    std::vector<int> order(rooms.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [&](int a, int c) { return rooms[a].area() > rooms[c].area(); });
    std::vector<std::vector<uint8_t>> rtype(levels, std::vector<uint8_t>(rooms.size(), R_BEDROOM));
    for (int z = 0; z < levels; z++) {
        for (size_t k = 0; k < order.size(); k++) {
            int ri = order[k]; uint8_t t;
            if (resid) {
                if (z == 0) t = k == 0 ? R_LIVING : (k == 1 ? R_KITCHEN : (k + 1 == order.size() && order.size() >= 3 ? R_BATH : R_BEDROOM));
                else t = (k + 1 == order.size() && order.size() >= 2) ? R_BATH : R_BEDROOM;
            } else t = k == 0 ? R_SALES : R_STORAGE;
            rtype[z][ri] = t;
        }
        for (size_t ri = 0; ri < rooms.size(); ri++)
            for (int y = rooms[ri].y0; y <= rooms[ri].y1; y++)
                for (int x = rooms[ri].x0; x <= rooms[ri].x1; x++) at(x, y, z).room = rtype[z][ri];
    }

    // сходи
    std::vector<std::pair<int, int>> reserved;
    auto nearDoor = [&](int x, int y, int z) {
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (at(x + dx, y + dy, z).kind == TK_DOOR) return true;
        return false;
    };
    if (levels > 1) {
        std::vector<std::pair<int, int>> cand;
        for (int y = by + 1; y < by + bh - 1; y++)
            for (int x = bx + 1; x < bx + bw - 1; x++) {
                if (at(x, y, 0).kind != TK_FLOOR_W && at(x, y, 0).kind != TK_FLOOR_T) continue;
                bool wallAdj = at(x + 1, y, 0).kind == TK_WALL || at(x - 1, y, 0).kind == TK_WALL || at(x, y + 1, 0).kind == TK_WALL || at(x, y - 1, 0).kind == TK_WALL;
                if (wallAdj && !nearDoor(x, y, 0)) cand.push_back({x, y});
            }
        shuffleVec(cand, r);
        std::vector<std::pair<int, int>> used;
        for (int k = 0; k < levels - 1; k++) {
            for (auto& c : cand) {
                bool ok = true;
                for (auto& u : used) if (std::abs(u.first - c.first) + std::abs(u.second - c.second) < 4) ok = false;
                if (!ok || nearDoor(c.first, c.second, k + 1)) continue;
                at(c.first, c.second, k).kind = TK_STAIR_UP;
                at(c.first, c.second, k + 1).kind = TK_STAIR_DOWN;
                used.push_back(c); reserved.push_back(c); break;
            }
        }
    }

    // зовнішні двері
    struct Cand { int x, y, side; };
    auto tryDoor = [&](int side) -> bool {
        std::vector<Cand> cs;
        if (side == 0 || side == 1) {
            int y = side == 0 ? by : by + bh - 1, iy = side == 0 ? y + 1 : y - 1;
            for (int x = bx + 1; x < bx + bw - 1; x++) {
                uint8_t k = at(x, iy, 0).kind;
                if ((k == TK_FLOOR_W || k == TK_FLOOR_T || k == TK_FLOOR_C) && at(x, iy, 0).room != 0) cs.push_back({x, y, side});
            }
        } else {
            int x = side == 2 ? bx + bw - 1 : bx, ix = side == 2 ? x - 1 : x + 1;
            for (int y = by + 1; y < by + bh - 1; y++) {
                uint8_t k = at(ix, y, 0).kind;
                if ((k == TK_FLOOR_W || k == TK_FLOOR_T || k == TK_FLOOR_C) && at(ix, y, 0).room != 0) cs.push_back({x, y, side});
            }
        }
        if (cs.empty()) return false;
        Cand c = cs[cs.size() / 2];
        Tile& t = at(c.x, c.y, 0); t.kind = TK_DOOR; t.hp = 60; t.flags = TF_INDOOR;
        if (r.chance(.35f)) t.flags |= TF_LOCKED;
        b.doorX = c.x; b.doorY = c.y;
        return true;
    };
    int sides[4] = {doorSide, doorSide ^ 1, 2, 3};
    for (int s = 0; s < 4; s++) if (tryDoor(sides[s])) break;

    // вікна
    for (int z = 0; z < levels; z++) {
        int step = (type == B_HOUSE || type == B_APARTMENT) ? 3 : 2;
        for (int x = bx + 1; x < bx + bw - 1; x++) {
            if ((x - bx) % step != 1) continue;
            for (int side = 0; side < 2; side++) {
                int y = side == 0 ? by : by + bh - 1, iy = side == 0 ? y + 1 : y - 1;
                if (at(x, y, z).kind == TK_WALL && at(x, iy, z).kind != TK_WALL && at(x, iy, z).kind != TK_DOOR && at(x, iy, z).kind != TK_AIR) {
                    Tile& t = at(x, y, z); t.kind = TK_WINDOW; t.hp = 20;
                }
            }
        }
        for (int y = by + 1; y < by + bh - 1; y++) {
            if ((y - by) % step != 1) continue;
            for (int side = 0; side < 2; side++) {
                int x = side == 0 ? bx : bx + bw - 1, ix = side == 0 ? x + 1 : x - 1;
                if (at(x, y, z).kind == TK_WALL && at(ix, y, z).kind != TK_WALL && at(ix, y, z).kind != TK_DOOR && at(ix, y, z).kind != TK_AIR) {
                    Tile& t = at(x, y, z); t.kind = TK_WINDOW; t.hp = 20;
                }
            }
        }
    }

    // меблі
    auto freeSpot = [&](int x, int y, int z) {
        if (!inMap(x, y, z)) return false;
        uint8_t k = at(x, y, z).kind;
        if (k != TK_FLOOR_W && k != TK_FLOOR_T && k != TK_FLOOR_C) return false;
        if (furnAt[tileIdx(x, y, z)] >= 0) return false;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            uint8_t kk = at(x + dx, y + dy, z).kind;
            if (kk == TK_DOOR || kk == TK_STAIR_UP || kk == TK_STAIR_DOWN) return false;
        }
        return true;
    };
    auto putFurn = [&](FurnKind k, int x, int y, int z, int room) {
        int idx = addFurn(k, x, y, z, false);
        if (furnIsContainer(k)) fillLoot(furn[idx], type, room, r);
    };
    for (int z = 0; z < levels; z++)
        for (size_t ri = 0; ri < rooms.size(); ri++) {
            Rect& rc = rooms[ri]; int room = rtype[z][ri];
            std::vector<std::pair<int, int>> wallTiles, inner;
            for (int y = rc.y0; y <= rc.y1; y++)
                for (int x = rc.x0; x <= rc.x1; x++) {
                    if (!freeSpot(x, y, z)) continue;
                    bool wa = false;
                    for (int d = 0; d < 4; d++) {
                        int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3);
                        uint8_t k = at(nx, ny, z).kind;
                        if (k == TK_WALL || k == TK_WINDOW) wa = true;
                    }
                    if (wa) wallTiles.push_back({x, y}); else inner.push_back({x, y});
                }
            shuffleVec(wallTiles, r); shuffleVec(inner, r);
            auto takeWall = [&](FurnKind k) {
                while (!wallTiles.empty()) {
                    auto p = wallTiles.back(); wallTiles.pop_back();
                    if (!freeSpot(p.first, p.second, z)) continue;
                    putFurn(k, p.first, p.second, z, room); return true;
                }
                return false;
            };
            auto takeInner = [&](FurnKind k) {
                while (!inner.empty()) {
                    auto p = inner.back(); inner.pop_back();
                    if (!freeSpot(p.first, p.second, z)) continue;
                    putFurn(k, p.first, p.second, z, room); return true;
                }
                return false;
            };
            int area = rc.area();
            if (resid) {
                if (room == R_LIVING) { takeWall(F_SOFA); takeWall(F_SHELF); if (area > 14) takeInner(F_TABLE); if (r.chance(.4f)) takeWall(F_CUPBOARD); if (levels == 1 || z == 0) if (ri == 0 && r.chance(.5f)) takeWall(F_COUNTER); }
                else if (room == R_KITCHEN) { takeWall(F_FRIDGE); takeWall(F_CUPBOARD); if (area > 12) takeWall(F_CUPBOARD); takeWall(F_COUNTER); takeWall(F_SINK); if (area > 14) takeInner(F_TABLE); }
                else if (room == R_BEDROOM) { takeWall(F_BED); takeWall(F_WARDROBE); if (r.chance(.4f)) takeWall(F_SHELF); }
                else if (room == R_BATH) { takeWall(F_CUPBOARD); takeWall(F_SINK); }
            } else if (type == B_WAREHOUSE) {
                int n = std::min(24, area / 6);
                for (int i = 0; i < n; i++) takeInner(i % 3 == 0 ? F_SHELF : F_CRATE);
                for (int i = 0; i < 4; i++) takeWall(F_CRATE);
            } else if (room == R_SALES) {
                if (type == B_GUN) { for (int i = 0; i < 4; i++) takeWall(F_GUNRACK); takeWall(F_COUNTER); takeWall(F_COUNTER); takeInner(F_GUNRACK); }
                else {
                    int rows = type == B_SUPER ? 99 : (type == B_GAS ? 1 : 2);
                    int done = 0;
                    for (int y = rc.y0 + 2; y <= rc.y1 - 1 && done < rows; y += 3, done++)
                        for (int x = rc.x0 + 1; x <= rc.x1 - 1; x++) if ((x - rc.x0) % 6 != 5 && freeSpot(x, y, z)) putFurn(F_SHELF, x, y, z, room);
                    takeWall(F_FRIDGE); takeWall(F_FRIDGE); takeWall(F_COUNTER); takeWall(F_COUNTER);
                    if (type == B_PHARMACY) { takeWall(F_SHELF); takeWall(F_SHELF); }
                }
            } else {  // склад магазину
                takeWall(F_CRATE); takeWall(F_CRATE); takeWall(F_SHELF); if (area > 10) takeWall(F_CRATE);
            }
        }
    b.name = b.name; buildings.push_back(b);
    return id;
}

void World::generate(uint32_t seed) {
    Rng r(seed);
    tiles.assign(MAPW * MAPH * LEVELS, Tile{});
    furn.clear(); furn.reserve(2000);
    furnAt.assign(MAPW * MAPH * LEVELS, -1);
    seen.assign(MAPW * MAPH * LEVELS, 0);
    buildings.clear(); indoorTiles.clear(); outdoorTiles.clear(); survivorSpots.clear();
    for (int y = 0; y < MAPH; y++)
        for (int x = 0; x < MAPW; x++) {
            Tile& t = at(x, y, 0); t.kind = TK_GRASS;
            int lx = x % 24, ly = y % 24; bool rx = lx < 4, ry = ly < 4;
            if (rx || ry) { bool road = (rx && (lx == 1 || lx == 2)) || (ry && (ly == 1 || ly == 2)); t.kind = road ? TK_ROAD : TK_SIDEWALK; }
            if (x == 0 || y == 0 || x == MAPW - 1 || y == MAPH - 1) t.kind = TK_FENCE;
        }
    enum Lot { L_HOUSES, L_SHOP, L_SUPER, L_PHARM, L_GUN, L_GAS, L_WARE, L_APART, L_PARK };
    std::vector<Lot> lots = {L_HOUSES, L_HOUSES, L_HOUSES, L_HOUSES, L_HOUSES, L_HOUSES, L_SHOP, L_SHOP, L_SUPER, L_PHARM, L_GUN, L_GAS, L_WARE, L_APART, L_APART, L_PARK};
    shuffleVec(lots, r);
    std::vector<int> houseIds;
    auto trees = [&](int x0, int y0, int x1, int y1, int n) {
        for (int i = 0; i < n; i++) {
            int x = r.range(x0, x1), y = r.range(y0, y1);
            if (inMap(x, y, 0) && at(x, y, 0).kind == TK_GRASS && at(x, y, 0).bld < 0) at(x, y, 0).kind = TK_TREE;
        }
    };
    int li = 0;
    for (int cy = 0; cy < 4; cy++)
        for (int cx = 0; cx < 4; cx++, li++) {
            int lx0 = cx * 24 + 4, ly0 = cy * 24 + 4, lw = std::min(20, MAPW - 1 - lx0), lh = std::min(20, MAPH - 1 - ly0);
            switch (lots[li]) {
                case L_HOUSES:
                    for (int half = 0; half < 2; half++) {
                        int w = r.range(9, 12), h = r.range(7, 8);
                        int x = r.range(lx0 + 1, lx0 + lw - w - 1), y = half == 0 ? ly0 + 1 : ly0 + 11;
                        if (y + h >= ly0 + lh) y = ly0 + lh - h - 1;
                        int levels = r.chance(.5f) ? 2 : 1;
                        houseIds.push_back(addBuilding(B_HOUSE, x, y, w, h, levels, half == 0 ? 0 : 1, r));
                    }
                    trees(lx0, ly0, lx0 + lw - 1, ly0 + lh - 1, 10);
                    break;
                case L_SHOP: addBuilding(B_SHOP, lx0 + 2, ly0 + 2, 16, 10, 1, 0, r); trees(lx0, ly0 + 13, lx0 + lw - 1, ly0 + lh - 1, 5); break;
                case L_SUPER: addBuilding(B_SUPER, lx0 + 1, ly0 + 2, 18, 13, 1, 0, r); break;
                case L_PHARM: addBuilding(B_PHARMACY, lx0 + 4, ly0 + 3, 11, 8, 1, 0, r); trees(lx0, ly0 + 12, lx0 + lw - 1, ly0 + lh - 1, 8); break;
                case L_GUN: addBuilding(B_GUN, lx0 + 4, ly0 + 3, 11, 8, 1, 0, r); trees(lx0, ly0 + 12, lx0 + lw - 1, ly0 + lh - 1, 8); break;
                case L_GAS: addBuilding(B_GAS, lx0 + 5, ly0 + 4, 9, 6, 1, 0, r); break;
                case L_WARE: addBuilding(B_WAREHOUSE, lx0 + 1, ly0 + 3, 18, 12, 1, 0, r); break;
                case L_APART: addBuilding(B_APARTMENT, lx0 + 3, ly0 + 3, 14, 12, 3, 0, r); trees(lx0, ly0 + 16, lx0 + lw - 1, ly0 + lh - 1, 6); break;
                case L_PARK: trees(lx0, ly0, lx0 + lw - 1, ly0 + lh - 1, 90); break;
            }
        }
    // машини на дорогах
    int cars = 0;
    for (int tries = 0; tries < 400 && cars < 18; tries++) {
        int x = r.range(2, MAPW - 3), y = r.range(2, MAPH - 3);
        if (at(x, y, 0).kind != TK_ROAD || (x % 24 < 4 && y % 24 < 4)) continue;
        if (furnAt[tileIdx(x, y, 0)] >= 0) continue;
        bool nearCar = false;
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) if (inMap(x + dx, y + dy, 0) && furnAt[tileIdx(x + dx, y + dy, 0)] >= 0) nearCar = true;
        if (nearCar) continue;
        int idx = addFurn(F_CAR, x, y, 0, false); fillLoot(furn[idx], B_HOUSE, 0, r); cars++;
    }
    // старт
    startBuilding = houseIds.empty() ? 0 : houseIds[r.range(0, (int)houseIds.size() - 1)];
    {
        Building& sb = buildings[startBuilding]; Vec2 best{(float)sb.x + 2, (float)sb.y + 2};
        for (int y = sb.y + 1; y < sb.y + sb.h - 1; y++) for (int x = sb.x + 1; x < sb.x + sb.w - 1; x++)
            if (at(x, y, 0).room == R_LIVING && !blocks(x, y, 0) && at(x, y, 0).kind != TK_STAIR_UP) { best = {x + .5f, y + .5f}; goto found; }
        for (int y = sb.y + 1; y < sb.y + sb.h - 1; y++) for (int x = sb.x + 1; x < sb.x + sb.w - 1; x++)
            if (!blocks(x, y, 0) && at(x, y, 0).kind != TK_STAIR_UP) { best = {x + .5f, y + .5f}; goto found; }
    found:
        playerSpawn = best;
        // дати гарантовану вільну клітинку навколо
    }
    // списки клітинок для спавну
    for (int z = 0; z < LEVELS; z++)
        for (int y = 1; y < MAPH - 1; y++)
            for (int x = 1; x < MAPW - 1; x++) {
                const Tile& t = at(x, y, z);
                if (blocks(x, y, z) || t.kind == TK_STAIR_UP || t.kind == TK_STAIR_DOWN) continue;
                if (t.flags & TF_INDOOR) { if (t.bld != startBuilding) indoorTiles.push_back({(int16_t)x, (int16_t)y, (int8_t)z}); }
                else if (z == 0) outdoorTiles.push_back({(int16_t)x, (int16_t)y, 0});
            }
    // місця схованих вцілілих
    std::vector<int> ids; for (size_t i = 0; i < buildings.size(); i++) if ((int)i != startBuilding) ids.push_back((int)i);
    shuffleVec(ids, r);
    for (size_t k = 0; k < ids.size() && survivorSpots.size() < 8; k++) {
        Building& b = buildings[ids[k]];
        for (int tries = 0; tries < 40; tries++) {
            int x = r.range(b.x + 1, b.x + b.w - 2), y = r.range(b.y + 1, b.y + b.h - 2);
            if (!blocks(x, y, 0) && at(x, y, 0).kind != TK_STAIR_UP && (at(x, y, 0).flags & TF_INDOOR)) { survivorSpots.push_back({(int16_t)x, (int16_t)y, 0}); break; }
        }
    }
}
