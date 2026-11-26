#include "game.h"

// ------------------------------------------------------------------ шлях і рух
bool Game::pathTo(std::vector<PNode>& path, int& pi, Vec2 from, int fz, Vec2 to, int tz, int mode, int maxNodes) {
    if (pathBudget_ <= 0) return false;
    pathBudget_--;
    PNode a{(int16_t)ftile(from.x), (int16_t)ftile(from.y), (int8_t)fz}, b{(int16_t)ftile(to.x), (int16_t)ftile(to.y), (int8_t)tz};
    std::vector<PNode> out;
    if (!world.findPath(a, b, out, maxNodes, mode)) { path.clear(); pi = 0; return false; }
    path = out; pi = 0;
    return true;
}
// Повертає true, якщо шлях пройдено. blockedByStruct — наступна клітинка перегороджена спорудою (для зомбі).
bool Game::followPath(std::vector<PNode>& path, int& pi, Vec2& pos, int& z, float speed, float dt, int& lx, int& ly, float radius, float& face, bool zombie, bool* blocked, PNode* blockTile) {
    if (blocked) *blocked = false;
    if (pi >= (int)path.size()) return true;
    PNode n = path[pi];
    // перехід по сходах: вузол на іншому поверсі в тій самій клітинці
    if (n.z != z) {
        if (ftile(pos.x) == n.x && ftile(pos.y) == n.y) { z = n.z; lx = n.x; ly = n.y; pi++; return pi >= (int)path.size(); }
    }
    Vec2 target(n.x + .5f, n.y + .5f);
    // двері / перешкоди
    Tile& t = world.at(n.x, n.y, z);
    if (world.blocks(n.x, n.y, z)) {
        if (t.kind == TK_DOOR && tryOpenDoor(n.x, n.y, z, zombie)) {}
        else {
            if (zombie && (t.kind == TK_DOOR || t.kind == TK_WINDOW || t.kind == TK_WALL)) { if (blocked) *blocked = true; if (blockTile) *blockTile = n; return false; }
            path.clear(); pi = 0; return false;
        }
    }
    Vec2 d = target - pos; float l = d.len();
    float step = speed * dt;
    if (l <= std::max(step, .08f)) { pos = target; pi++; if (pi >= (int)path.size()) return true; return false; }
    Vec2 mv = d * (step / l);
    face = std::atan2(d.y, d.x);
    if (!moveEntity(pos, z, mv, radius)) { /* застряг — вирішується таймером зовні */ }
    return false;
}

// ------------------------------------------------------------------ шум
void Game::makeNoise(Vec2 p, int z, float radius) {
    if (radius <= .5f) return;
    for (auto& zb : zs) {
        if (!zb.alive || !zb.active) continue;
        float d = dist(zb.pos, p) + std::abs(zb.z - z) * 5.f;
        if (d > radius * 1.7f) continue;
        if (d > radius * .6f && zb.z == z && !world.los(zb.pos.x, zb.pos.y, p.x, p.y, z)) d *= 1.6f;
        if (d > radius) continue;
        if (zb.st == ZS_CHASE) continue;
        zb.st = ZS_ALERT; zb.goal = p + Vec2(rng.f(-1.5f, 1.5f), rng.f(-1.5f, 1.5f)); zb.goalZ = z;
        zb.repathT = 0; zb.searchT = 0; zb.path.clear(); zb.pi = 0;
    }
}

// ------------------------------------------------------------------ шкода
void Game::hurtPlayer(float dmg, bool allowInfect) {
    if (!pl.alive) return;
    pl.hp -= dmg; pl.hitFlash = .3f; cancelAction(); wakeUp("Вас вдарили!");
    snd(SFX_HURT, pl.pos, pl.z, 1.f);
    if (rng.chance(.30f) && !pl.bleeding) { pl.bleeding = true; pl.bleedT = 0; say("Ви поранені — кровотеча!"); }
    if (allowInfect && !pl.infected && rng.chance(.06f)) { pl.infected = true; pl.infHours = 48; say("Вас вкусили... Ви відчуваєте жар."); }
    if (pl.hp <= 0) killPlayer("Зомбі вас розірвали.");
}
void Game::hurtColonist(Colonist& c, float dmg) {
    c.hp -= dmg; snd(SFX_HURT, c.pos, c.z, .7f); c.sleeping = false;
    if (c.hp <= 0 && c.alive) { c.alive = false; say(c.name + " загинув."); }
}
void Game::zombieDamage(Zombie& z, float dmg, Vec2 from, float knock) {
    z.hp -= dmg; z.stun = std::max(z.stun, .35f);
    Vec2 d = (z.pos - from).norm();
    if (knock > 0) moveEntity(z.pos, z.z, d * knock, .28f);
    if (z.st != ZS_CHASE) { z.st = ZS_CHASE; z.target = 0; }
    snd(SFX_HIT, z.pos, z.z, .8f);
    if (z.hp <= 0) { z.alive = false; kills++; snd(SFX_ZDIE, z.pos, z.z, .8f); }
}
void Game::bashTile(int x, int y, int z, float dmg, bool byZombie) {
    if (!inMap(x, y, z)) return;
    Tile& t = world.at(x, y, z);
    Vec2 c(x + .5f, y + .5f);
    if ((t.kind == TK_DOOR || t.kind == TK_WINDOW) && t.barr > 0) {
        if (t.bhp <= dmg) { t.barr--; t.bhp = (uint8_t)(t.barr ? 60 : 0); snd(SFX_WOOD, c, z); } else t.bhp -= (uint8_t)dmg;
        snd(SFX_WOOD, c, z, .8f); return;
    }
    if (t.kind == TK_DOOR || t.kind == TK_WINDOW) {
        if (!byZombie && (t.flags & TF_BUILT)) return;
        if (t.hp == 0) t.hp = t.kind == TK_DOOR ? 80 : 30;
        if (t.hp <= dmg) {
            t.hp = 1; t.flags |= TF_BROKEN | TF_OPEN; t.flags &= ~TF_LOCKED;
            snd(t.kind == TK_WINDOW ? SFX_GLASS : SFX_WOOD, c, z, 1.f); makeNoise(c, z, 12);
        } else { t.hp -= (uint8_t)dmg; snd(t.kind == TK_WINDOW ? SFX_GLASS : SFX_WOOD, c, z, .8f); makeNoise(c, z, byZombie ? 3.f : 9.f); }
        return;
    }
    if (t.kind == TK_WALL && (t.flags & TF_BUILT) && byZombie) {
        if (t.hp <= dmg) { t.kind = (t.flags & TF_INDOOR) ? TK_FLOOR_W : TK_GRASS; t.flags &= (uint8_t)TF_INDOOR; t.hp = 0; snd(SFX_WOOD, c, z, 1.f); }
        else { t.hp -= (uint8_t)dmg; snd(SFX_WOOD, c, z, .8f); }
    }
}

void Game::shootRay(Vec2 from, Vec2 dir, int z, float range, float dmg, int pierce, float spread) {
    float a = std::atan2(dir.y, dir.x) + rng.f(-spread, spread);
    Vec2 d(std::cos(a), std::sin(a));
    float maxT = range;
    for (float t = .3f; t <= range; t += .25f) {
        Vec2 p = from + d * t;
        if (world.opaque(ftile(p.x), ftile(p.y), z)) { maxT = t; break; }
        if (world.blocks(ftile(p.x), ftile(p.y), z) && world.at(ftile(p.x), ftile(p.y), z).kind != TK_WINDOW) { maxT = t; break; }
    }
    std::vector<std::pair<float, Zombie*>> hits;
    for (auto& zb : zs) {
        if (!zb.alive || zb.z != z) continue;
        Vec2 r = zb.pos - from; float t = r.x * d.x + r.y * d.y;
        if (t < 0 || t > maxT) continue;
        float perp = std::abs(r.x * d.y - r.y * d.x);
        if (perp < .42f) hits.push_back({t, &zb});
    }
    std::sort(hits.begin(), hits.end(), [](auto& a, auto& b) { return a.first < b.first; });
    int n = 0;
    for (auto& h : hits) {
        if (n > pierce) break;
        float dm = dmg * (1 + .04f * pl.sk[SK_FIREARM]) * rng.f(.85f, 1.15f);
        if (rng.chance(.08f + .01f * pl.sk[SK_FIREARM])) dm *= 2;
        zombieDamage(*h.second, dm, from, .25f); n++; gainSkill(SK_FIREARM, 4);
    }
}

void Game::attack(Vec2 aim) {
    if (!pl.alive || sleeping || act.kind != A_NONE || pl.atkCd > 0) return;
    Vec2 dir = aimDir(aim); pl.face = std::atan2(dir.y, dir.x);
    ItemId wid = pl.held.id;
    bool fist = wid == I_NONE;
    const ItemDef& d = idef(wid);
    if (!fist && d.type == IT_GUN) {
        if (pl.held.ammo <= 0) {
            snd(SFX_CLICK, pl.pos, pl.z, .8f); pl.atkCd = .4f;
            if (pl.inv.count(d.ammoItem) > 0) reload(); else say("Магазин порожній.");
            return;
        }
        pl.held.ammo--; pl.atkCd = d.cd;
        float spread = std::max(.015f, .07f - .006f * pl.sk[SK_FIREARM]);
        if (wid == I_SHOTGUN) { for (int i = 0; i < 6; i++) shootRay(pl.pos, dir, pl.z, d.range, d.dmgMin / 3.f + rng.f(0, (d.dmgMax - d.dmgMin) / 3.f), 0, .22f); }
        else shootRay(pl.pos, dir, pl.z, d.range, rng.f(d.dmgMin, d.dmgMax), wid == I_RIFLE ? 2 : 0, spread);
        snd(wid == I_PISTOL ? SFX_PISTOL : wid == I_SHOTGUN ? SFX_SHOTGUN : SFX_RIFLE, pl.pos, pl.z, 1.f);
        makeNoise(pl.pos, pl.z, d.noise); noiseR = d.noise; noiseShow = .8f;
        return;
    }
    // ближній бій
    float range = fist ? 1.15f : d.range, cd = fist ? .6f : d.cd, noise = fist ? 3.f : d.noise;
    int sk = fist ? SK_STRENGTH : d.skill;
    pl.atkCd = cd * (1 - .03f * pl.sk[sk == SK_NONE ? SK_STRENGTH : sk]);
    pl.endur = std::max(0.f, pl.endur - 4);
    snd(SFX_SWING, pl.pos, pl.z, .6f);
    int maxHits = (wid == I_BAT || wid == I_AXE) ? 2 : 1, hitN = 0;
    std::vector<std::pair<float, Zombie*>> cand;
    for (auto& zb : zs) {
        if (!zb.alive || zb.z != pl.z) continue;
        Vec2 r = zb.pos - pl.pos; float dd = r.len();
        if (dd > range + .3f) continue;
        float ang = std::atan2(r.y, r.x) - pl.face; while (ang > PI_F) ang -= 2 * PI_F;
                while (ang < -PI_F) ang += 2 * PI_F;
        if (dd > .6f && std::abs(ang) > .96f) continue;
        cand.push_back({dd, &zb});
    }
    std::sort(cand.begin(), cand.end(), [](auto& a, auto& b) { return a.first < b.first; });
    for (auto& c : cand) {
        if (hitN >= maxHits) break;
        Zombie& zb = *c.second;
        float hitCh = .62f + .04f * pl.sk[sk == SK_NONE ? SK_STRENGTH : sk] + (zb.stun > 0 ? .2f : 0);
        if (!rng.chance(clampv(hitCh, .2f, .97f))) continue;
        float dmg = fist ? rng.f(3, 6) : rng.f(d.dmgMin, d.dmgMax);
        dmg *= 1 + .04f * pl.sk[SK_STRENGTH];
        bool sneakHit = zb.st != ZS_CHASE && pl.sneak;
        if (sneakHit) dmg *= 2.5f;
        if (rng.chance(.08f + .01f * pl.sk[SK_STRENGTH])) dmg *= 2;
        zombieDamage(zb, dmg, pl.pos, fist ? .2f : .55f);
        gainSkill(sk == SK_NONE ? SK_STRENGTH : sk, 3); gainSkill(SK_STRENGTH, 1);
        hitN++;
    }
    if (hitN == 0) {
        // удар по дверях / вікнах / барикадах
        Vec2 f = pl.pos + dir * 1.0f; int tx = ftile(f.x), ty = ftile(f.y);
        if (inMap(tx, ty, pl.z)) {
            const Tile& t = world.at(tx, ty, pl.z);
            if ((t.kind == TK_DOOR && (t.barr > 0 || (t.flags & TF_LOCKED))) || (t.kind == TK_WINDOW && !(t.flags & TF_BROKEN)))
                bashTile(tx, ty, pl.z, (fist ? 5.f : (d.dmgMin + d.dmgMax) * .4f) * (1 + .05f * pl.sk[SK_STRENGTH]), false);
        }
    }
    makeNoise(pl.pos, pl.z, noise * .6f);
}

// ------------------------------------------------------------------ зомбі
void Game::spawnZombie(int x, int y, int z, bool horde) {
    Zombie zb; zb.id = nextId++; zb.pos = {x + .5f, y + .5f}; zb.z = z; zb.face = rng.f(0, 2 * PI_F);
    float r = rng.f(); float runner = std::min(.28f, .08f + .02f * day()), brute = .05f;
    if (r < brute) { zb.type = Z_BRUTE; zb.hp = 160; }
    else if (r < brute + runner) { zb.type = Z_RUNNER; zb.hp = 45; }
    else { zb.type = Z_WALKER; zb.hp = 60; }
    zb.horde = horde; zb.idleT = rng.f(1, 8); zb.lastPos = zb.pos;
    if (horde) { zb.st = ZS_ALERT; zb.goal = baseCenter; zb.goalZ = 0; zb.active = true; }
    zs.push_back(zb);
}
void Game::spawnHorde() {
    int n = std::min(60, 14 + 6 * day());
    int made = 0, guard = 0;
    while (made < n && guard++ < 2000) {
        if (world.outdoorTiles.empty()) break;
        PNode p = rng.pick(world.outdoorTiles);
        if (dist(Vec2(p.x + .5f, p.y + .5f), baseCenter) < 34) continue;
        spawnZombie(p.x, p.y, 0, true); made++;
    }
}
Colonist* Game::nearestColForZombie(const Zombie& z, float& d) {
    Colonist* best = nullptr; d = 1e9f;
    for (auto& c : cols) { if (!c.alive || c.z != z.z) continue; float dd = dist(c.pos, z.pos); if (dd < d) { d = dd; best = &c; } }
    return best;
}

void Game::updateZombies(float dt) {
    zTick_ += dt;
    bool recompute = zTick_ >= .5f;
    if (recompute) zTick_ = 0;
    for (auto& z : zs) {
        if (!z.alive) continue;
        if (recompute) {
            float d = pl.alive ? dist(z.pos, pl.pos) : 1e9f;
            for (auto& c : cols) if (c.alive) d = std::min(d, dist(z.pos, c.pos));
            z.active = d < 40 || z.horde;
        }
        if (z.active) updateZombie(z, dt);
    }
    // прибирання мертвих
    zs.erase(std::remove_if(zs.begin(), zs.end(), [](const Zombie& z) { return !z.alive; }), zs.end());
    // поповнення популяції
    spawnT_ += dt;
    if (spawnT_ > 20.f) {
        spawnT_ = 0;
        int target = std::min(170, 85 + 6 * day());
        if ((int)zs.size() < target && pl.alive) {
            for (int tries = 0; tries < 30; tries++) {
                if (world.outdoorTiles.empty()) break;
                PNode p = rng.pick(world.outdoorTiles);
                float d = dist(Vec2(p.x + .5f, p.y + .5f), pl.pos);
                if (d < 26 || d > 50 || vis[p.y * MAPW + p.x]) continue;
                spawnZombie(p.x, p.y, 0, false); break;
            }
        }
    }
}

void Game::updateZombie(Zombie& z, float dt) {
    z.atkCd = std::max(0.f, z.atkCd - dt); z.bashCd = std::max(0.f, z.bashCd - dt); z.repathT -= dt; z.senseT -= dt; z.groanT -= dt;
    if (z.stun > 0) { z.stun -= dt; return; }
    float baseSpd = z.type == Z_RUNNER ? 2.4f : z.type == Z_BRUTE ? 1.6f : 1.0f;
    // стони
    if (z.groanT <= 0) { z.groanT = rng.f(6, 16); if (z.st != ZS_IDLE && dist(z.pos, pl.pos) < 22) snd(SFX_GROAN, z.pos, z.z, .5f); }
    // відчуття
    if (z.senseT <= 0) {
        z.senseT = .25f;
        float dk = darkness();
        float range = lerpf(11.f, 6.f, dk);
        int best = -1; float bd = 1e9f;
        auto check = [&](int id, Vec2 p, int pz, bool sneak, bool light) {
            if (pz != z.z) return;
            float r = range; if (sneak) r *= clampv(.5f - .02f * pl.sk[SK_SNEAK] * 0 , .3f, 1.f) ; if (light && dk > .3f) r *= 1.5f;
            if (sneak) r *= (1 - .04f * pl.sk[SK_SNEAK]);
            float d = dist(p, z.pos); if (d > r) return;
            if (d > 2.2f) {
                Vec2 dirv = (p - z.pos).norm(); float ang = std::atan2(dirv.y, dirv.x) - z.face;
                while (ang > PI_F) ang -= 2 * PI_F;
                while (ang < -PI_F) ang += 2 * PI_F;
                if (std::abs(ang) > 1.25f) return;
            }
            if (!world.los(z.pos.x, z.pos.y, p.x, p.y, z.z)) return;
            if (d < bd) { bd = d; best = id; }
        };
        if (pl.alive) check(0, pl.pos, pl.z, pl.sneak, pl.light);
        for (auto& c : cols) if (c.alive) check(1000 + c.id, c.pos, c.z, false, false);
        if (best >= 0) {
            if (z.st != ZS_CHASE) { z.st = ZS_CHASE; snd(SFX_GROAN, z.pos, z.z, .8f); }
            z.target = best; z.lostT = 0; z.repathT = std::min(z.repathT, 0.f);
        }
    }
    // ціль
    Vec2 tp; int tz = z.z; bool hasT = false; Colonist* tc = nullptr;
    if (z.st == ZS_CHASE) {
        if (z.target == 0 && pl.alive) { tp = pl.pos; tz = pl.z; hasT = true; }
        else if (z.target >= 1000) { tc = colonist(z.target - 1000); if (tc && tc->alive) { tp = tc->pos; tz = tc->z; hasT = true; } }
        if (!hasT) { z.st = ZS_ALERT; z.goal = z.pos; z.goalZ = z.z; }
    }
    bool blocked = false; PNode bt{0, 0, 0};
    float spd = baseSpd;
    if (z.st == ZS_CHASE && hasT) {
        z.goal = tp; z.goalZ = tz;
        float d = dist(z.pos, tp);
        if (tz == z.z && d < .95f) {
            Vec2 dv = (tp - z.pos); z.face = std::atan2(dv.y, dv.x);
            if (z.atkCd <= 0) {
                z.atkCd = z.type == Z_RUNNER ? .9f : 1.2f;
                float dm = z.type == Z_BRUTE ? rng.f(18, 28) : z.type == Z_RUNNER ? rng.f(8, 14) : rng.f(10, 16);
                if (z.target == 0) hurtPlayer(dm, true); else if (tc) hurtColonist(*tc, dm);
            }
            return;
        }
        bool seesT = tz == z.z && world.los(z.pos.x, z.pos.y, tp.x, tp.y, z.z);
        if (!seesT) { z.lostT += dt; if (z.lostT > 6) { z.st = ZS_ALERT; z.searchT = 0; } } else z.lostT = 0;
        if (seesT && d < 5) {
            Vec2 dv = (tp - z.pos).norm(); float before = dist(z.pos, tp);
            Vec2 mv = dv * (spd * dt); z.face = std::atan2(dv.y, dv.x);
            if (moveEntity(z.pos, z.z, mv, .28f) || before < 1) { z.path.clear(); z.pi = 0; goto moved; }
        }
    } else if (z.st == ZS_ALERT) spd = baseSpd * .8f;
    else if (z.st == ZS_WANDER) spd = baseSpd * .5f;
    else if (z.st == ZS_IDLE) {
        z.idleT -= dt;
        if (z.idleT <= 0 && !z.horde) {
            // випадкове блукання
            for (int t = 0; t < 6; t++) {
                Vec2 g = z.pos + Vec2(rng.f(-9, 9), rng.f(-9, 9));
                if (!inMap(ftile(g.x), ftile(g.y), z.z) || world.blocks(ftile(g.x), ftile(g.y), z.z)) continue;
                z.goal = g; z.goalZ = z.z; z.st = ZS_WANDER; z.repathT = 0; break;
            }
            z.idleT = rng.f(2, 8);
        }
        return;
    }
    // рух за шляхом до goal
    if (z.st == ZS_ALERT && z.pi >= (int)z.path.size() && dist(z.pos, z.goal) < 1.2f && z.goalZ == z.z) {
        z.searchT += dt; z.face += dt * 1.5f;
        if (z.searchT > 4) { z.st = z.horde ? ZS_ALERT : ZS_IDLE; z.idleT = rng.f(2, 6); if (z.horde) { z.goal = baseCenter + Vec2(rng.f(-3, 3), rng.f(-3, 3)); z.goalZ = 0; z.searchT = 0; } }
        return;
    }
    if (z.pi >= (int)z.path.size() || z.repathT <= 0) {
        if (z.repathT <= 0) {
            z.repathT = z.st == ZS_CHASE ? .7f : 1.5f;
            if (!pathTo(z.path, z.pi, z.pos, z.z, z.goal, z.goalZ, PM_ZOMBIE, 2500) && pathBudget_ > 0) {
                if (z.st == ZS_WANDER) { z.st = ZS_IDLE; z.idleT = rng.f(1, 4); }
                else if (z.st == ZS_ALERT && !z.horde) { z.st = ZS_IDLE; z.idleT = rng.f(1, 4); }
            }
        }
    }
    if (z.pi < (int)z.path.size()) {
        bool done = followPath(z.path, z.pi, z.pos, z.z, spd, dt, z.lockX, z.lockY, .28f, z.face, true, &blocked, &bt);
        if (blocked && z.bashCd <= 0) { z.bashCd = .9f; bashTile(bt.x, bt.y, z.z, z.type == Z_BRUTE ? 22.f : 11.f, true); }
        if (done && z.st == ZS_WANDER) { z.st = ZS_IDLE; z.idleT = rng.f(2, 7); }
    } else if (z.st == ZS_WANDER) { z.st = ZS_IDLE; z.idleT = rng.f(2, 6); }
moved:
    tryStairs(z.pos, z.z, z.lockX, z.lockY);
    // застрягання
    if (dist(z.pos, z.lastPos) < .02f * spd * dt * 10 && !blocked) { z.stuckT += dt; if (z.stuckT > 1.5f) { z.path.clear(); z.pi = 0; z.repathT = 0; z.stuckT = 0; } }
    else { z.stuckT = 0; }
    z.lastPos = z.pos;
}

// ------------------------------------------------------------------ події
void Game::updateEvents(float dt) {
    (void)dt;
    if (!heliDone && day() >= 2 && hour() >= 14) {
        heliDone = true;
        say("Над містом прогудів гелікоптер... Звук привабив зомбі.");
        Vec2 p((float)rng.range(10, MAPW - 10), (float)rng.range(10, MAPH - 10));
        snd(SFX_HELI, p, 0, 1.f); makeNoise(p, 0, 45);
    }
    if (day() >= nextHordeDay && hour() >= 21 && !hordeAnnounced) {
        hordeAnnounced = true; say("Зі сторони міста чути гул... Орда наближається до бази!");
        spawnHorde();
    }
    if (hordeAnnounced && hour() < 12 && hour() > 6) { hordeAnnounced = false; nextHordeDay = day() + 2; }
    // дощова вода
    static int lastDay = 0;
    if (day() != lastDay) { lastDay = day(); for (auto& f : world.furn) if (f.alive && f.kind == F_BARREL && f.stage < 3) f.stage++; }
}
