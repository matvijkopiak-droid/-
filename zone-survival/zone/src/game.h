// Симуляція гри: герой (Project Zomboid) + колонія (Infection Free Zone).
#pragma once
#include "world.h"

constexpr float SIM_STEP = 0.05f;      // крок симуляції, секунд
constexpr float MIN_PER_SEC = 2.0f;    // ігрових хвилин за секунду
constexpr int WATER_CUTOFF_DAY = 14;   // день, коли відключають воду

enum Sfx : uint8_t { SFX_STEP, SFX_SWING, SFX_HIT, SFX_PISTOL, SFX_SHOTGUN, SFX_RIFLE, SFX_GROAN, SFX_ZDIE, SFX_DOOR, SFX_GLASS, SFX_WOOD,
                     SFX_PICK, SFX_CLICK, SFX_HURT, SFX_HELI, SFX_LEVELUP, SFX_EAT, SFX_COUNT };
struct SndEvt { Sfx id; Vec2 pos; int z; float vol; };

enum ZType : uint8_t { Z_WALKER, Z_RUNNER, Z_BRUTE };
enum ZState : uint8_t { ZS_IDLE, ZS_WANDER, ZS_ALERT, ZS_CHASE };
struct Zombie {
    int id = 0;
    Vec2 pos; int z = 0; float face = 0;
    float hp = 60; ZType type = Z_WALKER; ZState st = ZS_IDLE;
    Vec2 goal; int goalZ = 0;
    std::vector<PNode> path; int pi = 0;
    float repathT = 0, atkCd = 0, stun = 0, bashCd = 0, openT = 0, groanT = 0, idleT = 0, senseT = 0, lostT = 0, stuckT = 0, searchT = 0;
    bool horde = false, alive = true, active = false;
    int target = -1;               // 0 = гравець, 1000+id колоніста
    int lockX = -1, lockY = -1;    // щоб не бігати вгору-вниз по сходах
    Vec2 lastPos;
};

enum Job : uint8_t { J_FOLLOW, J_IDLE, J_GUARD, J_SCAV, J_FARM, J_BUILD, J_COUNT };
const char* jobName(int j);
struct Colonist {
    int id = 0; std::string name;
    Vec2 pos; int z = 0; float face = 0;
    float hp = 100, food = 70, water = 70, energy = 80;
    Job job = J_FOLLOW; Vec2 post; int postZ = 0;
    ItemId weapon = I_BAT; int ammo = 0;
    bool alive = true, sleeping = false, fleeing = false;
    std::vector<PNode> path; int pi = 0;
    float repathT = 0, atkCd = 0, timer = 0, idleT = 0;
    int state = 0, targetFurn = -1, targetBp = -1;
    Inventory carry;
    int lockX = -1, lockY = -1;
    Vec2 wander;
    int scavLvl = 1, buildLvl = 1, farmLvl = 1, combatLvl = 1;
    std::string status;
};
struct Survivor { int id = 0; std::string name; Vec2 pos; int z = 0; ItemId weapon = I_BAT; int ammo = 0; };

enum BuildKind : uint8_t { BK_WALL, BK_DOOR, BK_CRATE, BK_BED, BK_FARM, BK_FIRE, BK_BARREL, BK_COUNT };
struct BuildDef { const char* name; std::vector<std::pair<ItemId, int>> cost; float time; const char* desc; };
const BuildDef& buildDef(int k);
struct Blueprint { BuildKind kind = BK_WALL; int16_t x = 0, y = 0; int8_t z = 0; float progress = 0; bool alive = true; };

struct Recipe { const char* name; std::vector<std::pair<ItemId, int>> in; ItemId out; int outN; SkillId skill; int minLevel; bool needFire; ItemId tool; float time; };
const std::vector<Recipe>& recipes();

enum ActKind : uint8_t { A_NONE, A_SEARCH, A_BUILD, A_BARRICADE, A_UNBARRICADE, A_RELOAD, A_CRAFT, A_SLEEP, A_CHOP, A_FARM };
struct Action { ActKind kind = A_NONE; float t = 0, dur = 0; int a = -1, b = -1, x = 0, y = 0, z = 0; };

enum IntKind : uint8_t { IK_NONE, IK_FURN, IK_DOOR, IK_WINDOW, IK_BLUEPRINT, IK_SURVIVOR, IK_TREE };
struct Interact { IntKind kind = IK_NONE; int idx = -1; int x = 0, y = 0, z = 0; std::string label; };

struct Player {
    std::string name = "Гравець"; Occupation occ = OCC_POLICE;
    Vec2 pos; int z = 0; float face = 0;
    float hp = 100, food = 85, water = 85, energy = 90, endur = 100;
    bool bleeding = false, infected = false, light = false, alive = true, sprint = false, sneak = false, moving = false;
    float infHours = 48, bleedT = 0, atkCd = 0, stepT = 0, battery = 100, hitFlash = 0;
    Inventory inv; Item held;   // held — зброя в руках
    Skills sk;
    int lockX = -1, lockY = -1;
    int id = 0;
};

struct Moodle { std::string text; int sev; };   // sev 1..3, 0 = добре

class Game {
public:
    World world;
    Player pl;
    std::vector<Zombie> zs;
    std::vector<Colonist> cols;
    std::vector<Survivor> survivors;
    std::vector<Blueprint> bps;
    std::deque<std::string> log;
    std::vector<SndEvt> sounds;
    std::vector<uint8_t> vis;
    Vec2 baseCenter{20, 20}; float baseRadius = 16;
    double minutes = 8 * 60;
    int speedIdx = 1;   // 0 пауза, 1 = ×1, 2 = ×2, 3 = ×4
    bool gameOver = false, sleeping = false;
    uint32_t seed = 1;
    Action act;
    Interact hover;
    int openContainer = -1;
    int kills = 0, nextHordeDay = 3, nextId = 1;
    bool heliDone = false, hordeAnnounced = false;
    Rng rng{1};
    int controlled = -1;  // зарезервовано
    float noiseR = 0, noiseShow = 0;   // для візуалізації шуму героя

    void spawnZombieTest(int dummy) { (void)dummy; for (int k = 0; k < 4; k++) { spawnZombie(ftile(pl.pos.x) + 3 + k, ftile(pl.pos.y) + 2 - k, pl.z, false); zs.back().active = true; } }
    void newGame(uint32_t seed, Occupation occ, const std::string& name);
    void update(float dt);

    // час
    int day() const { return (int)(minutes / 1440.0) + 1; }
    float hour() const { return (float)std::fmod(minutes / 60.0, 24.0); }
    std::string clockStr() const;
    bool isNight() const { float h = hour(); return h >= 21 || h < 5.5f; }
    float darkness() const;  // 0 день .. 1 глибока ніч

    // керування героєм
    void setMove(Vec2 dir, bool sprint, bool sneak);
    void attack(Vec2 aim);
    void interact(bool alt = false);
    void setAim(Vec2 worldPos);
    void trySleep();
    void wake() { wakeUp("Ви прокинулись."); }
    void unequip();
    void barricade(bool remove);
    void toggleLight();
    void reload();
    void cancelAction();
    void useItem(int idx);
    void equipItem(int idx);
    void dropItem(int idx);
    void takeFrom(int furnIdx, int itemIdx);
    void putTo(int furnIdx, int itemIdx);
    void takeAll(int furnIdx);
    bool canCraft(int r) const;
    bool nearFire() const;
    void craft(int r);
    bool placeBlueprint(BuildKind k, int x, int y);
    void cancelBlueprint(int idx);
    bool blueprintSpotOk(BuildKind k, int x, int y, int z, std::string* why = nullptr) const;
    void setJob(int colId, Job j);
    void setPost(int colId);
    void switchToColonist();

    // запити
    float carryCap() const;
    std::vector<Moodle> moodles() const;
    std::vector<int> storageCrates() const;
    int storageCount(ItemId id) const;
    int storageFood() const;
    bool hasMaterial(ItemId id, int n) const;
    bool inBase(Vec2 p) const { return dist(p, baseCenter) <= baseRadius; }
    Colonist* colonist(int id);
    void say(const std::string& s);
    void snd(Sfx id, Vec2 p, int z, float vol = 1.f);
    void makeNoise(Vec2 p, int z, float radius);
    Vec2 aimDir(Vec2 aim) const;

    // збереження
    bool save(const std::string& path) const;
    bool load(const std::string& path);

private:
    friend struct GameTest;
    Vec2 moveIn_{0, 0}, aimPos_{0, 0};
    bool wantSprint_ = false, wantSneak_ = false, sleepBed_ = false;
    float noiseNow_ = 0, zTick_ = 0;
    float acc_ = 0, visT_ = 0, spawnT_ = 0, noiseCool_ = 0, infoT_ = 0;
    int moveDirAimSet_ = 0;
    std::vector<PNode> tmpPath_;
    int pathBudget_ = 0;

    void step(float dt);
    void updateNeeds(float dt);
    void updatePlayer(float dt);
    void updateAction(float dt);
    void updateHover();
    void updateVisibility();
    void updateZombies(float dt);
    void updateZombie(Zombie& z, float dt);
    void updateColonists(float dt);
    void updateColonist(Colonist& c, float dt);
    void updateFarms(float dt);
    void updateEvents(float dt);
    void spawnZombie(int x, int y, int z, bool horde);
    void spawnHorde();
    bool moveEntity(Vec2& pos, int z, Vec2 delta, float radius);
    void tryStairs(Vec2& pos, int& z, int& lx, int& ly);
    void bashTile(int x, int y, int z, float dmg, bool byZombie);
    void hurtPlayer(float dmg, bool allowInfect);
    void hurtColonist(Colonist& c, float dmg);
    void killPlayer(const char* why);
    void zombieDamage(Zombie& z, float dmg, Vec2 from, float knock);
    void shootRay(Vec2 from, Vec2 dir, int z, float range, float dmg, int pierce, float spread);
    void finishAction();
    void startAction(ActKind k, float dur, int a = -1, int b = -1, int x = 0, int y = 0, int z = 0);
    bool followPath(std::vector<PNode>& path, int& pi, Vec2& pos, int& z, float speed, float dt, int& lx, int& ly, float radius, float& face, bool zombie, bool* blockedByStruct, PNode* blockTile);
    bool pathTo(std::vector<PNode>& path, int& pi, Vec2 from, int fz, Vec2 to, int tz, int mode, int maxNodes = 6000);
    void gainSkill(int s, float xp);
    void removeInvAt(int idx, int n);
    int bestFoodIdx(bool water) const;
    void recruit(int survivorIdx);
    int takeMaterial(ItemId id, int n);   // з інвентаря героя і ящиків бази
    void completeBlueprint(Blueprint& b);
    void colEatDrink(Colonist& c);
    Vec2 randomFree(Vec2 around, float r, int z);
    void addColonistFromSurvivor(const Survivor& s);
    bool tryOpenDoor(int x, int y, int z, bool byZombie);
    void wakeUp(const char* why);
    void fixWorldAfterLoad();
    bool colGoTo(Colonist& c, Vec2 target, int tz, float stop, float speed, float dt);
    Colonist* nearestColForZombie(const Zombie& z, float& d);
};
