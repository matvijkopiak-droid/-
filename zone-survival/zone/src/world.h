// Світ: клітинки, будинки, меблі-контейнери, генерація міста, шляхи, видимість.
#pragma once
#include "common.h"
#include "items.h"

enum Tk : uint8_t { TK_GRASS, TK_ROAD, TK_SIDEWALK, TK_FLOOR_W, TK_FLOOR_T, TK_FLOOR_C, TK_WALL, TK_WINDOW, TK_DOOR, TK_STAIR_UP, TK_STAIR_DOWN, TK_FENCE, TK_TREE, TK_AIR };
enum TileFlag : uint8_t { TF_OPEN = 1, TF_LOCKED = 2, TF_BROKEN = 4, TF_INDOOR = 8, TF_BUILT = 16 };
enum RoomType : uint8_t { R_NONE, R_LIVING, R_KITCHEN, R_BEDROOM, R_BATH, R_SALES, R_STORAGE };
const char* roomName(int r);

struct Tile {
    uint8_t kind = TK_AIR;
    uint8_t hp = 0;      // міцність стіни/дверей/вікна
    uint8_t barr = 0;    // кількість дощок барикади 0..4
    uint8_t bhp = 0;     // міцність верхньої дошки
    uint8_t flags = 0;
    uint8_t room = 0;
    int16_t bld = -1;
};

enum BType : uint8_t { B_HOUSE, B_SHOP, B_SUPER, B_PHARMACY, B_GUN, B_GAS, B_WAREHOUSE, B_APARTMENT };
struct Building {
    int x = 0, y = 0, w = 0, h = 0, levels = 1;
    BType type = B_HOUSE;
    std::string name;
    int doorX = 0, doorY = 0;
};

enum FurnKind : uint8_t { F_FRIDGE, F_CUPBOARD, F_WARDROBE, F_SHELF, F_COUNTER, F_CRATE, F_BED, F_TABLE, F_SOFA, F_SINK, F_CAR, F_FARM, F_FIRE, F_BARREL, F_GUNRACK, F_PILE };
const char* furnName(int k);
bool furnIsContainer(int k);
bool furnSolid(int k);

struct Furn {
    FurnKind kind = F_CRATE;
    int16_t x = 0, y = 0;
    int8_t z = 0;
    bool alive = true, searched = false, built = false;
    uint8_t stage = 0;   // грядка: 0 порожньо, 1 посів, 2 росте, 3 дозріло; бочка: кількість води
    float timer = 0;
    Inventory inv;
};

struct PNode { int16_t x, y; int8_t z; };
enum PathMode { PM_HUMAN = 0, PM_ZOMBIE = 1 };

class World {
public:
    std::vector<Tile> tiles;
    std::vector<Furn> furn;
    std::vector<int> furnAt;
    std::vector<Building> buildings;
    std::vector<uint8_t> seen;   // пам'ять про відкриті клітинки
    std::vector<PNode> indoorTiles, outdoorTiles;
    std::vector<PNode> survivorSpots;
    Vec2 playerSpawn{10, 10};
    int startBuilding = 0;

    void generate(uint32_t seed);

    Tile& at(int x, int y, int z) { return tiles[tileIdx(x, y, z)]; }
    const Tile& at(int x, int y, int z) const { return tiles[tileIdx(x, y, z)]; }
    int furnIndexAt(int x, int y, int z) const { return inMap(x, y, z) ? furnAt[tileIdx(x, y, z)] : -1; }
    Furn* furnPtrAt(int x, int y, int z) { int i = furnIndexAt(x, y, z); return i >= 0 ? &furn[i] : nullptr; }
    int addFurn(FurnKind k, int x, int y, int z, bool built);
    void removeFurn(int idx);

    bool blocks(int x, int y, int z) const;      // чи не можна стояти/йти
    bool opaque(int x, int y, int z) const;      // чи не видно крізь
    bool isFloor(int x, int y, int z) const;     // є підлога/земля
    bool indoor(int x, int y, int z) const { return inMap(x, y, z) && (at(x, y, z).flags & TF_INDOOR); }
    int pathCost(int x, int y, int z, int mode) const;
    bool findPath(PNode from, PNode to, std::vector<PNode>& out, int maxNodes, int mode) const;
    bool los(float x0, float y0, float x1, float y1, int z) const;
    void computeVisible(int px, int py, int pz, int radius, std::vector<uint8_t>& vis);
    int buildingAt(int x, int y, int z) const { return inMap(x, y, z) ? at(x, y, z).bld : -1; }

private:
    mutable std::vector<float> gcost_;
    mutable std::vector<int> parent_, stamp_;
    mutable int curStamp_ = 0;
    int addBuilding(BType t, int bx, int by, int bw, int bh, int levels, int doorSide, Rng& r);
    void fillLoot(Furn& f, BType bt, int room, Rng& r);
};
