#include "render.h"

static float sDk = 0;
static Color C(int r, int g, int b, int a = 255) { return Color{(unsigned char)clampv(r, 0, 255), (unsigned char)clampv(g, 0, 255), (unsigned char)clampv(b, 0, 255), (unsigned char)clampv(a, 0, 255)}; }
static Color mulc(Color c, float k) { return C((int)(c.r * k), (int)(c.g * k), (int)(c.b * k), c.a); }
static Color lerpc(Color a, Color b, float t) { return C((int)lerpf(a.r, b.r, t), (int)lerpf(a.g, b.g, t), (int)lerpf(a.b, b.b, t), (int)lerpf(a.a, b.a, t)); }
static unsigned hsh(int x, int y, int z = 0) { unsigned h = (unsigned)(x * 73856093) ^ (unsigned)(y * 19349663) ^ (unsigned)(z * 83492791); h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return h; }
// освітлення: L — яскравість; уночі додаємо синюватий відтінок
static Color S(Color c, float L) {
    float r = c.r * L * (1 - .10f * sDk), g = c.g * L * (1 - .02f * sDk), b = c.b * L * (1 + .22f * sDk) + 6 * sDk * L;
    return C((int)r, (int)g, (int)b, c.a);
}

Vector2 Renderer::P(float x, float y, float zpx) const {
    float sx = (x - y) * TW * .5f, sy = (x + y) * TH * .5f - zpx;
    return {(sx - camX) * zoom + GetScreenWidth() * .5f, (sy - camY) * zoom + GetScreenHeight() * .5f};
}
Vec2 Renderer::screenToWorld(const Game& g, Vector2 s) const {
    float sx = (s.x - GetScreenWidth() * .5f) / zoom + camX;
    float sy = (s.y - GetScreenHeight() * .5f) / zoom + camY + g.pl.z * LV - 15.f;
    float a = sx / (TW * .5f), b = sy / (TH * .5f);
    return {(a + b) * .5f, (b - a) * .5f};
}
void Renderer::text(const char* t, float x, float y, float size, Color c) const { DrawTextEx(font, t, {x, y}, size, 0.5f, c); }
float Renderer::textW(const char* t, float size) const { return MeasureTextEx(font, t, size, 0.5f).x; }

void Renderer::snapCamera(const Game& g) {
    camX = (g.pl.pos.x - g.pl.pos.y) * TW * .5f;
    camY = (g.pl.pos.x + g.pl.pos.y) * TH * .5f - g.pl.z * LV - 16;
}
void Renderer::update(const Game& g, float dt) {
    time += dt;
    float tx = (g.pl.pos.x - g.pl.pos.y) * TW * .5f, ty = (g.pl.pos.x + g.pl.pos.y) * TH * .5f - g.pl.z * LV - 16;
    float k = 1 - std::exp(-dt * 9.f);
    camX += (tx - camX) * k; camY += (ty - camY) * k;
    zoom += (zoomTarget - zoom) * (1 - std::exp(-dt * 10.f));
}

// ------------------------------------------------------------------ примітиви
void Renderer::tri(Vector2 a, Vector2 b, Vector2 c, Color col) const {
    float cr = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cr == 0) return;
    if (cr > 0) std::swap(b, c);   // raylib малює лише трикутники проти годинникової стрілки
    DrawTriangle(a, b, c, col);
}
void Renderer::quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col) const { tri(a, b, c, col); tri(a, c, d, col); }
void Renderer::diamond(float x0, float y0, float x1, float y1, float zpx, Color c) const { quad(P(x0, y0, zpx), P(x1, y0, zpx), P(x1, y1, zpx), P(x0, y1, zpx), c); }
void Renderer::box(float x0, float y0, float x1, float y1, float zb, float h, Color top, Color left, Color right, bool edges) const {
    float zt = zb + h;
    // права грань (площина x = x1), ліва грань (площина y = y1), верх
    quad(P(x1, y0, zb), P(x1, y1, zb), P(x1, y1, zt), P(x1, y0, zt), right);
    quad(P(x0, y1, zb), P(x1, y1, zb), P(x1, y1, zt), P(x0, y1, zt), left);
    quad(P(x0, y0, zt), P(x1, y0, zt), P(x1, y1, zt), P(x0, y1, zt), top);
    if (edges && h > 3) {
        Color e = Fade(Color{0, 0, 0, 255}, .35f);
        DrawLineEx(P(x1, y1, zb), P(x1, y1, zt), 1.f, e);
        DrawLineEx(P(x0, y1, zt), P(x1, y1, zt), 1.f, e);
        DrawLineEx(P(x1, y1, zt), P(x1, y0, zt), 1.f, e);
    }
}
// прямокутник на грані: leftFace — площина y=y1, інакше площина x=x1; u — вздовж грані (0..1), v — висота (px)
void Renderer::faceQuad(bool leftFace, float x0, float y0, float x1, float y1, float zb, float u0, float u1, float v0, float v1, Color c) const {
    auto pt = [&](float u, float v) { return leftFace ? P(x0 + (x1 - x0) * u, y1, zb + v) : P(x1, y0 + (y1 - y0) * u, zb + v); };
    quad(pt(u0, v0), pt(u1, v0), pt(u1, v1), pt(u0, v1), c);
}

// ------------------------------------------------------------------ освітлення
void Renderer::computeLights(const Game& g) {
    lightAdd_.assign(MAPW * MAPH, 0.f);
    if (dk_ < .04f) return;
    auto add = [&](float cx, float cy, float R, float I, bool cone, float face, float halo) {
        for (int y = (int)(cy - R) - 1; y <= (int)(cy + R) + 1; y++)
            for (int x = (int)(cx - R) - 1; x <= (int)(cx + R) + 1; x++) {
                if (!inMap(x, y, pz_)) continue;
                float dx = x + .5f - cx, dy = y + .5f - cy, d = std::sqrt(dx * dx + dy * dy);
                if (d > R) continue;
                if (d > 1.2f && !g.world.los(cx, cy, x + .5f, y + .5f, pz_)) continue;
                float v = I * std::pow(1 - d / R, 1.3f);
                if (cone) {
                    float ang = std::atan2(dy, dx) - face; while (ang > PI_F) ang -= 2 * PI_F; while (ang < -PI_F) ang += 2 * PI_F;
                    float f = clampv(1.f - (std::abs(ang) - .35f) / .5f, 0.f, 1.f);
                    v *= std::max(f, d < halo ? 1.f : 0.f);
                }
                lightAdd_[y * MAPW + x] += v;
            }
    };
    if (g.pl.alive) {
        add(g.pl.pos.x, g.pl.pos.y, 3.6f, .42f, false, 0, 0);
        if (g.pl.light) add(g.pl.pos.x, g.pl.pos.y, 11.f, .95f, true, g.pl.face, 2.f);
    }
    for (auto& f : g.world.furn)
        if (f.alive && f.kind == F_FIRE && f.z == pz_) add(f.x + .5f, f.y + .5f, 6.5f, .8f + .1f * std::sin(time * 9 + f.x), false, 0, 0);
}
float Renderer::brightness(int x, int y, int z, float& vf) const {
    const Game& g = *g_; vf = 0;
    bool direct = (z == pz_) || (z > pz_ && pz_ == 0);
    int sz = (z > pz_) ? 0 : z;
    if (direct && g.vis[y * MAPW + x]) vf = 1;
    else if (g.world.seen[tileIdx(x, y, sz)]) vf = .5f;
    if (vf == 0) return 0;
    if (vf < 1) return .30f;
    bool in = g.world.indoor(x, y, z);
    float amb = in ? lerpf(.80f, .16f, dk_) : lerpf(1.f, .24f, dk_);
    float L = amb + (z == pz_ ? lightAdd_[y * MAPW + x] : 0.f);
    return std::min(1.18f, L);
}

// ------------------------------------------------------------------ палітри
static const Color WALL_PAL[8] = {C(214, 196, 164), C(170, 78, 66), C(150, 168, 184), C(196, 214, 204), C(92, 104, 76), C(214, 176, 74), C(130, 136, 146), C(188, 150, 116)};
static const Color ROOF_PAL[8] = {C(120, 56, 48), C(80, 82, 96), C(96, 104, 114), C(70, 110, 96), C(60, 66, 56), C(150, 60, 50), C(90, 92, 100), C(104, 76, 64)};

void Renderer::drawFurn(const Furn& f, float L) {
    float x = f.x, y = f.y, zb = f.z * LV;
    auto B = [&](float a, float b, float c, float d, float z0, float h, Color col) { box(x + a, y + b, x + c, y + d, zb + z0, h, S(mulc(col, 1.12f), L), S(mulc(col, .82f), L), S(mulc(col, .64f), L)); };
    switch (f.kind) {
        case F_FRIDGE: B(.14f, .14f, .86f, .86f, 0, 32, C(226, 230, 234)); faceQuad(true, x + .14f, y + .14f, x + .86f, y + .86f, zb, .1f, .9f, 20, 21.5f, S(C(90, 94, 100), L)); break;
        case F_CUPBOARD: B(.08f, .1f, .92f, .9f, 0, 24, C(150, 106, 66)); break;
        case F_WARDROBE: B(.08f, .08f, .92f, .92f, 0, 42, C(104, 72, 46)); faceQuad(true, x + .08f, y + .08f, x + .92f, y + .92f, zb, .48f, .52f, 4, 38, S(C(50, 34, 22), L)); break;
        case F_SHELF: {
            B(.1f, .1f, .9f, .9f, 0, 34, C(128, 92, 58));
            static const Color bk[4] = {C(190, 70, 60), C(70, 120, 190), C(220, 190, 80), C(90, 160, 100)};
            for (int i = 0; i < 4; i++) faceQuad(true, x + .1f, y + .1f, x + .9f, y + .9f, zb, .1f + i * .2f, .26f + i * .2f, 12 + (i & 1) * 10, 21 + (i & 1) * 10, S(bk[i], L));
            break;
        }
        case F_COUNTER: B(.03f, .05f, .97f, .95f, 0, 20, C(168, 138, 104)); box(x + .03f, y + .05f, x + .97f, y + .95f, zb + 20, 2, S(C(210, 210, 206), L), S(C(170, 170, 166), L), S(C(140, 140, 136), L)); break;
        case F_CRATE: B(.12f, .12f, .88f, .88f, 0, f.built ? 18 : 16, C(168, 120, 68)); faceQuad(false, x + .12f, y + .12f, x + .88f, y + .88f, zb, .0f, 1.f, 7, 8.2f, S(C(90, 60, 30), L)); break;
        case F_BED: B(.04f, .04f, .96f, .96f, 0, 8, C(120, 86, 56)); box(x + .08f, y + .08f, x + .92f, y + .92f, zb + 8, 4, S(C(104, 140, 196), L), S(C(80, 110, 160), L), S(C(64, 90, 130), L)); box(x + .1f, y + .1f, x + .4f, y + .9f, zb + 12, 3, S(C(236, 236, 240), L), S(C(200, 200, 206), L), S(C(170, 170, 176), L)); break;
        case F_TABLE:
            B(.1f, .1f, .2f, .2f, 0, 12, C(120, 84, 52)); B(.8f, .1f, .9f, .2f, 0, 12, C(120, 84, 52)); B(.1f, .8f, .2f, .9f, 0, 12, C(120, 84, 52)); B(.8f, .8f, .9f, .9f, 0, 12, C(120, 84, 52));
            B(.04f, .04f, .96f, .96f, 12, 4, C(158, 112, 70)); break;
        case F_SOFA: B(.05f, .1f, .95f, .9f, 0, 10, C(86, 128, 128)); B(.05f, .1f, .95f, .3f, 10, 12, C(70, 108, 108)); break;
        case F_SINK: B(.08f, .1f, .92f, .9f, 0, 20, C(214, 216, 220)); box(x + .25f, y + .25f, x + .75f, y + .75f, zb + 20, 1, S(C(90, 140, 190), L), S(C(80, 120, 170), L), S(C(60, 100, 150), L), false); break;
        case F_CAR: {
            static const Color cc[6] = {C(190, 60, 52), C(60, 98, 170), C(210, 200, 180), C(70, 74, 80), C(70, 140, 96), C(200, 150, 60)};
            Color c = cc[hsh(f.x, f.y) % 6];
            B(.02f, .12f, .98f, .88f, 4, 11, c); B(.2f, .18f, .78f, .82f, 15, 9, C(60, 70, 84));
            B(.06f, .16f, .14f, .26f, 0, 5, C(24, 24, 26)); B(.8f, .16f, .9f, .26f, 0, 5, C(24, 24, 26)); B(.06f, .74f, .14f, .84f, 0, 5, C(24, 24, 26)); B(.8f, .74f, .9f, .84f, 0, 5, C(24, 24, 26));
            break;
        }
        case F_FARM: {
            diamond(x + .04f, y + .04f, x + .96f, y + .96f, zb + 1, S(C(92, 62, 38), L));
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
                if (f.stage == 0) continue;
                Vector2 p = P(x + .2f + i * .3f, y + .2f + j * .3f, zb + 1);
                float r = (f.stage == 1 ? 2.f : f.stage == 2 ? 4.f : 5.f) * zoom;
                DrawCircleV({p.x, p.y - r}, r, S(f.stage == 3 ? C(210, 70, 50) : C(74, 160, 70), L));
                if (f.stage == 3) DrawCircleV({p.x - r * .6f, p.y - r * 2.1f}, r * .8f, S(C(84, 170, 72), L));
            }
            break;
        }
        case F_FIRE: {
            B(.25f, .25f, .75f, .75f, 0, 4, C(90, 90, 94));
            Vector2 p = P(x + .5f, y + .5f, zb + 4); float fl = 1 + .25f * std::sin(time * 12 + x * 3);
            tri({p.x, p.y - 22 * zoom * fl}, {p.x - 8 * zoom, p.y}, {p.x + 8 * zoom, p.y}, C(255, 140, 40, 235));
            tri({p.x, p.y - 14 * zoom * fl}, {p.x - 4.5f * zoom, p.y}, {p.x + 4.5f * zoom, p.y}, C(255, 224, 110, 245));
            break;
        }
        case F_BARREL: B(.2f, .2f, .8f, .8f, 0, 26, C(58, 96, 150)); box(x + .24f, y + .24f, x + .76f, y + .76f, zb + 26, 1, S(C(110, 170, 220), L), S(C(90, 150, 200), L), S(C(70, 120, 170), L), false); break;
        case F_GUNRACK: B(.08f, .1f, .92f, .9f, 0, 32, C(70, 74, 62)); for (int i = 0; i < 4; i++) faceQuad(true, x + .08f, y + .1f, x + .92f, y + .9f, zb, .12f + i * .2f, .18f + i * .2f, 8, 28, S(C(30, 30, 32), L)); break;
        case F_PILE: B(.25f, .25f, .75f, .75f, 0, 7, C(150, 120, 84)); B(.4f, .35f, .65f, .6f, 7, 4, C(120, 96, 66)); break;
        default: break;
    }
}

// ------------------------------------------------------------------ тайл
void Renderer::drawTile(int x, int y, int z) {
    const Game& g = *g_; const Tile& t = g.world.at(x, y, z);
    if (t.kind == TK_AIR) return;
    int rel = (x + y) - (ppx_ + ppy_), relx = (x - y) - (ppx_ - ppy_);
    bool curB = pBld_ >= 0 && t.bld == pBld_;
    if (curB && z > pz_) return;                 // розріз: верхні поверхи поточного будинку приховано
    if (curB && z < pz_) return;
    bool occl = !curB && pBld_ < 0 && rel >= 1 && rel <= 4 && std::abs(relx) <= 2;   // стіна перекриває героя ззовні
    if (occl && z > pz_) return;
    float vf; float L = brightness(x, y, z, vf);
    if (vf == 0) {
        // дах відомої будівлі малюємо навіть над невидимими клітинками, щоб не було «дір»
        if (t.bld >= 0 && !curB && !occl && z == g.world.buildings[t.bld].levels - 1 && z >= pz_ && bKnown_[t.bld]) {
            unsigned h = hsh(x, y, z);
            Color rc = ROOF_PAL[g.world.buildings[t.bld].type]; rc = mulc(rc, 1.f + ((int)(h % 9) - 4) / 60.f);
            float Lr = bKnown_[t.bld] == 2 ? std::min(1.f, lerpf(1.f, .16f, dk_)) : .30f;
            box(x - .04f, y - .04f, x + 1.04f, y + 1.04f, z * LV + WH, 5, S(mulc(rc, 1.1f), Lr), S(mulc(rc, .8f), Lr), S(mulc(rc, .62f), Lr), false);
        }
        return;
    }
    float zb = z * LV;
    bool low = (curB && rel > 0) || occl;
    // ---- підлога / земля
    Color base; unsigned h = hsh(x, y, z); int var = (int)(h % 13) - 6;
    uint8_t k = t.kind;
    switch (k) {
        case TK_GRASS: case TK_TREE: case TK_FENCE: base = C(70 + var, 112 + var, 58 + var / 2); break;
        case TK_ROAD: base = C(60 + var / 2, 62 + var / 2, 68 + var / 2); break;
        case TK_SIDEWALK: base = C(156 + var, 154 + var, 148 + var); break;
        case TK_FLOOR_W: base = C(150 + var, 108 + var, 68 + var); break;
        case TK_FLOOR_T: base = ((x + y) & 1) ? C(200, 200, 194) : C(176, 178, 176); break;
        case TK_FLOOR_C: base = C(110 + var, 116 + var, 128 + var); break;
        default: base = C(130 + var, 100 + var, 70 + var); break;
    }
    if (k == TK_FLOOR_W || k == TK_FLOOR_T || k == TK_FLOOR_C) {
        switch (t.room) {
            case R_KITCHEN: base = ((x + y) & 1) ? C(210, 206, 190) : C(186, 182, 168); break;
            case R_BATH: base = ((x + y) & 1) ? C(170, 206, 214) : C(150, 188, 198); break;
            case R_BEDROOM: base = C(128 + var, 104 + var, 132 + var); break;
            case R_STORAGE: base = C(120 + var, 120 + var, 116 + var); break;
            default: break;
        }
    }
    if (z == 0) diamond(x, y, x + 1, y + 1, zb, S(base, L));
    else box(x, y, x + 1, y + 1, zb - 6, 6, S(base, L), S(mulc(base, .6f), L), S(mulc(base, .45f), L), false);
    if (k == TK_ROAD) {
        int lx = x % 24, ly = y % 24; Color mk = S(C(200, 190, 90), L);
        if ((lx == 1 || lx == 2) && !(ly < 4) && lx == 1 && (y & 1)) quad(P(x + .94f, y + .2f, zb), P(x + 1.06f, y + .2f, zb), P(x + 1.06f, y + .8f, zb), P(x + .94f, y + .8f, zb), mk);
        if ((ly == 1 || ly == 2) && !(lx < 4) && ly == 1 && (x & 1)) quad(P(x + .2f, y + .94f, zb), P(x + .8f, y + .94f, zb), P(x + .8f, y + 1.06f, zb), P(x + .2f, y + 1.06f, zb), mk);
    }
    if (k == TK_GRASS && (h % 7) == 0) { Vector2 p = P(x + .3f + (h % 5) * .1f, y + .4f, zb); DrawCircleV({p.x, p.y - 2 * zoom}, 2.2f * zoom, S(C(52, 96, 46), L)); }
    // ---- особливі тайли
    if (k == TK_STAIR_UP) {
        for (int i = 0; i < 5; i++) box(x + i * .2f, y + .08f, x + (i + 1) * .2f, y + .92f, zb, 5.f + i * 8.f, S(C(190, 160, 120), L), S(C(150, 120, 86), L), S(C(120, 94, 66), L), false);
    } else if (k == TK_STAIR_DOWN) {
        diamond(x + .1f, y + .1f, x + .9f, y + .9f, zb + .5f, S(C(30, 26, 24), L));
        box(x, y, x + 1, y + .06f, zb, 12, S(C(160, 130, 90), L), S(C(130, 100, 70), L), S(C(100, 80, 56), L), false);
    }
    // ---- споруди
    if (k == TK_WALL || k == TK_DOOR || k == TK_WINDOW) {
        Color wc;
        if (t.flags & TF_BUILT) wc = C(166, 118, 66);
        else if (t.bld >= 0 && !(t.flags & TF_INDOOR)) wc = WALL_PAL[g.world.buildings[t.bld].type];
        else wc = C(204, 196, 178);
        wc = lerpc(wc, mulc(wc, .9f), (h % 5) / 10.f);
        float hh = low ? 12.f : WH;
        float oa = occl ? .45f : 1.f;
        auto SS = [&](Color c, float m) { Color r = S(mulc(c, m), L); r.a = (unsigned char)(255 * oa); return r; };
        bool wallAlongX = false;
        if (k != TK_WALL) wallAlongX = inMap(x - 1, y, z) && g.world.at(x - 1, y, z).kind == TK_WALL && inMap(x + 1, y, z) && g.world.at(x + 1, y, z).kind == TK_WALL;
        bool open = (t.flags & (TF_OPEN | TF_BROKEN)) != 0;
        if (k == TK_WALL) {
            box(x, y, x + 1, y + 1, zb, hh, SS(wc, 1.12f), SS(wc, .82f), SS(wc, .64f));
            if (t.flags & TF_BUILT) { for (int i = 1; i < 4 && !low; i++) { float v = i * 11.f; faceQuad(true, x, y, x + 1, y + 1, zb, 0, 1, v, v + 1.2f, SS(C(90, 60, 30), 1.f)); faceQuad(false, x, y, x + 1, y + 1, zb, 0, 1, v, v + 1.2f, SS(C(80, 54, 28), 1.f)); } }
        } else if (k == TK_WINDOW) {
            box(x, y, x + 1, y + 1, zb, hh, SS(wc, 1.12f), SS(wc, .82f), SS(wc, .64f));
            if (!low) {
                Color gl = (t.flags & TF_BROKEN) ? C(18, 20, 24) : C(150, 200, 230, 235);
                if (!(t.flags & TF_BROKEN) && vf == 1 && dk_ > .5f) gl = C(60, 70, 90);
                faceQuad(true, x, y, x + 1, y + 1, zb, .18f, .82f, 16, 36, S(gl, vf == 1 ? 1.f : L));
                faceQuad(false, x, y, x + 1, y + 1, zb, .18f, .82f, 16, 36, S(mulc(gl, .8f), vf == 1 ? 1.f : L));
                if (t.flags & TF_OPEN) { faceQuad(true, x, y, x + 1, y + 1, zb, .18f, .82f, 16, 36, S(C(20, 22, 26), L)); }
            }
        } else {   // двері
            if (!open) {
                Color dc = (t.flags & TF_LOCKED) ? C(112, 74, 44) : C(150, 104, 62);
                box(x, y, x + 1, y + 1, zb, low ? 12.f : 42.f, SS(dc, 1.1f), SS(dc, .84f), SS(dc, .66f));
                if (!low) { faceQuad(true, x, y, x + 1, y + 1, zb, .78f, .86f, 18, 22, S(C(230, 200, 90), L)); faceQuad(false, x, y, x + 1, y + 1, zb, .78f, .86f, 18, 22, S(C(230, 200, 90), L)); }
            } else if (!(t.flags & TF_BROKEN)) {
                Color dc = C(150, 104, 62);
                if (wallAlongX) box(x + .02f, y, x + .14f, y + 1, zb, low ? 12.f : 40.f, SS(dc, 1.1f), SS(dc, .84f), SS(dc, .66f));
                else box(x, y + .02f, x + 1, y + .14f, zb, low ? 12.f : 40.f, SS(dc, 1.1f), SS(dc, .84f), SS(dc, .66f));
            }
            if (!low) { // перемичка над дверима
                Color wcol = wc; box(x, y, x + 1, y + 1, zb + 42, WH - 42, SS(wcol, 1.12f), SS(wcol, .82f), SS(wcol, .64f), false);
            }
        }
        // барикада
        if (t.barr > 0 && k != TK_WALL) {
            for (int i = 0; i < t.barr; i++) {
                float v = 8.f + i * 8.5f;
                faceQuad(true, x, y, x + 1, y + 1, zb, .04f, .96f, v, v + 5, S(C(150, 104, 58), L));
                faceQuad(false, x, y, x + 1, y + 1, zb, .04f, .96f, v, v + 5, S(C(124, 84, 46), L));
            }
        }
    } else if (k == TK_FENCE) {
        box(x + .35f, y, x + .65f, y + 1, zb, 22, S(C(150, 152, 156, 220), L), S(C(120, 124, 130, 220), L), S(C(96, 100, 106, 220), L), false);
    } else if (k == TK_TREE) {
        Vector2 b = P(x + .5f, y + .5f, zb); float a = occl ? .4f : 1.f;
        box(x + .42f, y + .42f, x + .58f, y + .58f, zb, 20, S(C(110, 78, 48), L), S(C(86, 60, 36), L), S(C(66, 46, 28), L), false);
        float r = 20.f * zoom * (.85f + (h % 4) * .1f);
        auto cc = [&](float ox, float oy, float rr, Color col) { Color c2 = S(col, L); c2.a = (unsigned char)(255 * a); DrawCircleV({b.x + ox * zoom, b.y - oy * zoom}, rr, c2); };
        cc(0, 30, r, C(40 + var, 96 + var, 44)); cc(-9, 40, r * .78f, C(52 + var, 112 + var, 54)); cc(8, 44, r * .7f, C(60 + var, 124 + var, 58));
    }
    // ---- меблі
    int fi = g.world.furnIndexAt(x, y, z);
    if (fi >= 0 && g.world.furn[fi].alive && !(occl && false)) drawFurn(g.world.furn[fi], L);
    // ---- дах (у верхньому рівні будинку; поточний будинок розкрито)
    if (t.bld >= 0 && !curB && z == g.world.buildings[t.bld].levels - 1 && z >= pz_) {
        if (!(occl)) {
            Color rc = ROOF_PAL[g.world.buildings[t.bld].type]; rc = mulc(rc, 1.f + ((int)(h % 9) - 4) / 60.f);
            box(x - .04f, y - .04f, x + 1.04f, y + 1.04f, zb + WH, 5, S(mulc(rc, 1.1f), L), S(mulc(rc, .8f), L), S(mulc(rc, .62f), L), false);
        }
    }
}

// ------------------------------------------------------------------ істоти
void Renderer::drawPerson(Vec2 pos, int z, float face, Color body, Color headC, float sc, bool moving, int weapon, bool zombie, float L, const char* label, float hpFrac, float alpha) {
    Vector2 F = P(pos.x, pos.y, z * LV); float s = zoom * sc;
    DrawEllipse((int)F.x, (int)F.y, 10 * s, 5 * s, Fade(Color{0, 0, 0, 255}, .35f * alpha));
    float ph = time * (zombie ? 6.f : 11.f) + pos.x * 3 + pos.y * 2;
    float sw = moving ? std::sin(ph) : 0, bob = moving ? std::fabs(std::sin(ph)) * 1.5f * s : 0;
    Color leg = S(C(48, 50, 62), L); leg.a = (unsigned char)(255 * alpha);
    DrawLineEx({F.x - 3 * s, F.y - 10 * s}, {F.x - 3 * s + sw * 3.5f * s, F.y}, 4 * s, leg);
    DrawLineEx({F.x + 3 * s, F.y - 10 * s}, {F.x + 3 * s - sw * 3.5f * s, F.y}, 4 * s, leg);
    Color bc = S(body, L); bc.a = (unsigned char)(255 * alpha);
    quad({F.x - 6 * s, F.y - 24 * s - bob}, {F.x + 6 * s, F.y - 24 * s - bob}, {F.x + 5 * s, F.y - 9 * s - bob}, {F.x - 5 * s, F.y - 9 * s - bob}, bc);
    DrawLineEx({F.x - 6 * s, F.y - 24 * s - bob}, {F.x - 5 * s, F.y - 9 * s - bob}, 1, Fade(Color{0, 0, 0, 255}, .3f * alpha));
    // напрямок на екрані
    float fx = std::cos(face), fy = std::sin(face), sdx = (fx - fy) * 32.f, sdy = (fx + fy) * 16.f, sl = std::sqrt(sdx * sdx + sdy * sdy);
    if (sl > .01f) { sdx /= sl; sdy /= sl; } else { sdx = 0; sdy = 1; }
    Color skin = S(headC, L); skin.a = (unsigned char)(255 * alpha);
    Vector2 sh{F.x, F.y - 21 * s - bob};
    if (zombie) {
        float px = -sdy, py = sdx;
        DrawLineEx({sh.x + px * 4 * s, sh.y + py * 2 * s}, {sh.x + px * 4 * s + sdx * 12 * s, sh.y + py * 2 * s + sdy * 6 * s}, 3 * s, skin);
        DrawLineEx({sh.x - px * 4 * s, sh.y - py * 2 * s}, {sh.x - px * 4 * s + sdx * 12 * s, sh.y - py * 2 * s + sdy * 6 * s}, 3 * s, skin);
    } else {
        Vector2 hand{sh.x + sdx * 8 * s, sh.y + sdy * 4 * s + 3 * s};
        DrawLineEx(sh, hand, 3 * s, bc);
        if (weapon == 1) DrawLineEx(hand, {hand.x + sdx * 15 * s, hand.y + sdy * 8 * s - 4 * s}, 3.2f * s, S(C(170, 126, 70), L));
        else if (weapon == 2) DrawLineEx(hand, {hand.x + sdx * 8 * s, hand.y + sdy * 4 * s}, 2 * s, S(C(200, 204, 210), L));
        else if (weapon == 3) { DrawLineEx(hand, {hand.x + sdx * 14 * s, hand.y + sdy * 7 * s - 3 * s}, 2.6f * s, S(C(120, 84, 50), L)); DrawCircleV({hand.x + sdx * 14 * s, hand.y + sdy * 7 * s - 3 * s}, 4 * s, S(C(190, 194, 200), L)); }
        else if (weapon == 4) DrawLineEx(hand, {hand.x + sdx * 11 * s, hand.y + sdy * 6 * s}, 3.4f * s, S(C(36, 36, 40), L));
    }
    DrawCircleV({F.x, F.y - 29 * s - bob}, 5.6f * s, skin);
    if (!zombie) DrawCircleV({F.x + sdx * 2 * s, F.y - 31 * s - bob + sdy * .5f * s}, 4.4f * s, S(C(60, 44, 34), L));
    if (hpFrac < .999f) {
        float w = 22 * zoom;
        DrawRectangle((int)(F.x - w / 2), (int)(F.y - 44 * s), (int)w, 3, Color{0, 0, 0, 160});
        DrawRectangle((int)(F.x - w / 2), (int)(F.y - 44 * s), (int)(w * std::max(0.f, hpFrac)), 3, hpFrac > .5f ? C(90, 200, 90) : C(220, 70, 60));
    }
    if (label && *label) { float tw = textW(label, 13); text(label, F.x - tw / 2, F.y - 62 * s, 13, Fade(Color{245, 245, 235, 255}, .92f)); }
}

// ------------------------------------------------------------------ кадр
void Renderer::draw(const Game& g) {
    g_ = &g; const Player& p = g.pl;
    pz_ = p.z; ppx_ = ftile(p.pos.x); ppy_ = ftile(p.pos.y); dk_ = g.darkness(); sDk = dk_;
    pBld_ = (inMap(ppx_, ppy_, pz_) && g.world.indoor(ppx_, ppy_, pz_)) ? g.world.at(ppx_, ppy_, pz_).bld : -1;
    ClearBackground(Color{7, 8, 11, 255});
    computeLights(g);
    bKnown_.assign(g.world.buildings.size() + 1, 0);
    for (int y = 0; y < MAPH; y++) for (int x = 0; x < MAPW; x++) {
        int b = g.world.at(x, y, 0).bld; if (b < 0) continue;
        if (g.vis[y * MAPW + x]) bKnown_[b] = 2; else if (g.world.seen[tileIdx(x, y, 0)] && bKnown_[b] == 0) bKnown_[b] = 1;
    }
    struct E { int z, diag, type, idx; };
    std::vector<E> es;
    auto tileVis = [&](Vec2 q) { int tx = ftile(q.x), ty = ftile(q.y); return inMap(tx, ty, 0) && g.vis[ty * MAPW + tx]; };
    for (size_t i = 0; i < g.zs.size(); i++) { auto& z = g.zs[i]; if (z.alive && z.z == pz_ && tileVis(z.pos)) es.push_back({z.z, ftile(z.pos.x) + ftile(z.pos.y), 0, (int)i}); }
    for (size_t i = 0; i < g.cols.size(); i++) { auto& c = g.cols[i]; if (c.alive && c.z == pz_ && (tileVis(c.pos) || dist(c.pos, p.pos) < 3)) es.push_back({c.z, ftile(c.pos.x) + ftile(c.pos.y), 1, (int)i}); }
    for (size_t i = 0; i < g.survivors.size(); i++) { auto& s = g.survivors[i]; if (s.z == pz_ && tileVis(s.pos)) es.push_back({s.z, ftile(s.pos.x) + ftile(s.pos.y), 2, (int)i}); }
    if (p.alive) es.push_back({p.z, ppx_ + ppy_, 3, 0});
    std::sort(es.begin(), es.end(), [](const E& a, const E& b) { return a.z != b.z ? a.z < b.z : a.diag < b.diag; });
    size_t ei = 0;
    int W = GetScreenWidth(), H = GetScreenHeight();
    auto drawEnt = [&](const E& e) {
        float vf;
        if (e.type == 0) {
            const Zombie& z = g.zs[e.idx]; float L = brightness(ftile(z.pos.x), ftile(z.pos.y), z.z, vf); if (vf < 1) L = std::max(L, .3f);
            unsigned h = (unsigned)z.id * 2654435761u;
            Color body = z.type == Z_RUNNER ? C(140 + (h % 20), 80, 70) : z.type == Z_BRUTE ? C(92, 98, 84) : C(96 + (h % 40), 110 + ((h >> 4) % 30), 86 + ((h >> 8) % 20));
            float maxhp = z.type == Z_BRUTE ? 160.f : z.type == Z_RUNNER ? 45.f : 60.f;
            drawPerson(z.pos, z.z, z.face, body, C(150, 172, 130), z.type == Z_BRUTE ? 1.3f : 1.f, z.st != ZS_IDLE && z.stun <= 0, 0, true, L, nullptr, z.hp / maxhp);
        } else if (e.type == 1) {
            const Colonist& c = g.cols[e.idx]; float L = std::max(.4f, brightness(ftile(c.pos.x), ftile(c.pos.y), c.z, vf));
            int w = c.weapon == I_BAT ? 1 : c.weapon == I_KNIFE ? 2 : c.weapon == I_AXE ? 3 : idef(c.weapon).type == IT_GUN ? 4 : 0;
            drawPerson(c.pos, c.z, c.face, C(86, 150, 92), C(226, 190, 160), 1.f, !c.path.empty() && c.pi < (int)c.path.size(), w, false, L, c.name.c_str(), c.hp / 100.f);
        } else if (e.type == 2) {
            const Survivor& s = g.survivors[e.idx]; float L = std::max(.4f, brightness(ftile(s.pos.x), ftile(s.pos.y), s.z, vf));
            drawPerson(s.pos, s.z, 1.f, C(210, 164, 70), C(226, 190, 160), 1.f, false, 0, false, L, s.name.c_str(), 1.f);
            Vector2 F = P(s.pos.x, s.pos.y, s.z * LV); float bobv = std::sin(time * 4) * 2;
            tri({F.x, F.y - 70 * zoom + bobv}, {F.x - 5 * zoom, F.y - 78 * zoom + bobv}, {F.x + 5 * zoom, F.y - 78 * zoom + bobv}, C(250, 210, 60));
        } else {
            float L = std::max(.5f, brightness(ppx_, ppy_, p.z, vf));
            if (p.hitFlash > 0) L = 1.4f;
            if (g.sleeping) {
                Vector2 F = P(p.pos.x, p.pos.y, p.z * LV);
                DrawEllipse((int)F.x, (int)(F.y - 5 * zoom), 15 * zoom, 6 * zoom, S(C(70, 110, 180), L));
                DrawCircleV({F.x + 12 * zoom, F.y - 6 * zoom}, 5 * zoom, S(C(226, 190, 160), L));
                text("Z z z", F.x - 10, F.y - 36 * zoom - 4 * std::sin(time * 2), 15, Color{200, 220, 255, 230});
            } else {
                int w = p.held.id == I_NONE ? 0 : p.held.id == I_BAT ? 1 : p.held.id == I_KNIFE ? 2 : p.held.id == I_AXE ? 3 : 4;
                drawPerson(p.pos, p.z, p.face, C(70, 110, 190), C(232, 196, 166), 1.f, p.moving, w, false, L, nullptr, 1.f);
            }
        }
    };
    for (int z = 0; z < LEVELS; z++) {
        for (int d = 0; d <= MAPW + MAPH - 2; d++) {
            for (int x = std::max(0, d - (MAPH - 1)); x <= std::min(d, MAPW - 1); x++) {
                int y = d - x;
                Vector2 c = P(x + .5f, y + .5f, z * LV);
                if (c.x < -120 || c.x > W + 120 || c.y < -160 || c.y > H + 120) continue;
                drawTile(x, y, z);
            }
            while (ei < es.size() && (es[ei].z < z || (es[ei].z == z && es[ei].diag <= d))) drawEnt(es[ei++]);
        }
        while (ei < es.size() && es[ei].z <= z) drawEnt(es[ei++]);
    }
    // ---- накладки: плани будівництва, привид, підсвітка, кільце шуму
    for (auto& b : g.bps) {
        if (!b.alive || b.z != pz_) continue;
        float zb = b.z * LV; Color c = C(90, 200, 230, 90);
        diamond(b.x + .05f, b.y + .05f, b.x + .95f, b.y + .95f, zb, c);
        float hh = (b.kind == BK_WALL || b.kind == BK_DOOR) ? 30.f : 12.f;
        box(b.x + .1f, b.y + .1f, b.x + .9f, b.y + .9f, zb, hh, C(120, 220, 250, 70), C(90, 190, 230, 70), C(70, 160, 200, 70), false);
        if (b.progress > 0) { Vector2 q = P(b.x + .5f, b.y + .5f, zb + hh + 8); float fr = b.progress / buildDef(b.kind).time; DrawRectangle((int)q.x - 12, (int)q.y, 24, 3, Color{0, 0, 0, 160}); DrawRectangle((int)q.x - 12, (int)q.y, (int)(24 * clampv(fr, 0.f, 1.f)), 3, C(120, 230, 255)); }
    }
    if (ghost) {
        float zb = pz_ * LV; Color c = ghostOk ? C(90, 230, 120, 110) : C(240, 80, 70, 110);
        diamond(ghostX, ghostY, ghostX + 1, ghostY + 1, zb, c);
        float hh = (ghostKind == BK_WALL || ghostKind == BK_DOOR) ? 34.f : 14.f;
        Color cc = ghostOk ? C(120, 240, 140, 90) : C(250, 110, 100, 90);
        box(ghostX + .08f, ghostY + .08f, ghostX + .92f, ghostY + .92f, zb, hh, cc, mulc(cc, .8f), mulc(cc, .65f), false);
    }
    if (g.hover.kind != IK_NONE && g.hover.z == pz_) {
        float pulse = .55f + .35f * std::sin(time * 6);
        Vector2 a = P(g.hover.x, g.hover.y, pz_ * LV + .5f), b = P(g.hover.x + 1, g.hover.y, pz_ * LV + .5f), c = P(g.hover.x + 1, g.hover.y + 1, pz_ * LV + .5f), d = P(g.hover.x, g.hover.y + 1, pz_ * LV + .5f);
        Color hc = Fade(Color{255, 244, 170, 255}, pulse);
        DrawLineEx(a, b, 2.f, hc); DrawLineEx(b, c, 2.f, hc); DrawLineEx(c, d, 2.f, hc); DrawLineEx(d, a, 2.f, hc);
    }
    if (g.noiseShow > 0 && p.alive) {
        float a = clampv(g.noiseShow / .6f, 0.f, 1.f), r = g.noiseR * (1.f - .35f * a);
        for (int i = 0; i < 40; i++) {
            float a0 = i * 2 * PI_F / 40, a1 = (i + 1) * 2 * PI_F / 40;
            DrawLineEx(P(p.pos.x + std::cos(a0) * r, p.pos.y + std::sin(a0) * r, p.z * LV), P(p.pos.x + std::cos(a1) * r, p.pos.y + std::sin(a1) * r, p.z * LV), 1.5f, Fade(Color{255, 200, 90, 255}, .35f * a));
        }
    }
    // кров на краях екрана при пораненні
    if (p.hitFlash > 0) { Color rc = Fade(Color{190, 20, 20, 255}, p.hitFlash * .9f); DrawRectangle(0, 0, W, 14, rc); DrawRectangle(0, H - 14, W, 14, rc); DrawRectangle(0, 0, 14, H, rc); DrawRectangle(W - 14, 0, 14, H, rc); }
    // віньєтка ночі
    if (dk_ > .2f) { Color vc = Fade(Color{4, 6, 16, 255}, dk_ * .22f); DrawRectangle(0, 0, W, H, vc); }
}
