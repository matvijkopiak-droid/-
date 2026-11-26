#include "game.h"
#include <fstream>

namespace {
struct W {
    std::ofstream o;
    template <class T> void p(const T& v) { o.write((const char*)&v, sizeof(T)); }
    void s(const std::string& s) { uint32_t n = (uint32_t)s.size(); p(n); o.write(s.data(), n); }
    void inv(const Inventory& i) { uint32_t n = (uint32_t)i.items.size(); p(n); for (auto& it : i.items) { p((uint8_t)it.id); p((int32_t)it.n); p((int32_t)it.ammo); } }
};
struct R {
    std::ifstream i; bool ok = true;
    template <class T> void g(T& v) { i.read((char*)&v, sizeof(T)); if (!i) ok = false; }
    std::string s() { uint32_t n = 0; g(n); if (!ok || n > 4096) { ok = false; return ""; } std::string r(n, ' '); i.read(&r[0], n); if (!i) ok = false; return r; }
    void inv(Inventory& v) {
        uint32_t n = 0; g(n); v.items.clear(); if (!ok || n > 5000) { ok = false; return; }
        for (uint32_t k = 0; k < n; k++) { uint8_t id; int32_t c, a; g(id); g(c); g(a); if (!ok || id >= I_COUNT) { ok = false; return; } v.items.push_back({(ItemId)id, c, a}); }
    }
};
const uint32_t MAGIC = 0x5A4F4E32;   // ZON2
}

bool Game::save(const std::string& path) const {
    W w; w.o.open(path, std::ios::binary); if (!w.o) return false;
    w.p(MAGIC); w.p(seed); w.p(minutes); w.p(speedIdx); w.p(kills); w.p(nextHordeDay); w.p(nextId); w.p(heliDone); w.p(hordeAnnounced);
    w.p(baseCenter); w.p(baseRadius);
    // гравець
    const Player& p = pl;
    w.s(p.name); w.p((uint8_t)p.occ); w.p(p.pos); w.p(p.z); w.p(p.face); w.p(p.hp); w.p(p.food); w.p(p.water); w.p(p.energy); w.p(p.endur);
    w.p(p.bleeding); w.p(p.infected); w.p(p.light); w.p(p.alive); w.p(p.infHours); w.p(p.bleedT); w.p(p.battery);
    w.inv(p.inv); w.p((uint8_t)p.held.id); w.p((int32_t)p.held.ammo);
    for (int i = 0; i < SK_COUNT; i++) { w.p((int32_t)p.sk.lvl[i]); w.p(p.sk.xp[i]); }
    // світ
    uint32_t nt = (uint32_t)world.tiles.size(); w.p(nt); w.o.write((const char*)world.tiles.data(), nt * sizeof(Tile));
    uint32_t ns = (uint32_t)world.seen.size(); w.p(ns); w.o.write((const char*)world.seen.data(), ns);
    uint32_t nf = (uint32_t)world.furn.size(); w.p(nf);
    for (auto& f : world.furn) { w.p((uint8_t)f.kind); w.p(f.x); w.p(f.y); w.p(f.z); w.p(f.alive); w.p(f.searched); w.p(f.built); w.p(f.stage); w.p(f.timer); w.inv(f.inv); }
    // зомбі
    uint32_t nz = (uint32_t)zs.size(); w.p(nz);
    for (auto& z : zs) { w.p(z.id); w.p(z.pos); w.p(z.z); w.p(z.face); w.p(z.hp); w.p((uint8_t)z.type); w.p((uint8_t)z.st); w.p(z.goal); w.p(z.goalZ); w.p(z.horde); w.p(z.alive); w.p(z.target); }
    // колоністи
    uint32_t nc = (uint32_t)cols.size(); w.p(nc);
    for (auto& c : cols) {
        w.p(c.id); w.s(c.name); w.p(c.pos); w.p(c.z); w.p(c.hp); w.p(c.food); w.p(c.water); w.p(c.energy); w.p((uint8_t)c.job); w.p(c.post); w.p(c.postZ);
        w.p((uint8_t)c.weapon); w.p(c.ammo); w.p(c.alive); w.inv(c.carry); w.p(c.scavLvl); w.p(c.buildLvl); w.p(c.farmLvl); w.p(c.combatLvl);
    }
    uint32_t nv = (uint32_t)survivors.size(); w.p(nv);
    for (auto& s : survivors) { w.p(s.id); w.s(s.name); w.p(s.pos); w.p(s.z); w.p((uint8_t)s.weapon); w.p(s.ammo); }
    uint32_t nb = (uint32_t)bps.size(); w.p(nb);
    for (auto& b : bps) { w.p((uint8_t)b.kind); w.p(b.x); w.p(b.y); w.p(b.z); w.p(b.progress); w.p(b.alive); }
    return (bool)w.o;
}

bool Game::load(const std::string& path) {
    R r; r.i.open(path, std::ios::binary); if (!r.i) return false;
    uint32_t magic = 0; r.g(magic); if (magic != MAGIC) return false;
    Game g2; // тимчасовий, щоб не зіпсувати поточну гру при помилці
    uint32_t sd; r.g(sd);
    g2.world.generate(sd); g2.seed = sd;
    r.g(g2.minutes); r.g(g2.speedIdx); r.g(g2.kills); r.g(g2.nextHordeDay); r.g(g2.nextId); r.g(g2.heliDone); r.g(g2.hordeAnnounced);
    r.g(g2.baseCenter); r.g(g2.baseRadius);
    Player& p = g2.pl;
    p.name = r.s(); uint8_t u; r.g(u); p.occ = (Occupation)std::min<int>(u, OCC_COUNT - 1);
    r.g(p.pos); r.g(p.z); r.g(p.face); r.g(p.hp); r.g(p.food); r.g(p.water); r.g(p.energy); r.g(p.endur);
    r.g(p.bleeding); r.g(p.infected); r.g(p.light); r.g(p.alive); r.g(p.infHours); r.g(p.bleedT); r.g(p.battery);
    r.inv(p.inv); r.g(u); int32_t am; r.g(am); if (u >= I_COUNT) return false; p.held = {(ItemId)u, u ? 1 : 0, am};
    for (int i = 0; i < SK_COUNT; i++) { int32_t l; r.g(l); p.sk.lvl[i] = l; r.g(p.sk.xp[i]); }
    uint32_t nt; r.g(nt); if (!r.ok || nt != g2.world.tiles.size()) return false;
    r.i.read((char*)g2.world.tiles.data(), nt * sizeof(Tile));
    uint32_t ns; r.g(ns); if (!r.ok || ns != g2.world.seen.size()) return false;
    r.i.read((char*)g2.world.seen.data(), ns);
    uint32_t nf; r.g(nf); if (!r.ok || nf > 100000) return false;
    g2.world.furn.clear(); g2.world.furnAt.assign(MAPW * MAPH * LEVELS, -1);
    for (uint32_t k = 0; k < nf; k++) {
        Furn f; uint8_t kd; r.g(kd); r.g(f.x); r.g(f.y); r.g(f.z); r.g(f.alive); r.g(f.searched); r.g(f.built); r.g(f.stage); r.g(f.timer); r.inv(f.inv);
        if (!r.ok || kd > F_PILE || !inMap(f.x, f.y, f.z)) return false;
        f.kind = (FurnKind)kd; g2.world.furn.push_back(f);
        if (f.alive) g2.world.furnAt[tileIdx(f.x, f.y, f.z)] = (int)k;
    }
    uint32_t nz; r.g(nz); if (!r.ok || nz > 5000) return false;
    for (uint32_t k = 0; k < nz; k++) {
        Zombie z; uint8_t ty, st; r.g(z.id); r.g(z.pos); r.g(z.z); r.g(z.face); r.g(z.hp); r.g(ty); r.g(st); r.g(z.goal); r.g(z.goalZ); r.g(z.horde); r.g(z.alive); r.g(z.target);
        z.type = (ZType)std::min<int>(ty, Z_BRUTE); z.st = (ZState)std::min<int>(st, ZS_CHASE); z.lastPos = z.pos; g2.zs.push_back(z);
    }
    uint32_t nc; r.g(nc); if (!r.ok || nc > 200) return false;
    for (uint32_t k = 0; k < nc; k++) {
        Colonist c; uint8_t jb, wp; r.g(c.id); c.name = r.s(); r.g(c.pos); r.g(c.z); r.g(c.hp); r.g(c.food); r.g(c.water); r.g(c.energy); r.g(jb); r.g(c.post); r.g(c.postZ);
        r.g(wp); r.g(c.ammo); r.g(c.alive); r.inv(c.carry); r.g(c.scavLvl); r.g(c.buildLvl); r.g(c.farmLvl); r.g(c.combatLvl);
        if (!r.ok || wp >= I_COUNT) return false;
        c.job = (Job)std::min<int>(jb, J_COUNT - 1); c.weapon = (ItemId)wp; c.wander = c.pos; g2.cols.push_back(c);
    }
    uint32_t nv; r.g(nv); if (!r.ok || nv > 200) return false;
    for (uint32_t k = 0; k < nv; k++) { Survivor s; uint8_t wp; r.g(s.id); s.name = r.s(); r.g(s.pos); r.g(s.z); r.g(wp); r.g(s.ammo); if (wp >= I_COUNT) return false; s.weapon = (ItemId)wp; g2.survivors.push_back(s); }
    uint32_t nb; r.g(nb); if (!r.ok || nb > 5000) return false;
    for (uint32_t k = 0; k < nb; k++) { Blueprint b; uint8_t kd; r.g(kd); r.g(b.x); r.g(b.y); r.g(b.z); r.g(b.progress); r.g(b.alive); b.kind = (BuildKind)std::min<int>(kd, BK_COUNT - 1); g2.bps.push_back(b); }
    if (!r.ok) return false;
    // успіх: переносимо стан
    world = std::move(g2.world); pl = std::move(g2.pl); zs = std::move(g2.zs); cols = std::move(g2.cols); survivors = std::move(g2.survivors); bps = std::move(g2.bps);
    seed = g2.seed; minutes = g2.minutes; speedIdx = g2.speedIdx == 0 ? 1 : g2.speedIdx; kills = g2.kills; nextHordeDay = g2.nextHordeDay; nextId = g2.nextId;
    heliDone = g2.heliDone; hordeAnnounced = g2.hordeAnnounced; baseCenter = g2.baseCenter; baseRadius = g2.baseRadius;
    log.clear(); sounds.clear(); gameOver = false; sleeping = false; act = Action(); hover = Interact(); openContainer = -1; acc_ = 0; moveIn_ = {0, 0};
    rng = Rng(seed * 7u + (uint32_t)minutes);
    updateVisibility(); updateHover(); say("Гру завантажено.");
    return true;
}
void Game::fixWorldAfterLoad() {}
