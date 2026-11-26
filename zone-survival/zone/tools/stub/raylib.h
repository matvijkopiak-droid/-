// Програмна заглушка raylib для перевірки рендера без вікна (НЕ для користувача). Малює у буфер і записує текст.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

typedef struct Color { unsigned char r, g, b, a; } Color;
typedef struct Vector2 { float x, y; } Vector2;
typedef struct Rectangle { float x, y, width, height; } Rectangle;
typedef struct Texture2D { unsigned id; int width, height, mipmaps, format; } Texture2D;
typedef struct Font { int baseSize; int glyphCount; Texture2D texture; } Font;
typedef struct Wave { unsigned frameCount, sampleRate, sampleSize, channels; void* data; } Wave;
typedef struct Sound { int id; unsigned frameCount; } Sound;
enum { FLAG_MSAA_4X_HINT = 0x20, FLAG_WINDOW_RESIZABLE = 0x4, FLAG_VSYNC_HINT = 0x40 };
enum { TEXTURE_FILTER_BILINEAR = 1 };
enum { MOUSE_BUTTON_LEFT = 0, MOUSE_BUTTON_RIGHT = 1, MOUSE_BUTTON_MIDDLE = 2 };
enum {
    KEY_NULL = 0, KEY_SPACE = 32, KEY_ENTER = 257, KEY_TAB = 258, KEY_BACKSPACE = 259, KEY_ESCAPE = 256, KEY_F1 = 290, KEY_F5 = 294, KEY_F9 = 298, KEY_F11 = 300,
    KEY_RIGHT = 262, KEY_LEFT = 263, KEY_DOWN = 264, KEY_UP = 265, KEY_LEFT_SHIFT = 340, KEY_LEFT_CONTROL = 341, KEY_RIGHT_SHIFT = 344,
    KEY_A = 65, KEY_B = 66, KEY_C = 67, KEY_D = 68, KEY_E = 69, KEY_F = 70, KEY_G = 71, KEY_I = 73, KEY_K = 75, KEY_Q = 81, KEY_R = 82, KEY_S = 83, KEY_V = 86, KEY_W = 87, KEY_X = 88, KEY_Z = 90,
    KEY_ONE = 49, KEY_TWO = 50, KEY_THREE = 51, KEY_FOUR = 52, KEY_EQUAL = 61, KEY_MINUS = 45, KEY_PERIOD = 46, KEY_COMMA = 44
};

struct StubText { float x, y, size; Color c; std::string s; };
struct StubState {
    int W = 1280, H = 720;
    std::vector<uint8_t> fb;
    std::vector<StubText> texts;
    float mx = 0, my = 0; bool mdown[3] = {false, false, false}, mpressed[3] = {false, false, false};
    bool keys[512] = {}, pressed[512] = {};
    float dt = 1.f / 60;
};
inline StubState& stub() { static StubState s; return s; }
inline void stub_resize(int w, int h) { auto& s = stub(); s.W = w; s.H = h; s.fb.assign((size_t)w * h * 3, 0); s.texts.clear(); }
inline void stub_dump(const char* ppm, const char* txt) {
    auto& s = stub();
    FILE* f = fopen(ppm, "wb"); fprintf(f, "P6\n%d %d\n255\n", s.W, s.H); fwrite(s.fb.data(), 1, s.fb.size(), f); fclose(f);
    f = fopen(txt, "w");
    for (auto& t : s.texts) { std::string e; for (char ch : t.s) { if (ch == '\t' || ch == '\n') e += ' '; else e += ch; } fprintf(f, "%.1f\t%.1f\t%.1f\t%d\t%d\t%d\t%d\t%s\n", t.x, t.y, t.size, t.c.r, t.c.g, t.c.b, t.c.a, e.c_str()); }
    fclose(f);
}
inline Color Fade(Color c, float a) { c.a = (unsigned char)std::max(0.f, std::min(255.f, a * 255.f)); return c; }
inline Color ColorAlpha(Color c, float a) { return Fade(c, a); }
inline void stub_px(int x, int y, Color c) {
    auto& s = stub(); if (x < 0 || y < 0 || x >= s.W || y >= s.H || c.a == 0) return;
    uint8_t* p = &s.fb[((size_t)y * s.W + x) * 3];
    if (c.a == 255) { p[0] = c.r; p[1] = c.g; p[2] = c.b; return; }
    int a = c.a; p[0] = (uint8_t)((c.r * a + p[0] * (255 - a)) / 255); p[1] = (uint8_t)((c.g * a + p[1] * (255 - a)) / 255); p[2] = (uint8_t)((c.b * a + p[2] * (255 - a)) / 255);
}
inline void InitWindow(int, int, const char*) {}
inline void SetConfigFlags(unsigned) {}
inline void SetTargetFPS(int) {}
inline void SetExitKey(int) {}
inline bool WindowShouldClose() { return false; }
inline void CloseWindow() {}
inline int GetScreenWidth() { return stub().W; }
inline int GetScreenHeight() { return stub().H; }
inline float GetFrameTime() { return stub().dt; }
inline double GetTime() { return 0; }
inline void ToggleFullscreen() {}
inline bool IsWindowResized() { return false; }
inline void BeginDrawing() {}
inline void EndDrawing() {}
inline void ClearBackground(Color c) { auto& s = stub(); for (size_t i = 0; i < s.fb.size(); i += 3) { s.fb[i] = c.r; s.fb[i + 1] = c.g; s.fb[i + 2] = c.b; } s.texts.clear(); }
inline void DrawRectangle(int x, int y, int w, int h, Color c) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) stub_px(i, j, c); }
inline void DrawRectangleRec(Rectangle r, Color c) { DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, c); }
inline void DrawRectangleLines(int x, int y, int w, int h, Color c) { DrawRectangle(x, y, w, 1, c); DrawRectangle(x, y + h - 1, w, 1, c); DrawRectangle(x, y, 1, h, c); DrawRectangle(x + w - 1, y, 1, h, c); }
inline void DrawRectangleLinesEx(Rectangle r, float t, Color c) { int T = (int)t; DrawRectangle((int)r.x, (int)r.y, (int)r.width, T, c); DrawRectangle((int)r.x, (int)(r.y + r.height - T), (int)r.width, T, c); DrawRectangle((int)r.x, (int)r.y, T, (int)r.height, c); DrawRectangle((int)(r.x + r.width - T), (int)r.y, T, (int)r.height, c); }
inline void DrawTriangle(Vector2 a, Vector2 b, Vector2 c, Color col) {
    float cr = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cr > 0) return;           // як у справжньому raylib: не-CCW трикутники відкидаються
    if (cr == 0) return;
    // raylib 5.0: видимий CCW (в екранних координатах із віссю Y вниз) — тут перевіряємо, що виклик правильний
    int minx = (int)std::floor(std::min({a.x, b.x, c.x})), maxx = (int)std::ceil(std::max({a.x, b.x, c.x}));
    int miny = (int)std::floor(std::min({a.y, b.y, c.y})), maxy = (int)std::ceil(std::max({a.y, b.y, c.y}));
    auto edge = [](Vector2 p, Vector2 q, float x, float y) { return (q.x - p.x) * (y - p.y) - (q.y - p.y) * (x - p.x); };
    for (int y = miny; y <= maxy; y++) for (int x = minx; x <= maxx; x++) {
        float px = x + .5f, py = y + .5f;
        float e0 = edge(a, b, px, py), e1 = edge(b, c, px, py), e2 = edge(c, a, px, py);
        if ((e0 <= 0 && e1 <= 0 && e2 <= 0) || (e0 >= 0 && e1 >= 0 && e2 >= 0)) stub_px(x, y, col);
    }
}
inline void DrawLineEx(Vector2 a, Vector2 b, float th, Color c) {
    float dx = b.x - a.x, dy = b.y - a.y, l = std::sqrt(dx * dx + dy * dy); if (l < .01f) return;
    float nx = -dy / l * th * .5f, ny = dx / l * th * .5f;
    DrawTriangle({a.x + nx, a.y + ny}, {a.x - nx, a.y - ny}, {b.x - nx, b.y - ny}, c);
    DrawTriangle({a.x + nx, a.y + ny}, {b.x - nx, b.y - ny}, {b.x + nx, b.y + ny}, c);
}
inline void DrawLine(int x0, int y0, int x1, int y1, Color c) { DrawLineEx({(float)x0, (float)y0}, {(float)x1, (float)y1}, 1.f, c); }
inline void DrawEllipse(int cx, int cy, float rx, float ry, Color c) {
    for (int y = (int)(cy - ry); y <= (int)(cy + ry); y++) for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
        float u = (x + .5f - cx) / rx, v = (y + .5f - cy) / ry; if (u * u + v * v <= 1) stub_px(x, y, c);
    }
}
inline void DrawCircle(int cx, int cy, float r, Color c) { DrawEllipse(cx, cy, r, r, c); }
inline void DrawCircleV(Vector2 p, float r, Color c) { DrawEllipse((int)p.x, (int)p.y, r, r, c); }
inline void DrawCircleLines(int cx, int cy, float r, Color c) {
    for (int i = 0; i < 48; i++) { float a0 = i * 6.2832f / 48, a1 = (i + 1) * 6.2832f / 48; DrawLineEx({cx + std::cos(a0) * r, cy + std::sin(a0) * r}, {cx + std::cos(a1) * r, cy + std::sin(a1) * r}, 1.f, c); }
}
inline void BeginScissorMode(int, int, int, int) {}
inline void EndScissorMode() {}
inline Font LoadFontEx(const char*, int size, int*, int count) { Font f{}; f.baseSize = size; f.glyphCount = count; return f; }
inline void SetTextureFilter(Texture2D, int) {}
inline void UnloadFont(Font) {}
inline int stub_cps(const char* t) { int n = 0; for (const unsigned char* p = (const unsigned char*)t; *p; p++) if ((*p & 0xC0) != 0x80) n++; return n; }
inline Vector2 MeasureTextEx(Font, const char* t, float size, float sp) { int n = stub_cps(t); return {n * (size * .56f + sp), size}; }
inline void DrawTextEx(Font, const char* t, Vector2 p, float size, float, Color c) { stub().texts.push_back({p.x, p.y, size, c, t}); }
inline bool IsKeyDown(int k) { return stub().keys[k & 511]; }
inline bool IsKeyPressed(int k) { return stub().pressed[k & 511]; }
inline bool IsMouseButtonDown(int b) { return stub().mdown[b]; }
inline bool IsMouseButtonPressed(int b) { return stub().mpressed[b]; }
inline bool IsMouseButtonReleased(int) { return false; }
inline Vector2 GetMousePosition() { return {stub().mx, stub().my}; }
inline float GetMouseWheelMove() { return 0; }
inline int GetCharPressed() { return 0; }
inline void InitAudioDevice() {}
inline void CloseAudioDevice() {}
inline bool IsAudioDeviceReady() { return true; }
inline void SetMasterVolume(float) {}
inline Sound LoadSoundFromWave(Wave) { static int n = 0; return Sound{++n, 0}; }
inline void UnloadWave(Wave w) { free(w.data); }
inline void UnloadSound(Sound) {}
inline void PlaySound(Sound) {}
inline void SetSoundVolume(Sound, float) {}
inline void SetSoundPan(Sound, float) {}
inline void SetSoundPitch(Sound, float) {}
inline bool IsSoundPlaying(Sound) { return false; }
inline void StopSound(Sound) {}
inline Sound LoadSoundAlias(Sound s) { return s; }
inline void UnloadSoundAlias(Sound) {}
