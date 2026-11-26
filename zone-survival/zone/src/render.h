// Ізометричний рендер (raylib): тайли, стіни, меблі, істоти, освітлення, туман війни.
#pragma once
#include "game.h"
#include "raylib.h"

constexpr float TW = 64.f, TH = 32.f, LV = 52.f, WH = 46.f;   // розмір ромба, висота поверху і стін (px)

struct Renderer {
    Font font{};
    float zoom = 1.f, zoomTarget = 1.f;
    float camX = 0, camY = 0;   // центр камери в ізо-пікселях
    float time = 0;
    // режим розміщення споруди
    bool ghost = false; BuildKind ghostKind = BK_WALL; int ghostX = 0, ghostY = 0; bool ghostOk = true;

    void snapCamera(const Game& g);
    void update(const Game& g, float dt);
    void draw(const Game& g);
    Vector2 P(float x, float y, float zpx) const;
    Vec2 screenToWorld(const Game& g, Vector2 s) const;   // клітинки на рівні героя
    void text(const char* t, float x, float y, float size, Color c) const;
    float textW(const char* t, float size) const;

private:
    std::vector<float> lightAdd_;
    std::vector<uint8_t> bKnown_;   // 0 невідомий, 1 бачили, 2 бачимо зараз
    int pz_ = 0, ppx_ = 0, ppy_ = 0, pBld_ = -1;
    const Game* g_ = nullptr;
    float dk_ = 0;
    void computeLights(const Game& g);
    float brightness(int x, int y, int z, float& vf) const;
    void drawTile(int x, int y, int z);
    void drawFurn(const Furn& f, float L);
    void drawPerson(Vec2 pos, int z, float face, Color body, Color head, float scale, bool moving, int weapon, bool zombie, float L, const char* label, float hpFrac, float alpha = 1.f);
    void tri(Vector2 a, Vector2 b, Vector2 c, Color col) const;
    void quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col) const;
    void box(float x0, float y0, float x1, float y1, float zb, float h, Color top, Color left, Color right, bool edges = true) const;
    void faceQuad(bool leftFace, float x0, float y0, float x1, float y1, float zb, float u0, float u1, float v0, float v1, Color c) const;
    void diamond(float x0, float y0, float x1, float y1, float zpx, Color c) const;
};
