#include "../src/world.h"
#include <cstdio>
int main() {
    World w; w.generate(12345);
    printf("buildings %zu furn %zu indoor %zu outdoor %zu survivors %zu\n", w.buildings.size(), w.furn.size(), w.indoorTiles.size(), w.outdoorTiles.size(), w.survivorSpots.size());
    for (int y = 0; y < MAPH; y += 1) {
        for (int x = 0; x < MAPW; x++) {
            const Tile& t = w.at(x, y, 0); char c = '.';
            switch (t.kind) { case TK_ROAD: c = '='; break; case TK_SIDEWALK: c = ','; break; case TK_WALL: c = '#'; break; case TK_WINDOW: c = 'O'; break; case TK_DOOR: c = 'D'; break;
                case TK_STAIR_UP: c = '^'; break; case TK_FENCE: c = 'X'; break; case TK_TREE: c = 't'; break; case TK_FLOOR_W: c = 'w'; break; case TK_FLOOR_T: c = '_'; break; case TK_FLOOR_C: c = '-'; break; default: break; }
            if (w.furnAt[tileIdx(x, y, 0)] >= 0) c = 'f';
            if ((int)w.playerSpawn.x == x && (int)w.playerSpawn.y == y) c = '@';
            putchar(c);
        }
        putchar('\n');
    }
    // зв'язність: з точки спавну до кількох випадкових клітинок
    std::vector<PNode> path; int ok = 0, tot = 0;
    PNode from{(int16_t)w.playerSpawn.x, (int16_t)w.playerSpawn.y, 0};
    for (int i = 0; i < 40; i++) { PNode to = w.outdoorTiles[(i * 977) % w.outdoorTiles.size()]; tot++; if (w.findPath(from, to, path, 60000, PM_ZOMBIE)) ok++; }
    printf("zombie path ok %d/%d\n", ok, tot);
    ok = 0; tot = 0;
    for (int i = 0; i < 40; i++) { PNode to = w.outdoorTiles[(i * 977) % w.outdoorTiles.size()]; tot++; if (w.findPath(from, to, path, 60000, PM_HUMAN)) ok++; }
    printf("human path ok %d/%d (locked doors may block)\n", ok, tot);
    // upper floors reachable
    int up = 0, upok = 0;
    for (auto& p : w.indoorTiles) if (p.z > 0 && up < 30) { up++; if (w.findPath(from, p, path, 60000, PM_ZOMBIE)) upok++; }
    printf("upper reachable %d/%d\n", upok, up);
    return 0;
}
