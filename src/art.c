#include "art.h"

#include <math.h>

static const Color olive = { 60, 72, 48, 255 };
static const Color deep = { 44, 56, 40, 255 };
static const Color dirt = { 114, 104, 72, 255 };
static const Color shadow = { 25, 33, 26, 160 };
static RenderTexture2D mapPlate;

void label(const char *s, int x, int y, int size, Color color)
{
    DrawText(s, x, y, size, color);
}

void panel(Rectangle r, Color fill)
{
    int x = (int)r.x, y = (int)r.y, w = (int)r.width, h = (int)r.height;
    DrawRectangle(x+2, y+3, w, h, INK);
    DrawRectangle(x, y, w, h, fill);
    DrawRectangleLines(x, y, w, h, RIM);
    DrawRectangle(x+1, y+1, w-2, 1, Fade(MUTED, 0.25f));
}

static void screw(int x, int y)
{
    DrawRectangle(x, y, 3, 3, INK);
    DrawPixel(x+1, y+1, RIM);
}

static void tuft(int x, int y, int kind)
{
    Color leaf = kind ? (Color){ 85, 99, 56, 255 } : (Color){ 69, 84, 51, 255 };
    DrawRectangle(x-1, y, 7, 2, deep);
    DrawRectangle(x+2, y-4, 1, 5, leaf);
    DrawRectangle(x, y-2, 1, 3, leaf);
    DrawRectangle(x+4, y-3, 1, 3, leaf);
}

static void scrub(int x, int y)
{
    DrawEllipse(x+2, y+4, 13, 4, shadow);
    DrawRectangle(x-8, y-4, 17, 8, deep);
    DrawRectangle(x-6, y-7, 12, 9, (Color){ 56, 80, 48, 255 });
    DrawRectangle(x-4, y-9, 7, 3, (Color){ 77, 101, 55, 255 });
    DrawRectangle(x-7, y-3, 5, 2, (Color){ 79, 101, 59, 255 });
    DrawPixel(x+4, y-4, MINT);
}

static void shelter(void)
{
    panel((Rectangle){ 357, 294, 64, 27 }, deep);
    DrawRectangle(362, 283, 54, 12, (Color){ 102, 111, 80, 255 });
    DrawRectangle(358, 291, 62, 4, (Color){ 132, 137, 99, 255 });
    DrawRectangle(371, 298, 36, 15, INK);
    DrawRectangle(373, 300, 32, 2, RIM);
    for (int i = 0; i < 6; i++) DrawRectangle(374+i*5, 302, 2, 10, METAL);
    DrawRectangle(380, 285, 18, 2, METAL);
    DrawRectangle(385, 288, 8, 1, CREAM);
    DrawRectangle(409, 275, 1, 15, CREAM);
    screw(362, 315); screw(414, 315);
}

static void draw_terrain(void)
{
    BeginScissorMode(12, 60, 474, 266);
    DrawRectangleRec(battlefield, olive);
    for (int y = 62; y < 327; y += 8) {
        for (int x = 12; x < 486; x += 8) {
            unsigned int seed = ((unsigned int)(x*7151+y*4057))*2654435761u;
            DrawRectangle(x, y, 8, 8, Fade((seed & 32) ? CREAM : INK, (seed%7)*0.009f));
            if ((seed % 19) == 0) tuft(x+2, y+4, seed&1);
            if ((seed % 43) == 0) {
                DrawRectangle(x, y+2, 4, 2, deep);
                DrawPixel(x+1, y+2, RIM);
            }
        }
    }
    for (int i = 0; i < PATH_COUNT-1; i++) {
        DrawLineEx(route[i], route[i+1], 26, deep);
        DrawCircleV(route[i], ROAD_RADIUS, deep);
    }
    DrawCircleV(route[PATH_COUNT-1], ROAD_RADIUS, deep);
    for (int i = 0; i < PATH_COUNT-1; i++) {
        DrawLineEx(route[i], route[i+1], 22, dirt);
        DrawCircleV(route[i], 11, dirt);
        Vector2 a = route[i], b = route[i+1];
        int length = (int)(fabsf(b.x-a.x)+fabsf(b.y-a.y));
        for (int step = 9; step < length-8; step += 11) {
            int x = (int)(a.x+(b.x-a.x)*step/length);
            int y = (int)(a.y+(b.y-a.y)*step/length);
            DrawRectangle(x+((step%3)-1)*4, y+((step%5)-2)*2, 4, 1, Fade(INK, 0.24f));
            if (step%3 == 0) DrawPixel(x-4, y+5, Fade(CREAM, 0.45f));
        }
    }
    DrawCircleV(route[PATH_COUNT-1], 11, dirt);
    DrawTriangle((Vector2){ 25, 101 }, (Vector2){ 25, 111 }, (Vector2){ 33, 106 }, CREAM);
    for (int x = 18; x <= 479; x += 14) {
        DrawRectangle(x, 64, 9, 2, deep);
        DrawRectangle(x, 320, 8, 2, deep);
        DrawRectangle(x, 64, 2, 4, RIM);
        DrawRectangle(x, 318, 2, 4, RIM);
    }
    const Vector2 plants[] = {
        { 29, 74 }, { 204, 82 }, { 227, 93 }, { 467, 84 }, { 453, 97 },
        { 31, 302 }, { 47, 310 }, { 218, 307 }, { 241, 310 }, { 470, 306 }
    };
    for (unsigned i = 0; i < sizeof plants/sizeof plants[0]; i++) {
        scrub((int)plants[i].x, (int)plants[i].y);
    }
    shelter();
    label("MASUK", 18, 121, 10, CREAM);
    label("SHELTER", 370, 264, 10, CREAM);
    EndScissorMode();
    DrawRectangleLinesEx(battlefield, 1, RIM);
}

void load_map_art(void)
{
    mapPlate = LoadRenderTexture(WIDTH, HEIGHT);
    if (!mapPlate.id) return;
    SetTextureFilter(mapPlate.texture, TEXTURE_FILTER_POINT);
    BeginTextureMode(mapPlate);
    ClearBackground(BLANK);
    draw_terrain();
    EndTextureMode();
}

void unload_map_art(void)
{
    if (mapPlate.id) UnloadRenderTexture(mapPlate);
    mapPlate = (RenderTexture2D){0};
}

void draw_map_preview(Rectangle bounds)
{
    if (mapPlate.id) {
        DrawTexturePro(mapPlate.texture, (Rectangle){ 0, 0, WIDTH, -HEIGHT },
                       bounds, (Vector2){0,0}, 0, WHITE);
    }
}

void draw_map(const Game *g, bool reduced)
{
    if (mapPlate.id) draw_map_preview((Rectangle){0, 0, WIDTH, HEIGHT});
    else draw_terrain();
    float time = reduced ? 0 : g->time;
    int furl = (int)(time*4)%4 == 2;
    DrawRectangle(410, 276, 9-furl, 3, ALERT_RED);
    DrawRectangle(410, 279, 9-furl, 3, CREAM);
    Color lamp = g->hp > 5 ? MINT : ALERT_RED;
    DrawPixel(365, 302, lamp);
    DrawPixel(413, 302, lamp);
    if (g->hp > 0 && !reduced) {
        DrawPixel(410, 288, Fade(MINT, .55f+.35f*sinf(time*2)));
        for (int i = 0; i < 11; i++) {
            int x = 166+((i*37+(int)(time*(8+i%4)))%170);
            int y = (i%2 ? 76 : 301)+(i*13)%10;
            DrawRectangle(x, y, 1+i%2, 1, Fade(CREAM, .11f+.08f*sinf(time+i)));
        }
    }
}

static void pixel_sprite(const char *const rows[], int w, int h, int x, int y,
                         int facing, int scale, const Color colors[], Color tint)
{
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w && rows[j][i] != '\0'; i++) {
            char value = rows[j][i];
            if (value == '.') continue;
            Color c = colors[value-'0'];
            c = (Color){ c.r*tint.r/255, c.g*tint.g/255, c.b*tint.b/255, c.a*tint.a/255 };
            DrawRectangle(x+(facing > 0 ? i : w-i-1)*scale, y+j*scale, scale, scale, c);
        }
    }
}

static void archer(Vector2 p, int facing, float recoil, float time, Color tint)
{
    static const char *const rest[] = {
        "...0000.......", "..011110......", "..012110......", "..032220......",
        "...0330....0..", "..001100..04..", ".0115510.040..", ".0155510304...",
        ".0155111004...", "..011110..04..", "..011110...0..", "..011011......",
        "..010.010.....", "..000.000....."
    };
    static const char *const release[] = {
        "...0000.......", "..011110......", "..012110......", "..032220......",
        "...0330.....0.", "..001100...04.", ".011551000044.", ".015551333034.",
        ".015511000044.", "..011110...04.", "..011110....0.", "..011011......",
        "..010.010.....", "..000.000....."
    };
    static const Color colors[] = {
        { 24, 30, 25, 255 }, { 103, 127, 81, 255 }, { 196, 204, 135, 255 },
        { 215, 162, 111, 255 }, { 226, 182, 97, 255 }, { 64, 85, 59, 255 }
    };
    int x = (int)roundf(p.x), y = (int)roundf(p.y);
    int bob = recoil == 0 && ((int)(time*2) & 1);
    DrawEllipse(x, y, 10, 4, Fade(shadow, tint.a/255.0f));
    int shift = recoil > 0 && recoil <= 0.11f ? -facing : 0;
    pixel_sprite(recoil > 0.11f ? release : rest, 14, 14, x-7+shift, y-13-bob, facing, 1, colors, tint);
}

void draw_tower(const Tower *t, float time, Color tint)
{
    static const char *const cannon[] = {
        "....000............", "...05500...........", "...05300000000000..",
        "...003322222222220.", "..003331111111110..", ".00333100000000....",
        ".033330............", ".0333300...........", ".00330000..........",
        "..00000000.........", ".0555555550........", "00505050500........",
        ".000000000........."
    };
    static const char *const sniper[] = {
        "....0000............", "...011110...........", "...011210...........",
        "....0330............", "...001000400000040..", "..0115513334444440..",
        "..011555000000000...", "..0115550...........", "..011550...........",
        "...01500............", "...010100...........", "..010.010...........",
        "..000.000..........."
    };
    static const Color cannonColors[] = {
        { 21,28,23,255 }, { 63,83,72,255 }, { 167,180,153,255 },
        { 113,127,94,255 }, { 239,171,79,255 }, { 76,89,74,255 }
    };
    static const Color sniperColors[] = {
        { 21,28,23,255 }, { 116,122,75,255 }, { 206,218,150,255 },
        { 205,160,110,255 }, { 169,185,173,255 }, { 65,84,62,255 }
    };
    int x = (int)roundf(t->pos.x), y = (int)roundf(t->pos.y);
    int crouch = t->reload <= .27f ? 2 : 0;
    if (t->kind == ARCHER) {
        archer(t->pos, t->facing, t->recoil, time, tint);
    } else if (t->kind == CANNON) {
        int kick = t->recoil > .21f ? 3*t->facing : 0;
        DrawEllipse(x, y, 10, 4, Fade(shadow, tint.a/255.0f));
        pixel_sprite(cannon, 19, 13, x-9-kick, y-12, t->facing, 1, cannonColors, tint);
        if (t->reload > .25f && t->recoil < .1f) {
            DrawRectangle(x-3*t->facing, y-5, 2, 2, Fade(AMBER, tint.a*.62f/255));
        }
    } else {
        int kick = t->recoil > .16f ? t->facing : 0;
        DrawEllipse(x, y, 10, 4, Fade(shadow, tint.a/255.0f));
        pixel_sprite(sniper, 20, 13, x-10-kick, y-13+crouch, t->facing, 1, sniperColors, tint);
        if (t->reload > 0 && t->reload <= .27f) {
            DrawPixel(x+8*t->facing, y-6, Fade(MINT, tint.a/255.0f));
        }
    }
    if (t->level == MAX_LEVEL-1) {
        if (t->kind == ARCHER) {
            DrawRectangle(x-5*t->facing-1, y-9, 3, 6, Fade(AMBER, tint.a/255.0f));
            DrawPixel(x-5*t->facing, y-10, Fade(CREAM, tint.a/255.0f));
        } else if (t->kind == CANNON) {
            DrawRectangle(x-4, y-5, 7, 3, Fade(AMBER, tint.a/255.0f));
            DrawPixel(x, y-4, INK);
        } else {
            DrawRectangle(x+3*t->facing-1, y-9+crouch, 4, 2, Fade(AMBER, tint.a/255.0f));
        }
    }
    for (int rank = 0; rank < t->level; rank++) {
        DrawRectangle(x-3+4*rank, y+3, 3, 2, Fade(AMBER, tint.a/255.0f));
    }
}

static void draw_enemy(const Enemy *z, bool reduced)
{
    static const char *const body[] = {
        "....0000.....", "...011110....", "...012110....", "....1110.....",
        "...001000....", "..03333300...", ".0133443110..", ".0103433010..",
        "..0033330.0..", "....0550....."
    };
    static const char *const runner[] = {
        ".........000...", "........01110..", "........01120..", ".......00110...",
        "....00033000...", "...03333333100.", "..0333440001110", ".0133330....00.",
        "0010330........", "....0550......."
    };
    static const char *const mutant[] = {
        ".....00000.....", "....0111110....", "....0121120....", "...001111000...",
        "..03300003330..", ".0333444433330.", "033344444433330", "011344444331110",
        "011344444331110", ".001133331100..", "...00555500...."
    };
    static const char *const boss[] = {
        "......0.....0......", ".....010...010.....", ".....011000110.....",
        "....00111111100....", "...0330121210330...", "..033303111303330..",
        "..033330000033330..", ".03334443334443330.", "0113344444444331110",
        "0111344222443111110", ".01113444444311110.", "..00133444331000...",
        "....0055555000....."
    };
    Color colors[] = {
        { 30, 40, 32, 255 }, { 158, 175, 118, 255 }, { 237, 231, 147, 255 },
        { 117, 95, 82, 255 }, { 153, 119, 99, 255 }, { 71, 76, 65, 255 }
    };
    int x = (int)z->pos.x, y = (int)z->pos.y;
    static const float rates[] = {7, 13, 5, 4};
    int step = (int)(z->stride*rates[z->kind]) % 4;
    int bob = step == 1 || step == 3;
    int face = z->next < PATH_COUNT && route[z->next].x < z->pos.x ? -1 : 1;
    const char *const *sprite = body;
    int w = 13, h = 10, scale = 1;
    if (z->kind == RUNNER) {
        sprite = runner; w = 15;
        colors[3] = (Color){ 133,86,65,255 }; colors[4] = (Color){ 204,117,77,255 };
    } else if (z->kind == MUTANT) {
        sprite = mutant; w = 15; h = 11; scale = 2;
        colors[3] = (Color){ 74,99,76,255 }; colors[4] = (Color){ 109,137,92,255 };
    } else if (z->kind == BOSS) {
        sprite = boss; w = 19; h = 13; scale = 2;
        colors[3] = (Color){ 109,60,61,255 }; colors[4] = (Color){ 156,79,75,255 };
    }
    if (z->hit > 0 && !reduced) {
        colors[1] = CREAM;
        colors[3] = ALERT_RED;
    }
    DrawEllipse(x, y, w*scale/2, 3*scale, shadow);
    DrawRectangle(x-scale*(3-(step == 1)), y-3*scale-bob, 2*scale, 4*scale+bob, colors[5]);
    DrawRectangle(x+scale*(1-(step == 3)), y-4*scale, 2*scale, 4*scale+bob, colors[5]);
    pixel_sprite(sprite, w, h, x-w*scale/2, y-(h+3)*scale-bob, face, scale, colors, WHITE);
    if (z->hp < z->maxHp) {
        int bar = scale*10;
        int top = y-(h+3)*scale-4;
        DrawRectangle(x-bar/2, top, bar, 2, INK);
        DrawRectangle(x-bar/2+1, top, ((bar-2)*z->hp+z->maxHp-1)/z->maxHp, 1, AMBER);
    }
}

void draw_combat(const Game *g, bool reduced)
{
    BeginScissorMode(12, 60, 474, 266);
    for (int i = 0; i < MAX_ENEMIES; i++) if (g->enemies[i].active) draw_enemy(&g->enemies[i], reduced);
    for (int i = 0; i < g->towerCount; i++) draw_tower(&g->towers[i], reduced ? 0 : g->time+i*.11f, WHITE);
    for (int i = 0; i < MAX_FX; i++) {
        const Effect *fx = &g->effects[i];
        if (fx->left <= 0) continue;
        float progress = 1-fx->left/fx->duration;
        if (fx->kind == DUST && reduced) {
            continue;
        } else if (fx->kind == DUST) {
            for (int j = 0; j < 7; j++) {
                float radians = j*0.9f;
                int x = (int)(fx->from.x + cosf(radians)*(4+progress*12));
                int y = (int)(fx->from.y + sinf(radians)*(2+progress*5));
                DrawRectangle(x, y, 2, 2, Fade(CREAM, 0.65f*(1-progress)));
            }
        } else if (fx->kind == SHOT) {
            float tail = fmaxf(0, progress-0.25f);
            Vector2 a = { fx->from.x+(fx->to.x-fx->from.x)*tail,
                          fx->from.y+(fx->to.y-fx->from.y)*tail };
            Vector2 b = { fx->from.x+(fx->to.x-fx->from.x)*progress,
                          fx->from.y+(fx->to.y-fx->from.y)*progress };
            DrawLineV(a, b, fx->amount == SNIPER ? CREAM : AMBER);
        } else if (fx->kind == EXPLOSION) {
            DrawCircleLines((int)fx->from.x, (int)fx->from.y, fx->amount, Fade(AMBER, .78f*(1-progress)));
            for (int j = 0; !reduced && j < 10; j++) {
                float radians = j*.628f;
                int x = (int)(fx->from.x + cosf(radians)*(3+progress*(fx->amount-3)));
                int y = (int)(fx->from.y + sinf(radians)*(3+progress*(fx->amount-3)));
                DrawRectangle(x, y, 3, 2, Fade(AMBER, 1-progress));
            }
        } else if (fx->kind == UPGRADE) {
            int x = (int)fx->from.x, y = (int)fx->from.y;
            float expansion = reduced ? 0 : progress;
            DrawEllipseLines(x, y, 10+expansion*10, 4+expansion*4, Fade(CREAM, 1-progress));
            for (int j = 0; !reduced && j < 6; j++) {
                DrawRectangle(x-10+j*4, y-3-(int)(progress*(9+j%3*5)), 2, 3, Fade(AMBER, 1-progress));
            }
        } else {
            Color color = fx->kind == REWARD ? MINT : ALERT_RED;
            label(TextFormat("%c%d", fx->kind == REWARD ? '+' : '-', fx->amount),
                  (int)fx->from.x-5, (int)(fx->from.y-9-(reduced ? 0 : progress*13)), 10, Fade(color, 1-progress*0.7f));
        }
    }
    EndScissorMode();
}

void draw_crisis_warning(const Game *g, bool reduced)
{
    if (g->state != CRISIS_WARNING) return;
    float pulse = reduced ? .70f : .5f+.5f*sinf(g->phaseTime*7.0f);
    for (int band = 0; band < 6; band++) {
        int inset = band*3;
        Color edge = Fade(ALERT_RED, (.34f+.36f*pulse)*(6-band)/6);
        DrawRectangle(inset, inset, WIDTH-2*inset, 3, edge);
        DrawRectangle(inset, HEIGHT-inset-3, WIDTH-2*inset, 3, edge);
        DrawRectangle(inset, inset+3, 3, HEIGHT-2*inset-6, edge);
        DrawRectangle(WIDTH-inset-3, inset+3, 3, HEIGHT-2*inset-6, edge);
    }
}

void draw_portrait_scaled(int x, int y, int scale)
{
    static const char *const face[] = {
        ".....000000.....", "...0011111100...", "..011111111110..", ".0111222211110..",
        ".01111111111110.", ".00111000001100.", "..013333333310..", "..013443344310..",
        "..053333333350..", "..053333433350..", "...0333443300...", "....03333300....",
        "...0003330000...", ".00111000111100.", "0111111111111110", "0111511111151110"
    };
    static const Color colors[] = {
        { 15, 21, 18, 255 }, { 89, 107, 75, 255 }, { 146, 158, 101, 255 },
        { 189, 136, 96, 255 }, { 94, 66, 50, 255 }, { 226, 159, 63, 255 }
    };
    pixel_sprite(face, 16, 16, x, y, 1, scale, colors, WHITE);
}

void draw_portrait(int x, int y)
{
    draw_portrait_scaled(x, y, 3);
}
