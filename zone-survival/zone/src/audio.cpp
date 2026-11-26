#include "audio.h"

namespace {
const int SR = 22050;
struct Gen {
    uint32_t s = 2463534242u;
    float noise() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFF) / 32768.f - 1.f; }
};
template <class F> Wave make(float dur, F fn) {
    int n = (int)(dur * SR);
    short* d = (short*)malloc(sizeof(short) * n);
    Gen g;
    for (int i = 0; i < n; i++) { float t = (float)i / SR; float v = fn(t, g); v = clampv(v, -1.f, 1.f); d[i] = (short)(v * 30000); }
    Wave w{}; w.frameCount = (unsigned)n; w.sampleRate = SR; w.sampleSize = 16; w.channels = 1; w.data = d;
    return w;
}
const float TAU = 6.2831853f;
float lp(float& st, float x, float a) { st += a * (x - st); return st; }
}

void Audio::init() {
    Wave w[SFX_COUNT];
    float a = 0, b = 0, c = 0;
    w[SFX_STEP] = make(.09f, [&](float t, Gen& g) { return lp(a, g.noise(), .15f) * std::exp(-t * 45) * 1.6f; });
    w[SFX_SWING] = make(.22f, [&](float t, Gen& g) { float n = g.noise(); float hp = n - lp(b, n, .3f); return hp * std::sin(PI_F * t / .22f) * .55f; });
    w[SFX_HIT] = make(.2f, [&](float t, Gen& g) { return std::sin(TAU * (95 - 70 * t) * t) * std::exp(-t * 18) * .9f + lp(a, g.noise(), .4f) * std::exp(-t * 40) * .6f; });
    w[SFX_PISTOL] = make(.35f, [&](float t, Gen& g) { return g.noise() * std::exp(-t * 24) * .8f + std::sin(TAU * 70 * t) * std::exp(-t * 14) * .6f; });
    w[SFX_SHOTGUN] = make(.6f, [&](float t, Gen& g) { return lp(a, g.noise(), .6f) * std::exp(-t * 9) * 1.3f + std::sin(TAU * 48 * t) * std::exp(-t * 8) * .8f; });
    w[SFX_RIFLE] = make(.5f, [&](float t, Gen& g) { return g.noise() * std::exp(-t * 16) * .8f + std::sin(TAU * 60 * t) * std::exp(-t * 10) * .7f + lp(b, g.noise(), .1f) * std::exp(-t * 5) * .5f; });
    w[SFX_GROAN] = make(.95f, [&](float t, Gen& g) {
        float f = 92 + 22 * std::sin(TAU * 3.1f * t) - 15 * t; static float ph = 0; ph += f / SR; float saw = 2 * (ph - std::floor(ph)) - 1;
        float env = std::pow(std::sin(PI_F * t / .95f), .7f); return (lp(a, saw, .09f) * 1.8f + lp(b, g.noise(), .05f) * .6f) * env * .8f; });
    w[SFX_ZDIE] = make(1.1f, [&](float t, Gen& g) {
        float f = 120 - 60 * t; static float ph2 = 0; ph2 += f / SR; float saw = 2 * (ph2 - std::floor(ph2)) - 1;
        return (lp(a, saw, .07f) * 1.6f + lp(b, g.noise(), .04f) * .5f) * std::exp(-t * 2.4f) * .9f; });
    w[SFX_DOOR] = make(.45f, [&](float t, Gen& g) { return (std::sin(TAU * (160 + 50 * std::sin(TAU * 11 * t)) * t) * .25f + lp(a, g.noise(), .08f) * .5f) * std::sin(PI_F * t / .45f) + std::sin(TAU * 60 * t) * std::exp(-t * 20) * .5f; });
    w[SFX_GLASS] = make(.6f, [&](float t, Gen& g) { float n = g.noise(); float hp = n - lp(a, n, .25f); float e = std::exp(-t * 9) + (t > .12f ? std::exp(-(t - .12f) * 14) * .6f : 0.f) + (t > .27f ? std::exp(-(t - .27f) * 18) * .4f : 0.f); return hp * e * .7f; });
    w[SFX_WOOD] = make(.28f, [&](float t, Gen& g) { return std::sin(TAU * 140 * t) * std::exp(-t * 26) * .8f + lp(a, g.noise(), .3f) * std::exp(-t * 45) * .7f; });
    w[SFX_PICK] = make(.14f, [&](float t, Gen& g) { float n = g.noise(); return (n - lp(a, n, .2f)) * std::exp(-t * 40) * .5f * (std::sin(TAU * 30 * t) > 0 ? 1.f : .5f); });
    w[SFX_CLICK] = make(.06f, [&](float t, Gen& g) { return g.noise() * std::exp(-t * 160) * .8f; });
    w[SFX_HURT] = make(.4f, [&](float t, Gen& g) { return std::sin(TAU * (320 - 380 * t) * t) * std::exp(-t * 6) * .55f + lp(a, g.noise(), .3f) * std::exp(-t * 12) * .5f; });
    w[SFX_HELI] = make(3.2f, [&](float t, Gen& g) { float th = .5f + .5f * std::sin(TAU * 13 * t); th = th * th; return (lp(a, g.noise(), .06f) * 2.5f * th + std::sin(TAU * 40 * t) * .25f) * std::sin(PI_F * t / 3.2f); });
    w[SFX_LEVELUP] = make(.55f, [&](float t, Gen&) { float f = t < .15f ? 523.f : t < .3f ? 659.f : 784.f; return std::sin(TAU * f * t) * std::exp(-(std::fmod(t, .15f)) * 7) * .4f * (1 - t / .6f); });
    w[SFX_EAT] = make(.3f, [&](float t, Gen& g) { float n = g.noise(); float hp = n - lp(a, n, .3f); float gate = std::sin(TAU * 14 * t) > .2f ? 1.f : 0.f; return hp * gate * std::exp(-t * 7) * .6f; });
    (void)c;
    for (int i = 0; i < SFX_COUNT; i++) {
        snd[i][0] = LoadSoundFromWave(w[i]); UnloadWave(w[i]);
        for (int k = 1; k < POOL; k++) snd[i][k] = LoadSoundAlias(snd[i][0]);
    }
    ok = true;
}
void Audio::shutdown() {
    if (!ok) return;
    for (int i = 0; i < SFX_COUNT; i++) { for (int k = 1; k < POOL; k++) UnloadSoundAlias(snd[i][k]); UnloadSound(snd[i][0]); }
    ok = false;
}
void Audio::play(const SndEvt& e, Vec2 l, int lz) {
    if (!ok) return;
    float maxD = e.id == SFX_HELI ? 80.f : (e.id == SFX_PISTOL || e.id == SFX_SHOTGUN || e.id == SFX_RIFLE) ? 55.f : e.id == SFX_GROAN ? 24.f : 26.f;
    float d = dist(e.pos, l) + std::abs(e.z - lz) * 6.f;
    if (d >= maxD) return;
    float v = e.vol * (1.f - d / maxD); v *= v * 1.4f;
    if (v < .02f) return;
    Sound& s = snd[e.id][next[e.id]]; next[e.id] = (next[e.id] + 1) % POOL;
    SetSoundVolume(s, clampv(v, 0.f, 1.f));
    SetSoundPitch(s, .94f + (float)(rand() % 100) / 800.f);
    PlaySound(s);
}
