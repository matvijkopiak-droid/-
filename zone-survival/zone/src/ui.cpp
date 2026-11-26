#include "ui.h"

static Color C(int r, int g, int b, int a = 255) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
static const Color PANEL = C(22, 24, 28, 235), EDGE = C(90, 96, 104), TXT = C(232, 232, 224), DIM = C(150, 154, 150);

bool UI::mouseOnUI() const { for (auto& b : prevBlocks) if (over(b)) return true; return false; }
void UI::begin() { prevBlocks.swap(blocks); blocks.clear(); clickUsed = false; if (toastT > 0) toastT -= GetFrameTime(); }
bool UI::over(Rectangle rc) const { Vector2 m = GetMousePosition(); return m.x >= rc.x && m.y >= rc.y && m.x < rc.x + rc.width && m.y < rc.y + rc.height; }
bool UI::btn(Rectangle rc, const char* label, bool enabled, Color col) {
    bool hov = over(rc) && enabled;
    Color c = enabled ? (hov ? C(col.r + 40, col.g + 40, col.b + 40) : col) : C(40, 40, 42);
    DrawRectangleRec(rc, c); DrawRectangleLinesEx(rc, 1, hov ? C(250, 240, 180) : EDGE);
    float tw = r->textW(label, 17); r->text(label, rc.x + (rc.width - tw) / 2, rc.y + (rc.height - 17) / 2 - 1, 17, enabled ? TXT : DIM);
    if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !clickUsed) { clickUsed = true; return true; }
    return false;
}
void UI::panelBox(Rectangle rc, const char* title) {
    blocks.push_back(rc);
    DrawRectangleRec(rc, PANEL); DrawRectangleLinesEx(rc, 1, EDGE);
    r->text(title, rc.x + 12, rc.y + 8, 20, C(250, 225, 150));
    DrawRectangle((int)rc.x + 10, (int)rc.y + 34, (int)rc.width - 20, 1, EDGE);
    if (over(rc)) clickUsed = clickUsed || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
void UI::bar(float x, float y, float w, float h, float frac, Color c, const char* label) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, C(0, 0, 0, 170));
    DrawRectangle((int)x, (int)y, (int)(w * clampv(frac, 0.f, 1.f)), (int)h, c);
    DrawRectangleLines((int)x, (int)y, (int)w, (int)h, C(0, 0, 0, 200));
    r->text(label, x + 5, y - 1, 14, TXT);
}

// ------------------------------------------------------------------ HUD
void UI::hud(Game& g) {
    int W = GetScreenWidth(), H = GetScreenHeight(); const Player& p = g.pl;
    float x = 14, y = 12, w = 190, h = 17;
    bar(x, y, w, h, p.hp / 100, C(200, 50, 50), "Здоров'я"); y += h + 4;
    bar(x, y, w, h, p.food / 100, C(220, 150, 50), "Ситість"); y += h + 4;
    bar(x, y, w, h, p.water / 100, C(60, 130, 220), "Вода"); y += h + 4;
    bar(x, y, w, h, p.energy / 100, C(210, 200, 80), "Бадьорість"); y += h + 4;
    bar(x, y, w, h, p.endur / 100, C(90, 190, 110), "Витривалість"); y += h + 8;
    for (auto& m : g.moodles()) {
        Color c = m.sev == 1 ? C(230, 210, 90) : m.sev == 2 ? C(240, 150, 60) : C(240, 70, 60);
        DrawRectangle((int)x, (int)y, 8, 16, c); r->text(m.text.c_str(), x + 14, y - 1, 16, c); y += 19;
    }
    // годинник
    std::string clk = g.clockStr(); float cw = r->textW(clk.c_str(), 22);
    DrawRectangle(W / 2 - (int)cw / 2 - 12, 8, (int)cw + 24, 32, C(0, 0, 0, 150));
    r->text(clk.c_str(), W / 2 - cw / 2, 12, 22, TXT);
    const char* sp[] = {"Пауза", "×1", "×2", "×4"}; std::string si = g.sleeping ? "Сон ×12" : sp[g.speedIdx];
    r->text(si.c_str(), W / 2 - r->textW(si.c_str(), 15) / 2, 42, 15, DIM);
    char b[160]; snprintf(b, sizeof b, "Колонія: %zu  |  Вбито: %d  |  Склад (їжа): %d", g.cols.size(), g.kills, g.storageFood());
    r->text(b, W / 2 - r->textW(b, 15) / 2, 60, 15, DIM);
    // зброя
    {
        std::string wn = p.held.id == I_NONE ? "Кулаки" : idef(p.held.id).name;
        if (p.held.id != I_NONE && idef(p.held.id).type == IT_GUN) { char a[64]; snprintf(a, sizeof a, "  %d/%d  (запас %d)", p.held.ammo, idef(p.held.id).ammoCap, p.inv.count(idef(p.held.id).ammoItem)); wn += a; }
        float tw = r->textW(wn.c_str(), 20);
        DrawRectangle(W - (int)tw - 30, H - 50, (int)tw + 24, 36, C(0, 0, 0, 150)); r->text(wn.c_str(), W - tw - 18, H - 44, 20, TXT);
        if (p.light) r->text("Ліхтарик увімкнено", W - 200, H - 72, 15, C(255, 240, 160));
    }
    // журнал
    float ly = (float)H - 30;
    int shown = 0;
    for (int i = (int)g.log.size() - 1; i >= 0 && shown < 7; i--, shown++) {
        Color c = shown == 0 ? TXT : C(190, 192, 186, 255 - shown * 28);
        r->text(g.log[i].c_str(), 16, ly - shown * 19, 16, c);
    }
    // підказка взаємодії
    if (g.hover.kind != IK_NONE && !g.sleeping) {
        std::string t = "[E] " + g.hover.label; if (g.hover.kind == IK_DOOR || g.hover.kind == IK_WINDOW) t += "   [V] барикада";
        float tw = r->textW(t.c_str(), 18);
        DrawRectangle(W / 2 - (int)tw / 2 - 10, H - 96, (int)tw + 20, 28, C(0, 0, 0, 170)); r->text(t.c_str(), W / 2 - tw / 2, H - 92, 18, C(255, 244, 180));
    }
    // прогрес дії
    if (g.act.kind != A_NONE) {
        float fr = g.act.t / g.act.dur; DrawRectangle(W / 2 - 80, H - 130, 160, 12, C(0, 0, 0, 180)); DrawRectangle(W / 2 - 80, H - 130, (int)(160 * fr), 12, C(240, 210, 90));
    }
    if (toastT > 0) { float tw = r->textW(toast.c_str(), 18); r->text(toast.c_str(), W / 2 - tw / 2, H / 2 - 120, 18, Fade(C(255, 240, 160), std::min(1.f, toastT))); }
    // міні-мапа
    {
        const int S = 2; float mx = W - MAPW * S - 14, my = 12;
        DrawRectangle((int)mx - 2, (int)my - 2, MAPW * S + 4, MAPH * S + 4, C(0, 0, 0, 190));
        for (int yy = 0; yy < MAPH; yy++) for (int xx = 0; xx < MAPW; xx++) {
            if (!g.world.seen[tileIdx(xx, yy, p.z)]) continue;
            const Tile& t = g.world.at(xx, yy, p.z); Color c;
            switch (t.kind) { case TK_ROAD: c = C(70, 72, 78); break; case TK_SIDEWALK: c = C(120, 120, 116); break; case TK_GRASS: c = C(52, 90, 46); break; case TK_TREE: c = C(34, 70, 36); break;
                case TK_WALL: case TK_WINDOW: c = C(190, 180, 160); break; case TK_DOOR: c = C(190, 130, 70); break; default: c = C(130, 100, 70); }
            DrawRectangle((int)mx + xx * S, (int)my + yy * S, S, S, c);
        }
        for (auto& c : g.cols) if (c.alive && c.z == p.z) DrawRectangle((int)(mx + c.pos.x * S) - 1, (int)(my + c.pos.y * S) - 1, 4, 4, C(90, 230, 110));
        for (auto& z : g.zs) if (z.alive && z.z == p.z && g.vis[ftile(z.pos.y) * MAPW + ftile(z.pos.x)]) DrawRectangle((int)(mx + z.pos.x * S) - 1, (int)(my + z.pos.y * S) - 1, 3, 3, C(230, 60, 50));
        for (auto& s : g.survivors) if (s.z == p.z && g.world.seen[tileIdx(ftile(s.pos.x), ftile(s.pos.y), s.z)]) DrawRectangle((int)(mx + s.pos.x * S) - 1, (int)(my + s.pos.y * S) - 1, 4, 4, C(250, 210, 60));
        DrawRectangle((int)(mx + p.pos.x * S) - 2, (int)(my + p.pos.y * S) - 2, 5, 5, C(90, 170, 255));
        DrawCircleLines((int)(mx + g.baseCenter.x * S), (int)(my + g.baseCenter.y * S), g.baseRadius * S, C(255, 255, 255, 90));
        char fl[64]; snprintf(fl, sizeof fl, "Поверх %d", p.z + 1); r->text(fl, mx + 4, my + MAPH * S + 4, 14, DIM);
    }
    r->text("F1 — довідка", W - 112, H - 74, 13, C(120, 124, 120));
}

// ------------------------------------------------------------------ інвентар / контейнер
void UI::inventory(Game& g) {
    Rectangle rc{14, 250, 330, 440}; panelBox(rc, "Інвентар");
    char b[64]; float wt = g.pl.inv.weight() + idef(g.pl.held.id).weight;
    snprintf(b, sizeof b, "%.1f / %.1f кг", wt, g.carryCap()); r->text(b, rc.x + 200, rc.y + 10, 16, wt > g.carryCap() ? C(240, 110, 70) : DIM);
    float y = rc.y + 42;
    if (g.pl.held.id != I_NONE) {
        std::string s = std::string("В руках: ") + idef(g.pl.held.id).name; if (idef(g.pl.held.id).type == IT_GUN) s += " (" + std::to_string(g.pl.held.ammo) + ")";
        r->text(s.c_str(), rc.x + 12, y, 16, C(190, 230, 190));
        if (btn({rc.x + 250, y - 3, 68, 22}, "Зняти")) g.unequip();
        y += 26;
    }
    r->text("ЛКМ — використати/взяти в руки, ПКМ — викинути", rc.x + 12, y, 13, DIM); y += 20;
    for (int i = 0; i < (int)g.pl.inv.items.size(); i++) {
        const Item& it = g.pl.inv.items[i]; Rectangle row{rc.x + 8, y, rc.width - 16, 22};
        if (y > rc.y + rc.height - 26) { r->text("…", rc.x + 14, y, 16, DIM); break; }
        bool hov = over(row); if (hov) DrawRectangleRec(row, C(60, 66, 74, 200));
        std::string s = idef(it.id).name; if (it.n > 1) s += " ×" + std::to_string(it.n); if (idef(it.id).type == IT_GUN) s += " (" + std::to_string(it.ammo) + ")";
        r->text(s.c_str(), row.x + 6, y + 1, 16, TXT);
        snprintf(b, sizeof b, "%.1f", idef(it.id).weight * it.n); r->text(b, row.x + row.width - 42, y + 1, 15, DIM);
        if (hov && !clickUsed) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                clickUsed = true;
                if ((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) && g.openContainer >= 0) g.putTo(g.openContainer, i); else g.useItem(i);
                break;
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) { clickUsed = true; g.dropItem(i); break; }
        }
        y += 23;
    }
}
void UI::container(Game& g) {
    if (g.openContainer < 0 || g.openContainer >= (int)g.world.furn.size()) { g.openContainer = -1; return; }
    Furn& f = g.world.furn[g.openContainer];
    if (!f.alive || dist(Vec2(f.x + .5f, f.y + .5f), g.pl.pos) > 3.4f || f.z != g.pl.z) { g.openContainer = -1; return; }
    int W = GetScreenWidth(); Rectangle rc{(float)W - 350, 250, 330, 380};
    panelBox(rc, furnName(f.kind));
    if (btn({rc.x + rc.width - 120, rc.y + 5, 108, 24}, "Взяти все")) { g.takeAll(g.openContainer); return; }
    float y = rc.y + 42;
    if (f.inv.empty()) r->text("Порожньо", rc.x + 14, y, 16, DIM);
    for (int i = 0; i < (int)f.inv.items.size(); i++) {
        const Item& it = f.inv.items[i]; Rectangle row{rc.x + 8, y, rc.width - 16, 22};
        if (y > rc.y + rc.height - 26) { r->text("…", rc.x + 14, y, 16, DIM); break; }
        bool hov = over(row); if (hov) DrawRectangleRec(row, C(60, 66, 74, 200));
        std::string s = idef(it.id).name; if (it.n > 1) s += " ×" + std::to_string(it.n);
        r->text(s.c_str(), row.x + 6, y + 1, 16, TXT);
        if (hov && !clickUsed && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { clickUsed = true; g.takeFrom(g.openContainer, i); break; }
        y += 23;
    }
    if (f.built || f.kind == F_CAR || f.kind == F_PILE) r->text("Покласти: відкрийте інвентар (I), Shift+ЛКМ по предмету", rc.x + 12, rc.y + rc.height - 22, 12, DIM);
    // Shift+ЛКМ по інвентарю кладе предмет у контейнер — обробляється в inventory(): тут просто підказка
}

// ------------------------------------------------------------------ крафт
void UI::craft(Game& g) {
    Rectangle rc{360, 90, 440, 420}; panelBox(rc, "Крафт");
    const auto& R = recipes(); float y = rc.y + 44;
    if (g.nearFire()) r->text("Поруч вогнище — можна готувати", rc.x + 12, y - 2, 13, C(255, 190, 110));
    y += 18;
    for (int i = 0; i < (int)R.size(); i++) {
        const Recipe& rc2 = R[i]; bool ok = g.canCraft(i);
        std::string in; for (auto& p : rc2.in) { if (!in.empty()) in += ", "; in += std::string(idef(p.first).name) + " ×" + std::to_string(p.second); }
        std::string extra;
        if (rc2.tool != I_NONE) extra += std::string("  [потрібно: ") + idef(rc2.tool).name + "]";
        if (rc2.needFire) extra += "  [біля вогнища]";
        if (rc2.minLevel > 0) extra += std::string("  [") + skillName(rc2.skill) + " " + std::to_string(rc2.minLevel) + "]";
        r->text(rc2.name, rc.x + 14, y, 18, ok ? TXT : DIM);
        r->text((in + extra).c_str(), rc.x + 14, y + 20, 13, DIM);
        if (btn({rc.x + rc.width - 100, y + 2, 88, 30}, "Зробити", ok, C(60, 90, 60))) g.craft(i);
        y += 58;
    }
}
void UI::skills(Game& g) {
    Rectangle rc{360, 90, 440, 560}; panelBox(rc, (g.pl.name + " — навички").c_str());
    float y = rc.y + 44;
    for (int s = 0; s < SK_COUNT; s++) {
        int lv = g.pl.sk[s]; float need = Skills::needed(lv);
        r->text(skillName(s), rc.x + 14, y, 17, TXT);
        char b[32]; snprintf(b, sizeof b, "рів. %d", lv); r->text(b, rc.x + 150, y, 16, C(250, 225, 150));
        DrawRectangle((int)rc.x + 220, (int)y + 5, 200, 10, C(0, 0, 0, 170));
        DrawRectangle((int)rc.x + 220, (int)y + 5, (int)(200 * (lv >= 10 ? 1.f : g.pl.sk.xp[s] / need)), 10, C(110, 190, 110));
        r->text(skillDesc(s), rc.x + 14, y + 20, 12, DIM);
        y += 40;
    }
}

// ------------------------------------------------------------------ будівництво
void UI::build(Game& g) {
    Rectangle rc{360, 90, 440, 470}; panelBox(rc, "Будівництво (у межах бази)");
    float y = rc.y + 44;
    r->text("Оберіть споруду, потім ЛКМ по клітинці. ПКМ/Esc — скасувати.", rc.x + 12, y - 4, 13, DIM); y += 20;
    for (int k = 0; k < BK_COUNT; k++) {
        const BuildDef& d = buildDef(k); bool ok = true; std::string cs;
        for (auto& c : d.cost) { if (!cs.empty()) cs += ", "; cs += std::string(idef(c.first).name) + " ×" + std::to_string(c.second); if (!g.hasMaterial(c.first, c.second)) ok = false; }
        bool sel = buildSel == k;
        r->text(d.name, rc.x + 14, y, 18, sel ? C(160, 240, 160) : (ok ? TXT : DIM));
        r->text(cs.c_str(), rc.x + 14, y + 20, 13, ok ? DIM : C(200, 110, 90));
        if (btn({rc.x + rc.width - 100, y + 2, 88, 30}, sel ? "Обрано" : "Обрати", true, sel ? C(50, 110, 60) : C(60, 70, 80))) { buildSel = k; panel = PN_NONE; tip(std::string("Розміщення: ") + d.name); }
        y += 54;
    }
    r->text("Плани будують і колоністи (робота «Будувати»). Матеріали беруться з інвентаря та скринь бази.", rc.x + 12, rc.y + rc.height - 22, 12, DIM);
}

// ------------------------------------------------------------------ колонія
void UI::colony(Game& g) {
    int W = GetScreenWidth(); Rectangle rc{(float)W / 2 - 330, 90, 660, 520}; panelBox(rc, "Колонія");
    float y = rc.y + 44;
    char b[160];
    snprintf(b, sizeof b, "Склад: їжа/вода %d | дошки %d | цвяхи %d | насіння %d | набої 9мм %d", g.storageFood(), g.storageCount(I_PLANK), g.storageCount(I_NAILS), g.storageCount(I_SEEDS), g.storageCount(I_AMMO9));
    r->text(b, rc.x + 14, y, 15, C(250, 225, 150)); y += 24;
    if (g.storageCrates().empty()) r->text("Збудуйте скриню (B) у межах бази — це спільний склад.", rc.x + 14, y, 14, C(240, 140, 100));
    y += 24;
    if (g.cols.empty()) r->text("Колоністів немає. Шукайте вцілілих по місту (жовті мітки).", rc.x + 14, y, 16, DIM);
    for (auto& c : g.cols) {
        DrawRectangle((int)rc.x + 8, (int)y, (int)rc.width - 16, 76, C(34, 38, 44, 220));
        snprintf(b, sizeof b, "%s  — %s", c.name.c_str(), c.status.c_str()); r->text(b, rc.x + 16, y + 4, 18, TXT);
        snprintf(b, sizeof b, "HP %.0f  Їжа %.0f  Вода %.0f  Сили %.0f", c.hp, c.food, c.water, c.energy); r->text(b, rc.x + 16, y + 28, 14, DIM);
        snprintf(b, sizeof b, "Зброя: %s%s", idef(c.weapon).name, idef(c.weapon).type == IT_GUN ? (" (" + std::to_string(c.ammo) + ")").c_str() : "");
        r->text(b, rc.x + 16, y + 48, 13, DIM);
        if (btn({rc.x + 330, y + 8, 220, 28}, (std::string("Робота: ") + jobName(c.job)).c_str(), true, C(50, 70, 100))) g.setJob(c.id, (Job)((c.job + 1) % J_COUNT));
        if (btn({rc.x + 330, y + 40, 140, 28}, "Пост тут", true, C(70, 70, 50))) g.setPost(c.id);
        y += 84;
        if (y > rc.y + rc.height - 90) break;
    }
}

// ------------------------------------------------------------------ довідка і меню
void UI::helpScreen() {
    int W = GetScreenWidth(), H = GetScreenHeight(); Rectangle rc{(float)W / 2 - 360, (float)H / 2 - 270, 720, 540}; panelBox(rc, "Довідка");
    const char* L[] = {
        "WASD — рух (по екрану)    Shift — спринт (шумно!)    Ctrl — крастися (тихо)",
        "Миша — приціл/напрямок    ЛКМ — удар/постріл    R — перезарядка",
        "E — взаємодія (обшук, двері, вікна, вцілілі, грядки, ліжко)    Shift+E — замкнути двері",
        "V — барикада вікна/дверей (дошки+цвяхи+молоток)    Shift+V — зняти",
        "F — ліхтарик    Z — спати (краще в ліжку)    I — інвентар    C — крафт",
        "K — навички    B — будівництво    Tab — колонія    +/- або колесо — масштаб",
        "Пробіл — пауза    1/2/3 — швидкість гри    F5 — зберегти    F9 — завантажити    Esc — меню",
        "",
        "Мета: вижити, знайти вцілілих, укріпити базу (радіус 16 клітинок від стартового дому).",
        "Зомбі чують шум: біг, постріли, удари по дверях. Вночі видно гірше — візьміть ліхтарик.",
        "Укуси можуть заразити (48 год до смерті). Бинти зупиняють кровотечу.",
        "Колоністам задайте роботу в панелі Tab: збирати припаси, будувати, садити, охороняти.",
        "День 3 і далі кожні 3 дні вночі приходить орда. Скрині на базі — спільний склад.",
    };
    float y = rc.y + 46; for (auto* s : L) { r->text(s, rc.x + 16, y, 16, TXT); y += 30; }
    r->text("F1 — закрити", rc.x + 16, rc.y + rc.height - 28, 14, DIM);
}
int UI::title() {
    int W = GetScreenWidth(), H = GetScreenHeight(); int res = 0;
    DrawRectangle(0, 0, W, H, C(10, 12, 14));
    // фон-силует міста
    for (int i = 0; i < 14; i++) { int bw = 60 + (i * 37) % 50, bh = 80 + (i * 53) % 160; DrawRectangle(i * (W / 13), H - 40 - bh, bw, bh + 40, C(18 + i % 3 * 3, 20, 24)); }
    const char* t = "ЗОНА ВИЖИВАННЯ"; float tw = r->textW(t, 56); r->text(t, W / 2 - tw / 2, H / 2 - 190, 56, C(230, 210, 140));
    const char* s = "Колонія + виживання одного героя в мертвому місті"; r->text(s, W / 2 - r->textW(s, 20) / 2, H / 2 - 120, 20, DIM);
    float bx = W / 2 - 130, by = H / 2 - 50;
    if (btn({bx, by, 260, 44}, "Нова гра", true, C(60, 90, 60))) res = 1;
    if (btn({bx, by + 56, 260, 44}, "Продовжити (збереження)", hasSave, C(60, 80, 100))) res = 2;
    if (btn({bx, by + 112, 260, 44}, "Вийти", true, C(100, 60, 60))) res = 3;
    r->text("Isometric 2.5D · C++17 · raylib", W / 2 - 110, H - 40, 15, DIM);
    return res;
}
int UI::occupation(int& chosen) {
    int W = GetScreenWidth(), H = GetScreenHeight(); int res = 0;
    DrawRectangle(0, 0, W, H, C(10, 12, 14));
    r->text("Оберіть професію героя", W / 2 - 170, 60, 32, C(230, 210, 140));
    float y = 140;
    for (int o = 0; o < OCC_COUNT; o++) {
        Rectangle rc{(float)W / 2 - 330, y, 660, 66}; bool sel = chosen == o;
        DrawRectangleRec(rc, sel ? C(48, 70, 52) : C(28, 32, 36)); DrawRectangleLinesEx(rc, 1, sel ? C(170, 240, 170) : EDGE);
        r->text(occName(o), rc.x + 16, rc.y + 8, 24, TXT); r->text(occDesc(o), rc.x + 16, rc.y + 38, 16, DIM);
        if (over(rc) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) chosen = o;
        y += 76;
    }
    if (btn({(float)W / 2 - 200, y + 20, 190, 44}, "Почати", true, C(60, 100, 60))) res = 1;
    if (btn({(float)W / 2 + 10, y + 20, 190, 44}, "Назад", true, C(90, 60, 60))) res = 2;
    return res;
}
int UI::pause() {
    int W = GetScreenWidth(), H = GetScreenHeight(); int res = 0;
    DrawRectangle(0, 0, W, H, C(0, 0, 0, 150));
    Rectangle rc{(float)W / 2 - 160, (float)H / 2 - 150, 320, 300}; panelBox(rc, "Пауза");
    float y = rc.y + 52;
    if (btn({rc.x + 30, y, 260, 40}, "Продовжити")) res = 1;
    y += 50;
    if (btn({rc.x + 30, y, 260, 40}, "Зберегти (F5)")) res = 2;
    y += 50;
    if (btn({rc.x + 30, y, 260, 40}, "Завантажити (F9)", hasSave)) res = 3;
    y += 50;
    if (btn({rc.x + 30, y, 260, 40}, "У головне меню", true, C(100, 60, 60))) res = 4;
    return res;
}
bool UI::over_screen(const Game& g) {
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, C(60, 0, 0, 170));
    r->text("ВИ ЗАГИНУЛИ", W / 2 - 170, H / 2 - 130, 52, C(240, 90, 80));
    char b[160]; snprintf(b, sizeof b, "Ви протрималися: %s    Вбито зомбі: %d", g.clockStr().c_str(), g.kills);
    r->text(b, W / 2 - r->textW(b, 20) / 2, H / 2 - 50, 20, TXT);
    return btn({(float)W / 2 - 110, (float)H / 2 + 10, 220, 44}, "У головне меню", true, C(90, 60, 60));
}
