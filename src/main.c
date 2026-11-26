#include "art.h"
#include "audio.h"
#include "story.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * State tingkat aplikasi (antarmuka, menu, pengaturan lokal, dan kontrol waktu)
 * yang membungkus simulasi inti Game dan sekuens sinematik Story.
 */
typedef struct {
    Game game;
    Story story;
    bool menu, paused, help, confirmMenu, storyJump, criticalHeard;
    int speedIndex;
    bool settings, reducedMotion, settingsSaveFailed;
    int selected, placing, musicVolume, sfxVolume, highScore;
    bool newRecord;
    float noticeTime;
    const char *notice;
} App;

static const int speeds[] = {1, 2, 3, 5};
static const char *const target_names[TARGET_MODE_COUNT] = {
    "DEPAN", "KUAT", "DEKAT"
};

static bool fighting(RunState state)
{
    return state == WAVE || state == CRISIS_WAVE;
}

/* Tata letak tombol pada kanvas virtual 640x360 piksel. */
static const Rectangle playButton          = {  28, 240, 196, 31 };
static const Rectangle settingsButton      = {  28, 278, 196, 24 };
static const Rectangle exitButton          = {  28, 308, 196, 22 };
static const Rectangle helpButton          = { 496,  19,  52, 24 };
static const Rectangle pauseButton         = { 554,  19,  72, 24 };
static const Rectangle targetButton        = { 500, 231, 124, 15 };
static const Rectangle upgradeButton       = { 500, 248, 124, 16 };
static const Rectangle sellButton          = { 500, 266, 124, 16 };
static const Rectangle startButton         = { 500, 285, 124, 31 };
static const Rectangle pauseResumeButton   = { 225, 174, 190, 26 };
static const Rectangle pauseSettingsButton = { 225, 206, 190, 24 };
static const Rectangle pauseMenuButton     = { 225, 236, 190, 24 };
static const Rectangle resultRetryButton   = { 195, 226, 250, 27 };
static const Rectangle resultMenuButton    = { 195, 258, 250, 23 };
static const Rectangle motionButton        = { 198,  98, 244, 24 };
static const Rectangle volumeDown          = { 198, 152,  44, 24 };
static const Rectangle volumeUp            = { 398, 152,  44, 24 };
static const Rectangle sfxDown             = { 198, 216,  44, 24 };
static const Rectangle sfxUp               = { 398, 216,  44, 24 };
static const Rectangle settingsBackButton  = { 198, 280, 244, 26 };
static const Rectangle nextSceneButton     = { 492, 282, 114, 22 };
static const Rectangle skipButton          = { 492, 308, 114, 22 };
static const Rectangle helpCloseButton     = { 456,  45, 116, 22 };

/* Menentukan lokasi file pengaturan lokal (%LOCALAPPDATA%/FarFromHome/settings.txt pada Windows). */
static bool settings_path(char path[1024], bool createDir)
{
#ifdef _WIN32
    const char *base = getenv("LOCALAPPDATA");
#else
    const char *base = getenv("XDG_CONFIG_HOME");
#endif
    if (!base || !*base) return false;
    char folder[1024];
    int written = snprintf(folder, sizeof folder, "%s/FarFromHome", base);
    if (written < 0 || written >= (int)sizeof folder) return false;
    written = snprintf(path, 1024, "%s/settings.txt", folder);
    if (written < 0 || written >= 1024) return false;
    return !createDir || DirectoryExists(folder) || MakeDirectory(folder) == 0;
}

static void load_settings(App *app)
{
    app->musicVolume = 60;
    app->sfxVolume = 70;
    app->highScore = 0;
    char path[1024];
    if (!settings_path(path, false) || !FileExists(path)) return;
    char *value = LoadFileText(path);
    if (!value) return;
    int motion = 0, volume = 60, sfx = 70, best = 0;
    int fields = sscanf(value, "%d%d%d%d", &motion, &volume, &sfx, &best);
    app->reducedMotion = fields >= 1 && motion == 1;
    if (fields >= 2 && volume >= 0 && volume <= 100 && volume%10 == 0) app->musicVolume = volume;
    if (fields >= 3 && sfx >= 0 && sfx <= 100 && sfx%10 == 0) app->sfxVolume = sfx;
    if (fields >= 4 && best >= 0) app->highScore = best;
    UnloadFileText(value);
}

static bool save_settings(const App *app)
{
    char path[1024];
    char value[64];
    snprintf(value, sizeof value, "%d\n%d\n%d\n%d\n",
             app->reducedMotion, app->musicVolume, app->sfxVolume, app->highScore);
    return settings_path(path, true) && SaveFileText(path, value);
}

static int campaign_score(const Game *g)
{
    int clearedWaves = g->state == WON ? WAVE_COUNT : (g->wave - 1);
    return g->totalKills * 10 + clearedWaves * 100 + g->hp * 50 + (g->crisisCleared ? 300 : 0);
}

static const char *commander_rank(const Game *g)
{
    if (g->state != WON) return "GUGUR";
    if (g->hp >= 18) return "S";
    if (g->hp >= 13) return "A";
    if (g->hp >= 7)  return "B";
    return "C";
}

static bool over(Vector2 mouse, Rectangle rect)
{
    return CheckCollisionPointRec(mouse, rect);
}

static Rectangle shop_card(int kind)
{
    return (Rectangle){ 500, 86+kind*31, 124, 28 };
}

static void button(Rectangle r, const char *text, bool enabled, bool primary, Vector2 mouse)
{
    bool hover = enabled && over(mouse, r);
    bool held = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Color fill = !enabled ? INK : primary ? (hover ? (Color){ 251, 207, 116, 255 } : AMBER) :
                 hover ? (Color){ 64, 79, 60, 255 } : METAL;
    int x = (int)r.x, y = (int)r.y, w = (int)r.width, h = (int)r.height;
    DrawRectangle(x, y+2, w, h, INK);
    DrawRectangleRec(r, fill);
    DrawRectangleLines(x, y, w, h, hover ? CREAM : RIM);
    DrawRectangle(x+1, y+1, w-2, 1, Fade(CREAM, 0.32f));
    label(text, x+(w-MeasureText(text, 10))/2, y+(h-10)/2+held, 10,
          !enabled ? MUTED : primary ? INK : CREAM);
}

static void new_run(App *app)
{
    bool reduced = app->reducedMotion;
    int musicVolume = app->musicVolume;
    int sfxVolume = app->sfxVolume;
    int highScore = app->highScore;
    bool saveFailed = app->settingsSaveFailed;
    *app = (App){
        .selected = -1, .placing = -1,
        .reducedMotion = reduced,
        .musicVolume = musicVolume,
        .sfxVolume = sfxVolume,
        .highScore = highScore,
        .settingsSaveFailed = saveFailed
    };
    reset_game(&app->game);
}

static void notice(App *app, const char *text)
{
    app->notice = text;
    app->noticeTime = 2.2f;
}

/* Memproses seluruh input mouse & keyboard sesuai layar yang sedang aktif. */
static bool update_input(App *app, GameAudio *audio, Vector2 mouse)
{
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (app->settings) {
        if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(mouse, settingsBackButton))) {
            app->settings = false;
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
        } else if (IsKeyPressed(KEY_ENTER) || (clicked && over(mouse, motionButton))) {
            app->reducedMotion = !app->reducedMotion;
            app->settingsSaveFailed = !save_settings(app);
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
        } else {
            int oldMusic = app->musicVolume;
            int oldSfx = app->sfxVolume;
            if ((IsKeyPressed(KEY_LEFT) || (clicked && over(mouse, volumeDown))) && oldMusic > 0)
                app->musicVolume -= 10;
            if ((IsKeyPressed(KEY_RIGHT) || (clicked && over(mouse, volumeUp))) && oldMusic < 100)
                app->musicVolume += 10;
            if ((IsKeyPressed(KEY_MINUS) || (clicked && over(mouse, sfxDown))) && oldSfx > 0)
                app->sfxVolume -= 10;
            if ((IsKeyPressed(KEY_EQUAL) || (clicked && over(mouse, sfxUp))) && oldSfx < 100)
                app->sfxVolume += 10;
            if (oldMusic != app->musicVolume || oldSfx != app->sfxVolume) {
                app->settingsSaveFailed = !save_settings(app);
                play_sfx(audio, SFX_CLICK, app->sfxVolume);
            }
        }
        return false;
    }
    if (app->help) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_H) || (clicked && over(mouse, helpCloseButton))) {
            app->help = false;
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
        }
        return false;
    }
    if (app->story.playing) {
        if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(mouse, skipButton))) {
            app->story.playing = false;
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
        } else if (IsKeyPressed(KEY_ENTER) || (clicked && over(mouse, nextSceneButton))) {
            advance_story(&app->story);
            app->storyJump = app->story.playing;
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
        }
        return false;
    }
    if (app->menu) {
        if (IsKeyPressed(KEY_ENTER) || (clicked && over(mouse, playButton))) {
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
            new_run(app);
            begin_story(&app->story);
            return false;
        }
        if (clicked && over(mouse, settingsButton)) {
            app->settings = true;
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
        }
        return clicked && over(mouse, exitButton);
    }

    Game *g = &app->game;
    if (g->state == WON || g->state == LOST) {
        if (IsKeyPressed(KEY_ENTER) || (clicked && over(mouse, resultRetryButton))) {
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
            new_run(app);
        } else if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(mouse, resultMenuButton))) {
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
            app->menu = true;
        }
        return false;
    }

    if (!app->paused && (IsKeyPressed(KEY_H) || (clicked && over(mouse, helpButton)))) {
        app->help = true;
        play_sfx(audio, SFX_CLICK, app->sfxVolume);
        return false;
    }
    if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(mouse, pauseButton))) {
        app->paused = !app->paused;
        app->confirmMenu = false;
        play_sfx(audio, SFX_CLICK, app->sfxVolume);
        return false;
    }
    if (app->paused) {
        if (clicked && over(mouse, pauseResumeButton)) {
            app->paused = false;
            app->confirmMenu = false;
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
        } else if (clicked && over(mouse, pauseSettingsButton)) {
            app->settings = true;
            app->confirmMenu = false;
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
        } else if (clicked && over(mouse, pauseMenuButton)) {
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
            if (!app->confirmMenu) {
                app->confirmMenu = true;
            } else {
                app->paused = false;
                app->confirmMenu = false;
                app->menu = true;
            }
        }
        return false;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        app->placing = -1;
        app->selected = -1;
        return false;
    }
    for (int kind = 0; kind < TOWER_KIND_COUNT; kind++) {
        if (IsKeyPressed(KEY_ONE+kind) || (clicked && over(mouse, shop_card(kind)))) {
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
            if (g->towerCount >= MAX_TOWERS) {
                notice(app, "Slot 6/6 penuh. Jual 1 peleton jika ingin mengganti.");
                return false;
            }
            app->placing = app->placing == kind ? -1 : kind;
            app->selected = -1;
            app->noticeTime = 0;
            return false;
        }
    }
    if (app->selected >= 0) {
        if (IsKeyPressed(KEY_TAB) || (clicked && over(mouse, targetButton))) {
            cycle_tower_target(g, app->selected);
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
            notice(app, TextFormat("Prioritas bidik: %s.", target_names[g->towers[app->selected].targetMode]));
            return false;
        }
        if (IsKeyPressed(KEY_U) || (clicked && over(mouse, upgradeButton))) {
            const Tower *t = &g->towers[app->selected];
            int price = tower_specs[t->kind].levels[t->level].upgradeCost;
            if (upgrade_tower(g, app->selected)) notice(app, "Upgrade berhasil.");
            else notice(app, price ? "Dana tidak cukup." : "Peleton sudah level maksimum.");
            return false;
        }
        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE) ||
            (clicked && over(mouse, sellButton))) {
            if (sell_tower(g, app->selected)) {
                app->selected = -1;
                notice(app, "Peleton dijual.");
            }
            return false;
        }
    }
    if (fighting(g->state) && (IsKeyPressed(KEY_F) || (clicked && over(mouse, startButton)))) {
        app->speedIndex = (app->speedIndex+1)%4;
        play_sfx(audio, SFX_CLICK, app->sfxVolume);
        return false;
    }
    if (IsKeyPressed(KEY_SPACE) || (clicked && over(mouse, startButton))) {
        if (start_wave(g)) {
            app->noticeTime = 0;
            play_sfx(audio, SFX_CLICK, app->sfxVolume);
        }
        return false;
    }
    if (clicked && over(mouse, battlefield)) {
        if (app->placing >= 0) {
            if (buy_tower(g, (TowerKind)app->placing, mouse)) {
                app->placing = -1;
                app->selected = g->towerCount-1;
                notice(app, "Peleton siap tempur.");
            } else if (g->money < tower_specs[app->placing].price) {
                notice(app, "Dana tidak cukup.");
            } else if (g->towerCount == MAX_TOWERS) {
                notice(app, "Kapasitas peleton penuh.");
            } else {
                notice(app, "Pilih posisi di luar jalur dan peleton lain.");
            }
        } else {
            app->selected = tower_at(g, mouse);
            if (app->selected >= 0) play_sfx(audio, SFX_CLICK, app->sfxVolume);
        }
    }
    return false;
}

static void draw_menu(const App *app, Vector2 mouse)
{
    draw_map(&app->game, app->reducedMotion);
    DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(INK, 0.17f));
    for (int y = 0; y < HEIGHT; y += 4) DrawRectangle(0, y, WIDTH, 1, Fade(INK, 0.11f));
    panel((Rectangle){ 14, 36, 265, 304 }, METAL);
    DrawRectangle(15, 40, 3, 296, AMBER);
    label("PERTAHANAN SHELTER", 28, 55, 10, AMBER);
    label("THE AMAZING", 28, 79, 19, CREAM);
    label("ZOMBIE TD", 26, 102, 32, CREAM);
    label("FAR FROM HOME", 29, 140, 14, AMBER);
    DrawRectangle(28, 165, 233, 1, RIM);
    DrawRectangle(28, 178, 50, 50, INK);
    draw_portrait(29, 179);
    label("OPERATOR SHELTER", 86, 182, 10, AMBER);
    label("Siapkan peleton.", 86, 198, 10, CREAM);
    label("Jaga gerbang tetap berdiri.", 86, 211, 10, CREAM);
    button(playButton, "MULAI OPERASI  [ENTER]", true, true, mouse);
    button(settingsButton, "PENGATURAN", true, false, mouse);
    button(exitButton, "KELUAR", true, false, mouse);

    panel((Rectangle){ 486, 60, 136, 96 }, METAL);
    label("SEKTOR 07", 497, 70, 10, AMBER);
    DrawRectangle(497, 85, 114, 1, RIM);
    label(TextFormat("Tahan %d wave", WAVE_COUNT), 497, 94, 10, CREAM);
    label("Lindungi shelter.", 497, 109, 10, CREAM);
    label(app->highScore > 0 ? TextFormat("Skor Terbaik %d", app->highScore) : "Belum ada rekor",
          497, 130, 10, app->highScore > 0 ? MINT : MUTED);

    panel((Rectangle){ 438, 289, 184, 28 }, METAL);
    label("POS KOMANDO  /  2042", 451, 298, 10, CREAM);
}

/* Menampilkan intelijen gelombang berikutnya atau progres gelombang aktif pada panel kanan. */
static void draw_wave_intel(const Game *g)
{
    static const char *const names[ENEMY_KIND_COUNT] = {
        "Pejalan", "Pelari", "Mutan", "Boss", "Berzirah", "Pendobrak"
    };
    if (g->state == PREPARE) {
        label(TextFormat("INTEL WAVE %02d", g->wave), 503, 185, 10, AMBER);
        int counts[ENEMY_KIND_COUNT];
        preview_wave_counts(g->wave, counts);
        int y = 199;
        for (int k = 0; k < ENEMY_KIND_COUNT; k++) {
            if (counts[k] <= 0) continue;
            Color c = k == BREACHER ? ALERT_RED : k == ARMORED ? AMBER : CREAM;
            label(names[k], 503, y, 10, c);
            const char *qty = TextFormat("x%d", counts[k]);
            label(qty, 622 - MeasureText(qty, 10), y, 10, c);
            y += 12;
        }
        if (g->wave >= WAVE_COUNT - 4 && !g->crisisCleared) {
            label("Status: Rawan Krisis", 503, 265, 10, ALERT_RED);
        } else if (g->wave == 6) {
            label("Tip: Beli Sniper", 503, 265, 10, MINT);
        } else if (g->wave == 9) {
            label("Tip: Jaga belokan akhir", 503, 265, 10, MINT);
        }
        return;
    }

    bool crisis = g->state == CRISIS_WARNING || g->state == CRISIS_WAVE;
    label(crisis ? "SERBUAN KRISIS" : g->state == CRISIS_GAP ? "RADAR PANTAU" : "STATUS TEMPUR",
          503, 186, 10, crisis ? ALERT_RED : AMBER);
    if (fighting(g->state)) {
        int rem = remaining_enemies(g);
        int total = g->queueLength > 0 ? g->queueLength : 1;
        float done = 1.0f - (float)rem / total;
        if (done < 0.0f) done = 0.0f;
        label(TextFormat("Tersisa   %d / %d", rem, total), 503, 202, 10, CREAM);
        DrawRectangle(503, 217, 118, 6, INK);
        DrawRectangleLines(503, 217, 118, 6, RIM);
        DrawRectangle(504, 218, (int)(116 * done), 4, crisis ? ALERT_RED : MINT);
    } else if (g->state == CRISIS_GAP) {
        label("Getaran terdeteksi...", 503, 204, 10, CREAM);
    } else if (g->state == CRISIS_WARNING) {
        label("Sirene darurat aktif!", 503, 204, 10, ALERT_RED);
    } else {
        label(g->state == WON ? "Operasi tuntas." : "Gerbang jebol.", 503, 204, 10, MUTED);
    }
    label("Klik unit: kelola", 503, 236, 10, MUTED);
    label("[H] Intelijen musuh", 503, 250, 10, MUTED);
    label(TextFormat("Tewas: %d", g->totalKills), 503, 265, 10, MUTED);
}

static void draw_hud(const App *app, Vector2 mouse)
{
    const Game *g = &app->game;
    panel((Rectangle){ 12, 12, 614, 36 }, METAL);
    label("SHELTER INDONESIA", 22, 19, 10, CREAM);
    label("SEKTOR 07", 22, 32, 10, MUTED);
    DrawRectangle(150, 17, 1, 25, RIM);
    label("WAVE", 160, 19, 10, MUTED);
    label(TextFormat("%02d / %02d", g->wave, WAVE_COUNT), 160, 31, 10, CREAM);
    if (fighting(g->state) && g->queueLength > 0) {
        int rem = remaining_enemies(g);
        float ratio = 1.0f - (float)rem / g->queueLength;
        DrawRectangle(160, 43, 56, 3, INK);
        DrawRectangle(160, 43, (int)(56 * fmaxf(0.0f, ratio)), 3,
                      g->state == CRISIS_WAVE ? ALERT_RED : AMBER);
    }
    label("DANA", 237, 19, 10, MUTED);
    label(TextFormat("%03d", g->money), 237, 31, 10, AMBER);
    label(TextFormat("HP SHELTER  %02d/%02d", g->hp, MAX_HP), 322, 19, 10, CREAM);
    for (int i = 0; i < MAX_HP; i++) {
        DrawRectangle(322+i*8, 35, 6, 4, i < g->hp ? (g->hp <= 5 ? ALERT_RED : MINT) : INK);
    }
    button(helpButton, "INTEL", true, false, mouse);
    button(pauseButton, "JEDA", true, false, mouse);

    panel((Rectangle){ 494, 60, 136, 266 }, METAL);
    bool full = g->towerCount >= MAX_TOWERS;
    label(full ? "PELETON  6/6 PENUH" : TextFormat("PELETON   %d/%d", g->towerCount, MAX_TOWERS),
          502, 71, 10, full ? AMBER : CREAM);
    for (int kind = 0; kind < TOWER_KIND_COUNT; kind++) {
        Rectangle card = shop_card(kind);
        int y = (int)card.y;
        bool active = app->placing == kind;
        bool hover = !full && over(mouse, card);
        DrawRectangleRec(card, active ? (Color){ 82, 79, 46, 255 } :
            hover ? (Color){ 60, 73, 54, 255 } : (Color){ 47, 58, 46, 255 });
        DrawRectangleLines(500, y, 124, 28, active ? AMBER : RIM);
        if (active || (hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT))) DrawRectangle(501, y+1, 2, 26, AMBER);
        DrawRectangle(506, y+3, 29, 22, INK);
        Tower icon = { .kind = (TowerKind)kind, .pos = { 520, y+21 }, .facing = 1 };
        draw_tower(&icon, app->reducedMotion ? 0 : g->time, WHITE);
        const TowerSpec *spec = &tower_specs[kind];
        label(spec->name, 540, y+4, 10, full ? MUTED : CREAM);
        /* Harga selalu ditampilkan meski slot penuh agar pemain tahu dana penggantian. */
        Color priceColor = full ? MUTED : (g->money >= spec->price ? AMBER : ALERT_RED);
        label(TextFormat("%d dana", spec->price), 540, y+15, 10, priceColor);
        label(TextFormat("[%d]", kind+1), 602, y+15, 10, MUTED);
    }
    DrawRectangle(501, 180, 122, 1, RIM);

    if (app->selected >= 0) {
        const Tower *t = &g->towers[app->selected];
        const TowerSpec *spec = &tower_specs[t->kind];
        const TowerStats *stats = &spec->levels[t->level];
        const TowerStats *next = t->level + 1 < MAX_LEVEL ? &spec->levels[t->level + 1] : NULL;
        label(TextFormat("%s LV%d", spec->name, t->level+1), 503, 184, 10, AMBER);
        const char *killsText = TextFormat("%d kill", t->kills);
        label(killsText, 622 - MeasureText(killsText, 10), 184, 10, MUTED);

        if (next && over(mouse, upgradeButton)) {
            label(TextFormat("Dmg   %d -> %d", stats->damage, next->damage), 503, 196, 10, MINT);
            label(TextFormat("Jarak %d -> %d", stats->range, next->range), 503, 207, 10, MINT);
            label(TextFormat("Laju  %.2f->%.2fs", stats->reload, next->reload), 503, 218, 10, MINT);
        } else if (next) {
            label(TextFormat("Dmg   %d (+%d)", stats->damage, next->damage - stats->damage), 503, 196, 10, CREAM);
            label(TextFormat("Jarak %d (+%d)", stats->range, next->range - stats->range), 503, 207, 10, CREAM);
            label(TextFormat("Laju  %.2fs", stats->reload), 503, 218, 10, CREAM);
        } else {
            label(TextFormat("Damage    %d", stats->damage), 503, 196, 10, CREAM);
            label(TextFormat("Jarak     %d", stats->range), 503, 207, 10, CREAM);
            label(TextFormat("Reload    %.2fs", stats->reload), 503, 218, 10, CREAM);
        }
        button(targetButton, TextFormat("BIDIK: %s [TAB]", target_names[t->targetMode]),
               can_manage(g), false, mouse);
        int price = stats->upgradeCost;
        button(upgradeButton, price ? TextFormat("UPGRADE %d [U]", price) : "LEVEL MAKS",
               price > 0 && g->money >= price && can_manage(g), false, mouse);
        button(sellButton, TextFormat("JUAL +%d [X]", sale_value(t)), can_manage(g), false, mouse);
    } else if (app->placing >= 0) {
        const TowerSpec *spec = &tower_specs[app->placing];
        const TowerStats *stats = &spec->levels[0];
        label(TextFormat("%s   LV 1", spec->name), 503, 186, 10, AMBER);
        label(TextFormat("Damage    %d", stats->damage), 503, 200, 10, CREAM);
        label(TextFormat("Jarak     %d", stats->range), 503, 213, 10, CREAM);
        label(TextFormat("Reload    %.2fs", stats->reload), 503, 226, 10, CREAM);
        if (app->placing == SNIPER) {
            label("Tembus rompi zirah", 503, 244, 10, MINT);
        } else if (spec->splash) {
            label(TextFormat("Ledakan area %dpx", spec->splash), 503, 244, 10, MINT);
        } else {
            label("Tembakan cepat", 503, 244, 10, MINT);
        }
        label("Klik kanan: Batal", 503, 260, 10, MUTED);
    } else {
        draw_wave_intel(g);
    }

    const char *action = fighting(g->state) ? TextFormat("LAJU %dx   [F]", speeds[app->speedIndex]) :
                         g->state == PREPARE ? "MULAI WAVE" : "SIAGA";
    button(startButton, action, g->state == PREPARE || fighting(g->state), true, mouse);

    DrawRectangle(13, 335, 613, 1, RIM);
    if (app->noticeTime > 0) {
        label(app->notice, 15, 343, 10, CREAM);
    } else if (g->state == CRISIS_WARNING) {
        label("PERINGATAN KRISIS: Gerombolan mutan mendekat! Perkuat peleton!", 15, 343, 10, ALERT_RED);
    } else if (g->state == CRISIS_GAP) {
        label("Operator: Tunggu... radar mendeteksi getaran baru di luar pagar.", 15, 343, 10, AMBER);
    } else if (app->placing >= 0) {
        label(TextFormat("Klik di peta untuk menempatkan %s.", tower_specs[app->placing].name), 15, 343, 10, AMBER);
    } else if (g->state == PREPARE && g->wave == 6) {
        label("Operator: Ada yang pakai rompi. Tempatkan Sniper.", 15, 343, 10, AMBER);
    } else if (g->state == PREPARE && g->wave == 9) {
        label("Pendobrak berlari setelah belokan terakhir.", 15, 343, 10, AMBER);
    } else if (g->lastBonus) {
        label(TextFormat("Bonus wave +%d dana%s", g->lastBonus, g->lastHeal ? "  /  Shelter +1 HP" : ""),
              15, 343, 10, MINT);
    } else if (g->state == PREPARE && g->wave == 1) {
        label("Operator: Tempatkan peleton, lalu tekan Spasi.", 15, 343, 10, CREAM);
    } else if (g->state == PREPARE) {
        label("SPASI Mulai wave   /   [H] Panduan   /   Klik kanan Batal", 15, 343, 10, MUTED);
    } else {
        label("1-3 Pilih peleton   /   [H] Panduan   /   Klik kanan Batal", 15, 343, 10, MUTED);
    }
    label("F11 Layar penuh", 626-MeasureText("F11 Layar penuh",10), 343, 10, MUTED);
}

static void draw_placement(const App *app, Vector2 mouse)
{
    BeginScissorMode(12, 60, 474, 266);
    if (app->placing >= 0 && over(mouse, battlefield)) {
        const TowerSpec *spec = &tower_specs[app->placing];
        bool valid = valid_ground(&app->game, mouse) && app->game.money >= spec->price &&
            app->game.towerCount < MAX_TOWERS;
        Color color = valid ? MINT : ALERT_RED;
        DrawCircleV(mouse, spec->levels[0].range, Fade(color, 0.13f));
        DrawCircleLines((int)mouse.x, (int)mouse.y, spec->levels[0].range, color);
        Tower ghost = { .pos = mouse, .facing = 1, .kind = (TowerKind)app->placing };
        draw_tower(&ghost, 0, Fade(WHITE, 0.64f));
        int x = (int)mouse.x, y = (int)mouse.y;
        DrawRectangle(x+9, y-22, 12, 12, INK);
        if (valid) {
            DrawLine(x+11, y-16, x+14, y-13, color);
            DrawLine(x+14, y-13, x+19, y-19, color);
        } else {
            DrawLine(x+12, y-19, x+18, y-13, color);
            DrawLine(x+12, y-13, x+18, y-19, color);
        }
    } else if (app->selected >= 0) {
        const Tower *t = &app->game.towers[app->selected];
        const TowerSpec *spec = &tower_specs[t->kind];
        Vector2 p = t->pos;
        DrawCircleLines((int)p.x, (int)p.y, spec->levels[t->level].range, Fade(AMBER, 0.63f));
        if (t->level + 1 < MAX_LEVEL && over(mouse, upgradeButton)) {
            float nextRange = (float)spec->levels[t->level + 1].range;
            DrawCircleV(p, nextRange, Fade(MINT, 0.08f));
            DrawCircleLines((int)p.x, (int)p.y, nextRange, Fade(MINT, 0.78f));
        }
        DrawEllipseLines((int)p.x, (int)p.y, 10, 4, AMBER);
    }
    EndScissorMode();
}

static void draw_overlay(const App *app, Vector2 mouse)
{
    DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(INK, 0.78f));
    if (app->paused) {
        panel((Rectangle){ 209, 108, 222, 168 }, METAL);
        label("OPERASI DIJEDA", 225, 124, 14, CREAM);
        label(app->confirmMenu ? "Progres sesi ini akan ditinggalkan." : "ESC untuk melanjutkan.",
              225, 147, 10, app->confirmMenu ? ALERT_RED : MUTED);
        button(pauseResumeButton, "LANJUTKAN", true, true, mouse);
        button(pauseSettingsButton, "PENGATURAN", true, false, mouse);
        button(pauseMenuButton, app->confirmMenu ? "YAKIN KE MENU?" : "MENU UTAMA",
               true, app->confirmMenu, mouse);
        return;
    }

    /* Kartu Laporan Operasi Akhir saat menang (WON) maupun kalah (LOST). */
    const Game *g = &app->game;
    bool won = g->state == WON;
    panel((Rectangle){ 175, 66, 290, 228 }, METAL);
    label(won ? "SHELTER BERTAHAN" : "SHELTER JATUH", 195, 82, 15, won ? MINT : ALERT_RED);
    if (app->newRecord) {
        label("REKOR BARU!", 445 - MeasureText("REKOR BARU!", 10), 85, 10, AMBER);
    }
    DrawRectangle(195, 104, 250, 1, RIM);

    int score = campaign_score(g);
    label("Peringkat Komandan", 195, 114, 10, MUTED);
    const char *rankStr = TextFormat("%s  (%d poin)", commander_rank(g), score);
    label(rankStr, 445 - MeasureText(rankStr, 10), 114, 10, AMBER);

    label("Gelombang Tuntas", 195, 132, 10, MUTED);
    const char *waveStr = won ? TextFormat("%d / %d (Tuntas)", WAVE_COUNT, WAVE_COUNT) :
                                TextFormat("Gugur di Wave %02d", g->wave);
    label(waveStr, 445 - MeasureText(waveStr, 10), 132, 10, CREAM);

    label("Zombie Ditewaskan", 195, 150, 10, MUTED);
    const char *killStr = TextFormat("%d ekor", g->totalKills);
    label(killStr, 445 - MeasureText(killStr, 10), 150, 10, CREAM);

    label("Sisa HP Shelter", 195, 168, 10, MUTED);
    const char *hpStr = TextFormat("%02d / %02d HP", g->hp, MAX_HP);
    label(hpStr, 445 - MeasureText(hpStr, 10), 168, 10, won ? MINT : ALERT_RED);

    label("Status Mode Krisis", 195, 186, 10, MUTED);
    const char *crisisStr = g->crisisCleared ? TextFormat("Ditahan (W%02d)", g->crisisWave) :
                            TextFormat("Wave %02d", g->crisisWave);
    label(crisisStr, 445 - MeasureText(crisisStr, 10), 186, 10, g->crisisCleared ? MINT : MUTED);

    label("Total Dana Operasi", 195, 204, 10, MUTED);
    const char *earnStr = TextFormat("%d dana", g->totalEarned);
    label(earnStr, 445 - MeasureText(earnStr, 10), 204, 10, AMBER);

    button(resultRetryButton, won ? "OPERASI BARU  [ENTER]" : "COBA LAGI  [ENTER]", true, true, mouse);
    button(resultMenuButton, "MENU UTAMA  [ESC]", true, false, mouse);
}

/* Panel Panduan Lapangan & Ensiklopedia 6 Jenis Zombie ([H] / tombol INTEL). */
static void draw_help(Vector2 mouse)
{
    static const struct {
        EnemyKind kind;
        const char *title;
        const char *stats;
        const char *note;
    } entries[6] = {
        { WALKER,   "Pejalan (Walker)",      "HP 31 (+5/w) | Bocor -2",  "Zombie standar bergerombol." },
        { RUNNER,   "Pelari (Runner)",       "HP 23 (+4/w) | Bocor -2",  "Cepat, cegat dengan Archer." },
        { MUTANT,   "Mutan Berat",           "HP 110 (+14/w) | Bocor -3","Lambat namun ber-HP tebal." },
        { ARMORED,  "Berzirah (Wave 6+)",    "HP 110 | Tahan 40% dmg",   "Hanya Sniper memberi 100% dmg!" },
        { BREACHER, "Pendobrak (Wave 9+)",   "HP 165 | Bocor -5 HP",     "Lari cepat di belokan terakhir!" },
        { BOSS,     "Boss Krisis (W11-15)",  "HP 550+ (+25%) | Bocor -5","Muncul saat sirene Mode Krisis." }
    };

    DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(INK, 0.86f));
    panel((Rectangle){ 52, 34, 536, 292 }, METAL);
    label("PANDUAN KOMANDO & INTELIJEN MUSUH", 68, 48, 13, AMBER);
    button(helpCloseButton, "TUTUP [H/ESC]", true, false, mouse);
    DrawRectangle(68, 72, 504, 1, RIM);

    for (int i = 0; i < 6; i++) {
        int col = i % 2, row = i / 2;
        int cx = 68 + col * 256;
        int cy = 82 + row * 64;
        DrawRectangle(cx, cy, 246, 56, (Color){ 31, 40, 34, 255 });
        DrawRectangleLines(cx, cy, 246, 56, RIM);
        DrawRectangle(cx + 5, cy + 5, 46, 46, INK);
        draw_enemy_icon(entries[i].kind, cx + 28, cy + 45);
        Color titleColor = (entries[i].kind == ARMORED) ? AMBER :
                           (entries[i].kind == BREACHER || entries[i].kind == BOSS) ? ALERT_RED : CREAM;
        label(entries[i].title, cx + 57, cy + 7, 10, titleColor);
        label(entries[i].stats, cx + 57, cy + 22, 10, MUTED);
        label(entries[i].note,  cx + 57, cy + 37, 10, CREAM);
    }

    DrawRectangle(68, 278, 504, 1, RIM);
    label("KONTROL:  [1-3] Beli   [U] Upgrade   [X] Jual (70%)   [TAB] Mode Bidik   [F] Laju   [SPASI] Wave",
          68, 288, 10, MINT);
    label("TIPS: Jual 1 Archer di wave akhir bila slot penuh untuk menambah Sniper/Cannon penahan Krisis.",
          68, 304, 10, MUTED);
}

static void draw_settings(const App *app, Vector2 mouse)
{
    DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(INK, .84f));
    panel((Rectangle){ 179, 44, 282, 274 }, METAL);
    label("PENGATURAN", 197, 60, 15, CREAM);
    label("Redupkan kedipan dan gerak dekoratif.", 198, 82, 10, MUTED);
    button(motionButton, app->reducedMotion ? "KURANGI ANIMASI   /   AKTIF" :
           "KURANGI ANIMASI   /   NONAKTIF", true, app->reducedMotion, mouse);

    DrawRectangle(198, 130, 244, 1, RIM);
    label("VOLUME MUSIK", 198, 137, 10, CREAM);
    label("PANAH KIRI/KANAN", 336, 137, 10, MUTED);
    button(volumeDown, "-", app->musicVolume > 0, false, mouse);
    button(volumeUp, "+", app->musicVolume < 100, false, mouse);
    const char *volume = TextFormat("%d%%", app->musicVolume);
    label(volume, 320-MeasureText(volume, 10)/2, 158, 10, AMBER);
    for (int i = 0; i < 10; i++)
        DrawRectangle(256+i*13, 179, 11, 5, i < app->musicVolume/10 ? AMBER : INK);

    DrawRectangle(198, 194, 244, 1, RIM);
    label("VOLUME EFEK (SFX)", 198, 201, 10, CREAM);
    label("TOMBOL [ - / = ]", 342, 201, 10, MUTED);
    button(sfxDown, "-", app->sfxVolume > 0, false, mouse);
    button(sfxUp, "+", app->sfxVolume < 100, false, mouse);
    const char *sfxText = TextFormat("%d%%", app->sfxVolume);
    label(sfxText, 320-MeasureText(sfxText, 10)/2, 222, 10, MINT);
    for (int i = 0; i < 10; i++)
        DrawRectangle(256+i*13, 243, 11, 5, i < app->sfxVolume/10 ? MINT : INK);

    if (app->settingsSaveFailed) label("Pilihan berlaku untuk sesi ini.", 198, 260, 10, AMBER);
    button(settingsBackButton, "KEMBALI  [ESC]", true, false, mouse);
}

static MusicCue current_cue(const App *app)
{
    if (app->menu) return MUSIC_MENU;
    if (app->story.playing && app->story.kind == OPENING) return MUSIC_STORY;
    switch (app->game.state) {
        case PREPARE:
        case CRISIS_GAP: return MUSIC_PREPARE;
        case WAVE: return MUSIC_WAVE;
        case CRISIS_WARNING:
        case CRISIS_WAVE: return MUSIC_CRISIS;
        case WON: return MUSIC_WIN;
        case LOST: return MUSIC_LOSS;
    }
    return MUSIC_PREPARE;
}

/* Menghitung kotak tujuan letterbox dengan skala bilangan bulat (integer scaling) agar piksel tetap tajam. */
static Rectangle viewport(int screenWidth, int screenHeight)
{
    float scale = fminf((float)screenWidth/WIDTH, (float)screenHeight/HEIGHT);
    if (scale >= 1) scale = floorf(scale);
    float w = WIDTH*scale, h = HEIGHT*scale;
    return (Rectangle){ floorf((screenWidth-w)/2), floorf((screenHeight-h)/2), w, h };
}

/* Mengubah posisi mouse di jendela asli ke koordinat kanvas virtual 640x360. */
static Vector2 window_to_canvas(Vector2 cursor, Rectangle destination)
{
    if (destination.width <= 0 || destination.height <= 0) return (Vector2){-1,-1};
    return (Vector2){
        (cursor.x-destination.x)*WIDTH/destination.width,
        (cursor.y-destination.y)*HEIGHT/destination.height
    };
}

/*
 * Menjalankan simulasi dengan pola Fixed-Timestep Accumulator (1/60 detik per tick).
 * Laju 2x/3x/5x hanya mempercepat pengisian accumulator sehingga perilaku fisika tetap identik.
 */
static void step_app(App *app, float dt, float *accumulator)
{
    const float fixedStep = 1.0f/60.0f;
    if (app->story.playing) {
        *accumulator = 0;
        tick_story(&app->story, dt);
    } else if (!app->menu && !app->paused && !app->help) {
        app->noticeTime -= dt;
        RunState before = app->game.state;
        *accumulator += dt * (fighting(before) ? speeds[app->speedIndex] : 1);
        while (*accumulator >= fixedStep) {
            tick_game(&app->game, fixedStep);
            *accumulator -= fixedStep;
            if (app->game.state != before) {
                *accumulator = 0;
                if (app->game.state == CRISIS_GAP || app->game.state == WON || app->game.state == LOST)
                    app->speedIndex = 0;
                if (app->game.state == WON || app->game.state == LOST) {
                    int finalScore = campaign_score(&app->game);
                    if (finalScore > app->highScore) {
                        app->highScore = finalScore;
                        app->newRecord = true;
                        app->settingsSaveFailed = !save_settings(app);
                    }
                    begin_ending(&app->story, app->game.state == WON);
                }
                if (app->game.state == PREPARE && app->game.wave == 6) {
                    notice(app, "Bu Sari: Keluarga dari mobil terakhir sudah masuk.");
                    app->noticeTime = 3;
                }
                break;
            }
        }
    } else {
        *accumulator = 0;
        if (app->menu && !app->settings) app->game.time += dt;
    }
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "The Amazing Zombie TD - Far From Home");
    SetWindowMinSize(WIDTH, HEIGHT);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    RenderTexture2D frame = LoadRenderTexture(WIDTH, HEIGHT);
    if (frame.id == 0) {
        CloseWindow();
        return 1;
    }
    SetTextureFilter(frame.texture, TEXTURE_FILTER_POINT);
    load_map_art();
    GameAudio audio = load_game_audio();
    App app = {0};
    load_settings(&app);
    new_run(&app);
    app.menu = true;
    float accumulator = 0;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        Rectangle dest = viewport(GetScreenWidth(), GetScreenHeight());
        Vector2 mouse = window_to_canvas(GetMousePosition(), dest);
        if (update_input(&app, &audio, mouse)) break;
        float dt = fminf(GetFrameTime(), 0.1f);
        step_app(&app, dt, &accumulator);

        play_game_events(&audio, app.game.events, app.sfxVolume);
        app.game.events = 0;

        MusicCue cue = current_cue(&app);
        bool warning = cue == MUSIC_CRISIS && app.game.state == CRISIS_WARNING;
        if (!app.menu && !app.criticalHeard && app.game.hp > 0 && app.game.hp <= 5) {
            play_critical_call(&audio, app.sfxVolume);
            app.criticalHeard = true;
            notice(&app, "Pos medis: Mereka sudah di pintu dalam.");
        }
        float seek = app.storyJump ? app.story.scene*story_scene_seconds(&app.story) : -1;
        bool freezeAudio = (app.paused || app.help) && !app.menu;
        update_game_audio(&audio, cue, warning, freezeAudio, app.musicVolume, app.sfxVolume, dt, seek);
        app.storyJump = false;

        BeginTextureMode(frame);
        ClearBackground(INK);
        Vector2 underlyingMouse = (app.settings || app.paused || app.help) ? (Vector2){ -1, -1 } : mouse;
        if (app.story.playing) {
            draw_story(&app.story, app.reducedMotion);
            button(nextSceneButton, "LANJUT [ENTER]", true, true, mouse);
            button(skipButton, "LEWATI [ESC]", true, false, mouse);
        } else if (app.menu) {
            draw_menu(&app, underlyingMouse);
        } else {
            draw_map(&app.game, app.reducedMotion);
            draw_combat(&app.game, app.reducedMotion);
            if (!app.paused && !app.help && app.game.state != WON && app.game.state != LOST)
                draw_placement(&app, mouse);
            draw_hud(&app, underlyingMouse);
            draw_crisis_warning(&app.game, app.reducedMotion);
            if (app.paused || app.game.state == WON || app.game.state == LOST)
                draw_overlay(&app, (app.settings || app.help) ? underlyingMouse : mouse);
        }
        if (app.help) draw_help(mouse);
        if (app.settings) draw_settings(&app, mouse);
        EndTextureMode();

        BeginDrawing();
        ClearBackground((Color){ 10, 14, 12, 255 });
        DrawTexturePro(frame.texture, (Rectangle){ 0, 0, WIDTH, -HEIGHT }, dest,
                       (Vector2){ 0, 0 }, 0, WHITE);
        EndDrawing();
    }
    UnloadRenderTexture(frame);
    unload_map_art();
    unload_game_audio(&audio);
    CloseWindow();
    return 0;
}
