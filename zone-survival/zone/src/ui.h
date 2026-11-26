// Інтерфейс: HUD, інвентар, крафт, навички, будівництво, колонія, меню.
#pragma once
#include "render.h"

enum Panel { PN_NONE, PN_INV, PN_CRAFT, PN_SKILLS, PN_BUILD, PN_COLONY };
enum Screen { SC_TITLE, SC_OCC, SC_PLAY, SC_PAUSE, SC_OVER };

struct UI {
    Renderer* r = nullptr;
    Panel panel = PN_NONE;
    bool help = false, clickUsed = false;
    int buildSel = -1;          // обрана споруда для розміщення
    int occSel = 0;
    float scroll = 0;
    bool hasSave = false;
    std::string toast; float toastT = 0;
    std::vector<Rectangle> blocks, prevBlocks;
    bool mouseOnUI() const;

    void begin();               // на початку кадру
    bool over(Rectangle rc) const;
    bool btn(Rectangle rc, const char* label, bool enabled = true, Color col = Color{60, 70, 60, 255});
    void panelBox(Rectangle rc, const char* title);
    void bar(float x, float y, float w, float h, float frac, Color c, const char* label);
    void hud(Game& g);
    void inventory(Game& g);
    void container(Game& g);
    void craft(Game& g);
    void skills(Game& g);
    void build(Game& g);
    void colony(Game& g);
    void helpScreen();
    // повертає 0 нічого, 1 нова гра, 2 продовжити, 3 вийти
    int title();
    int occupation(int& chosen);
    // 0 нічого, 1 продовжити, 2 зберегти, 3 завантажити, 4 в меню
    int pause();
    bool over_screen(const Game& g);
    void tip(const std::string& s) { toast = s; toastT = 2.5f; }
};
