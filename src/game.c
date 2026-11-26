#include "game.h"

#include <math.h>
#include <stddef.h>

/* Area bermain tempat peleton boleh dipasang dan zombie bergerak. */
const Rectangle battlefield = { 12, 60, 474, 266 };

/* Tujuh titik belok (waypoint) jalur zombie dari kiri atas menuju gerbang shelter. */
const Vector2 route[PATH_COUNT] = {
    { 13, 106 }, { 137, 106 }, { 137, 255 }, { 275, 255 },
    { 275, 147 }, { 389, 147 }, { 389, 294 }
};

/* Tabel spesifikasi tiga jenis peleton beserta stat untuk Level 1 sampai Level 3. */
const TowerSpec tower_specs[TOWER_KIND_COUNT] = {
    [ARCHER] = { "Archer", 55, 0, {{11, 77, 40, .56f}, {15, 84, 65, .49f}, {20, 92, 0, .41f}} },
    [CANNON] = { "Cannon", 100, 24, {{30, 82, 75, 1.35f}, {39, 91, 110, 1.20f}, {52, 100, 0, 1.04f}} },
    [SNIPER] = { "Sniper", 125, 0, {{69, 130, 85, 1.65f}, {90, 145, 130, 1.48f}, {122, 160, 0, 1.30f}} }
};

/*
 * Stat dasar tiap tipe musuh:
 * hp = darah di wave 1, growth = tambahan HP tiap naik wave,
 * reward = dana saat mati, leak = pengurangan HP shelter jika lolos.
 */
static const struct {
    int hp, growth, reward, leak;
    float speed;
} enemies[ENEMY_KIND_COUNT] = {
    [WALKER] = { 31, 5, 5, 2, 40 },
    [RUNNER] = { 23, 4, 6, 2, 67 },
    [MUTANT] = { 110, 14, 10, 3, 28 },
    [BOSS] = { 550, 35, 32, 5, 22 },
    [ARMORED] = { 110, 14, 10, 3, 28 },
    [BREACHER] = { 165, 21, 10, 5, 28 }
};

/* Komposisi dasar 15 wave normal (Walker, Runner, Mutant, Boss) dan jeda spawn. */
static const struct {
    int counts[ENEMY_KIND_COUNT];
    float interval;
} waves[WAVE_COUNT] = {
    {{9,0,0,0},.90f},   {{12,3,0,0},.78f}, {{15,6,0,0},.67f},
    {{13,6,3,0},.64f},  {{14,8,4,0},.58f}, {{15,10,5,0},.54f},
    {{15,12,6,0},.50f}, {{17,13,8,0},.48f},{{19,14,9,0},.46f},
    {{18,17,10,0},.43f},{{21,16,11,0},.40f},{{22,18,12,0},.39f},
    {{23,20,13,0},.37f},{{24,20,15,0},.35f},{{24,23,17,0},.33f}
};

/* Empat paket serbuan tambahan untuk Mode Krisis (dua di antaranya memuat Boss). */
static const int crisis_groups[][ENEMY_KIND_COUNT] = {
    { 8, 17, 4, 0, 5, 5 }, { 13, 6, 10, 0, 8, 4 },
    { 7, 8, 4, 1, 7, 6 }, { 8, 5, 7, 1, 4, 9 }
};

/* Menghitung jarak kuadrat dua titik (lebih cepat karena menghindari sqrtf saat cek jangkauan). */
static float distance_squared(Vector2 a, Vector2 b)
{
    float x = a.x-b.x, y = a.y-b.y;
    return x*x+y*y;
}

/*
 * Menghitung jarak kuadrat terdekat dari titik p ke ruas garis jalan a->b
 * menggunakan proyeksi vektor (dot product) yang dibatasi pada rentang [0, 1].
 */
static float segment_distance_squared(Vector2 p, Vector2 a, Vector2 b)
{
    float x = b.x-a.x, y = b.y-a.y;
    float along = ((p.x-a.x)*x + (p.y-a.y)*y)/(x*x+y*y);
    along = fmaxf(0, fminf(1, along));
    return distance_squared(p, (Vector2){ a.x+x*along, a.y+y*along });
}

/* Menyimpan partikel/efek visual ke dalam ring buffer berkapasitas tetap MAX_FX. */
static void fx(Game *g, FxKind kind, Vector2 from, Vector2 to, float duration, int amount)
{
    g->effects[g->nextEffect] = (Effect){ kind, from, to, duration, duration, amount };
    g->nextEffect = (g->nextEffect + 1) % MAX_FX;
}

/* Mengatur ulang seluruh state permainan dan mengundi satu wave Krisis di antara wave 11..15. */
void reset_game(Game *g)
{
    *g = (Game){ .state = PREPARE, .wave = 1, .money = 115, .hp = MAX_HP,
                 .crisisWave = GetRandomValue(WAVE_COUNT-4, WAVE_COUNT) };
}

/* Menghitung komposisi aktual musuh pada nomor wave tertentu (termasuk konversi Zirah & Pendobrak). */
void preview_wave_counts(int wave, int counts[ENEMY_KIND_COUNT])
{
    if (wave < 1) wave = 1;
    if (wave > WAVE_COUNT) wave = WAVE_COUNT;
    for (int i = 0; i < ENEMY_KIND_COUNT; i++) counts[i] = waves[wave-1].counts[i];
    if (wave >= 6) counts[ARMORED] = 1 + (wave-6)/2;
    if (wave >= 9) counts[BREACHER] = 1 + (wave-9)/2;
    counts[MUTANT] -= counts[ARMORED] + counts[BREACHER];
}

/* Menghitung total musuh yang masih harus dihadapi pada gelombang aktif (antrean + hidup di peta). */
int remaining_enemies(const Game *g)
{
    int alive = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g->enemies[i].active) alive++;
    }
    return (g->queueLength - g->queueNext) + alive;
}

/*
 * Menyusun antrean spawn agar tipe-tipe zombie tersebar proporsional sepanjang wave
 * (weighted round-robin berbasis integer sehingga jenis musuh tidak menumpuk di satu sisi).
 */
static void queue_group(Game *g, const int counts[ENEMY_KIND_COUNT])
{
    g->queueLength = 0;
    g->queueNext = 0;
    g->spawnIn = 0;
    int total = 0;
    for (int k = 0; k < ENEMY_KIND_COUNT; k++) total += counts[k];
    int emitted[ENEMY_KIND_COUNT] = {0};
    for (int slot = 0; slot < total && slot < MAX_QUEUE; slot++) {
        int candidate = -1, highest = 0;
        for (int k = 0; k < ENEMY_KIND_COUNT; k++) {
            if (emitted[k] >= counts[k]) continue;
            int score = (slot+1)*counts[k] - emitted[k]*total;
            if (candidate < 0 || score > highest) {
                candidate = k;
                highest = score;
            }
        }
        g->queue[g->queueLength++] = (EnemyKind)candidate;
        emitted[candidate]++;
    }
}

bool start_wave(Game *g)
{
    if (g->state != PREPARE) return false;
    g->state = WAVE;
    g->lastBonus = 0;
    g->lastHeal = 0;
    int counts[ENEMY_KIND_COUNT];
    preview_wave_counts(g->wave, counts);
    queue_group(g, counts);
    return true;
}

bool can_manage(const Game *g)
{
    return g->state != WON && g->state != LOST;
}

/* Memvalidasi apakah koordinat p berada di dalam peta dan tidak menabrak shelter, jalan, atau peleton lain. */
bool valid_ground(const Game *g, Vector2 p)
{
    if (p.x-TOWER_RADIUS < battlefield.x || p.x+TOWER_RADIUS > battlefield.x+battlefield.width ||
        p.y-TOWER_RADIUS < battlefield.y || p.y+TOWER_RADIUS > battlefield.y+battlefield.height) return false;
    if (CheckCollisionCircleRec(p, TOWER_RADIUS, (Rectangle){ 357, 284, 64, 39 })) return false;
    for (int i = 1; i < PATH_COUNT; i++) {
        float clearance = ROAD_RADIUS + TOWER_RADIUS;
        if (segment_distance_squared(p, route[i-1], route[i]) < clearance*clearance) return false;
    }
    for (int i = 0; i < g->towerCount; i++) {
        if (distance_squared(p, g->towers[i].pos) < 4*TOWER_RADIUS*TOWER_RADIUS) return false;
    }
    return true;
}

bool buy_tower(Game *g, TowerKind kind, Vector2 p)
{
    int price = tower_specs[kind].price;
    if (!can_manage(g) || g->money < price || g->towerCount == MAX_TOWERS || !valid_ground(g, p)) return false;
    g->towers[g->towerCount++] = (Tower){ .pos = p, .facing = 1, .kind = kind, .invested = price };
    g->money -= price;
    g->events |= EV_PLACE;
    fx(g, DUST, p, p, .37f, 0);
    return true;
}

int tower_at(const Game *g, Vector2 point)
{
    for (int i = 0; i < g->towerCount; i++) {
        Vector2 body = g->towers[i].pos;
        body.y -= 6;
        if (distance_squared(point, body) <= 144) return i;
    }
    return -1;
}

int sale_value(const Tower *tower)
{
    return tower->invested*70/100;
}

bool upgrade_tower(Game *g, int index)
{
    if (!can_manage(g) || index < 0 || index >= g->towerCount) return false;
    Tower *t = &g->towers[index];
    int price = tower_specs[t->kind].levels[t->level].upgradeCost;
    if (!price || g->money < price) return false;
    g->money -= price;
    t->invested += price;
    t->level++;
    g->events |= EV_UPGRADE;
    fx(g, UPGRADE, t->pos, t->pos, .30f, 0);
    return true;
}

/* Menjual peleton dengan pengembalian 70% modal lalu menggeser isi array ke kiri agar rapat. */
bool sell_tower(Game *g, int index)
{
    if (!can_manage(g) || index < 0 || index >= g->towerCount) return false;
    Tower *t = &g->towers[index];
    int refund = sale_value(t);
    g->money += refund;
    g->events |= EV_SELL;
    fx(g, DUST, t->pos, t->pos, .37f, 0);
    for (int i = index; i+1 < g->towerCount; i++) g->towers[i] = g->towers[i+1];
    g->towerCount--;
    return true;
}

void cycle_tower_target(Game *g, int index)
{
    if (!can_manage(g) || index < 0 || index >= g->towerCount) return;
    Tower *t = &g->towers[index];
    t->targetMode = (TargetMode)((t->targetMode + 1) % TARGET_MODE_COUNT);
}

/* Mencari slot kosong pada object pool enemies[] untuk memunculkan zombie berikutnya dari antrean. */
static void spawn_next(Game *g, float dt)
{
    if (g->queueNext >= g->queueLength) return;
    g->spawnIn = fmaxf(0, g->spawnIn-dt);
    if (g->spawnIn > 0) return;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g->enemies[i].active) continue;
        EnemyKind kind = g->queue[g->queueNext++];
        int hp = enemies[kind].hp + (g->wave-1)*enemies[kind].growth;
        if (g->state == CRISIS_WAVE) hp = hp*125/100;
        g->enemies[i] = (Enemy){ .pos = route[0], .speed = enemies[kind].speed, .kind = kind,
            .hp = hp, .maxHp = hp, .next = 1, .active = true, .stride = (g->queueNext%3)*.13f };
        g->spawnIn = g->state == CRISIS_WAVE ? .23f : waves[g->wave-1].interval;
        return;
    }
}

/* Menggerakkan zombie menyusuri waypoint; Pendobrak (BREACHER) berlari di segmen lurus terakhir. */
static void move_enemy(Enemy *z, float dt)
{
    if (z->kind == BREACHER && z->next == PATH_COUNT-1) z->speed = enemies[RUNNER].speed;
    float left = z->speed*dt;
    while (left > 0 && z->next < PATH_COUNT) {
        Vector2 end = route[z->next];
        float length = sqrtf(distance_squared(end, z->pos));
        if (length <= left) {
            z->pos = end;
            z->walked += length;
            left -= length;
            z->next++;
        } else {
            z->pos.x += (end.x-z->pos.x)*left/length;
            z->pos.y += (end.y-z->pos.y)*left/length;
            z->walked += left;
            left = 0;
        }
    }
    z->hit = fmaxf(0, z->hit-dt);
    z->stride += dt;
}

/* Mengurangi HP musuh (Zirah menahan 40% damage senjata non-Sniper) dan memberi hadiah saat mati. */
static void hurt_enemy(Game *g, Enemy *target, int damage, Tower *attacker)
{
    if (target->kind == ARMORED && attacker->kind != SNIPER) damage = damage*6/10;
    target->hp -= damage;
    target->hit = .12f;
    if (target->hp > 0) return;
    target->active = false;
    attacker->kills++;
    g->totalKills++;
    int reward = enemies[target->kind].reward;
    g->money += reward;
    g->totalEarned += reward;
    fx(g, REWARD, target->pos, target->pos, .69f, reward);
}

/* Mengevaluasi apakah kandidat z lebih diprioritaskan daripada target saat ini sesuai mode bidik. */
static bool prefers_target(const Tower *t, const Enemy *current, const Enemy *z, float curDistSq, float zDistSq)
{
    if (!current) return true;
    if (t->kind == SNIPER && t->targetMode == TARGET_FIRST &&
        (current->kind == ARMORED) != (z->kind == ARMORED)) {
        return z->kind == ARMORED;
    }
    if (t->targetMode == TARGET_STRONG && z->hp != current->hp) {
        return z->hp > current->hp;
    }
    if (t->targetMode == TARGET_CLOSE && fabsf(zDistSq - curDistSq) > 0.01f) {
        return zDistSq < curDistSq;
    }
    return z->walked > current->walked;
}

static void fire_tower(Game *g, Tower *t)
{
    const TowerSpec *spec = &tower_specs[t->kind];
    const TowerStats *stats = &spec->levels[t->level];
    Enemy *target = NULL;
    float bestDistSq = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *z = &g->enemies[i];
        if (!z->active) continue;
        float distSq = distance_squared(t->pos, z->pos);
        if (distSq > stats->range*stats->range) continue;
        if (prefers_target(t, target, z, bestDistSq, distSq)) {
            target = z;
            bestDistSq = distSq;
        }
    }
    if (!target) return;

    t->facing = target->pos.x < t->pos.x ? -1 : 1;
    t->reload = stats->reload;
    t->recoil = t->kind == CANNON ? .33f : .22f;
    g->events |= (t->kind == ARCHER ? EV_BOW : t->kind == CANNON ? EV_CANNON : EV_SNIPER);
    Vector2 impact = target->pos;
    Vector2 muzzle = { t->pos.x+t->facing*9, t->pos.y-10 };
    fx(g, SHOT, muzzle, (Vector2){ impact.x, impact.y-7 }, .14f, t->kind);
    if (!spec->splash) {
        hurt_enemy(g, target, stats->damage, t);
        return;
    }
    fx(g, EXPLOSION, impact, impact, .28f, spec->splash);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *z = &g->enemies[i];
        if (z->active && distance_squared(z->pos, impact) <= spec->splash*spec->splash)
            hurt_enemy(g, z, z == target ? stats->damage : stats->damage/2, t);
    }
}

static void finish_wave(Game *g)
{
    if (g->state == CRISIS_WAVE) g->crisisCleared = true;
    g->lastBonus = 20 + (g->wave-1)*3;
    g->money += g->lastBonus;
    g->totalEarned += g->lastBonus;
    if (g->wave == WAVE_COUNT) {
        g->state = WON;
        return;
    }
    if (g->hp < MAX_HP-1) {
        g->hp++;
        g->lastHeal = 1;
    }
    g->wave++;
    g->state = PREPARE;
}

/*
 * Satu langkah simulasi (dipanggil 60 kali per detik waktu simulasi):
 * 1. Spawn & gerakkan musuh lebih dulu.
 * 2. Proses tembakan peleton sebelum cek kebocoran (musuh yang tewas tepat di gerbang tidak melukai shelter).
 * 3. Kurangi HP shelter untuk musuh yang masih hidup di titik akhir dan kelola transisi wave/Krisis.
 */
void tick_game(Game *g, float dt)
{
    g->time += dt;
    for (int i = 0; i < MAX_FX; i++) g->effects[i].left = fmaxf(0, g->effects[i].left-dt);
    if (!can_manage(g)) return;

    bool combat = g->state == WAVE || g->state == CRISIS_WAVE;
    if (combat) {
        spawn_next(g, dt);
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (g->enemies[i].active) move_enemy(&g->enemies[i], dt);
        }
    }
    for (int i = 0; i < g->towerCount; i++) {
        Tower *t = &g->towers[i];
        t->recoil = fmaxf(0, t->recoil-dt);
        t->reload = fmaxf(0, t->reload-dt);
        if (combat && t->reload == 0) fire_tower(g, t);
    }

    if (g->state == CRISIS_GAP || g->state == CRISIS_WARNING) {
        g->phaseTime -= dt;
        if (g->phaseTime <= 0) {
            if (g->state == CRISIS_GAP) {
                g->state = CRISIS_WARNING;
                g->phaseTime = CRISIS_WARNING_SECONDS;
            } else {
                g->state = CRISIS_WAVE;
                int group = GetRandomValue(0, (int)(sizeof crisis_groups/sizeof crisis_groups[0])-1);
                queue_group(g, crisis_groups[group]);
            }
        }
        return;
    }
    if (!combat) return;

    bool active = false;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *z = &g->enemies[i];
        if (!z->active) continue;
        if (z->next < PATH_COUNT) {
            active = true;
            continue;
        }
        z->active = false;
        int damage = enemies[z->kind].leak;
        g->hp = g->hp > damage ? g->hp-damage : 0;
        g->events |= EV_LEAK;
        fx(g, LEAK, z->pos, z->pos, .65f, damage);
    }
    if (g->hp == 0) {
        g->state = LOST;
    } else if (!active && g->queueNext == g->queueLength) {
        if (g->state == WAVE && g->wave == g->crisisWave) {
            g->state = CRISIS_GAP;
            g->phaseTime = CRISIS_GAP_SECONDS;
        } else {
            finish_wave(g);
        }
    }
}
