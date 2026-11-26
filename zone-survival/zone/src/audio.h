// Процедурний звук: усі ефекти синтезуються в пам'яті, окремі аудіофайли не потрібні.
#pragma once
#include "game.h"
#include "raylib.h"

struct Audio {
    static constexpr int POOL = 3;
    Sound snd[SFX_COUNT][POOL]{};
    int next[SFX_COUNT]{};
    bool ok = false;
    void init();
    void shutdown();
    void play(const SndEvt& e, Vec2 listener, int lz);
};
