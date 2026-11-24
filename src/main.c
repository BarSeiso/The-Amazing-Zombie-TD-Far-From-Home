#include "art.h"
#include "audio.h"
#include "story.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    Game game;
    Story story;
    bool menu, paused, storyJump;
    bool settings, reducedMotion, settingsSaveFailed;
    int selected, placing, musicVolume;
    float noticeTime;
    const char *notice;
} App;

static const Rectangle playButton = { 28, 240, 196, 31 };
static const Rectangle settingsButton = { 28, 278, 196, 24 };
static const Rectangle exitButton = { 28, 308, 196, 22 };
static const Rectangle pauseButton = { 554, 19, 72, 24 };
static const Rectangle upgradeButton = { 500, 244, 124, 17 };
static const Rectangle sellButton = { 500, 264, 124, 17 };
static const Rectangle startButton = { 500, 285, 124, 31 };
static const Rectangle resumeButton = { 225, 183, 190, 28 };
static const Rectangle menuButton = { 225, 218, 190, 28 };
static const Rectangle pauseSettingsButton = { 225, 217, 190, 23 };
static const Rectangle pauseMenuButton = { 225, 247, 190, 23 };
static const Rectangle motionButton = { 198, 145, 244, 26 };
static const Rectangle volumeDown = { 198, 219, 44, 26 };
static const Rectangle volumeUp = { 398, 219, 44, 26 };
static const Rectangle settingsBackButton = { 198, 273, 244, 26 };
static const Rectangle skipButton = { 498, 299, 104, 28 };

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
    char path[1024];
    if (!settings_path(path, false) || !FileExists(path)) return;
    char *value = LoadFileText(path);
    if (!value) return;
    int motion = 0, volume;
    int fields = sscanf(value, "%d%d", &motion, &volume);
    app->reducedMotion = fields >= 1 && motion == 1;
    if (fields == 2 && volume >= 0 && volume <= 100 && volume%10 == 0) app->musicVolume = volume;
    UnloadFileText(value);
}

static bool save_settings(const App *app)
{
    char path[1024];
    char value[24];
    snprintf(value, sizeof value, "%d\n%d\n", app->reducedMotion, app->musicVolume);
    return settings_path(path, true) && SaveFileText(path, value);
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
    bool saveFailed = app->settingsSaveFailed;
    *app = (App){ .selected = -1, .placing = -1, .reducedMotion = reduced,
                  .musicVolume = musicVolume, .settingsSaveFailed = saveFailed };
    reset_game(&app->game);
}

static void notice(App *app, const char *text)
{
    app->notice = text;
    app->noticeTime = 2.2f;
}

static bool update_input(App *app, Vector2 mouse)
{
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (app->settings) {
        if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(mouse, settingsBackButton))) {
            app->settings = false;
        } else if (IsKeyPressed(KEY_ENTER) || (clicked && over(mouse, motionButton))) {
            app->reducedMotion = !app->reducedMotion;
            app->settingsSaveFailed = !save_settings(app);
        } else {
            int oldVolume = app->musicVolume;
            if ((IsKeyPressed(KEY_LEFT) || (clicked && over(mouse, volumeDown))) && oldVolume > 0)
                app->musicVolume -= 10;
            if ((IsKeyPressed(KEY_RIGHT) || (clicked && over(mouse, volumeUp))) && oldVolume < 100)
                app->musicVolume += 10;
            if (oldVolume != app->musicVolume) app->settingsSaveFailed = !save_settings(app);
        }
        return false;
    }
    if (app->story.playing) {
        if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(mouse, skipButton))) {
            app->story.playing = false;
        } else if (IsKeyPressed(KEY_ENTER)) {
            advance_story(&app->story);
            app->storyJump = app->story.playing;
        }
        return false;
    }
    if (app->menu) {
        if (IsKeyPressed(KEY_ENTER) || (clicked && over(mouse, playButton))) {
            new_run(app);
            begin_story(&app->story);
            return false;
        }
        if (clicked && over(mouse, settingsButton)) app->settings = true;
        return clicked && over(mouse, exitButton);
    }

    Game *g = &app->game;
    if (g->state == WON || g->state == LOST) {
        if (IsKeyPressed(KEY_ENTER) || (clicked && over(mouse, resumeButton))) new_run(app);
        else if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(mouse, menuButton))) app->menu = true;
        return false;
    }

    if (IsKeyPressed(KEY_ESCAPE) || (clicked && over(mouse, pauseButton))) {
        app->paused = !app->paused;
        return false;
    }
    if (app->paused) {
        if (clicked && over(mouse, resumeButton)) app->paused = false;
        else if (clicked && over(mouse, pauseSettingsButton)) app->settings = true;
        else if (clicked && over(mouse, pauseMenuButton)) app->menu = true;
        return false;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        app->placing = -1;
        app->selected = -1;
        return false;
    }
    for (int kind = 0; kind < TOWER_KIND_COUNT; kind++) {
        if (IsKeyPressed(KEY_ONE+kind) || (clicked && over(mouse, shop_card(kind)))) {
            app->placing = app->placing == kind ? -1 : kind;
            app->selected = -1;
            app->noticeTime = 0;
            return false;
        }
    }
    if (clicked && app->selected >= 0) {
        if (over(mouse, upgradeButton)) {
            const Tower *t = &g->towers[app->selected];
            int price = tower_specs[t->kind].levels[t->level].upgradeCost;
            if (upgrade_tower(g, app->selected)) notice(app, "Upgrade berhasil.");
            else notice(app, price ? "Dana tidak cukup." : "Peleton sudah level maksimum.");
            return false;
        }
        if (over(mouse, sellButton)) {
            if (sell_tower(g, app->selected)) {
                app->selected = -1;
                notice(app, "Peleton dijual.");
            }
            return false;
        }
    }
    if (IsKeyPressed(KEY_SPACE) || (clicked && over(mouse, startButton))) {
        if (start_wave(g)) app->noticeTime = 0;
        return false;
    }
    if (clicked && over(mouse, battlefield)) {
        if (app->placing >= 0) {
            if (buy_tower(g, (TowerKind)app->placing, mouse)) {
                app->placing = -1;
                app->selected = g->towerCount-1;
                notice(app, "Peleton siap.");
            } else if (g->money < tower_specs[app->placing].price) {
                notice(app, "Dana tidak cukup.");
            } else if (g->towerCount == MAX_TOWERS) {
                notice(app, "Kapasitas peleton penuh.");
            } else {
                notice(app, "Pilih posisi di luar jalur dan peleton lain.");
            }
        } else {
            app->selected = tower_at(g, mouse);
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
    panel((Rectangle){ 502, 62, 117, 78 }, METAL);
    label("SEKTOR 07", 511, 72, 10, AMBER);
    DrawRectangle(511, 87, 97, 1, RIM);
    label(TextFormat("Tahan %d wave", WAVE_COUNT), 511, 97, 10, CREAM);
    label("Lindungi shelter.", 511, 116, 10, CREAM);
    panel((Rectangle){ 438, 289, 181, 28 }, METAL);
    label("POS KOMANDO  /  2042", 451, 298, 10, CREAM);
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
    label("DANA", 237, 19, 10, MUTED);
    label(TextFormat("%03d", g->money), 237, 31, 10, AMBER);
    label(TextFormat("HP SHELTER  %02d/%02d", g->hp, MAX_HP), 322, 19, 10, CREAM);
    for (int i = 0; i < MAX_HP; i++) {
        DrawRectangle(322+i*8, 35, 6, 4, i < g->hp ? (g->hp <= 5 ? ALERT_RED : MINT) : INK);
    }
    button(pauseButton, "JEDA", true, false, mouse);

    panel((Rectangle){ 494, 60, 136, 266 }, METAL);
    label("PELETON", 502, 71, 10, CREAM);
    for (int kind = 0; kind < TOWER_KIND_COUNT; kind++) {
        Rectangle card = shop_card(kind);
        int y = (int)card.y;
        bool active = app->placing == kind;
        bool hover = over(mouse, card);
        DrawRectangleRec(card, active ? (Color){ 82, 79, 46, 255 } :
            hover ? (Color){ 60, 73, 54, 255 } : (Color){ 47, 58, 46, 255 });
        DrawRectangleLines(500, y, 124, 28, active ? AMBER : RIM);
        if (active || (hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT))) DrawRectangle(501, y+1, 2, 26, AMBER);
        DrawRectangle(506, y+3, 29, 22, INK);
        Tower icon = { .kind = (TowerKind)kind, .pos = { 520, y+21 }, .facing = 1 };
        draw_tower(&icon, app->reducedMotion ? 0 : g->time, WHITE);
        const TowerSpec *spec = &tower_specs[kind];
        label(spec->name, 540, y+4, 10, CREAM);
        label(TextFormat("%d dana", spec->price), 540, y+15, 10, g->money >= spec->price ? AMBER : ALERT_RED);
        label(TextFormat("[%d]", kind+1), 602, y+15, 10, MUTED);
    }
    DrawRectangle(501, 181, 122, 1, RIM);
    if (app->selected >= 0 || app->placing >= 0) {
        const Tower *t = app->selected >= 0 ? &g->towers[app->selected] : NULL;
        const TowerSpec *spec = &tower_specs[t ? t->kind : (TowerKind)app->placing];
        int level = t ? t->level : 0;
        const TowerStats *stats = &spec->levels[level];
        label(TextFormat("%s   LV %d", spec->name, level+1), 503, 187, 10, AMBER);
        label(TextFormat("Damage    %d", stats->damage), 503, 201, 10, CREAM);
        label(TextFormat("Jarak     %d", stats->range), 503, 214, 10, CREAM);
        label(TextFormat("Reload    %.2fs", stats->reload), 503, 227, 10, CREAM);
        if (t) {
            int price = stats->upgradeCost;
            button(upgradeButton, price ? TextFormat("UPGRADE %d", price) : "LEVEL MAKS",
                   price > 0 && g->money >= price && can_manage(g), false, mouse);
            button(sellButton, TextFormat("JUAL +%d", sale_value(t)), can_manage(g), false, mouse);
        } else if (spec->splash) {
            label(TextFormat("Radius ledakan %d", spec->splash), 503, 246, 10, MUTED);
        }
    } else {
        label(TextFormat("Peleton aktif %02d/%02d", g->towerCount, MAX_TOWERS), 503, 190, 10, CREAM);
        label("Pilih kartu untuk", 503, 216, 10, MUTED);
        label("membeli. Klik peleton", 503, 230, 10, MUTED);
        label("untuk mengelolanya.", 503, 244, 10, MUTED);
    }
    button(startButton, g->state == PREPARE ? "MULAI WAVE" : "WAVE AKTIF",
           g->state == PREPARE, true, mouse);

    DrawRectangle(13, 335, 613, 1, RIM);
    if (app->noticeTime > 0) {
        label(app->notice, 15, 343, 10, CREAM);
    } else if (app->placing >= 0) {
        label(TextFormat("Klik di peta untuk menempatkan %s.", tower_specs[app->placing].name), 15, 343, 10, AMBER);
    } else if (g->lastBonus) {
        label(TextFormat("Bonus wave +%d dana%s", g->lastBonus, g->lastHeal ? "  /  Shelter +1 HP" : ""),
              15, 343, 10, MINT);
    } else if (g->state == PREPARE && g->wave == 1) {
        label("Operator: Tempatkan peleton, lalu tekan Spasi.", 15, 343, 10, CREAM);
    } else if (g->state == PREPARE) {
        label("SPASI Mulai wave   /   Klik kanan Batal", 15, 343, 10, MUTED);
    } else {
        label("1-3 Pilih peleton   /   Klik kanan Batal", 15, 343, 10, MUTED);
    }
    label("F11 Layar penuh", 541, 343, 10, MUTED);
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
        Vector2 p = t->pos;
        DrawCircleLines((int)p.x, (int)p.y, tower_specs[t->kind].levels[t->level].range, Fade(AMBER, 0.63f));
        DrawEllipseLines((int)p.x, (int)p.y, 10, 4, AMBER);
    }
    EndScissorMode();
}

static void draw_overlay(const App *app, Vector2 mouse)
{
    DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(INK, 0.78f));
    panel((Rectangle){ 209, 117, 222, app->paused ? 166 : 143 }, METAL);
    const char *title = app->paused ? "OPERASI DIJEDA" :
                       app->game.state == WON ? "SHELTER BERTAHAN" : "SHELTER JATUH";
    label(title, 222, 132, 14, app->game.state == LOST ? ALERT_RED : CREAM);
    const char *summary = app->paused ? "ESC untuk melanjutkan." :
        app->game.state == WON ? TextFormat("Sisa HP %02d   /   Bonus +%d", app->game.hp, app->game.lastBonus) :
        TextFormat("Gerbang jebol di wave %02d.", app->game.wave);
    label(summary, 223, 155, 10, MUTED);
    if (!app->paused) {
        label(app->game.state == WON ? "Operator: Shelter aman." : "Operator: Siapkan ulang.", 223, 168, 10, CREAM);
        draw_portrait(375, 127);
    }
    button(resumeButton, app->paused ? "LANJUTKAN" : "COBA LAGI  [ENTER]", true, true, mouse);
    if (app->paused) {
        button(pauseSettingsButton, "PENGATURAN", true, false, mouse);
        button(pauseMenuButton, "MENU UTAMA", true, false, mouse);
    } else {
        button(menuButton, "MENU UTAMA", true, false, mouse);
    }
}

static void draw_settings(const App *app, Vector2 mouse)
{
    DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(INK, .84f));
    panel((Rectangle){ 179, 69, 282, 244 }, METAL);
    label("PENGATURAN", 197, 88, 16, CREAM);
    label("Redupkan kedipan dan gerak dekoratif.", 198, 126, 10, MUTED);
    button(motionButton, app->reducedMotion ? "KURANGI ANIMASI   /   AKTIF" :
           "KURANGI ANIMASI   /   NONAKTIF", true, app->reducedMotion, mouse);
    DrawRectangle(198, 185, 244, 1, RIM);
    label("VOLUME MUSIK", 198, 199, 10, CREAM);
    label("PANAH KIRI / KANAN", 333, 199, 10, MUTED);
    button(volumeDown, "-", app->musicVolume > 0, false, mouse);
    button(volumeUp, "+", app->musicVolume < 100, false, mouse);
    const char *volume = TextFormat("%d%%", app->musicVolume);
    label(volume, 320-MeasureText(volume, 10)/2, 222, 10, AMBER);
    for (int i = 0; i < 10; i++)
        DrawRectangle(256+i*13, 237, 11, 5, i < app->musicVolume/10 ? AMBER : INK);
    if (app->settingsSaveFailed) label("Pilihan berlaku untuk sesi ini.", 198, 254, 10, AMBER);
    button(settingsBackButton, "KEMBALI  [ESC]", true, false, mouse);
}

static MusicCue current_cue(const App *app)
{
    if (app->menu) return MUSIC_MENU;
    if (app->story.playing) return MUSIC_STORY;
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
    float scale = fminf((float)screenWidth/WIDTH, (float)screenHeight/HEIGHT);
    if (scale >= 1) scale = floorf(scale);
    float w = WIDTH*scale, h = HEIGHT*scale;
    return (Rectangle){ floorf((screenWidth-w)/2), floorf((screenHeight-h)/2), w, h };
}

static Vector2 window_to_canvas(Vector2 cursor, Rectangle destination)
{
    if (destination.width <= 0 || destination.height <= 0) return (Vector2){-1,-1};
    return (Vector2){
        (cursor.x-destination.x)*WIDTH/destination.width,
        (cursor.y-destination.y)*HEIGHT/destination.height
    };
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
    const float fixedStep = 1.0f/60.0f;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        Rectangle dest = viewport(GetScreenWidth(), GetScreenHeight());
        Vector2 mouse = window_to_canvas(GetMousePosition(), dest);
        if (update_input(&app, mouse)) break;
        float dt = fminf(GetFrameTime(), 0.1f);
        if (app.story.playing) {
            accumulator = 0;
            tick_story(&app.story, dt);
        } else if (!app.menu && !app.paused) {
            app.noticeTime -= dt;
            accumulator += dt;
            while (accumulator >= fixedStep) {
                tick_game(&app.game, fixedStep);
                accumulator -= fixedStep;
            }
        } else {
            accumulator = 0;
            if (app.menu && !app.settings) app.game.time += dt;
        }
        MusicCue cue = current_cue(&app);
        bool warning = cue == MUSIC_CRISIS && app.game.state == CRISIS_WARNING;
        float seek = app.storyJump ? app.story.scene*STORY_SCENE_SECONDS : -1;
        update_game_audio(&audio, cue, warning, app.paused && !app.menu, app.musicVolume, dt, seek);
        app.storyJump = false;

        BeginTextureMode(frame);
        ClearBackground(INK);
        Vector2 underlyingMouse = app.settings || app.paused ? (Vector2){ -1, -1 } : mouse;
        if (app.story.playing) {
            draw_story(&app.story, app.reducedMotion);
            button(skipButton, "SKIP  [ESC]", true, false, mouse);
        } else if (app.menu) {
            draw_menu(&app, underlyingMouse);
        } else {
            draw_map(&app.game, app.reducedMotion);
            draw_combat(&app.game, app.reducedMotion);
            if (!app.paused && app.game.state != WON && app.game.state != LOST) draw_placement(&app, mouse);
            draw_hud(&app, underlyingMouse);
            draw_crisis_warning(&app.game, app.reducedMotion);
            if (app.paused || app.game.state == WON || app.game.state == LOST)
                draw_overlay(&app, app.settings ? underlyingMouse : mouse);
        }
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
