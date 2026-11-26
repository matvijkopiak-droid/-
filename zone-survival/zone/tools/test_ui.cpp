#include "../src/ui.h"
struct GameTest { static void add(Game& g, const Survivor& s) { g.addColonistFromSurvivor(s); } };
int main(int argc, char** argv) {
    int mode = argc > 1 ? atoi(argv[1]) : 0;
    stub_resize(1280, 720);
    Game g; g.newGame(777, OCC_NURSE, "T");
    for (int i = 0; i < 3; i++) { Survivor s = g.survivors[i]; g.survivors.erase(g.survivors.begin() + i); GameTest::add(g, s); }
    g.pl.pos = g.world.playerSpawn;
    for (int i = 0; i < 40; i++) g.update(0.05f);
    Renderer r; r.snapCamera(g); UI ui; ui.r = &r; ui.begin();
    ClearBackground(Color{0, 0, 0, 255});
    r.draw(g); ui.hud(g);
    if (mode == 0) { ui.inventory(g); }
    if (mode == 1) ui.craft(g);
    if (mode == 2) ui.skills(g);
    if (mode == 3) ui.build(g);
    if (mode == 4) ui.colony(g);
    if (mode == 5) ui.helpScreen();
    if (mode == 6) { ui.title(); }
    if (mode == 7) { int c = 1; ui.occupation(c); }
    stub_dump("/tmp/ui.ppm", "/tmp/ui.txt");
}
