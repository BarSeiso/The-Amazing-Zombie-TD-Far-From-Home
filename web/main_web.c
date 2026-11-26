#include "art.h"
#include "audio.h"
#include "story.h"

#include <emscripten/emscripten.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Entry point khusus versi Web / Browser (Emscripten + WebAssembly).
 * Memakai ulang seluruh modul inti di ../src/ tanpa mengubah src/main.c desktop.
 */

static void web_toggle_fullscreen(void)
{
    EM_ASM({
        var root = document.getElementById("stage") || document.documentElement;
        if (!document.fullscreenElement) {
            if (root.requestFullscreen) root.requestFullscreen().catch(function() {});
        } else if (document.exitFullscreen) {
            document.exitFullscreen().catch(function() {});
        }
    });
}

typedef struct {
    Vector2 pos;
    bool pressed, held, rightPressed, isTouch;
} Pointer;

typedef struct {
    Game game;
    Story story;
    bool menu, paused, help, confirmMenu, storyJump, criticalHeard;
    int speedIndex;
    bool settings, reducedMotion;
    int selected, placing, musicVolume, sfxVolume, highScore;
    bool newRecord, hasTouchAim;
    Vector2 touchAim;
    float noticeTime;
    const char *notice;
} App;

typedef struct {
    App app;
    GameAudio audio;
    RenderTexture2D frame;
    float accumulator, touchGrace;
    bool audioReady, wasTouching;
} WebContext;

static WebContext g_ctx;

static const int speeds[] = {1, 2, 3, 5};
static const char *const target_names[TARGET_MODE_COUNT] = {
    "DEPAN", "KUAT", "DEKAT"
};

static bool fighting(RunState state)
{
    return state == WAVE || state == CRISIS_WAVE;
}

static const Rectangle playButton            = {  28, 240, 196, 31 };
static const Rectangle settingsButton        = {  28, 278, 196, 24 };
static const Rectangle fullscreenButton      = {  28, 308, 196, 22 };
static const Rectangle helpButton            = { 496,  19,  52, 24 };
static const Rectangle pauseButton           = { 554,  19,  72, 24 };
static const Rectangle targetButton          = { 500, 231, 124, 15 };
static const Rectangle upgradeButton         = { 500, 248, 124, 16 };
static const Rectangle sellButton            = { 500, 266, 124, 16 };
static const Rectangle placeConfirmButton    = { 500, 255,  60, 22 };
static const Rectangle cancelPlaceHalfButton = { 564, 255,  60, 22 };
static const Rectangle cancelPlaceFullButton = { 500, 255, 124, 22 };
static const Rectangle startButton           = { 500, 285, 124, 31 };
static const Rectangle pauseResumeButton     = { 225, 174, 190, 26 };
static const Rectangle pauseSettingsButton   = { 225, 206, 190, 24 };
static const Rectangle pauseMenuButton       = { 225, 236, 190, 24 };
static const Rectangle resultRetryButton     = { 195, 226, 250, 27 };
static const Rectangle resultMenuButton      = { 195, 258, 250, 23 };
static const Rectangle motionButton          = { 198,  98, 244, 24 };
static const Rectangle volumeDown            = { 198, 152,  44, 24 };
static const Rectangle volumeUp              = { 398, 152,  44, 24 };
static const Rectangle sfxDown               = { 198, 216,  44, 24 };
static const Rectangle sfxUp                 = { 398, 216,  44, 24 };
static const Rectangle settingsBackButton    = { 198, 280, 244, 26 };
static const Rectangle nextSceneButton       = { 492, 282, 114, 22 };
static const Rectangle skipButton            = { 492, 308, 114, 22 };
static const Rectangle helpCloseButton       = { 456,  45, 116, 22 };

/* Dipanggil dari tombol HTML "BUKA POS KOMANDO" untuk membuka izin WebAudio browser. */
EMSCRIPTEN_KEEPALIVE void web_unlock_audio(void)
{
    if (g_ctx.audioReady) return;
    g_ctx.audio = load_game_audio();
    g_ctx.audioReady = true;
}

static void ui_sfx(SfxKind kind)
{
    if (g_ctx.audioReady) play_sfx(&g_ctx.audio, kind, g_ctx.app.sfxVolume);
}

static void load_settings(App *app)
{
    int motion = EM_ASM_INT({
        try { return parseInt(localStorage.getItem("farFromHome_motion") || "0", 10) === 1 ? 1 : 0; }
        catch (e) { return 0; }
    });
    int vol = EM_ASM_INT({
        try {
            var v = parseInt(localStorage.getItem("farFromHome_music"), 10);
            return isNaN(v) ? 60 : v;
        } catch (e) { return 60; }
    });
    int sfx = EM_ASM_INT({
        try {
            var v = parseInt(localStorage.getItem("farFromHome_sfx"), 10);
            return isNaN(v) ? 70 : v;
        } catch (e) { return 70; }
    });
    int best = EM_ASM_INT({
        try {
            var v = parseInt(localStorage.getItem("farFromHome_best"), 10);
            return (isNaN(v) || v < 0) ? 0 : v;
        } catch (e) { return 0; }
    });
    app->reducedMotion = motion == 1;
    app->musicVolume = (vol >= 0 && vol <= 100 && vol % 10 == 0) ? vol : 60;
    app->sfxVolume   = (sfx >= 0 && sfx <= 100 && sfx % 10 == 0) ? sfx : 70;
    app->highScore   = best >= 0 ? best : 0;
}

static void save_settings(const App *app)
{
    EM_ASM({
        try {
            localStorage.setItem("farFromHome_motion", String($0));
            localStorage.setItem("farFromHome_music", String($1));
            localStorage.setItem("farFromHome_sfx", String($2));
            localStorage.setItem("farFromHome_best", String($3));
        } catch (e) {}
    }, app->reducedMotion ? 1 : 0, app->musicVolume, app->sfxVolume, app->highScore);
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

static bool over(Vector2 p, Rectangle rect)
{
    return CheckCollisionPointRec(p, rect);
}

static Rectangle shop_card(int kind)
{
    return (Rectangle){ 500, (float)(86 + kind * 31), 124, 28 };
}

static void button(Rectangle r, const char *text, bool enabled, bool primary, Pointer ptr)
{
    bool hover = enabled && over(ptr.pos, r);
    bool held = hover && ptr.held;
    Color fill = !enabled ? INK : primary ? (hover ? (Color){ 251, 207, 116, 255 } : AMBER) :
                 hover ? (Color){ 64, 79, 60, 255 } : METAL;
    int x = (int)r.x, y = (int)r.y, w = (int)r.width, h = (int)r.height;
    DrawRectangle(x, y + 2, w, h, INK);
    DrawRectangleRec(r, fill);
    DrawRectangleLines(x, y, w, h, hover ? CREAM : RIM);
    DrawRectangle(x + 1, y + 1, w - 2, 1, Fade(CREAM, 0.32f));
    label(text, x + (w - MeasureText(text, 10)) / 2, y + (h - 10) / 2 + held, 10,
          !enabled ? MUTED : primary ? INK : CREAM);
}

static void new_run(App *app)
{
    bool reduced = app->reducedMotion;
    int musicVolume = app->musicVolume;
    int sfxVolume = app->sfxVolume;
    int highScore = app->highScore;
    *app = (App){
        .selected = -1, .placing = -1,
        .reducedMotion = reduced,
        .musicVolume = musicVolume,
        .sfxVolume = sfxVolume,
        .highScore = highScore
    };
    reset_game(&app->game);
}

static void notice(App *app, const char *text)
{
    app->notice = text;
    app->noticeTime = 2.2f;
}

static void attempt_place(App *app, Vector2 pos)
{
    Game *g = &app->game;
    if (app->placing < 0) return;
    if (buy_tower(g, (TowerKind)app->placing, pos)) {
        app->placing = -1;
        app->hasTouchAim = false;
        app->selected = g->towerCount - 1;
        notice(app, "Peleton siap tempur.");
    } else if (g->money < tower_specs[app->placing].price) {
        notice(app, "Dana tidak cukup.");
    } else if (g->towerCount == MAX_TOWERS) {
        notice(app, "Kapasitas peleton penuh.");
    } else {
        notice(app, "Pilih posisi di luar jalur dan peleton lain.");
    }
}

static void update_input(App *app, Pointer ptr)
{
    bool clicked = ptr.pressed;
    if (clicked && !g_ctx.audioReady) web_unlock_audio();

    if (app->settings) {
        if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(ptr.pos, settingsBackButton))) {
            app->settings = false;
            ui_sfx(SFX_CLICK);
        } else if (IsKeyPressed(KEY_ENTER) || (clicked && over(ptr.pos, motionButton))) {
            app->reducedMotion = !app->reducedMotion;
            save_settings(app);
            ui_sfx(SFX_CLICK);
        } else {
            int oldMusic = app->musicVolume;
            int oldSfx = app->sfxVolume;
            if ((IsKeyPressed(KEY_LEFT) || (clicked && over(ptr.pos, volumeDown))) && oldMusic > 0)
                app->musicVolume -= 10;
            if ((IsKeyPressed(KEY_RIGHT) || (clicked && over(ptr.pos, volumeUp))) && oldMusic < 100)
                app->musicVolume += 10;
            if ((IsKeyPressed(KEY_MINUS) || (clicked && over(ptr.pos, sfxDown))) && oldSfx > 0)
                app->sfxVolume -= 10;
            if ((IsKeyPressed(KEY_EQUAL) || (clicked && over(ptr.pos, sfxUp))) && oldSfx < 100)
                app->sfxVolume += 10;
            if (oldMusic != app->musicVolume || oldSfx != app->sfxVolume) {
                save_settings(app);
                ui_sfx(SFX_CLICK);
            }
        }
        return;
    }
    if (app->help) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_H) || (clicked && over(ptr.pos, helpCloseButton))) {
            app->help = false;
            ui_sfx(SFX_CLICK);
        }
        return;
    }
    if (app->story.playing) {
        if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(ptr.pos, skipButton))) {
            app->story.playing = false;
            ui_sfx(SFX_CLICK);
        } else if (IsKeyPressed(KEY_ENTER) || (clicked && over(ptr.pos, nextSceneButton))) {
            advance_story(&app->story);
            app->storyJump = app->story.playing;
            ui_sfx(SFX_CLICK);
        }
        return;
    }
    if (app->menu) {
        if (IsKeyPressed(KEY_ENTER) || (clicked && over(ptr.pos, playButton))) {
            web_unlock_audio();
            ui_sfx(SFX_CLICK);
            new_run(app);
            begin_story(&app->story);
            return;
        }
        if (clicked && over(ptr.pos, settingsButton)) {
            app->settings = true;
            ui_sfx(SFX_CLICK);
        } else if (clicked && over(ptr.pos, fullscreenButton)) {
            ui_sfx(SFX_CLICK);
            web_toggle_fullscreen();
        }
        return;
    }

    Game *g = &app->game;
    if (g->state == WON || g->state == LOST) {
        if (IsKeyPressed(KEY_ENTER) || (clicked && over(ptr.pos, resultRetryButton))) {
            ui_sfx(SFX_CLICK);
            new_run(app);
        } else if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(ptr.pos, resultMenuButton))) {
            ui_sfx(SFX_CLICK);
            app->menu = true;
        }
        return;
    }

    if (!app->paused && (IsKeyPressed(KEY_H) || (clicked && over(ptr.pos, helpButton)))) {
        app->help = true;
        ui_sfx(SFX_CLICK);
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(ptr.pos, pauseButton))) {
        app->paused = !app->paused;
        app->confirmMenu = false;
        ui_sfx(SFX_CLICK);
        return;
    }
    if (app->paused) {
        if (clicked && over(ptr.pos, pauseResumeButton)) {
            app->paused = false;
            app->confirmMenu = false;
            ui_sfx(SFX_CLICK);
        } else if (clicked && over(ptr.pos, pauseSettingsButton)) {
            app->settings = true;
            app->confirmMenu = false;
            ui_sfx(SFX_CLICK);
        } else if (clicked && over(ptr.pos, pauseMenuButton)) {
            ui_sfx(SFX_CLICK);
            if (!app->confirmMenu) {
                app->confirmMenu = true;
            } else {
                app->paused = false;
                app->confirmMenu = false;
                app->menu = true;
            }
        }
        return;
    }

    if (ptr.rightPressed) {
        app->placing = -1;
        app->hasTouchAim = false;
        app->selected = -1;
        return;
    }
    for (int kind = 0; kind < TOWER_KIND_COUNT; kind++) {
        if (IsKeyPressed(KEY_ONE + kind) || (clicked && over(ptr.pos, shop_card(kind)))) {
            ui_sfx(SFX_CLICK);
            if (g->towerCount >= MAX_TOWERS) {
                notice(app, "Slot 6/6 penuh. Jual 1 peleton jika ingin mengganti.");
                return;
            }
            app->placing = app->placing == kind ? -1 : kind;
            app->hasTouchAim = false;
            app->selected = -1;
            app->noticeTime = 0;
            return;
        }
    }
    if (app->placing >= 0 && clicked) {
        Rectangle cancelRect = app->hasTouchAim ? cancelPlaceHalfButton : cancelPlaceFullButton;
        if (over(ptr.pos, cancelRect)) {
            app->placing = -1;
            app->hasTouchAim = false;
            ui_sfx(SFX_CLICK);
            return;
        }
        if (app->hasTouchAim && over(ptr.pos, placeConfirmButton)) {
            attempt_place(app, app->touchAim);
            return;
        }
    }
    if (app->selected >= 0) {
        if (IsKeyPressed(KEY_TAB) || (clicked && over(ptr.pos, targetButton))) {
            cycle_tower_target(g, app->selected);
            ui_sfx(SFX_CLICK);
            notice(app, TextFormat("Prioritas bidik: %s.", target_names[g->towers[app->selected].targetMode]));
            return;
        }
        if (IsKeyPressed(KEY_U) || (clicked && over(ptr.pos, upgradeButton))) {
            const Tower *t = &g->towers[app->selected];
            int price = tower_specs[t->kind].levels[t->level].upgradeCost;
            if (upgrade_tower(g, app->selected)) notice(app, "Upgrade berhasil.");
            else notice(app, price ? "Dana tidak cukup." : "Peleton sudah level maksimum.");
            return;
        }
        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE) ||
            (clicked && over(ptr.pos, sellButton))) {
            if (sell_tower(g, app->selected)) {
                app->selected = -1;
                notice(app, "Peleton dijual.");
            }
            return;
        }
    }
    if (fighting(g->state) && (IsKeyPressed(KEY_F) || (clicked && over(ptr.pos, startButton)))) {
        app->speedIndex = (app->speedIndex + 1) % 4;
        ui_sfx(SFX_CLICK);
        return;
    }
    if (IsKeyPressed(KEY_SPACE) || (clicked && over(ptr.pos, startButton))) {
        if (start_wave(g)) {
            app->noticeTime = 0;
            ui_sfx(SFX_CLICK);
        }
        return;
    }

    /* Penanganan interaksi di atas peta untuk mouse laptop maupun layar sentuh HP. */
    if (app->placing >= 0 && ptr.isTouch && ptr.held && over(ptr.pos, battlefield)) {
        float dx = ptr.pos.x - app->touchAim.x;
        float dy = ptr.pos.y - app->touchAim.y;
        if (clicked && app->hasTouchAim && (dx * dx + dy * dy) <= 324.0f) {
            attempt_place(app, app->touchAim);
        } else {
            app->touchAim = ptr.pos;
            app->hasTouchAim = true;
        }
        return;
    }
    if (clicked && over(ptr.pos, battlefield)) {
        if (app->placing >= 0) {
            app->hasTouchAim = false;
            attempt_place(app, ptr.pos);
        } else {
            app->selected = tower_at(g, ptr.pos);
            if (app->selected >= 0) ui_sfx(SFX_CLICK);
        }
    }
}

static void draw_menu(const App *app, Pointer ptr)
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
    button(playButton, "MULAI OPERASI  [ENTER]", true, true, ptr);
    button(settingsButton, "PENGATURAN", true, false, ptr);
    button(fullscreenButton, "LAYAR PENUH  [F11]", true, false, ptr);

    panel((Rectangle){ 486, 60, 136, 96 }, METAL);
    label("SEKTOR 07", 497, 70, 10, AMBER);
    DrawRectangle(497, 85, 114, 1, RIM);
    label(TextFormat("Tahan %d wave", WAVE_COUNT), 497, 94, 10, CREAM);
    label("Lindungi shelter.", 497, 109, 10, CREAM);
    label(app->highScore > 0 ? TextFormat("Skor Terbaik %d", app->highScore) : "Belum ada rekor",
          497, 130, 10, app->highScore > 0 ? MINT : MUTED);

    panel((Rectangle){ 438, 289, 184, 28 }, METAL);
    label("POS KOMANDO  /  WEB", 451, 298, 10, CREAM);
}

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
    label("Ketuk unit: kelola", 503, 236, 10, MUTED);
    label("[INTEL] Info musuh", 503, 250, 10, MUTED);
    label(TextFormat("Tewas: %d", g->totalKills), 503, 265, 10, MUTED);
}

static void draw_hud(const App *app, Pointer ptr)
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
        DrawRectangle(322 + i * 8, 35, 6, 4, i < g->hp ? (g->hp <= 5 ? ALERT_RED : MINT) : INK);
    }
    button(helpButton, "INTEL", true, false, ptr);
    button(pauseButton, "JEDA", true, false, ptr);

    panel((Rectangle){ 494, 60, 136, 266 }, METAL);
    bool full = g->towerCount >= MAX_TOWERS;
    label(full ? "PELETON  6/6 PENUH" : TextFormat("PELETON   %d/%d", g->towerCount, MAX_TOWERS),
          502, 71, 10, full ? AMBER : CREAM);
    for (int kind = 0; kind < TOWER_KIND_COUNT; kind++) {
        Rectangle card = shop_card(kind);
        int y = (int)card.y;
        bool active = app->placing == kind;
        bool hover = !full && over(ptr.pos, card);
        DrawRectangleRec(card, active ? (Color){ 82, 79, 46, 255 } :
            hover ? (Color){ 60, 73, 54, 255 } : (Color){ 47, 58, 46, 255 });
        DrawRectangleLines(500, y, 124, 28, active ? AMBER : RIM);
        if (active || (hover && ptr.held)) DrawRectangle(501, y + 1, 2, 26, AMBER);
        DrawRectangle(506, y + 3, 29, 22, INK);
        Tower icon = { .kind = (TowerKind)kind, .pos = { 520, (float)(y + 21) }, .facing = 1 };
        draw_tower(&icon, app->reducedMotion ? 0 : g->time, WHITE);
        const TowerSpec *spec = &tower_specs[kind];
        label(spec->name, 540, y + 4, 10, full ? MUTED : CREAM);
        Color priceColor = full ? MUTED : (g->money >= spec->price ? AMBER : ALERT_RED);
        label(TextFormat("%d dana", spec->price), 540, y + 15, 10, priceColor);
        label(TextFormat("[%d]", kind + 1), 602, y + 15, 10, MUTED);
    }
    DrawRectangle(501, 180, 122, 1, RIM);

    if (app->selected >= 0) {
        const Tower *t = &g->towers[app->selected];
        const TowerSpec *spec = &tower_specs[t->kind];
        const TowerStats *stats = &spec->levels[t->level];
        const TowerStats *next = t->level + 1 < MAX_LEVEL ? &spec->levels[t->level + 1] : NULL;
        label(TextFormat("%s LV%d", spec->name, t->level + 1), 503, 184, 10, AMBER);
        const char *killsText = TextFormat("%d kill", t->kills);
        label(killsText, 622 - MeasureText(killsText, 10), 184, 10, MUTED);

        if (next && over(ptr.pos, upgradeButton)) {
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
        button(targetButton, TextFormat("BIDIK: %s", target_names[t->targetMode]),
               can_manage(g), false, ptr);
        int price = stats->upgradeCost;
        button(upgradeButton, price ? TextFormat("UPGRADE %d", price) : "LEVEL MAKS",
               price > 0 && g->money >= price && can_manage(g), false, ptr);
        button(sellButton, TextFormat("JUAL +%d", sale_value(t)), can_manage(g), false, ptr);
    } else if (app->placing >= 0) {
        const TowerSpec *spec = &tower_specs[app->placing];
        const TowerStats *stats = &spec->levels[0];
        label(TextFormat("%s   LV 1", spec->name), 503, 184, 10, AMBER);
        label(TextFormat("Damage    %d", stats->damage), 503, 196, 10, CREAM);
        label(TextFormat("Jarak     %d", stats->range), 503, 208, 10, CREAM);
        label(TextFormat("Reload    %.2fs", stats->reload), 503, 220, 10, CREAM);
        if (app->placing == SNIPER) {
            label("Tembus rompi zirah", 503, 235, 10, MINT);
        } else if (spec->splash) {
            label(TextFormat("Ledakan area %dpx", spec->splash), 503, 235, 10, MINT);
        } else {
            label("Tembakan cepat", 503, 235, 10, MINT);
        }
        if (app->hasTouchAim) {
            bool canPlace = valid_ground(g, app->touchAim) && g->money >= spec->price && g->towerCount < MAX_TOWERS;
            button(placeConfirmButton, "PASANG", canPlace, true, ptr);
            button(cancelPlaceHalfButton, "BATAL", true, false, ptr);
        } else {
            button(cancelPlaceFullButton, "BATAL PASANG", true, false, ptr);
        }
    } else {
        draw_wave_intel(g);
    }

    const char *action = fighting(g->state) ? TextFormat("LAJU %dx   [F]", speeds[app->speedIndex]) :
                         g->state == PREPARE ? "MULAI WAVE" : "SIAGA";
    button(startButton, action, g->state == PREPARE || fighting(g->state), true, ptr);

    DrawRectangle(13, 335, 613, 1, RIM);
    if (app->noticeTime > 0) {
        label(app->notice, 15, 343, 10, CREAM);
    } else if (g->state == CRISIS_WARNING) {
        label("PERINGATAN KRISIS: Gerombolan mutan mendekat! Perkuat peleton!", 15, 343, 10, ALERT_RED);
    } else if (g->state == CRISIS_GAP) {
        label("Operator: Tunggu... radar mendeteksi getaran baru di luar pagar.", 15, 343, 10, AMBER);
    } else if (app->placing >= 0 && app->hasTouchAim) {
        label("Ketuk titik hijau lagi atau tekan PASANG untuk menaruh peleton.", 15, 343, 10, MINT);
    } else if (app->placing >= 0) {
        label(TextFormat("Klik/ketuk peta untuk menempatkan %s.", tower_specs[app->placing].name), 15, 343, 10, AMBER);
    } else if (g->state == PREPARE && g->wave == 6) {
        label("Operator: Ada yang pakai rompi. Tempatkan Sniper.", 15, 343, 10, AMBER);
    } else if (g->state == PREPARE && g->wave == 9) {
        label("Pendobrak berlari setelah belokan terakhir.", 15, 343, 10, AMBER);
    } else if (g->lastBonus) {
        label(TextFormat("Bonus wave +%d dana%s", g->lastBonus, g->lastHeal ? "  /  Shelter +1 HP" : ""),
              15, 343, 10, MINT);
    } else if (g->state == PREPARE && g->wave == 1) {
        label("Operator: Tempatkan peleton, lalu tekan MULAI WAVE.", 15, 343, 10, CREAM);
    } else {
        label("Pilih kartu untuk beli   /   Ketuk INTEL untuk panduan musuh", 15, 343, 10, MUTED);
    }
    label("F11 Layar penuh", 626 - MeasureText("F11 Layar penuh", 10), 343, 10, MUTED);
}

static void draw_placement(const App *app, Pointer ptr)
{
    BeginScissorMode(12, 60, 474, 266);
    Vector2 aim = app->hasTouchAim ? app->touchAim : ptr.pos;
    bool showGhost = app->placing >= 0 && (app->hasTouchAim || (!ptr.isTouch && over(ptr.pos, battlefield)));
    if (showGhost) {
        const TowerSpec *spec = &tower_specs[app->placing];
        bool valid = valid_ground(&app->game, aim) && app->game.money >= spec->price &&
            app->game.towerCount < MAX_TOWERS;
        Color color = valid ? MINT : ALERT_RED;
        DrawCircleV(aim, (float)spec->levels[0].range, Fade(color, 0.14f));
        DrawCircleLines((int)aim.x, (int)aim.y, (float)spec->levels[0].range, color);
        Tower ghost = { .pos = aim, .facing = 1, .kind = (TowerKind)app->placing };
        draw_tower(&ghost, 0, Fade(WHITE, 0.68f));
        int x = (int)aim.x, y = (int)aim.y;
        DrawRectangle(x + 9, y - 22, 12, 12, INK);
        if (valid) {
            DrawLine(x + 11, y - 16, x + 14, y - 13, color);
            DrawLine(x + 14, y - 13, x + 19, y - 19, color);
        } else {
            DrawLine(x + 12, y - 19, x + 18, y - 13, color);
            DrawLine(x + 12, y - 13, x + 18, y - 19, color);
        }
    } else if (app->selected >= 0) {
        const Tower *t = &app->game.towers[app->selected];
        const TowerSpec *spec = &tower_specs[t->kind];
        Vector2 p = t->pos;
        DrawCircleLines((int)p.x, (int)p.y, (float)spec->levels[t->level].range, Fade(AMBER, 0.63f));
        if (t->level + 1 < MAX_LEVEL && over(ptr.pos, upgradeButton)) {
            float nextRange = (float)spec->levels[t->level + 1].range;
            DrawCircleV(p, nextRange, Fade(MINT, 0.08f));
            DrawCircleLines((int)p.x, (int)p.y, nextRange, Fade(MINT, 0.78f));
        }
        DrawEllipseLines((int)p.x, (int)p.y, 10, 4, AMBER);
    }
    EndScissorMode();
}

static void draw_overlay(const App *app, Pointer ptr)
{
    DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(INK, 0.78f));
    if (app->paused) {
        panel((Rectangle){ 209, 108, 222, 168 }, METAL);
        label("OPERASI DIJEDA", 225, 124, 14, CREAM);
        label(app->confirmMenu ? "Progres sesi ini akan ditinggalkan." : "Ketuk LANJUTKAN untuk kembali.",
              225, 147, 10, app->confirmMenu ? ALERT_RED : MUTED);
        button(pauseResumeButton, "LANJUTKAN", true, true, ptr);
        button(pauseSettingsButton, "PENGATURAN", true, false, ptr);
        button(pauseMenuButton, app->confirmMenu ? "YAKIN KE MENU?" : "MENU UTAMA",
               true, app->confirmMenu, ptr);
        return;
    }

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

    button(resultRetryButton, won ? "OPERASI BARU  [ENTER]" : "COBA LAGI  [ENTER]", true, true, ptr);
    button(resultMenuButton, "MENU UTAMA  [ESC]", true, false, ptr);
}

static void draw_help(Pointer ptr)
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
    button(helpCloseButton, "TUTUP [H/ESC]", true, false, ptr);
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
    label("KONTROL:  Kartu [1-3] Beli   UPGRADE [U]   JUAL [X]   BIDIK [TAB]   LAJU [F]   [SPASI] Wave",
          68, 288, 10, MINT);
    label("TIPS: Jual 1 Archer di wave akhir bila slot penuh untuk menambah Sniper/Cannon penahan Krisis.",
          68, 304, 10, MUTED);
}

static void draw_settings(const App *app, Pointer ptr)
{
    DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(INK, .84f));
    panel((Rectangle){ 179, 44, 282, 274 }, METAL);
    label("PENGATURAN", 197, 60, 15, CREAM);
    label("Tersimpan otomatis di browser ini.", 198, 82, 10, MUTED);
    button(motionButton, app->reducedMotion ? "KURANGI ANIMASI   /   AKTIF" :
           "KURANGI ANIMASI   /   NONAKTIF", true, app->reducedMotion, ptr);

    DrawRectangle(198, 130, 244, 1, RIM);
    label("VOLUME MUSIK", 198, 137, 10, CREAM);
    label("PANAH KIRI/KANAN", 336, 137, 10, MUTED);
    button(volumeDown, "-", app->musicVolume > 0, false, ptr);
    button(volumeUp, "+", app->musicVolume < 100, false, ptr);
    const char *volume = TextFormat("%d%%", app->musicVolume);
    label(volume, 320 - MeasureText(volume, 10) / 2, 158, 10, AMBER);
    for (int i = 0; i < 10; i++)
        DrawRectangle(256 + i * 13, 179, 11, 5, i < app->musicVolume / 10 ? AMBER : INK);

    DrawRectangle(198, 194, 244, 1, RIM);
    label("VOLUME EFEK (SFX)", 198, 201, 10, CREAM);
    label("TOMBOL [ - / = ]", 342, 201, 10, MUTED);
    button(sfxDown, "-", app->sfxVolume > 0, false, ptr);
    button(sfxUp, "+", app->sfxVolume < 100, false, ptr);
    const char *sfxText = TextFormat("%d%%", app->sfxVolume);
    label(sfxText, 320 - MeasureText(sfxText, 10) / 2, 222, 10, MINT);
    for (int i = 0; i < 10; i++)
        DrawRectangle(256 + i * 13, 243, 11, 5, i < app->sfxVolume / 10 ? MINT : INK);

    button(settingsBackButton, "KEMBALI  [ESC]", true, false, ptr);
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

static Rectangle viewport(int screenWidth, int screenHeight)
{
    float scale = fminf((float)screenWidth / WIDTH, (float)screenHeight / HEIGHT);
    if (scale >= 2.0f) scale = floorf(scale);
    float w = floorf(WIDTH * scale), h = floorf(HEIGHT * scale);
    return (Rectangle){ floorf((screenWidth - w) / 2.0f), floorf((screenHeight - h) / 2.0f), w, h };
}

static Vector2 window_to_canvas(Vector2 cursor, Rectangle destination)
{
    if (destination.width <= 0 || destination.height <= 0) return (Vector2){ -1, -1 };
    return (Vector2){
        (cursor.x - destination.x) * WIDTH / destination.width,
        (cursor.y - destination.y) * HEIGHT / destination.height
    };
}

static Pointer read_pointer(WebContext *ctx, Rectangle dest, float dt)
{
    Pointer ptr = {0};
    int touches = GetTouchPointCount();
    if (touches > 0) {
        ptr.pos = window_to_canvas(GetTouchPosition(0), dest);
        ptr.pressed = !ctx->wasTouching;
        ptr.held = true;
        ptr.isTouch = true;
        ctx->touchGrace = 0.35f;
    } else {
        ctx->touchGrace = fmaxf(0.0f, ctx->touchGrace - dt);
        ptr.pos = window_to_canvas(GetMousePosition(), dest);
        if (ctx->touchGrace <= 0.0f) {
            ptr.pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            ptr.held = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            ptr.rightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        }
    }
    ctx->wasTouching = touches > 0;
    return ptr;
}

static void step_app(App *app, float dt, float *accumulator)
{
    const float fixedStep = 1.0f / 60.0f;
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
                        save_settings(app);
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

static void web_frame(void *userData)
{
    WebContext *ctx = (WebContext *)userData;
    App *app = &ctx->app;

    if (IsKeyPressed(KEY_F11)) web_toggle_fullscreen();
    float dt = fminf(GetFrameTime(), 0.1f);
    Rectangle dest = viewport(GetScreenWidth(), GetScreenHeight());
    Pointer ptr = read_pointer(ctx, dest, dt);

    update_input(app, ptr);
    step_app(app, dt, &ctx->accumulator);

    if (ctx->audioReady) {
        play_game_events(&ctx->audio, app->game.events, app->sfxVolume);
        MusicCue cue = current_cue(app);
        bool warning = cue == MUSIC_CRISIS && app->game.state == CRISIS_WARNING;
        if (!app->menu && !app->criticalHeard && app->game.hp > 0 && app->game.hp <= 5) {
            play_critical_call(&ctx->audio, app->sfxVolume);
            app->criticalHeard = true;
            notice(app, "Pos medis: Mereka sudah di pintu dalam.");
        }
        float seek = app->storyJump ? app->story.scene * story_scene_seconds(&app->story) : -1;
        bool freezeAudio = (app->paused || app->help) && !app->menu;
        update_game_audio(&ctx->audio, cue, warning, freezeAudio,
                          app->musicVolume, app->sfxVolume, dt, seek);
    }
    app->game.events = 0;
    app->storyJump = false;

    BeginTextureMode(ctx->frame);
    ClearBackground(INK);
    Pointer hiddenPtr = { .pos = { -1, -1 } };
    Pointer bgPtr = (app->settings || app->paused || app->help) ? hiddenPtr : ptr;
    if (app->story.playing) {
        draw_story(&app->story, app->reducedMotion);
        button(nextSceneButton, "LANJUT [ENTER]", true, true, ptr);
        button(skipButton, "LEWATI [ESC]", true, false, ptr);
    } else if (app->menu) {
        draw_menu(app, bgPtr);
    } else {
        draw_map(&app->game, app->reducedMotion);
        draw_combat(&app->game, app->reducedMotion);
        if (!app->paused && !app->help && app->game.state != WON && app->game.state != LOST)
            draw_placement(app, ptr);
        draw_hud(app, bgPtr);
        draw_crisis_warning(&app->game, app->reducedMotion);
        if (app->paused || app->game.state == WON || app->game.state == LOST)
            draw_overlay(app, (app->settings || app->help) ? hiddenPtr : ptr);
    }
    if (app->help) draw_help(ptr);
    if (app->settings) draw_settings(app, ptr);
    EndTextureMode();

    BeginDrawing();
    ClearBackground((Color){ 10, 14, 12, 255 });
    DrawTexturePro(ctx->frame.texture, (Rectangle){ 0, 0, WIDTH, -HEIGHT }, dest,
                   (Vector2){ 0, 0 }, 0, WHITE);
    EndDrawing();
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "The Amazing Zombie TD - Far From Home");
    SetWindowMinSize(240, 135);
    SetExitKey(KEY_NULL);

    g_ctx.frame = LoadRenderTexture(WIDTH, HEIGHT);
    if (g_ctx.frame.id == 0) {
        CloseWindow();
        return 1;
    }
    SetTextureFilter(g_ctx.frame.texture, TEXTURE_FILTER_POINT);
    load_map_art();
    load_settings(&g_ctx.app);
    new_run(&g_ctx.app);
    g_ctx.app.menu = true;

    emscripten_set_main_loop_arg(web_frame, &g_ctx, 0, 1);

    UnloadRenderTexture(g_ctx.frame);
    unload_map_art();
    if (g_ctx.audioReady) unload_game_audio(&g_ctx.audio);
    CloseWindow();
    return 0;
}
