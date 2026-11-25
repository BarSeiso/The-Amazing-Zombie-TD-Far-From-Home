#include "game.h"
#include <stdio.h>
#include <stdlib.h>

static const Vector2 positions[MAX_TOWERS] = {
    {109,142}, {164,212}, {223,228}, {231,177}, {298,185}, {333,188}
};
static const TowerKind formation[MAX_TOWERS] = {ARCHER, ARCHER, CANNON, SNIPER, CANNON, SNIPER};

typedef enum { MIXED, SWITCH_SNIPER, SWITCH_CANNON, ARCHERS_ONLY } Strategy;

// Pilih hasil undian lewat raylib agar empat kelompok bisa diuji pada setiap wave.
static unsigned seed_for_group(int group)
{
    for (unsigned seed=1; seed<1000; seed++) {
        SetRandomSeed(seed);
        if (GetRandomValue(0,3) == group) return seed;
    }
    exit(2);
}

static bool play_campaign(int crisisWave, int group, Strategy strategy)
{
    Game game;
    reset_game(&game);
    game.crisisWave = crisisWave;
    unsigned seed = seed_for_group(group);
    bool switched = false;
    const float step = 1.0f/60;
    for (int frame=0; frame<150000; frame++) {
        if (game.state == PREPARE) start_wave(&game);
        if (!switched && (strategy == SWITCH_SNIPER || strategy == SWITCH_CANNON) &&
            game.wave >= 9 && game.towerCount == MAX_TOWERS && game.money >= 125) {
            Vector2 pos = game.towers[0].pos;
            TowerKind replacement = strategy == SWITCH_SNIPER ? SNIPER : CANNON;
            if (!sell_tower(&game,0) || !buy_tower(&game,replacement,pos)) exit(2);
            switched = true;
        }
        int slot = game.towerCount;
        bool bought = false;
        if (slot < MAX_TOWERS) {
            TowerKind kind = strategy == ARCHERS_ONLY ? ARCHER : formation[slot];
            if (game.money >= tower_specs[kind].price) {
                bought = buy_tower(&game,kind,positions[slot]);
                if (!bought) exit(2);
            }
        }
        if (!bought && game.money >= 140) {
            for (int i=slot-1; i>=0; i--) if (upgrade_tower(&game,i)) break;
        }
        if (game.state == CRISIS_WARNING && game.phaseTime <= step) SetRandomSeed(seed);
        tick_game(&game,step);
        if (game.state == WON || game.state == LOST) return game.state == WON;
    }
    exit(2);
}

int main(void)
{
    int wins[4] = {0};
    for (int wave=11; wave<=WAVE_COUNT; wave++) {
        for (int group=0; group<4; group++) {
            for (int strategy=MIXED; strategy<=ARCHERS_ONLY; strategy++)
                wins[strategy] += play_campaign(wave,group,(Strategy)strategy);
        }
    }
    printf("Campuran %d/20, ganti Sniper %d/20, ganti Cannon %d/20, Archer %d/20\n",
           wins[MIXED],wins[SWITCH_SNIPER],wins[SWITCH_CANNON],wins[ARCHERS_ONLY]);
    return !(wins[MIXED] > 0 && wins[MIXED] < 20 && wins[SWITCH_SNIPER] == 20 &&
             wins[SWITCH_CANNON] == 20 && wins[ARCHERS_ONLY] == 0);
}

