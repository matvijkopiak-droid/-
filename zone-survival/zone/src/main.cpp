// Зона виживання — точка входу: вікно raylib, цикл, ввід, збереження.
#include "ui.h"
#include "audio.h"
#include <ctime>
#include <fstream>
#include <functional>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __EMSCRIPTEN__
static const char* SAVE_FILE = "/saves/zone_save.dat";   // IDBFS: зберігається в браузері
static void syncSaves() { EM_ASM(FS.syncfs(false, function(e) {});); }
#else
static const char* SAVE_FILE = "zone_save.dat";
static void syncSaves() {}
#endif
static bool fileExists(const std::string& p) { std::ifstream f(p, std::ios::binary); return (bool)f; }

static Font loadFont() {
    std::vector<int> cp;
    for (int c = 32; c < 127; c++) cp.push_back(c);
    for (int c = 0xA0; c <= 0xFF; c++) cp.push_back(c);
    for (int c = 0x400; c <= 0x4FF; c++) cp.push_back(c);
    for (int c : {0x2014, 0x2013, 0x2026, 0x2019, 0x00D7, 0x2022, 0x2116}) cp.push_back(c);
    const char* cands[] = {"assets/DejaVuSans.ttf", "../assets/DejaVuSans.ttf", "../../assets/DejaVuSans.ttf", "DejaVuSans.ttf"};
    for (auto* c : cands) if (fileExists(c)) {
        Font f = LoadFontEx(c, 24, cp.data(), (int)cp.size());
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
        return f;
    }
    return Font{};
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
#ifdef __EMSCRIPTEN__
    EM_ASM(try { FS.mkdir('/saves'); } catch (e) {} FS.mount(IDBFS, {}, '/saves'); FS.syncfs(true, function(e) {}););
#endif
    InitWindow(1280, 720, "Зона виживання");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();
    Audio audio; audio.init();
    Game* gp = new Game(); Game& g = *gp;
    Renderer rend; rend.font = loadFont();
    UI ui; ui.r = &rend;
    Screen screen = SC_TITLE; int occ = 0; int prevSpeed = 1;
    std::string lastName;
    bool quit = false, pauseFrame = false;

    auto startNew = [&]() {
        g.newGame((uint32_t)time(nullptr), (Occupation)occ, "Виживший");
        rend.snapCamera(g); rend.zoom = rend.zoomTarget = 1.f; ui.panel = PN_NONE; ui.buildSel = -1; ui.help = false; screen = SC_PLAY; lastName = g.pl.name;
    };
    auto doLoad = [&]() {
        if (g.load(SAVE_FILE)) { rend.snapCamera(g); screen = SC_PLAY; ui.panel = PN_NONE; ui.buildSel = -1; lastName = g.pl.name; ui.tip("Гру завантажено"); }
        else ui.tip("Не вдалося завантажити");
    };
    auto doSave = [&]() { if (g.save(SAVE_FILE)) { syncSaves(); ui.tip("Гру збережено"); } else ui.tip("Помилка збереження"); };

    std::function<void()> frame = [&]() {
        float dt = GetFrameTime(); ui.begin(); ui.hasSave = fileExists(SAVE_FILE);
        if (screen == SC_PLAY) {
            Vec2 mw = rend.screenToWorld(g, GetMousePosition());
            bool ctrl = IsKeyDown(KEY_LEFT_CONTROL), shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            // ---- ввід
            if (IsKeyPressed(KEY_ESCAPE)) {
                if (ui.help) ui.help = false;
                else if (ui.buildSel >= 0) ui.buildSel = -1;
                else if (ui.panel != PN_NONE) ui.panel = PN_NONE;
                else if (g.openContainer >= 0) g.openContainer = -1;
                else { screen = SC_PAUSE; pauseFrame = true; }
            }
            auto toggle = [&](Panel p) { ui.panel = ui.panel == p ? PN_NONE : p; ui.buildSel = -1; };
            if (IsKeyPressed(KEY_I)) toggle(PN_INV);
            if (IsKeyPressed(KEY_C)) toggle(PN_CRAFT);
            if (IsKeyPressed(KEY_K)) toggle(PN_SKILLS);
            if (IsKeyPressed(KEY_B)) toggle(PN_BUILD);
            if (IsKeyPressed(KEY_TAB)) toggle(PN_COLONY);
            if (IsKeyPressed(KEY_F1)) ui.help = !ui.help;
            if (IsKeyPressed(KEY_F5)) doSave();
            if (IsKeyPressed(KEY_F9)) doLoad();
            if (IsKeyPressed(KEY_SPACE)) { if (g.speedIdx == 0) g.speedIdx = prevSpeed ? prevSpeed : 1; else { prevSpeed = g.speedIdx; g.speedIdx = 0; } }
            if (IsKeyPressed(KEY_ONE)) g.speedIdx = 1;
            if (IsKeyPressed(KEY_TWO)) g.speedIdx = 2;
            if (IsKeyPressed(KEY_THREE)) g.speedIdx = 3;
            float wheel = GetMouseWheelMove();
            if (wheel != 0 && !ui.mouseOnUI()) rend.zoomTarget = clampv(rend.zoomTarget + wheel * .1f, .55f, 1.8f);
            if (IsKeyPressed(KEY_EQUAL)) rend.zoomTarget = std::min(1.8f, rend.zoomTarget + .1f);
            if (IsKeyPressed(KEY_MINUS)) rend.zoomTarget = std::max(.55f, rend.zoomTarget - .1f);

            float kx = (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT) ? 1.f : 0.f) - (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT) ? 1.f : 0.f);
            float ky = (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) ? 1.f : 0.f) - (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) ? 1.f : 0.f);
            Vec2 dir((kx + ky) * .5f, (ky - kx) * .5f); dir = dir.norm();
            if (g.sleeping && (kx != 0 || ky != 0)) g.wake();
            g.setMove(dir, shift, ctrl);
            g.setAim(mw);
            if (IsKeyPressed(KEY_E)) g.interact(shift);
            if (IsKeyPressed(KEY_V)) g.barricade(shift);
            if (IsKeyPressed(KEY_F)) g.toggleLight();
            if (IsKeyPressed(KEY_R)) g.reload();
            if (IsKeyPressed(KEY_Z)) { if (g.sleeping) g.wake(); else g.trySleep(); }
            // будівництво: привид + розміщення
            rend.ghost = false;
            if (ui.buildSel >= 0) {
                int gx = ftile(mw.x), gy = ftile(mw.y);
                rend.ghost = true; rend.ghostKind = (BuildKind)ui.buildSel; rend.ghostX = gx; rend.ghostY = gy;
                rend.ghostOk = g.blueprintSpotOk(rend.ghostKind, gx, gy, g.pl.z);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ui.mouseOnUI()) g.placeBlueprint(rend.ghostKind, gx, gy);
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) ui.buildSel = -1;
            } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !ui.mouseOnUI() && !ui.help) {
                g.attack(mw);
            }
            // ---- оновлення
            g.update(dt);
            rend.update(g, dt);
            if (g.pl.name != lastName) { lastName = g.pl.name; rend.snapCamera(g); }
            for (auto& e : g.sounds) audio.play(e, g.pl.pos, g.pl.z);
            g.sounds.clear();
            if (g.gameOver) screen = SC_OVER;
        }
        // ---- малювання
        BeginDrawing();
        ClearBackground(Color{8, 9, 12, 255});
        switch (screen) {
            case SC_TITLE: { int r = ui.title(); if (r == 1) screen = SC_OCC; else if (r == 2) doLoad(); else if (r == 3) quit = true; break; }
            case SC_OCC: { int r = ui.occupation(occ); if (r == 1) startNew(); else if (r == 2) screen = SC_TITLE; break; }
            case SC_PLAY: case SC_PAUSE: case SC_OVER: {
                rend.draw(g);
                if (screen == SC_PLAY) {
                    ui.hud(g);
                    if (ui.panel == PN_INV) ui.inventory(g);
                    if (g.openContainer >= 0) ui.container(g);
                    if (ui.panel == PN_CRAFT) ui.craft(g);
                    if (ui.panel == PN_SKILLS) ui.skills(g);
                    if (ui.panel == PN_BUILD) ui.build(g);
                    if (ui.panel == PN_COLONY) ui.colony(g);
                    if (ui.help) ui.helpScreen();
                    if (g.openContainer >= 0 && ui.panel != PN_INV) { ui.panel = ui.panel == PN_NONE ? PN_INV : ui.panel; }
                } else if (screen == SC_PAUSE) {
                    ui.hud(g);
                    int r = ui.pause();
                    if (r == 1 || (!pauseFrame && IsKeyPressed(KEY_ESCAPE))) screen = SC_PLAY;
                    else if (r == 2) doSave();
                    else if (r == 3) doLoad();
                    else if (r == 4) screen = SC_TITLE;
                } else {
                    if (ui.over_screen(g)) screen = SC_TITLE;
                }
                break;
            }
        }
        pauseFrame = false;
        EndDrawing();
    };
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg([](void* f) { (*static_cast<std::function<void()>*>(f))(); }, &frame, 0, 1);
#else
    while (!WindowShouldClose() && !quit) frame();
#endif
    audio.shutdown(); CloseAudioDevice(); CloseWindow();
    delete gp;
    return 0;
}
