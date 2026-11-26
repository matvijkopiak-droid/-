#include "../src/render.h"
#include <cstdio>
int main(int argc, char** argv) {
    int seed = argc > 1 ? atoi(argv[1]) : 777; float hour = argc > 2 ? atof(argv[2]) : 12; int mode = argc > 3 ? atoi(argv[3]) : 0;
    stub_resize(1280, 720);
    Game g; g.newGame(seed, OCC_POLICE, "T");
    g.minutes = hour * 60 + 1440 * 0;
    Renderer r; r.snapCamera(g);
    if (mode == 1) { // вийти надвір і піти до дверей
        auto& sb = g.world.buildings[g.world.startBuilding]; g.pl.pos = {sb.doorX + .5f, sb.doorY + 2.5f}; if (g.pl.z) g.pl.z = 0;
    }
    if (mode == 2) { // підійти до багатоповерхівки, на 2й поверх
        for (auto& b : g.world.buildings) if (b.levels == 3) { for (int y = b.y; y < b.y + b.h; y++) for (int x = b.x; x < b.x + b.w; x++) if (g.world.at(x, y, 1).kind == TK_FLOOR_W && g.world.at(x,y,1).room) { g.pl.pos = {x + .5f, y + .5f}; g.pl.z = 1; goto done; } }
    done:;
    }
    if (mode == 3) { // поряд зомбі та колоніст
        g.survivors[0].pos = g.pl.pos + Vec2(2, 1); g.survivors[0].z = 0;
        g.spawnZombieTest(0);
    }
    r.snapCamera(g);
    // кілька кадрів, щоб оновити видимість
    for (int i = 0; i < 5; i++) { g.update(0.05f); r.update(g, 0.05f); }
    r.snapCamera(g);
    ClearBackground(Color{0, 0, 0, 255});
    r.draw(g);
    stub_dump("/tmp/frame.ppm", "/tmp/frame.txt");
    return 0;
}
