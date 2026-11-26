#include "game.h"

const char* jobName(int j) {
    static const char* n[J_COUNT] = {"Йти за мною", "Відпочивати", "Охороняти пост", "Збирати припаси", "Працювати на грядках", "Будувати"};
    return n[j];
}
static const BuildDef BDEFS[BK_COUNT] = {
    {"Стіна", {{I_PLANK, 2}, {I_NAILS, 4}}, 6, "Міцна дерев'яна стіна. Зомбі можуть її зламати."},
    {"Двері", {{I_PLANK, 3}, {I_NAILS, 6}}, 8, "Двері, які можна замикати зсередини (Shift+E)."},
    {"Скриня", {{I_PLANK, 4}, {I_NAILS, 8}}, 8, "Спільний склад колонії."},
    {"Ліжко", {{I_PLANK, 3}, {I_RAG, 4}}, 8, "Швидше відновлює сили."},
    {"Грядка", {{I_PLANK, 1}}, 4, "Вирощування овочів із насіння."},
    {"Вогнище", {{I_LOG, 2}}, 5, "Готування їжі. Світить уночі."},
    {"Бочка з водою", {{I_PLANK, 3}, {I_NAILS, 4}}, 6, "Збирає дощову воду."},
};
const BuildDef& buildDef(int k) { return BDEFS[k]; }

const std::vector<Recipe>& recipes() {
    static const std::vector<Recipe> R = {
        {"Бинт", {{I_RAG, 2}}, I_BANDAGE, 1, SK_FIRSTAID, 0, false, I_NONE, 3},
        {"Дошки (3)", {{I_LOG, 1}}, I_PLANK, 3, SK_CARPENTRY, 0, false, I_SAW, 5},
        {"Бита", {{I_PLANK, 2}, {I_NAILS, 4}}, I_BAT, 1, SK_CARPENTRY, 1, false, I_HAMMER, 6},
        {"Страва з консерви", {{I_BEANS, 1}, {I_BREAD, 1}}, I_MEAL, 1, SK_COOKING, 0, true, I_NONE, 6},
        {"Овочева страва", {{I_VEG, 3}}, I_MEAL, 1, SK_COOKING, 0, true, I_NONE, 6},
        {"Аптечка", {{I_BANDAGE, 3}, {I_PILLS, 1}}, I_FIRSTAID, 1, SK_FIRSTAID, 2, false, I_NONE, 5},
    };
    return R;
}

std::string Game::clockStr() const {
    int m = (int)std::fmod(minutes, 1440.0), h = m / 60, mm = m % 60;
    char b[64]; snprintf(b, sizeof b, "День %d, %02d:%02d", day(), h, mm);
    return b;
}
float Game::darkness() const {
    float h = hour();
    if (h >= 7 && h <= 19) return 0;
    if (h > 19 && h < 21.5f) return (h - 19) / 2.5f;
    if (h >= 21.5f || h < 5) return 1;
    return 1 - (h - 5) / 2.f;   // 5..7
}
void Game::say(const std::string& s) { log.push_back(s); while (log.size() > 60) log.pop_front(); }
void Game::snd(Sfx id, Vec2 p, int z, float vol) { if (sounds.size() < 64) sounds.push_back({id, p, z, vol}); }
float Game::carryCap() const { return 12 + 1.5f * pl.sk[SK_STRENGTH] + (pl.inv.count(I_BACKPACK) ? 8.f : 0.f); }
Colonist* Game::colonist(int id) { for (auto& c : cols) if (c.id == id) return &c; return nullptr; }
Vec2 Game::aimDir(Vec2 aim) const {
    Vec2 d = aim - pl.pos;
    if (d.len() < 0.05f) return {std::cos(pl.face), std::sin(pl.face)};
    return d.norm();
}
void Game::gainSkill(int s, float xp) {
    if (s < 0 || s >= SK_COUNT) return;
    if (pl.sk.addXp(s, xp)) { say(std::string("Навичка підвищена: ") + skillName(s) + " " + std::to_string(pl.sk[s])); snd(SFX_LEVELUP, pl.pos, pl.z, .8f); }
}

// ------------------------------------------------------------------ нова гра
void Game::newGame(uint32_t sd, Occupation occ, const std::string& name) {
    seed = sd; rng = Rng(sd * 7u + 13u);
    world.generate(sd);
    zs.clear(); cols.clear(); survivors.clear(); bps.clear(); log.clear(); sounds.clear(); vis.assign(MAPW * MAPH, 0);
    pl = Player(); pl.name = name.empty() ? "Гравець" : name; pl.occ = occ;
    pl.pos = world.playerSpawn; pl.z = 0; pl.id = 0;
    minutes = 8 * 60; speedIdx = 1; gameOver = false; sleeping = false; act = Action(); hover = Interact();
    openContainer = -1; kills = 0; nextHordeDay = 3; nextId = 1; heliDone = false; hordeAnnounced = false;
    acc_ = 0; visT_ = 0; spawnT_ = 0; moveIn_ = {0, 0};
    baseCenter = pl.pos; baseRadius = 16;
    pl.inv.add(I_BEANS, 1); pl.inv.add(I_BREAD, 1); pl.inv.add(I_WATER, 1); pl.inv.add(I_RAG, 2);
    pl.held = {I_BAT, 1, 0};
    switch (occ) {
        case OCC_POLICE: pl.held = {I_PISTOL, 1, 15}; pl.inv.add(I_AMMO9, 15); pl.inv.add(I_BAT); pl.sk.lvl[SK_FIREARM] = 2; pl.sk.lvl[SK_STRENGTH] = 1; break;
        case OCC_CARPENTER: pl.inv.add(I_HAMMER); pl.inv.add(I_SAW); pl.inv.add(I_PLANK, 6); pl.inv.add(I_NAILS, 20); pl.sk.lvl[SK_CARPENTRY] = 3; pl.sk.lvl[SK_STRENGTH] = 1; break;
        case OCC_NURSE: pl.inv.add(I_FIRSTAID); pl.inv.add(I_BANDAGE, 4); pl.inv.add(I_PILLS, 2); pl.sk.lvl[SK_FIRSTAID] = 3; pl.sk.lvl[SK_COOKING] = 1; break;
        case OCC_FARMER: pl.held = {I_AXE, 1, 0}; pl.inv.add(I_SEEDS, 6); pl.sk.lvl[SK_FARMING] = 3; pl.sk.lvl[SK_FORAGING] = 1; break;
        case OCC_BURGLAR: pl.held = {I_KNIFE, 1, 0}; pl.inv.add(I_FLASHLIGHT); pl.inv.add(I_BATTERY, 2); pl.sk.lvl[SK_LIGHTFOOT] = 3; pl.sk.lvl[SK_SNEAK] = 2; break;
        default: break;
    }
    static const char* NAMES[] = {"Олег", "Марія", "Тарас", "Ірина", "Андрій", "Софія", "Максим", "Олена", "Дмитро", "Катерина"};
    for (size_t i = 0; i < world.survivorSpots.size(); i++) {
        Survivor s; s.id = nextId++; s.name = NAMES[(i + sd) % 10];
        s.pos = {world.survivorSpots[i].x + .5f, world.survivorSpots[i].y + .5f}; s.z = world.survivorSpots[i].z;
        int r = rng.range(0, 5); s.weapon = r < 3 ? I_BAT : (r < 5 ? I_KNIFE : I_PISTOL); s.ammo = s.weapon == I_PISTOL ? 10 : 0;
        survivors.push_back(s);
    }
    // зомбі
    int want = 110, guard = 0;
    while ((int)zs.size() < want && guard++ < 4000) {
        bool out = rng.chance(.55f);
        const auto& L = out ? world.outdoorTiles : world.indoorTiles;
        if (L.empty()) continue;
        PNode n = rng.pick(L);
        if (dist(Vec2(n.x + .5f, n.y + .5f), pl.pos) < 24) continue;
        spawnZombie(n.x, n.y, n.z, false);
    }
    updateVisibility(); updateHover();
    say("Ви прокинулись у своєму будинку. Місто мертве. Знайдіть інших вцілілих.");
    say("F1 — довідка. Тихіше ходіть: зомбі реагують на шум.");
}

// ------------------------------------------------------------------ рух
bool Game::moveEntity(Vec2& pos, int z, Vec2 d, float r) {
    auto freeAt = [&](float x, float y) {
        return !world.blocks(ftile(x - r), ftile(y - r), z) && !world.blocks(ftile(x + r), ftile(y - r), z) &&
               !world.blocks(ftile(x - r), ftile(y + r), z) && !world.blocks(ftile(x + r), ftile(y + r), z);
    };
    bool moved = false;
    if (!freeAt(pos.x, pos.y)) { pos += d; return true; }   // застряг — виштовхуємо
    if (d.x != 0 && freeAt(pos.x + d.x, pos.y)) { pos.x += d.x; moved = true; }
    if (d.y != 0 && freeAt(pos.x, pos.y + d.y)) { pos.y += d.y; moved = true; }
    return moved;
}
void Game::tryStairs(Vec2& pos, int& z, int& lx, int& ly) {
    int tx = ftile(pos.x), ty = ftile(pos.y);
    if (!inMap(tx, ty, z)) return;
    if (lx != tx || ly != ty) { lx = ly = -1; }
    uint8_t k = world.at(tx, ty, z).kind;
    if (lx == -1) {
        if (k == TK_STAIR_UP && z + 1 < LEVELS) { z++; lx = tx; ly = ty; }
        else if (k == TK_STAIR_DOWN && z > 0) { z--; lx = tx; ly = ty; }
    }
}
bool Game::tryOpenDoor(int x, int y, int z, bool byZombie) {
    if (!inMap(x, y, z)) return false;
    Tile& t = world.at(x, y, z);
    if (t.kind != TK_DOOR || (t.flags & (TF_OPEN | TF_BROKEN)) || (t.flags & TF_LOCKED) || t.barr > 0) return false;
    t.flags |= TF_OPEN;
    snd(SFX_DOOR, {x + .5f, y + .5f}, z, .7f);
    makeNoise({x + .5f, y + .5f}, z, byZombie ? 3.f : 4.f);
    return true;
}
void Game::setMove(Vec2 dir, bool sprint, bool sneak) { moveIn_ = dir.len() > 1 ? dir.norm() : dir; wantSprint_ = sprint; wantSneak_ = sneak; }
void Game::setAim(Vec2 p) { aimPos_ = p; Vec2 d = p - pl.pos; if (d.len() > 0.15f && pl.alive && !sleeping) pl.face = std::atan2(d.y, d.x); }

void Game::updatePlayer(float dt) {
    Player& p = pl; if (!p.alive) return;
    p.atkCd = std::max(0.f, p.atkCd - dt); p.hitFlash = std::max(0.f, p.hitFlash - dt);
    bool want = moveIn_.len() > 0.01f && !sleeping;
    if (want && act.kind != A_NONE) cancelAction();
    p.moving = false; p.sprint = false;
    bool busy = act.kind != A_NONE || sleeping;
    float wt = p.inv.weight() + idef(p.held.id).weight;
    if (want && !busy) {
        bool sprint = wantSprint_ && !wantSneak_ && p.endur > 3 && p.energy > 8;
        p.sneak = wantSneak_ && !sprint;
        float sp = p.sneak ? 1.5f : (sprint ? 5.2f * (1 + .03f * p.sk[SK_SPRINT]) : 3.0f);
        float cap = carryCap(); if (wt > cap) sp *= std::max(.55f, cap / wt);
        if (p.hp < 25) sp *= .8f;
        if (p.energy < 10) sp *= .85f;
        Vec2 d = moveIn_ * (sp * dt);
        Vec2 probe = p.pos + moveIn_.norm() * .5f;
        tryOpenDoor(ftile(probe.x), ftile(probe.y), p.z, false);
        if (moveEntity(p.pos, p.z, d, .28f)) {
            p.moving = true; p.sprint = sprint;
            tryStairs(p.pos, p.z, p.lockX, p.lockY);
            p.stepT -= dt;
            if (p.stepT <= 0) {
                p.stepT = sprint ? .28f : (p.sneak ? .55f : .4f);
                float rad = sprint ? 14.f : (p.sneak ? 2.f : 7.f);
                rad *= (1 - .05f * p.sk[SK_LIGHTFOOT]);
                makeNoise(p.pos, p.z, rad); noiseR = rad; noiseShow = .6f;
                snd(SFX_STEP, p.pos, p.z, p.sneak ? .25f : .6f);
            }
            if (sprint) { p.endur -= 12 * (1 - .04f * p.sk[SK_SPRINT]) * dt; gainSkill(SK_SPRINT, .5f * dt); gainSkill(SK_FITNESS, .2f * dt); }
            else if (p.sneak) { gainSkill(SK_SNEAK, .25f * dt); gainSkill(SK_LIGHTFOOT, .1f * dt); }
            else { gainSkill(SK_FITNESS, .08f * dt); gainSkill(SK_LIGHTFOOT, .05f * dt); }
        }
    } else p.sneak = wantSneak_;
    if (!p.sprint) p.endur += (p.moving ? 3.f : 9.f) * (1 + .06f * p.sk[SK_FITNESS]) * dt;
    p.endur = clampv(p.endur, 0.f, 100.f);
}

// ------------------------------------------------------------------ потреби
void Game::wakeUp(const char* why) {
    if (!sleeping) return;
    sleeping = false; say(why);
}
void Game::updateNeeds(float gm) {
    Player& p = pl; if (!p.alive) return;
    float sf = sleeping ? .35f : 1.f;
    p.food -= .040f * gm * sf; p.water -= .075f * gm * sf;
    if (sleeping) p.energy += (sleepBed_ ? .30f : .18f) * gm; else p.energy -= .075f * gm;
    p.food = std::max(0.f, p.food); p.water = std::max(0.f, p.water); p.energy = clampv(p.energy, 0.f, 100.f);
    if (p.food <= 0) p.hp -= .03f * gm;
    if (p.water <= 0) p.hp -= .08f * gm;
    if (p.energy <= 0 && !sleeping) p.hp -= .02f * gm;
    if (p.bleeding) {
        p.hp -= .22f * gm; p.bleedT += gm;
        if (p.bleedT > 220) { p.bleeding = false; say("Кровотеча припинилась сама."); }
    } else if (p.food > 30 && p.water > 30 && p.hp < 100) p.hp = std::min(100.f, p.hp + .04f * gm * (sleeping ? 2.f : 1.f));
    if (p.infected) {
        p.infHours -= gm / 60.f;
        if (p.infHours < 12 && rng.chance(.0005f * gm)) p.hp -= 2;
        if (p.infHours <= 0) { killPlayer("Ви померли від інфекції."); return; }
    }
    if (p.light) {
        p.battery -= .55f * gm;
        if (p.battery <= 0) {
            if (p.inv.take(I_BATTERY, 1)) { p.battery = 100; say("Батарейку в ліхтарику замінено."); }
            else { p.light = false; p.battery = 0; say("Ліхтарик розрядився."); }
        }
    }
    if (sleeping) {
        if (p.energy >= 100) wakeUp("Ви виспались.");
        else if (p.food < 8 || p.water < 8) wakeUp("Ви прокинулись від голоду й спраги.");
    }
    if (p.hp <= 0) {
        if (p.water <= 0) killPlayer("Ви померли від зневоднення.");
        else if (p.food <= 0) killPlayer("Ви померли з голоду.");
        else if (p.bleeding) killPlayer("Ви стекли кров'ю.");
        else killPlayer("Ви загинули.");
    }
}
std::vector<Moodle> Game::moodles() const {
    std::vector<Moodle> m; const Player& p = pl;
    auto lv = [](float v, float a, float b, float c) { return v < c ? 3 : v < b ? 2 : v < a ? 1 : 0; };
    int s;
    if ((s = lv(p.food, 35, 20, 8))) m.push_back({"Голодний", s});
    if ((s = lv(p.water, 35, 20, 8))) m.push_back({"Спрага", s});
    if ((s = lv(p.energy, 40, 22, 10))) m.push_back({"Втомлений", s});
    if ((s = lv(p.endur, 40, 25, 10))) m.push_back({"Задишка", s});
    if ((s = lv(p.hp, 60, 40, 22))) m.push_back({"Поранений", s});
    if (p.bleeding) m.push_back({"Кровотеча", 3});
    if (p.infected) m.push_back({p.infHours < 18 ? "Лихоманка" : "Заражений", p.infHours < 18 ? 3 : 2});
    if (p.inv.weight() + idef(p.held.id).weight > carryCap()) m.push_back({"Перевантаження", 2});
    return m;
}

// ------------------------------------------------------------------ дії
void Game::startAction(ActKind k, float dur, int a, int b, int x, int y, int z) {
    act = Action(); act.kind = k; act.dur = std::max(.2f, dur); act.a = a; act.b = b; act.x = x; act.y = y; act.z = z;
}
void Game::cancelAction() { act = Action(); }
void Game::updateAction(float dt) {
    if (act.kind == A_NONE || !pl.alive) return;
    act.t += dt;
    if (act.t >= act.dur) { Action a = act; act = Action(); act = a; finishAction(); }
}
void Game::finishAction() {
    Action a = act; act = Action();
    switch (a.kind) {
        case A_SEARCH: {
            if (a.a < 0 || a.a >= (int)world.furn.size()) break;
            world.furn[a.a].searched = true; openContainer = a.a;
            gainSkill(SK_FORAGING, 4); snd(SFX_PICK, pl.pos, pl.z, .6f); makeNoise(pl.pos, pl.z, 4);
            break;
        }
        case A_BUILD: {
            if (a.a < 0 || a.a >= (int)bps.size() || !bps[a.a].alive) break;
            completeBlueprint(bps[a.a]); gainSkill(SK_CARPENTRY, 10);
            break;
        }
        case A_BARRICADE: {
            Tile& t = world.at(a.x, a.y, a.z);
            if (a.b == 0) {
                if (t.barr < 4 && pl.inv.count(I_PLANK) >= 1 && pl.inv.count(I_NAILS) >= 2) {
                    pl.inv.take(I_PLANK, 1); pl.inv.take(I_NAILS, 2);
                    t.barr++; t.bhp = (uint8_t)std::min(250, 60 + 14 * pl.sk[SK_CARPENTRY]);
                    gainSkill(SK_CARPENTRY, 6); snd(SFX_WOOD, {a.x + .5f, a.y + .5f}, a.z); makeNoise({a.x + .5f, a.y + .5f}, a.z, 8);
                }
            } else if (t.barr > 0) { t.barr--; pl.inv.add(I_PLANK, 1); snd(SFX_WOOD, {a.x + .5f, a.y + .5f}, a.z); }
            break;
        }
        case A_RELOAD: {
            if (pl.held.id == I_NONE || idef(pl.held.id).type != IT_GUN) break;
            const ItemDef& d = idef(pl.held.id);
            int need = d.ammoCap - pl.held.ammo, have = pl.inv.count(d.ammoItem), t = std::min(need, have);
            if (t > 0) { pl.inv.take(d.ammoItem, t); pl.held.ammo += t; snd(SFX_CLICK, pl.pos, pl.z, .8f); }
            break;
        }
        case A_CRAFT: {
            const auto& R = recipes(); if (a.a < 0 || a.a >= (int)R.size()) break;
            const Recipe& r = R[a.a]; if (!canCraft(a.a)) break;
            for (auto& in : r.in) pl.inv.take(in.first, in.second);
            int n = r.outN; if (r.skill == SK_COOKING && pl.sk[SK_COOKING] >= 3) n++;
            pl.inv.add(r.out, n); gainSkill(r.skill, 8); say(std::string("Створено: ") + r.name);
            snd(SFX_PICK, pl.pos, pl.z, .6f);
            break;
        }
        case A_CHOP: {
            Tile& t = world.at(a.x, a.y, a.z);
            if (t.kind == TK_TREE) { t.kind = TK_GRASS; pl.inv.add(I_LOG, 2 + (pl.sk[SK_STRENGTH] >= 3 ? 1 : 0)); gainSkill(SK_STRENGTH, 6); gainSkill(SK_FITNESS, 3); snd(SFX_WOOD, pl.pos, pl.z); makeNoise(pl.pos, pl.z, 10); }
            break;
        }
        case A_FARM: {
            if (a.a < 0 || a.a >= (int)world.furn.size()) break;
            Furn& f = world.furn[a.a]; if (!f.alive) break;
            if (a.b == 0 && f.stage == 0) {
                if (takeMaterial(I_SEEDS, 1) > 0) { f.stage = 1; f.timer = 0; gainSkill(SK_FARMING, 5); say("Насіння посаджено."); }
            } else if (a.b == 1 && f.stage == 3) {
                int n = 2 + pl.sk[SK_FARMING] / 2 + rng.range(0, 2);
                pl.inv.add(I_VEG, n); if (rng.chance(.6f)) pl.inv.add(I_SEEDS, 1);
                f.stage = 0; f.timer = 0; gainSkill(SK_FARMING, 12); say("Зібрано врожай: овочі ×" + std::to_string(n));
            }
            break;
        }
        default: break;
    }
}

// ------------------------------------------------------------------ наведення та взаємодія
void Game::updateHover() {
    hover = Interact();
    if (!pl.alive || sleeping) return;
    int px = ftile(pl.pos.x), py = ftile(pl.pos.y), z = pl.z;
    Vec2 fd(std::cos(pl.face), std::sin(pl.face));
    float best = 1e9f;
    auto consider = [&](IntKind k, int idx, int x, int y, const std::string& label, float bias) {
        Vec2 c(x + .5f, y + .5f); float d = dist(pl.pos, c);
        if (d > 1.9f) return;
        Vec2 dir = (c - pl.pos).norm();
        float score = d - .45f * (dir.x * fd.x + dir.y * fd.y) + bias;
        if (score < best) { best = score; hover.kind = k; hover.idx = idx; hover.x = x; hover.y = y; hover.z = z; hover.label = label; }
    };
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++) {
            int x = px + dx, y = py + dy; if (!inMap(x, y, z)) continue;
            const Tile& t = world.at(x, y, z);
            if (t.kind == TK_DOOR) {
                std::string l = t.barr > 0 ? "Двері (забарикадовано)" : (t.flags & TF_BROKEN) ? "Зламані двері" : (t.flags & TF_OPEN) ? "Зачинити двері" : (t.flags & TF_LOCKED) ? "Двері зачинено" : "Відчинити двері";
                consider(IK_DOOR, -1, x, y, l, 0);
            } else if (t.kind == TK_WINDOW) {
                std::string l = t.barr > 0 ? "Вікно (забарикадовано)" : (t.flags & TF_BROKEN) ? "Розбите вікно" : (t.flags & TF_OPEN) ? "Зачинити вікно" : "Відчинити вікно";
                consider(IK_WINDOW, -1, x, y, l, .1f);
            } else if (t.kind == TK_TREE) consider(IK_TREE, -1, x, y, "Зрубати дерево (потрібна сокира)", .5f);
            int fi = world.furnIndexAt(x, y, z);
            if (fi >= 0 && world.furn[fi].alive) {
                const Furn& f = world.furn[fi]; std::string n = furnName(f.kind); std::string l;
                if (furnIsContainer(f.kind)) l = (f.searched || f.built || f.kind == F_PILE) ? "Відкрити: " + n : "Обшукати: " + n;
                else if (f.kind == F_BED) l = "Спати";
                else if (f.kind == F_FARM) l = f.stage == 0 ? "Посадити насіння" : f.stage == 3 ? "Зібрати врожай" : "";
                else if (f.kind == F_BARREL) l = "Напитися з бочки";
                else if (f.kind == F_SINK) l = "Напитися з крана";
                if (!l.empty()) consider(IK_FURN, fi, x, y, l, 0);
            }
        }
    for (size_t i = 0; i < bps.size(); i++) {
        const Blueprint& b = bps[i]; if (!b.alive || b.z != z) continue;
        if (std::abs(b.x - px) <= 2 && std::abs(b.y - py) <= 2) consider(IK_BLUEPRINT, (int)i, b.x, b.y, std::string("Будувати: ") + buildDef(b.kind).name, -.2f);
    }
    for (size_t i = 0; i < survivors.size(); i++) {
        const Survivor& s = survivors[i]; if (s.z != z) continue;
        if (dist(s.pos, pl.pos) < 2.2f) consider(IK_SURVIVOR, (int)i, ftile(s.pos.x), ftile(s.pos.y), "Поговорити: " + s.name, -.8f);
    }
}
void Game::interact(bool alt) {
    if (!pl.alive || sleeping || act.kind != A_NONE) return;
    int forage = pl.sk[SK_FORAGING];
    switch (hover.kind) {
        case IK_FURN: {
            Furn& f = world.furn[hover.idx];
            if (furnIsContainer(f.kind)) {
                if (f.searched || f.built || f.kind == F_PILE) { openContainer = hover.idx; snd(SFX_PICK, pl.pos, pl.z, .4f); }
                else startAction(A_SEARCH, 2.4f / (1 + .1f * forage), hover.idx);
            } else if (f.kind == F_BED) trySleep();
            else if (f.kind == F_FARM) {
                if (f.stage == 0) { if (hasMaterial(I_SEEDS, 1)) startAction(A_FARM, 3.f, hover.idx, 0); else say("Потрібне насіння."); }
                else if (f.stage == 3) startAction(A_FARM, 2.5f, hover.idx, 1);
            } else if (f.kind == F_BARREL) {
                if (f.stage > 0) { f.stage--; pl.water = std::min(100.f, pl.water + 35); snd(SFX_EAT, pl.pos, pl.z, .6f); } else say("Бочка порожня.");
            } else if (f.kind == F_SINK) {
                if (day() < WATER_CUTOFF_DAY) { pl.water = std::min(100.f, pl.water + 35); snd(SFX_EAT, pl.pos, pl.z, .6f); } else say("З крана більше не тече вода.");
            }
            break;
        }
        case IK_DOOR: {
            Tile& t = world.at(hover.x, hover.y, hover.z);
            if (t.barr > 0) { say("Забарикадовано. Shift+V — зняти."); break; }
            if (alt) {
                if ((t.flags & TF_BUILT) || world.indoor(ftile(pl.pos.x), ftile(pl.pos.y), pl.z)) {
                    t.flags ^= TF_LOCKED; t.flags &= ~TF_OPEN; say((t.flags & TF_LOCKED) ? "Двері замкнено." : "Двері відімкнено.");
                    snd(SFX_CLICK, {hover.x + .5f, hover.y + .5f}, hover.z, .7f);
                }
                break;
            }
            if (t.flags & TF_BROKEN) break;
            if (t.flags & TF_LOCKED) {
                if (world.indoor(ftile(pl.pos.x), ftile(pl.pos.y), pl.z)) { t.flags &= ~TF_LOCKED; say("Двері відімкнено."); }
                else say("Зачинено. Виламайте (удари) або шукайте інший вхід.");
                break;
            }
            t.flags ^= TF_OPEN; snd(SFX_DOOR, {hover.x + .5f, hover.y + .5f}, hover.z, .8f); makeNoise({hover.x + .5f, hover.y + .5f}, hover.z, 3);
            break;
        }
        case IK_WINDOW: {
            Tile& t = world.at(hover.x, hover.y, hover.z);
            if (t.barr > 0) { say("Забарикадовано. Shift+V — зняти."); break; }
            if (t.flags & TF_BROKEN) break;
            t.flags ^= TF_OPEN; snd(SFX_DOOR, {hover.x + .5f, hover.y + .5f}, hover.z, .5f);
            break;
        }
        case IK_BLUEPRINT: {
            Blueprint& b = bps[hover.idx];
            if (b.kind != BK_FARM && b.kind != BK_FIRE && !pl.inv.count(I_HAMMER)) { say("Потрібен молоток."); break; }
            startAction(A_BUILD, buildDef(b.kind).time / (1 + .12f * pl.sk[SK_CARPENTRY]), hover.idx);
            break;
        }
        case IK_SURVIVOR: recruit(hover.idx); break;
        case IK_TREE: {
            if (pl.held.id != I_AXE && !pl.inv.count(I_AXE)) { say("Потрібна сокира."); break; }
            startAction(A_CHOP, 6.f / (1 + .1f * pl.sk[SK_STRENGTH]), -1, -1, hover.x, hover.y, hover.z);
            break;
        }
        default: break;
    }
}
void Game::barricade(bool remove) {
    if (!pl.alive || sleeping || act.kind != A_NONE) return;
    if (hover.kind != IK_DOOR && hover.kind != IK_WINDOW) { say("Підійдіть до вікна або дверей."); return; }
    Tile& t = world.at(hover.x, hover.y, hover.z);
    if (remove) { if (t.barr == 0) return; startAction(A_BARRICADE, 2.f, -1, 1, hover.x, hover.y, hover.z); return; }
    if (t.barr >= 4) { say("Максимум 4 дошки."); return; }
    if (!pl.inv.count(I_HAMMER)) { say("Потрібен молоток."); return; }
    if (pl.inv.count(I_PLANK) < 1 || pl.inv.count(I_NAILS) < 2) { say("Потрібні дошка та 2 цвяхи."); return; }
    startAction(A_BARRICADE, 3.f / (1 + .1f * pl.sk[SK_CARPENTRY]), -1, 0, hover.x, hover.y, hover.z);
}
void Game::toggleLight() {
    if (!pl.inv.count(I_FLASHLIGHT)) { say("У вас немає ліхтарика."); return; }
    if (!pl.light && pl.battery <= 0 && !pl.inv.count(I_BATTERY)) { say("Немає батарейок."); return; }
    if (!pl.light && pl.battery <= 0) { pl.inv.take(I_BATTERY, 1); pl.battery = 100; }
    pl.light = !pl.light; snd(SFX_CLICK, pl.pos, pl.z, .5f);
}
void Game::reload() {
    if (!pl.alive || act.kind != A_NONE || sleeping) return;
    if (pl.held.id == I_NONE || idef(pl.held.id).type != IT_GUN) return;
    const ItemDef& d = idef(pl.held.id);
    if (pl.held.ammo >= d.ammoCap) return;
    if (pl.inv.count(d.ammoItem) <= 0) { say("Немає набоїв."); return; }
    startAction(A_RELOAD, std::max(.8f, 2.2f - .15f * pl.sk[SK_FIREARM]));
}
void Game::trySleep() {
    if (!pl.alive || sleeping) return;
    if (pl.energy > 85) { say("Ви не хочете спати."); return; }
    for (auto& z : zs) if (z.alive && z.active && z.z == pl.z && z.st != ZS_IDLE && dist(z.pos, pl.pos) < 16) { say("Занадто небезпечно спати: зомбі поруч."); return; }
    sleepBed_ = false;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int fi = world.furnIndexAt(ftile(pl.pos.x) + dx, ftile(pl.pos.y) + dy, pl.z);
        if (fi >= 0 && world.furn[fi].alive && world.furn[fi].kind == F_BED) sleepBed_ = true;
    }
    sleeping = true; act = Action(); say(sleepBed_ ? "Ви лягли спати." : "Ви лягли спати просто на підлозі.");
}

// ------------------------------------------------------------------ інвентар
int Game::bestFoodIdx(bool) const { return -1; }
void Game::removeInvAt(int idx, int n) { pl.inv.removeAt(idx, n); }
void Game::useItem(int idx) {
    if (idx < 0 || idx >= (int)pl.inv.items.size() || !pl.alive || act.kind != A_NONE) return;
    Item it = pl.inv.items[idx]; const ItemDef& d = idef(it.id);
    switch (d.type) {
        case IT_FOOD: case IT_DRINK: {
            float f = d.food * (it.id == I_MEAL ? 1 + .08f * pl.sk[SK_COOKING] : 1.f);
            pl.food = std::min(100.f, pl.food + f); pl.water = std::min(100.f, pl.water + d.water);
            if (it.id == I_BOTTLE) break;
            pl.inv.removeAt(idx, 1); snd(SFX_EAT, pl.pos, pl.z, .7f);
            if (it.id == I_WATER) pl.inv.add(I_BOTTLE, 1);
            break;
        }
        case IT_MED: {
            float h = d.heal * (1 + .08f * pl.sk[SK_FIRSTAID]);
            if (it.id == I_BANDAGE || it.id == I_FIRSTAID) { if (pl.bleeding) { pl.bleeding = false; say("Кровотечу зупинено."); } }
            pl.hp = std::min(100.f, pl.hp + h); gainSkill(SK_FIRSTAID, 6);
            pl.inv.removeAt(idx, 1); snd(SFX_EAT, pl.pos, pl.z, .5f);
            break;
        }
        case IT_MELEE: case IT_GUN: equipItem(idx); break;
        case IT_AMMO: reload(); break;
        case IT_TOOL: if (it.id == I_FLASHLIGHT) toggleLight(); break;
        case IT_SEED: say("Посадіть насіння на грядці."); break;
        default: break;
    }
}
void Game::equipItem(int idx) {
    if (idx < 0 || idx >= (int)pl.inv.items.size()) return;
    Item it = pl.inv.items[idx]; ItemType t = idef(it.id).type;
    if (t != IT_MELEE && t != IT_GUN) return;
    pl.inv.removeAt(idx, 1);
    if (pl.held.id != I_NONE) pl.inv.add(pl.held.id, 1, pl.held.ammo);
    pl.held = it; pl.held.n = 1; cancelAction();
}
void Game::unequip() { if (pl.held.id != I_NONE) { pl.inv.add(pl.held.id, 1, pl.held.ammo); pl.held = Item{I_NONE, 0, 0}; } }
void Game::dropItem(int idx) {
    if (idx < 0 || idx >= (int)pl.inv.items.size()) return;
    int x = ftile(pl.pos.x), y = ftile(pl.pos.y);
    int fi = world.furnIndexAt(x, y, pl.z);
    if (fi >= 0 && world.furn[fi].kind != F_PILE) { say("Тут немає місця."); return; }
    if (fi < 0) { fi = world.addFurn(F_PILE, x, y, pl.z, false); world.furn[fi].searched = true; }
    Item it = pl.inv.items[idx];
    world.furn[fi].inv.add(it.id, it.n, it.ammo); pl.inv.removeAt(idx, it.n);
}
void Game::takeFrom(int fi, int ii) {
    if (fi < 0 || fi >= (int)world.furn.size()) return;
    Furn& f = world.furn[fi]; if (!f.alive || ii < 0 || ii >= (int)f.inv.items.size()) return;
    if (dist(Vec2(f.x + .5f, f.y + .5f), pl.pos) > 3.2f || f.z != pl.z) return;
    Item it = f.inv.items[ii]; f.inv.removeAt(ii, it.n); pl.inv.add(it.id, it.n, it.ammo);
    snd(SFX_PICK, pl.pos, pl.z, .5f);
    if (f.kind == F_PILE && f.inv.empty()) { world.removeFurn(fi); if (openContainer == fi) openContainer = -1; }
}
void Game::takeAll(int fi) {
    if (fi < 0 || fi >= (int)world.furn.size()) return;
    while (fi >= 0 && fi < (int)world.furn.size() && world.furn[fi].alive && !world.furn[fi].inv.empty()) {
        size_t before = world.furn[fi].inv.items.size(); takeFrom(fi, 0);
        if (!world.furn[fi].alive || (world.furn[fi].inv.items.size() == before)) break;
    }
}
void Game::putTo(int fi, int ii) {
    if (fi < 0 || fi >= (int)world.furn.size() || ii < 0 || ii >= (int)pl.inv.items.size()) return;
    Furn& f = world.furn[fi]; if (!f.alive || !furnIsContainer(f.kind)) return;
    if (dist(Vec2(f.x + .5f, f.y + .5f), pl.pos) > 3.2f || f.z != pl.z) return;
    Item it = pl.inv.items[ii]; f.inv.add(it.id, it.n, it.ammo); pl.inv.removeAt(ii, it.n); snd(SFX_PICK, pl.pos, pl.z, .5f);
}

// ------------------------------------------------------------------ крафт
bool Game::nearFire() const {
    for (auto& f : world.furn) if (f.alive && f.kind == F_FIRE && f.z == pl.z && dist(Vec2(f.x + .5f, f.y + .5f), pl.pos) < 3.2f) return true;
    return false;
}
bool Game::canCraft(int r) const {
    const auto& R = recipes(); if (r < 0 || r >= (int)R.size()) return false;
    const Recipe& rc = R[r];
    if (pl.sk[rc.skill] < rc.minLevel) return false;
    if (rc.needFire && !nearFire()) return false;
    if (rc.tool != I_NONE && !pl.inv.count(rc.tool)) return false;
    for (auto& in : rc.in) if (pl.inv.count(in.first) < in.second) return false;
    return true;
}
void Game::craft(int r) {
    if (!pl.alive || act.kind != A_NONE || sleeping) return;
    if (!canCraft(r)) { say("Не вистачає матеріалів, інструмента або навички."); return; }
    startAction(A_CRAFT, recipes()[r].time, r);
}

// ------------------------------------------------------------------ склад і будівництво
std::vector<int> Game::storageCrates() const {
    std::vector<int> v;
    for (size_t i = 0; i < world.furn.size(); i++) {
        const Furn& f = world.furn[i];
        if (f.alive && f.kind == F_CRATE && f.built && inBase({f.x + .5f, f.y + .5f})) v.push_back((int)i);
    }
    return v;
}
int Game::storageCount(ItemId id) const { int n = 0; for (int i : storageCrates()) n += world.furn[i].inv.count(id); return n; }
int Game::storageFood() const {
    int n = 0;
    for (int i : storageCrates()) for (auto& it : world.furn[i].inv.items) if (idef(it.id).food > 0 || idef(it.id).water > 0) n += it.n;
    return n;
}
bool Game::hasMaterial(ItemId id, int n) const { return pl.inv.count(id) + storageCount(id) >= n; }
int Game::takeMaterial(ItemId id, int n) {
    int got = 0, t = std::min(n, pl.inv.count(id));
    if (t > 0) { pl.inv.take(id, t); got += t; }
    for (int i : storageCrates()) {
        if (got >= n) break;
        int c = std::min(n - got, world.furn[i].inv.count(id));
        if (c > 0) { world.furn[i].inv.take(id, c); got += c; }
    }
    return got;
}
bool Game::blueprintSpotOk(BuildKind k, int x, int y, int z, std::string* why) const {
    auto no = [&](const char* s) { if (why) *why = s; return false; };
    if (!inMap(x, y, z)) return no("Поза картою");
    if (!inBase({x + .5f, y + .5f})) return no("Поза межами бази");
    const Tile& t = world.at(x, y, z);
    if (k == BK_FARM) { if (t.kind != TK_GRASS || (t.flags & TF_INDOOR)) return no("Грядка — тільки на траві"); }
    else if (!(t.kind == TK_GRASS || t.kind == TK_SIDEWALK || t.kind == TK_FLOOR_W || t.kind == TK_FLOOR_T || t.kind == TK_FLOOR_C)) return no("Тут не можна будувати");
    if (world.furnIndexAt(x, y, z) >= 0) return no("Місце зайняте");
    for (auto& b : bps) if (b.alive && b.x == x && b.y == y && b.z == z) return no("Тут уже є план");
    if (k != BK_DOOR && k != BK_FARM && k != BK_FIRE && ftile(pl.pos.x) == x && ftile(pl.pos.y) == y && pl.z == z) return no("Ви стоїте на цьому місці");
    return true;
}
bool Game::placeBlueprint(BuildKind k, int x, int y) {
    std::string why;
    if (!blueprintSpotOk(k, x, y, pl.z, &why)) { say(why); return false; }
    const BuildDef& d = buildDef(k);
    for (auto& c : d.cost) if (!hasMaterial(c.first, c.second)) { say(std::string("Не вистачає: ") + idef(c.first).name + " ×" + std::to_string(c.second)); return false; }
    for (auto& c : d.cost) takeMaterial(c.first, c.second);
    Blueprint b; b.kind = k; b.x = (int16_t)x; b.y = (int16_t)y; b.z = (int8_t)pl.z; bps.push_back(b);
    return true;
}
void Game::cancelBlueprint(int idx) {
    if (idx < 0 || idx >= (int)bps.size() || !bps[idx].alive) return;
    for (auto& c : buildDef(bps[idx].kind).cost) pl.inv.add(c.first, c.second);
    bps[idx].alive = false;
}
void Game::completeBlueprint(Blueprint& b) {
    b.alive = false;
    Tile& t = world.at(b.x, b.y, b.z);
    switch (b.kind) {
        case BK_WALL: t.kind = TK_WALL; t.hp = 180; t.barr = 0; t.flags = (t.flags & TF_INDOOR) | TF_BUILT; break;
        case BK_DOOR: t.kind = TK_DOOR; t.hp = 120; t.barr = 0; t.flags = (t.flags & TF_INDOOR) | TF_BUILT; break;
        case BK_CRATE: { int i = world.addFurn(F_CRATE, b.x, b.y, b.z, true); world.furn[i].searched = true; break; }
        case BK_BED: world.addFurn(F_BED, b.x, b.y, b.z, true); break;
        case BK_FARM: world.addFurn(F_FARM, b.x, b.y, b.z, true); break;
        case BK_FIRE: world.addFurn(F_FIRE, b.x, b.y, b.z, true); break;
        case BK_BARREL: { int i = world.addFurn(F_BARREL, b.x, b.y, b.z, true); world.furn[i].stage = 2; break; }
        default: break;
    }
    snd(SFX_WOOD, {b.x + .5f, b.y + .5f}, b.z, .8f); makeNoise({b.x + .5f, b.y + .5f}, b.z, 9);
}

// ------------------------------------------------------------------ видимість та основний цикл
void Game::updateVisibility() {
    float d = darkness();
    int r = (int)std::round(lerpf(17.f, 7.f, d));
    if (pl.light && pl.alive) r = std::max(r, 13);
    if (!pl.alive) { vis.assign(MAPW * MAPH, 0); return; }
    world.computeVisible(ftile(pl.pos.x), ftile(pl.pos.y), pl.z, r, vis);
}
void Game::update(float dt) {
    if (gameOver || speedIdx == 0) return;
    float scale = sleeping ? 12.f : (speedIdx == 1 ? 1.f : speedIdx == 2 ? 2.f : 4.f);
    acc_ += std::min(dt, .1f) * scale;
    int n = 0;
    while (acc_ >= SIM_STEP && n < 60) { acc_ -= SIM_STEP; step(SIM_STEP); n++; if (gameOver) break; }
}
void Game::step(float dt) {
    float gm = dt * MIN_PER_SEC;
    minutes += gm; pathBudget_ = 14; noiseShow = std::max(0.f, noiseShow - dt);
    updatePlayer(dt); updateAction(dt); updateNeeds(gm);
    updateZombies(dt); updateColonists(dt); updateFarms(gm); updateEvents(dt);
    visT_ += dt;
    if (visT_ >= .1f) { visT_ = 0; updateVisibility(); updateHover(); }
    if (!pl.alive && !gameOver) switchToColonist();
}
void Game::killPlayer(const char* why) {
    if (!pl.alive) return;
    pl.alive = false; sleeping = false; act = Action(); say(why);
}
void Game::switchToColonist() {
    for (size_t i = 0; i < cols.size(); i++) {
        if (!cols[i].alive) continue;
        Colonist c = cols[i]; cols.erase(cols.begin() + i);
        Player np; np.name = c.name; np.occ = OCC_POLICE; np.pos = c.pos; np.z = c.z; np.hp = std::max(40.f, c.hp); np.food = c.food; np.water = c.water; np.energy = c.energy;
        np.inv = c.carry; np.held = {c.weapon, 1, c.ammo};
        np.sk.lvl[SK_FITNESS] = 1; np.sk.lvl[SK_STRENGTH] = 1;
        np.sk.lvl[SK_FORAGING] = c.scavLvl; np.sk.lvl[SK_CARPENTRY] = c.buildLvl; np.sk.lvl[SK_FARMING] = c.farmLvl;
        np.sk.lvl[SK_BLUNT] = c.combatLvl; np.sk.lvl[SK_BLADE] = c.combatLvl; np.sk.lvl[SK_FIREARM] = c.combatLvl;
        np.alive = true; pl = np; sleeping = false; act = Action(); openContainer = -1;
        say("Ви більше не той, ким були. Тепер ви — " + c.name + ".");
        return;
    }
    gameOver = true; say("Усі загинули. Кінець.");
}
