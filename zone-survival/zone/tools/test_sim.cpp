#include "../src/game.h"
#include <cstdio>
#include <cassert>
int main() {
    Game g; g.newGame(12345, OCC_CARPENTER, "Тест");
    printf("zombies %zu survivors %zu spawn %.1f,%.1f day%d\n", g.zs.size(), g.survivors.size(), g.pl.pos.x, g.pl.pos.y, g.day());
    // 1) пройти 5 хв гри, стоячи
    for (int i = 0; i < 20 * 120; i++) g.update(0.05f);
    printf("after 2min idle: hp %.0f food %.1f water %.1f clock %s\n", g.pl.hp, g.pl.food, g.pl.water, g.clockStr().c_str());
    // 2) вийти і бігти (шум), дивитися реакцію зомбі
    int alerted = 0;
    for (int i = 0; i < 20 * 30; i++) { g.setMove({1, 0}, true, false); g.update(0.05f); }
    for (auto& z : g.zs) if (z.st == ZS_ALERT || z.st == ZS_CHASE) alerted++;
    printf("sprinted 30s -> pos %.1f,%.1f endur %.0f alerted zombies %d hp %.0f\n", g.pl.pos.x, g.pl.pos.y, g.pl.endur, alerted, g.pl.hp);
    // 3) побудова: поставити ящик і збудувати
    g.setMove({0, 0}, false, false);
    g.pl.pos = g.baseCenter; g.pl.z = 0;
    bool ok = g.placeBlueprint(BK_CRATE, ftile(g.baseCenter.x) + 2, ftile(g.baseCenter.y));
    printf("place crate: %d, planks left %d\n", ok, g.pl.inv.count(I_PLANK));
    for (int i = 0; i < 10; i++) g.update(0.05f);
    printf("hover: %d '%s'\n", g.hover.kind, g.hover.label.c_str());
    if (g.hover.kind == IK_BLUEPRINT) { g.interact(); for (int i = 0; i < 20 * 10; i++) g.update(0.05f); }
    printf("crates: %zu\n", g.storageCrates().size());
    // 4) рекрут + скавенджинг
    if (!g.survivors.empty()) {
        auto s = g.survivors[0]; g.pl.pos = s.pos; g.pl.z = s.z;
        for (int i = 0; i < 5; i++) g.update(0.05f);
        printf("hover surv: %d\n", g.hover.kind);
        g.interact();
        printf("colonists %zu\n", g.cols.size());
        if (!g.cols.empty()) {
            g.pl.pos = g.baseCenter; g.cols[0].pos = g.baseCenter; g.cols[0].z = 0;
            g.setJob(g.cols[0].id, J_SCAV);
            for (int i = 0; i < 20 * 240; i++) g.update(0.05f);
            if (!g.cols.empty()) printf("scav: status '%s' hp %.0f carry %.1f storage food %d zombies %zu\n", g.cols[0].status.c_str(), g.cols[0].hp, g.cols[0].carry.weight(), g.storageFood(), g.zs.size());
            else printf("colonist died\n");
        }
    }
    // 5) save/load
    bool sv = g.save("/tmp/zone_test.sav");
    Game h; bool ld = h.load("/tmp/zone_test.sav");
    printf("save %d load %d zombies %zu/%zu furn %zu/%zu cols %zu/%zu min %.1f/%.1f\n", sv, ld, g.zs.size(), h.zs.size(), g.world.furn.size(), h.world.furn.size(), g.cols.size(), h.cols.size(), g.minutes, h.minutes);
    // 6) довгий прогін 3 дні з ботом, що спить/їсть — перевірка стабільності і подій
    h.pl.food = h.pl.water = h.pl.energy = 100;
    for (int i = 0; i < 20 * 60 * 40; i++) { h.update(0.05f); if (h.gameOver) break; }
    printf("long run: %s hp %.0f alive %d zombies %zu gameOver %d kills %d\n", h.clockStr().c_str(), h.pl.hp, h.pl.alive, h.zs.size(), h.gameOver, h.kills);
    return 0;
}
