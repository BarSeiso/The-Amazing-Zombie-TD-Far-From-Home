#ifndef GAME_H
#define GAME_H

#include "raylib.h"

/*
 * Konstanta utama simulasi permainan.
 * Seluruh entitas memakai array berukuran tetap (tanpa malloc/free di simulasi)
 * agar penggunaan memori deterministik dan mudah dipahami pada bahasa C dasar.
 */
enum {
    WIDTH = 640, HEIGHT = 360, WAVE_COUNT = 15, MAX_HP = 20, MAX_LEVEL = 3,
    MAX_TOWERS = 6, MAX_ENEMIES = 40, MAX_FX = 96, PATH_COUNT = 7, MAX_QUEUE = 64
};

#define TOWER_RADIUS 10.0f
#define ROAD_RADIUS 13.0f
#define CRISIS_GAP_SECONDS 2.5f
#define CRISIS_WARNING_SECONDS 4.0f

extern const Rectangle battlefield;
extern const Vector2 route[PATH_COUNT];

typedef enum { ARCHER, CANNON, SNIPER, TOWER_KIND_COUNT } TowerKind;
typedef enum { WALKER, RUNNER, MUTANT, BOSS, ARMORED, BREACHER, ENEMY_KIND_COUNT } EnemyKind;
typedef enum { PREPARE, WAVE, CRISIS_GAP, CRISIS_WARNING, CRISIS_WAVE, WON, LOST } RunState;
typedef enum { DUST, SHOT, EXPLOSION, REWARD, LEAK, UPGRADE } FxKind;

/* Mode prioritas bidikan peleton (default 0 = TARGET_FIRST agar perilaku awal terjaga). */
typedef enum { TARGET_FIRST, TARGET_STRONG, TARGET_CLOSE, TARGET_MODE_COUNT } TargetMode;

/* Bitmask kejadian suara per frame agar game.c tetap bebas dari ketergantungan audio. */
typedef enum {
    EV_BOW     = 1 << 0,
    EV_CANNON  = 1 << 1,
    EV_SNIPER  = 1 << 2,
    EV_PLACE   = 1 << 3,
    EV_UPGRADE = 1 << 4,
    EV_SELL    = 1 << 5,
    EV_LEAK    = 1 << 6
} GameEvent;

typedef struct {
    int damage, range, upgradeCost;
    float reload;
} TowerStats;

typedef struct {
    const char *name;
    int price, splash;
    TowerStats levels[MAX_LEVEL];
} TowerSpec;

extern const TowerSpec tower_specs[TOWER_KIND_COUNT];

typedef struct {
    Vector2 pos;
    float reload, recoil;
    int facing, level, invested, kills;
    TowerKind kind;
    TargetMode targetMode;
} Tower;

typedef struct {
    Vector2 pos;
    float walked, speed, hit, stride;
    int next, hp, maxHp;
    EnemyKind kind;
    bool active;
} Enemy;

typedef struct {
    FxKind kind;
    Vector2 from, to;
    float left, duration;
    int amount;
} Effect;

typedef struct {
    RunState state;
    int wave, money, hp, towerCount, lastBonus, lastHeal, crisisWave;
    int totalKills, totalEarned;
    bool crisisCleared;
    unsigned int events;
    int queueLength, queueNext;
    float spawnIn, phaseTime, time;
    EnemyKind queue[MAX_QUEUE];
    Tower towers[MAX_TOWERS];
    Enemy enemies[MAX_ENEMIES];
    Effect effects[MAX_FX];
    int nextEffect;
} Game;

void reset_game(Game *game);
void preview_wave_counts(int wave, int counts[ENEMY_KIND_COUNT]);
int remaining_enemies(const Game *game);
bool start_wave(Game *game);
bool can_manage(const Game *game);
bool valid_ground(const Game *game, Vector2 point);
bool buy_tower(Game *game, TowerKind kind, Vector2 point);
int tower_at(const Game *game, Vector2 point);
int sale_value(const Tower *tower);
bool upgrade_tower(Game *game, int index);
bool sell_tower(Game *game, int index);
void cycle_tower_target(Game *game, int index);
void tick_game(Game *game, float dt);

#endif
