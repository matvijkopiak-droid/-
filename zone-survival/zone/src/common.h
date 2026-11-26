// Спільні типи та константи.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <random>
#include <string>
#include <vector>

constexpr int MAPW = 96, MAPH = 96, LEVELS = 3;
constexpr float PI_F = 3.14159265358979f;

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float X, float Y) : x(X), y(Y) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float k) const { return {x * k, y * k}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    float len() const { return std::sqrt(x * x + y * y); }
    Vec2 norm() const { float l = len(); return l > 1e-6f ? Vec2{x / l, y / l} : Vec2{0, 0}; }
};
inline float dist(Vec2 a, Vec2 b) { return (a - b).len(); }
inline float dist2(Vec2 a, Vec2 b) { Vec2 d = a - b; return d.x * d.x + d.y * d.y; }
template <class T> inline T clampv(T v, T a, T b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

struct Rng {
    std::mt19937 g;
    explicit Rng(uint32_t s = 1) : g(s) {}
    int range(int a, int b) { return std::uniform_int_distribution<int>(a, b)(g); }
    float f() { return std::uniform_real_distribution<float>(0.f, 1.f)(g); }
    float f(float a, float b) { return a + (b - a) * f(); }
    bool chance(float p) { return f() < p; }
    template <class T> const T& pick(const std::vector<T>& v) { return v[range(0, (int)v.size() - 1)]; }
};

inline int tileIdx(int x, int y, int z) { return (z * MAPH + y) * MAPW + x; }
inline bool inMap(int x, int y, int z) { return x >= 0 && y >= 0 && z >= 0 && x < MAPW && y < MAPH && z < LEVELS; }
inline int ftile(float v) { return (int)std::floor(v); }
