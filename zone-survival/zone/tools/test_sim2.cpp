#include "../src/game.h"
#include <cstdio>
struct GameTest {
    static void run() {
        Game g; g.newGame(777, OCC_POLICE, "T");
        auto& sb = g.world.buildings[g.world.startBuilding];
        printf("start bld %d,%d %dx%d door %d,%d spawn %.1f %.1f\n", sb.x, sb.y, sb.w, sb.h, sb.doorX, sb.doorY, g.pl.pos.x, g.pl.pos.y);
        // 1) бій бітою
        g.zs.clear(); g.pl.held = {I_BAT, 1, 0};
        g.spawnZombie(ftile(g.pl.pos.x) + 1, ftile(g.pl.pos.y), 0, false);
        g.zs[0].active = true; g.zs[0].st = ZS_CHASE; g.zs[0].target = 0;
        int swings = 0;
        for (int i = 0; i < 20 * 20 && !g.zs.empty(); i++) {
            if (i % 12 == 0 && !g.zs.empty()) { g.attack(g.zs[0].pos); swings++; }
            g.update(0.05f);
        }
        printf("bat fight: swings %d zombies left %zu kills %d hp %.0f bleeding %d\n", swings, g.zs.size(), g.kills, g.pl.hp, g.pl.bleeding);
        // 2) пістолет
        g.pl.hp = 100; g.pl.held = {I_PISTOL, 1, 15};
        g.spawnZombie(ftile(g.pl.pos.x) + 1, ftile(g.pl.pos.y), 0, false);
        g.zs.back().pos = g.pl.pos + Vec2(4, 0); g.zs.back().active = true;
        for (int i = 0; i < 10 && !g.zs.empty(); i++) { g.attack(g.zs.back().pos); g.pl.atkCd = 0; g.update(0.05f); }
        printf("pistol: zombies %zu ammo %d kills %d\n", g.zs.size(), g.pl.held.ammo, g.kills);
        // 3) шум: постріл будить зомбі у радіусі
        g.zs.clear();
        for (int k = 0; k < 6; k++) { g.spawnZombie(ftile(g.pl.pos.x) + 10 + k, ftile(g.pl.pos.y) + 3, 0, false); g.zs.back().active = true; }
        g.makeNoise(g.pl.pos, 0, 42);
        int al = 0; for (auto& z : g.zs) if (z.st == ZS_ALERT) al++;
        printf("noise 42: alerted %d/6\n", al);
        // 4) шлях до гравця: зомбі в ALERT доходять
        for (int i = 0; i < 20 * 25; i++) g.update(0.05f);
        float md = 1e9; for (auto& z : g.zs) md = std::min(md, dist(z.pos, g.pl.pos));
        printf("after 25s min dist %.1f hp %.0f\n", md, g.pl.hp);
        g.zs.clear(); g.gameOver=false; g.pl.alive=true; g.pl.hp=100; g.pl.pos=g.world.playerSpawn;
        // 5) вікно і барикада
        int wx = -1, wy = -1;
        for (int y = sb.y; y < sb.y + sb.h && wx < 0; y++) for (int x = sb.x; x < sb.x + sb.w; x++) if (g.world.at(x, y, 0).kind == TK_WINDOW) { wx = x; wy = y; break; }
        printf("window at %d,%d\n", wx, wy);
        if (wx >= 0) {
            g.pl.pos = {wx + .5f, wy + 1.0f}; g.pl.inv.add(I_HAMMER); g.pl.inv.add(I_PLANK, 4); g.pl.inv.add(I_NAILS, 10);
            for (int i = 0; i < 4; i++) g.update(0.05f);
            printf("hover %d '%s'\n", g.hover.kind, g.hover.label.c_str());
            g.hover.kind = IK_WINDOW; g.hover.x = wx; g.hover.y = wy; g.hover.z = 0; if (g.hover.kind == IK_WINDOW) { g.barricade(false); for (int i = 0; i < 100; i++) g.update(0.05f); }
            printf("barr %d\n", g.world.at(wx, wy, 0).barr);
        }
        // 6) обшук контейнера
        g.zs.clear();
        for (size_t i = 0; i < g.world.furn.size(); i++) {
            Furn& f = g.world.furn[i];
            if (f.alive && furnIsContainer(f.kind) && !f.inv.empty() && f.kind != F_CAR && g.world.buildingAt(f.x, f.y, f.z) == g.world.startBuilding) {
                g.pl.pos = {f.x + .5f, f.y + 1.2f}; g.pl.z = f.z; break;
            }
        }
        for (int i = 0; i < 4; i++) g.update(0.05f);
        printf("hover %d '%s'\n", g.hover.kind, g.hover.label.c_str());
        g.interact(); for (int i = 0; i < 100; i++) g.update(0.05f);
        printf("open container %d items %zu\n", g.openContainer, g.openContainer >= 0 ? g.world.furn[g.openContainer].inv.items.size() : 0);
        if (g.openContainer >= 0) { g.takeAll(g.openContainer); printf("inv weight %.1f\n", g.pl.inv.weight()); }
        // 7) їжа
        g.pl.food = 20; for (size_t i = 0; i < g.pl.inv.items.size(); i++) if (idef(g.pl.inv.items[i].id).food > 0) { g.useItem((int)i); break; }
        printf("food after eat %.0f\n", g.pl.food);
        // 8) сон
        g.pl.energy = 30; g.trySleep(); for (int i = 0; i < 20 * 60 * 3 && g.sleeping; i++) g.update(0.05f);
        printf("sleep -> energy %.0f sleeping %d %s\n", g.pl.energy, g.sleeping, g.clockStr().c_str());
        // 9) сходи
        for (size_t b = 0; b < g.world.buildings.size(); b++) if (g.world.buildings[b].levels > 1) {
            auto& bd = g.world.buildings[b]; int sx = -1, sy = -1;
            for (int y = bd.y; y < bd.y + bd.h; y++) for (int x = bd.x; x < bd.x + bd.w; x++) if (g.world.at(x, y, 0).kind == TK_STAIR_UP) { sx = x; sy = y; }
            if (sx < 0) continue;
            g.pl.pos = {sx - .5f, sy + .5f}; g.pl.z = 0; g.pl.lockX = g.pl.lockY = -1;
            for (int i = 0; i < 20; i++) { g.setMove({1, 0}, false, false); g.update(0.05f); }
            g.setMove({0, 0}, false, false);
            printf("stairs: z=%d at %.1f,%.1f (stairs %d,%d)\n", g.pl.z, g.pl.pos.x, g.pl.pos.y, sx, sy); break;
        }
    }
};
int main() { GameTest::run(); return 0; }
