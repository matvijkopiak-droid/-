#include "game.h"

void Game::recruit(int si) {
    if (si < 0 || si >= (int)survivors.size()) return;
    Survivor s = survivors[si]; survivors.erase(survivors.begin() + si);
    addColonistFromSurvivor(s);
    say("Вцілілий " + s.name + " приєднався до вас!"); snd(SFX_LEVELUP, pl.pos, pl.z, .8f);
}
void Game::addColonistFromSurvivor(const Survivor& s) {
    Colonist c; c.id = s.id; c.name = s.name; c.pos = s.pos; c.z = s.z; c.weapon = s.weapon; c.ammo = s.ammo;
    c.post = s.pos; c.postZ = s.z; c.wander = s.pos;
    c.scavLvl = rng.range(1, 3); c.buildLvl = rng.range(1, 3); c.farmLvl = rng.range(1, 3); c.combatLvl = rng.range(1, 3);
    c.food = rng.f(55, 85); c.water = rng.f(55, 85); c.energy = rng.f(60, 90);
    cols.push_back(c);
}
void Game::setJob(int id, Job j) {
    Colonist* c = colonist(id); if (!c) return;
    c->job = j; c->path.clear(); c->pi = 0; c->state = 0; c->timer = 0; c->idleT = 0; c->targetFurn = -1; c->targetBp = -1;
    if (j == J_GUARD) { c->post = c->pos; c->postZ = c->z; }
}
void Game::setPost(int id) { Colonist* c = colonist(id); if (c) { c->post = pl.pos; c->postZ = pl.z; } }

// ------------------------------------------------------------------ рух колоніста
bool Game::colGoTo(Colonist& c, Vec2 target, int tz, float stop, float speed, float dt) {
    if (c.z == tz && dist(c.pos, target) <= stop) { c.path.clear(); c.pi = 0; return true; }
    c.repathT -= dt;
    if (dist(c.wander, target) > .6f || c.wander.x < 0 || (c.pi >= (int)c.path.size() && c.repathT <= 0) || c.repathT < -3) {
        if (c.repathT <= 0) {
            c.wander = target; c.repathT = 1.2f;
            pathTo(c.path, c.pi, c.pos, c.z, target, tz, PM_HUMAN, 6000);
        }
    }
    if (c.pi < (int)c.path.size()) {
        float dummy = c.face;
        followPath(c.path, c.pi, c.pos, c.z, speed, dt, c.lockX, c.lockY, .28f, dummy, false, nullptr, nullptr);
        c.face = dummy; tryStairs(c.pos, c.z, c.lockX, c.lockY);
    }
    return c.z == tz && dist(c.pos, target) <= stop;
}

void Game::colEatDrink(Colonist& c) {
    // їсть із власних запасів, інакше з ящиків бази
    auto tryEat = [&](Inventory& inv) {
        int bi = -1; float bs = 0;
        for (size_t i = 0; i < inv.items.size(); i++) {
            const ItemDef& d = idef(inv.items[i].id);
            float need = 0;
            if (c.food < 55) need += d.food;
            if (c.water < 55) need += d.water * 1.2f;
            if (need > bs) { bs = need; bi = (int)i; }
        }
        if (bi < 0) return false;
        const ItemDef& d = idef(inv.items[bi].id);
        c.food = std::min(100.f, c.food + d.food); c.water = std::min(100.f, c.water + d.water);
        inv.removeAt(bi, 1); snd(SFX_EAT, c.pos, c.z, .5f);
        return true;
    };
    if (tryEat(c.carry)) return;
    for (int i : storageCrates()) if (tryEat(world.furn[i].inv)) return;
}

// ------------------------------------------------------------------ окремий колоніст
void Game::updateColonist(Colonist& c, float dt) {
    c.atkCd = std::max(0.f, c.atkCd - dt);
    float gm = dt * MIN_PER_SEC;
    c.food -= .026f * gm * (c.sleeping ? .4f : 1.f); c.water -= .05f * gm * (c.sleeping ? .4f : 1.f);
    if (c.sleeping) c.energy += .25f * gm; else c.energy -= .05f * gm;
    c.food = std::max(0.f, c.food); c.water = std::max(0.f, c.water); c.energy = clampv(c.energy, 0.f, 100.f);
    if (c.food <= 0) c.hp -= .02f * gm;
    if (c.water <= 0) c.hp -= .05f * gm;
    if (c.food > 30 && c.water > 30 && c.hp < 100) c.hp = std::min(100.f, c.hp + .03f * gm);
    if (c.hp <= 0) { if (c.alive) { c.alive = false; say(c.name + " помер."); } return; }
    // їжа
    if ((c.food < 35 || c.water < 35) && (c.food < 55 || c.water < 55)) colEatDrink(c);
    // найближча загроза
    Zombie* th = nullptr; float td = 1e9f;
    for (auto& z : zs) {
        if (!z.alive || z.z != c.z) continue;
        float d = dist(z.pos, c.pos);
        if (d < td && d < 9 && world.los(c.pos.x, c.pos.y, z.pos.x, z.pos.y, c.z)) { td = d; th = &z; }
    }
    if (c.sleeping) {
        if (th || c.energy >= 95 || !isNight()) { c.sleeping = false; }
        else { c.status = "спить"; return; }
    }
    if (th) {
        c.fleeing = c.hp < 30;
        if (c.fleeing) {
            Vec2 away = (c.pos - th->pos).norm();
            Vec2 target = baseCenter; if (dist(c.pos, baseCenter) < 4) target = c.pos + away * 6;
            colGoTo(c, target, c.z, 1.f, 3.2f, dt); c.status = "тікає"; return;
        }
        c.status = "бореться";
        const ItemDef& d = idef(c.weapon);
        if (d.type == IT_GUN && c.ammo > 0 && td < d.range) {
            if (c.atkCd <= 0) {
                c.atkCd = d.cd + .3f; c.ammo--;
                Vec2 dir = (th->pos - c.pos).norm(); c.face = std::atan2(dir.y, dir.x);
                float a = std::atan2(dir.y, dir.x) + rng.f(-.08f, .08f);
                if (rng.chance(.7f)) zombieDamage(*th, rng.f(d.dmgMin, d.dmgMax), c.pos, .2f);
                (void)a;
                snd(c.weapon == I_PISTOL ? SFX_PISTOL : c.weapon == I_SHOTGUN ? SFX_SHOTGUN : SFX_RIFLE, c.pos, c.z, 1.f);
                makeNoise(c.pos, c.z, d.noise);
            }
            return;
        }
        if (d.type == IT_GUN && c.ammo <= 0) c.weapon = I_BAT;
        float reach = idef(c.weapon).range > 0 ? idef(c.weapon).range : 1.2f;
        if (td > reach * .8f) { colGoTo(c, th->pos, c.z, reach * .7f, 3.0f, dt); }
        else if (c.atkCd <= 0) {
            c.atkCd = std::max(.5f, idef(c.weapon).cd);
            Vec2 dir = (th->pos - c.pos).norm(); c.face = std::atan2(dir.y, dir.x);
            const ItemDef& w = idef(c.weapon);
            if (rng.chance(.55f + .05f * c.combatLvl)) zombieDamage(*th, rng.f(std::max(5.f, w.dmgMin), std::max(9.f, w.dmgMax)), c.pos, .45f);
            snd(SFX_SWING, c.pos, c.z, .5f);
        }
        return;
    }
    c.fleeing = false;
    // нічний сон для не-охоронців
    if (isNight() && c.energy < 70 && c.job != J_GUARD && inBase(c.pos)) { c.sleeping = true; c.status = "спить"; return; }
    switch (c.job) {
        case J_FOLLOW: {
            c.status = "іде за вами";
            if (pl.alive && (dist(c.pos, pl.pos) > 3.2f || c.z != pl.z)) {
                Vec2 t = pl.pos + Vec2(rng.f(-1, 1), rng.f(-1, 1)) * 0.0f;
                colGoTo(c, t, pl.z, 2.4f, pl.sprint ? 4.6f : 3.0f, dt);
            }
            break;
        }
        case J_IDLE: c.status = "відпочиває"; break;
        case J_GUARD: {
            c.status = "на посту";
            if (c.z != c.postZ || dist(c.pos, c.post) > 1.5f) colGoTo(c, c.post, c.postZ, 1.0f, 3.0f, dt);
            break;
        }
        case J_BUILD: {
            // знайти план
            Blueprint* b = (c.targetBp >= 0 && c.targetBp < (int)bps.size() && bps[c.targetBp].alive) ? &bps[c.targetBp] : nullptr;
            if (!b) { c.targetBp = -1; float bd = 1e9f; for (size_t i = 0; i < bps.size(); i++) if (bps[i].alive) { float d = dist(Vec2(bps[i].x + .5f, bps[i].y + .5f), c.pos); if (d < bd) { bd = d; c.targetBp = (int)i; } } if (c.targetBp >= 0) b = &bps[c.targetBp]; }
            if (!b) { c.status = "немає чого будувати"; break; }
            c.status = "будує";
            Vec2 t(b->x + .5f, b->y + .5f);
            if (colGoTo(c, t, b->z, 1.6f, 3.0f, dt)) {
                b->progress += dt * (.6f + .15f * c.buildLvl);
                if (b->progress >= buildDef(b->kind).time) { completeBlueprint(*b); c.targetBp = -1; if (c.buildLvl < 10 && rng.chance(.15f)) c.buildLvl++; }
            }
            break;
        }
        case J_FARM: {
            // знайти грядку, що потребує уваги
            Furn* f = (c.targetFurn >= 0 && c.targetFurn < (int)world.furn.size() && world.furn[c.targetFurn].alive) ? &world.furn[c.targetFurn] : nullptr;
            if (f && !(f->kind == F_FARM && (f->stage == 0 || f->stage == 3))) { f = nullptr; c.targetFurn = -1; }
            if (!f) {
                float bd = 1e9f;
                for (size_t i = 0; i < world.furn.size(); i++) {
                    Furn& g = world.furn[i];
                    if (!g.alive || g.kind != F_FARM || !(g.stage == 3 || (g.stage == 0 && hasMaterial(I_SEEDS, 1)))) continue;
                    float d = dist(Vec2(g.x + .5f, g.y + .5f), c.pos); if (d < bd) { bd = d; c.targetFurn = (int)i; }
                }
                if (c.targetFurn >= 0) f = &world.furn[c.targetFurn];
            }
            if (!f) { c.status = "грядки не потребують роботи"; break; }
            c.status = "працює на грядці";
            Vec2 t(f->x + .5f, f->y + .5f);
            if (colGoTo(c, t, f->z, 1.3f, 3.0f, dt)) {
                c.timer += dt;
                if (c.timer > 3.5f) {
                    c.timer = 0;
                    if (f->stage == 0) { if (takeMaterial(I_SEEDS, 1) > 0) { f->stage = 1; f->timer = 0; } }
                    else if (f->stage == 3) {
                        int n = 2 + c.farmLvl / 2 + rng.range(0, 2);
                        auto cr = storageCrates();
                        Inventory& dst = cr.empty() ? c.carry : world.furn[cr[0]].inv;
                        dst.add(I_VEG, n); if (rng.chance(.6f)) dst.add(I_SEEDS, 1);
                        f->stage = 0; f->timer = 0; if (c.farmLvl < 10 && rng.chance(.1f)) c.farmLvl++;
                    }
                    c.targetFurn = -1;
                }
            }
            break;
        }
        case J_SCAV: {
            auto crates = storageCrates();
            float cw = c.carry.weight();
            // повернутися здати здобич
            if (c.state == 2 || cw > 9 || (c.state == 0 && c.timer >= 4)) {
                if (crates.empty()) { c.status = "немає скрині для здачі"; c.state = 0; c.timer = 0; break; }
                Furn& cr = world.furn[crates[0]];
                c.status = "несе здобич на базу";
                if (colGoTo(c, {cr.x + .5f, cr.y + .5f}, cr.z, 1.6f, 3.0f, dt)) {
                    for (auto& it : c.carry.items) cr.inv.add(it.id, it.n, it.ammo);
                    c.carry.items.clear(); c.state = 0; c.timer = 0; c.targetFurn = -1;
                    if (c.scavLvl < 10 && rng.chance(.12f)) c.scavLvl++;
                }
                break;
            }
            if (c.hp < 50) { c.status = "лікується"; colGoTo(c, baseCenter, 0, 3.f, 3.f, dt); break; }
            Furn* f = (c.targetFurn >= 0 && c.targetFurn < (int)world.furn.size() && world.furn[c.targetFurn].alive && !world.furn[c.targetFurn].searched) ? &world.furn[c.targetFurn] : nullptr;
            if (!f) {
                c.targetFurn = -1; float bd = 1e9f;
                for (size_t i = 0; i < world.furn.size(); i++) {
                    Furn& g = world.furn[i];
                    if (!g.alive || g.searched || g.built || !furnIsContainer(g.kind) || g.kind == F_PILE || g.inv.empty()) continue;
                    Vec2 gc(g.x + .5f, g.y + .5f); if (dist(gc, baseCenter) < 6) continue;
                    float d = dist(gc, c.pos) + std::abs(g.z - c.z) * 8.f; if (dist(gc, baseCenter) > 45) continue;
                    if (d < bd) { bd = d; c.targetFurn = (int)i; }
                }
                if (c.targetFurn >= 0) f = &world.furn[c.targetFurn];
            }
            if (!f) { c.status = c.carry.empty() ? "немає що збирати" : "несе здобич на базу"; if (!c.carry.empty()) c.state = 2; break; }
            c.status = "шукає припаси";
            // стати поруч (на сусідній прохідній клітинці)
            Vec2 t(f->x + .5f, f->y + .5f);
            bool near = c.z == f->z && dist(c.pos, t) < 1.9f;
            if (!near) {
                // шлях до однієї із сусідніх клітинок
                Vec2 best = t; float bd = 1e9f; bool ok = false;
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                    int nx = f->x + dx, ny = f->y + dy; if ((!dx && !dy) || world.blocks(nx, ny, f->z)) continue;
                    float d = dist(Vec2(nx + .5f, ny + .5f), c.pos); if (d < bd) { bd = d; best = {nx + .5f, ny + .5f}; ok = true; }
                }
                if (!ok) { f->searched = true; c.targetFurn = -1; break; }
                colGoTo(c, best, f->z, .5f, 3.0f, dt);
                c.timer = c.timer >= 4 ? c.timer : 0;
            } else {
                c.sleeping = false;
                float work = 3.5f / (1 + .12f * c.scavLvl);
                c.idleT += dt;
                if (c.idleT >= work) {
                    c.idleT = 0; f->searched = true;
                    for (auto& it : f->inv.items) c.carry.add(it.id, it.n, it.ammo);
                    f->inv.items.clear(); c.targetFurn = -1; c.timer += 1; c.state = 0;
                    snd(SFX_PICK, c.pos, c.z, .4f);
                }
            }
            break;
        }
        default: break;
    }
}

void Game::updateColonists(float dt) {
    for (auto& c : cols) if (c.alive) updateColonist(c, dt);
    cols.erase(std::remove_if(cols.begin(), cols.end(), [](const Colonist& c) { return !c.alive; }), cols.end());
}

// ------------------------------------------------------------------ грядки
void Game::updateFarms(float gm) {
    for (auto& f : world.furn) {
        if (!f.alive || f.kind != F_FARM || f.stage == 0 || f.stage == 3) continue;
        f.timer += gm;
        if (f.stage == 1 && f.timer > 720) f.stage = 2;
        if (f.stage == 2 && f.timer > 2880) f.stage = 3;
    }
}
