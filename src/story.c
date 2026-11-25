#include "story.h"
#include "art.h"

#include <stdlib.h>

static const Color navy = { 22, 33, 39, 255 };
static const Color steel = { 81, 100, 99, 255 };
static const Color coal = { 26, 31, 32, 255 };
static const Color ochre = { 167, 121, 72, 255 };

static void block(int x, int y, int w, int h, Color color)
{
    DrawRectangle(x, y, w, h, color);
}

static void earth(float t)
{
    block(24, 20, 592, 245, (Color){14, 22, 28, 255});
    for (int i = 0; i < 105; i++) {
        int x = 26+(i*149+i*i*17)%585, y = 27+(i*i*29+i*47)%229;
        int size = i%13 == 0 ? 2 : 1;
        block(x, y, size, size, i%4 == 0 ? CREAM : steel);
    }
    const int cx = 390+(int)(2*t), cy = 141, radius = 104;
    static const Rectangle land[] = {
        {-93,-57,79,12}, {-83,-45,102,15}, {-73,-30,96,17}, {-64,-13,84,12},
        {-37,-1,28,13}, {-23,12,11,15}, {1,-1,23,19}, {13,18,10,9},
        {29,16,15,13}, {28,31,12,7}, {13,39,29,6}, {45,41,13,5},
        {59,49,29,12}, {47,61,38,12}, {57,73,20,7}, {-40,-70,64,14}
    };
    static const Rectangle clouds[] = {
        {-91,-21,22,5}, {-76,-19,18,4}, {-61,-16,16,3}, {-27,33,21,4},
        {-41,36,24,4}, {-53,39,19,3}, {35,-57,35,5}, {53,-52,28,4},
        {-82,15,20,4}, {-78,19,31,3}, {32,66,22,4}, {64,5,26,5}, {76,10,17,4}
    };
    for (int y = -radius; y < radius; y += 3) {
        for (int x = -radius; x < radius; x += 3) {
            if (x*x+y*y > radius*radius) continue;
            bool terrain = false;
            for (unsigned i = 0; i < sizeof land/sizeof land[0]; i++) {
                if (CheckCollisionPointRec((Vector2){x,y}, land[i])) { terrain = true; break; }
            }
            Color c = terrain ? (Color){ 105, 139, 102, 255 } : (Color){ 53, 99, 115, 255 };
            unsigned grain = (unsigned)(x*7417+y*7777)*2654435761u;
            if (grain%9 == 0) c = ColorBrightness(c, .07f);
            for (unsigned i = 0; i < sizeof clouds/sizeof clouds[0]; i++) {
                if (CheckCollisionPointRec((Vector2){x,y}, clouds[i])) {
                    c = ColorLerp(c, CREAM, .45f);
                    break;
                }
            }
            if (x-y > 77) c = ColorBrightness(c, -.33f);
            if (x*x+y*y > (radius-4)*(radius-4)) c = steel;
            block(cx+x, cy+y, 3, 3, c);
        }
    }
    block(51, 98, 139, 1, steel);
    label("2042", 50, 111, 47, CREAM);
    label("ARSIP POS KOMANDO", 53, 169, 10, AMBER);
    DrawLine(196, 160, 261, 160, steel);
    DrawLine(261, 160, 285, 143, steel);
}

static void skyline(int base, int offset, Color color, bool damaged)
{
    for (int i = 0; i < 24; i++) {
        int x = 24+i*26-offset, w = 16+(i*11)%12, h = 18+(i*37+i*i)%94;
        block(x, base-h, w, h, color);
        if (i%5 == 0) block(x+w/2, base-h-9, 2, 9, color);
        if (!damaged) {
            for (int dy = 9; dy < h-4; dy += 9) {
                if ((dy+i)%4 != 0) block(x+4, base-h+dy, 2, 3, i%5 == 0 ? ochre : steel);
                if ((dy+i)%5 != 0) block(x+10, base-h+dy, 2, 3, coal);
            }
        } else if (i%3 != 1) {
            block(x+w-7, base-h, 7, 9+(i%4)*5, navy);
            block(x+4, base-h+24, 5, 2, steel);
        }
    }
}

static void outbreak(float t)
{
    for (int i = 0; i < 11; i++) block(24, 20+i*22, 592, 23, ColorLerp(navy, steel, .07f*i));
    skyline(161, 5+(int)t, steel, false);
    skyline(209, 0, coal, false);
    block(24, 209, 592, 58, (Color){ 53, 58, 54, 255 });
    for (int x = 35; x < 600; x += 34) block(x, 230, 18, 2, steel);
    const Vector2 cities[] = {{123,115}, {223,153}, {345,83}, {445,133}, {538,111}};
    for (int i = 0; i < 5; i++) {
        if (i) DrawLineV(cities[i-1], cities[i], Fade(ALERT_RED, .6f));
        int x = (int)cities[i].x, y = (int)cities[i].y;
        int pulse = 10+((int)(t*8+i*4)%6)*2;
        DrawRectangleLines(x-pulse/2, y-pulse/2, pulse, pulse, Fade(ALERT_RED, .65f));
        block(x-2, y-2, 5, 5, ALERT_RED);
    }
    block(36, 36, 106, 19, navy);
    label("SINYAL DARURAT", 42, 41, 10, AMBER);
}

static void ruins(float t)
{
    block(24, 20, 592, 245, navy);
    for (int i = 0; i < 9; i++) block(25, 24+i*14, 590, 14, ColorLerp(navy, ochre, .051f*i));
    DrawCircle(453, 113, 44, (Color){168,120,79,255});
    for (int i = 0; i < 5; i++) block(371, 88+i*13, 164, 3, Fade(navy, .3f));
    skyline(210, 7+(int)t, coal, true);
    block(24, 207, 592, 58, coal);
    for (int i = 0; i < 46; i++) {
        int x = 34+(i*i*31+i*57)%568, y = 205+(i*13)%55;
        block(x, y, 3+i%8, 2, i%6 == 0 ? ochre : steel);
    }
    block(55, 126, 6, 104, coal);
    block(58, 126, 55, 5, coal);
    block(108, 129, 6, 9, coal);
    block(101, 137, 19, 4, steel);
    for (int i = 0; i < 28; i++) {
        int x = 32+(i*71+(int)(t*(i%3+1)))%575, y = 34+(i*17)%157;
        block(x, y, 2, 1, Fade(CREAM, .26f));
    }
}

static void survivor(int x, int y, bool step, Color coat)
{
    block(x+5, y, 8, 7, coal);
    block(x+6, y+2, 6, 5, ochre);
    block(x+3, y+7, 11, 11, coat);
    block(x, y+9, 4, 9, navy);
    block(x+12, y+9+(step ? 2 : 0), 4, 7, coat);
    block(x+4, y+17, 4, 8+(step ? 1 : 0), coal);
    block(x+10, y+17, 4, 7+(step ? 0 : 2), coal);
}

static void refuge(float t)
{
    block(24, 20, 592, 245, (Color){ 101, 118, 104, 255 });
    for (int i = 0; i < 5; i++) {
        int x = 42+i*104;
        block(x, 43+(i%3)*15, 48+(i%2)*33, 2, Fade(CREAM, .23f));
        block(x+21, 46+(i%3)*15, 35, 2, Fade(CREAM, .12f));
    }
    for (int i = 0; i < 15; i++) block(24+i*42, 94+(i*37)%30, 42, 79, RIM);
    block(24, 177, 592, 89, (Color){ 75, 83, 57, 255 });
    for (int y = 159; y < 265; y += 3) {
        int width = 12+(y-159)*2;
        block(381-width/2, y, width, 3, (Color){ 128, 114, 80, 255 });
    }
    for (int i = 0; i < 115; i++) {
        int x = 27+(i*59+i*i*11)%584, y = 183+(i*23)%78;
        bool onRoad = abs(x-381) < 6+(y-159);
        block(x, y, 2+i%4, 1, Fade(onRoad ? INK : CREAM, .15f));
    }
    block(246, 174, 285, 5, Fade(INK, .4f));
    block(253, 75, 269, 99, METAL);
    block(250, 77, 275, 9, RIM);
    block(253, 86, 269, 2, INK);
    block(249, 168, 277, 6, RIM);
    for (int x = 261; x < 513; x += 17) block(x, 90, 2, 72, RIM);
    for (int x = 270; x < 500; x += 204) {
        block(x, 109, 29, 22, INK);
        block(x+2, 112, 25, 3, steel);
        for (int y = 117; y < 129; y += 3) block(x+3, y, 23, 1, RIM);
    }
    block(341, 117, 79, 57, INK);
    block(345, 120, 71, 3, AMBER);
    block(345, 123, 4, 49, METAL);
    block(412, 123, 4, 49, METAL);
    block(338, 124, 3, 6, AMBER);
    block(420, 124, 3, 6, AMBER);
    label("SHELTER INDONESIA", 329, 99, 10, CREAM);
    block(469, 49, 2, 29, CREAM);
    block(471, 49, 20, 6, ALERT_RED);
    block(471, 55, 20, 6, CREAM);
    const int shrubs[] = {63, 113, 189, 220, 471, 530, 575};
    for (unsigned i = 0; i < sizeof shrubs/sizeof shrubs[0]; i++) {
        int x = shrubs[i], y = i%2 ? 223 : 245;
        block(x-4, y+4, 29, 5, Fade(INK, .3f));
        block(x, y-4, 19, 10, METAL);
        block(x+3, y-8, 13, 8, RIM);
        block(x+6, y-10, 6, 3, steel);
    }
    for (int i = 0; i < 5; i++) {
        int x = 309+i*23, y = 214-i*10-(int)(t*.6f);
        survivor(x, y, ((int)(t*3)+i)%2, i%2 ? ochre : steel);
    }
}

// Meja pendataan yang sama muncul saat Sari datang dan di akhir permainan.
static void desk(int x, int y, bool fallen, int names)
{
    block(x, y, 122, 7, ochre);
    block(x+5, y+7, 6, 30, coal);
    block(x+111, y+7, 6, 30, coal);
    block(x+26, y-13, 34, 15, CREAM);
    for (int i=0; i<names; i++) block(x+29, y-10+i*3, 13+i, 1, steel);
    block(x+52, y-3, 23, 2, AMBER);
    int rx=x+(fallen ? 85 : 13), ry=y+(fallen ? 24 : -15);
    block(rx, ry, 22, 15, coal);
    block(rx+2, ry+3, 9, 3, steel);
    block(rx+16, ry+4, 3, 2, fallen ? ALERT_RED : MINT);
    DrawLine(rx+19,ry,rx+(fallen ? 34 : 22),ry-(fallen ? 5 : 18),steel);
}

static void ending_scene(const Story *s, float t)
{
    bool lost = s->kind == OVERRUN;
    bool dawn = s->scene == 2;
    Color sky = dawn ? (Color){163,155,120,255} : navy;
    block(24,20,592,245,sky);
    for (int i=0; i<18; i++) {
        int h=12+(i*37)%54;
        block(24+i*34,155-h,32,h,steel);
        block(28+i*34,163-h,4,5,dawn ? navy : ochre);
    }
    block(24,155,592,110,METAL);
    for (int i=0;i<96;i++) {
        int x=28+(i*73)%580, y=165+(i*29)%92;
        block(x,y,2+i%6,1,Fade(steel,.36f));
    }
    block(32,33,88,13,Fade(coal,.8f));
    label(dawn ? "05.41 WIB" : "03.17 WIB",40,35,10,CREAM);
    if (s->scene != 1) {
        for (int i=0; i<7; i++) {
            int x=173+i*44+(int)(t*1.5f), y=151+(i%3)*8;
            survivor(x,y,((int)(t*2)+i)%2, i==2 ? ochre : steel);
            if (lost && dawn) {
                block(x+6,y+2,7,5,MINT);
                block(x+10,y+4,3,1,INK);
                block(x-5,y+9,12,3,MINT);
            }
            if (i==2) { block(x+12,y+10,8,11,ochre); block(x+13,y+12,6,7,CREAM); }
        }
        for (int x=80; x<560; x+=16) block(x,114,3,92,RIM);
        block(73,124,495,4,coal);
        block(73,197,495,5,coal);
        block(316,108,7,98,coal);
        if (!lost && dawn) DrawLineEx((Vector2){320,149},(Vector2){406,98},5,AMBER);
        else {
            int drop=lost ? (int)(t*17) : 0;
            block(300,152,18,6,ochre);
            block(319,152+drop,22,5,AMBER);
        }
        desk(60,223,lost && dawn,3);
    } else {
        block(116,62,410,157,coal);
        block(121,66,400,148,METAL);
        block(122,191,398,23,navy);
        for (int x=122;x<520;x+=33) block(x,192,1,22,RIM);
        block(424,83,75,68,coal);
        block(428,87,67,60,sky);
        for (int x=432;x<496;x+=13) block(x,87,2,60,RIM);
        block(428,118,67,3,steel);
        block(137,113,46,54,ochre);
        block(140,116,40,48,CREAM);
        for (int row=0;row<5;row++) block(144,123+row*7,24-row,1,steel);
        block(245,113,5,23,coal);
        block(235,135,25,3,AMBER);
        block(229,138,37,2,steel);
        label("POS PENDAFTARAN",155,85,14,CREAM);
        if (!lost) survivor(225,139,false,ochre);
        desk(214,172,lost,4);
        block(394,95,17,37,navy);
        block(398,99,9,8,lost ? ALERT_RED : MINT);
    }
}

static void command(float t, bool reduced)
{
    block(24, 20, 592, 245, navy);
    panel((Rectangle){ 55, 51, 152, 152 }, METAL);
    block(62, 58, 138, 138, INK);
    draw_portrait_scaled(75, 73, 7);
    label("OPERATOR SHELTER", 74, 210, 10, AMBER);
    panel((Rectangle){ 232, 51, 351, 181 }, METAL);
    block(240, 59, 335, 166, (Color){36, 55, 47, 255});
    for (int x = 241; x < 575; x += 14) block(x, 60, 1, 164, Fade(MINT, .12f));
    for (int y = 61; y < 225; y += 14) block(241, y, 333, 1, Fade(MINT, .12f));
    for (int i = 1; i < PATH_COUNT; i++) {
        Vector2 a = {251+(route[i-1].x-12)*.69f, 63+(route[i-1].y-60)*.62f};
        Vector2 b = {251+(route[i].x-12)*.69f, 63+(route[i].y-60)*.62f};
        DrawLineEx(a, b, 3, AMBER);
        block((int)b.x-2, (int)b.y-2, 5, 5, CREAM);
    }
    label("PETA PERTAHANAN", 254, 205, 10, CREAM);
    if (t >= 9.0f && !reduced) {
        float transition = (t-9)/3;
        if (transition < .5f) {
            block(24, 20, 592, 245, Fade(INK, transition*2));
        } else {
            block(24, 20, 592, 245, INK);
            draw_map_preview((Rectangle){ 12, -29, 800, 330 });
            block(24, 20, 592, 245, Fade(INK, (1-transition)*2));
        }
    }
}

typedef struct { const char *title, *line1, *line2; } Caption;
static const Caption endings[2][3] = {
    {
        {"PAGAR TIMUR", "Tembakan terakhir berhenti. Pagar masih terkunci.", "Operator: Bu Sari, daftarnya masih di sana?"},
        {"DAFTAR HADIR", "Bu Sari menarik kertas dari bawah radio.", "Bu Sari: Pak Darma?"},
        {"GILIRAN PAGI", "Dari balik pagar seseorang menjawab, \"Hadir, Bu.\"", "Operator menaruh radio dan mengangkat palang."}
    },{
        {"GERENDEL", "Gerendel jatuh di sebelah dalam pagar.", "Operator: Tutup pintu ruang pendaftaran!"},
        {"SALURAN TUJUH", "Radio tergeletak di bawah meja. Lampunya menyala.", "Operator: Bu Sari? Jawab kalau dengar."},
        {"PAGI DI SEKTOR 07", "Di balik pagar, semua warga sudah terinfeksi.", "Bu Sari masih menggenggam papan daftar hadir."}
    }
};
static const Caption scenes[STORY_SCENES] = {
    { "BUMI / 2042", "Wabah global memutus komunikasi antarkota.", "Saluran darurat mulai menyiarkan pesan terakhir." },
    { "KOTA-KOTA TERISOLASI", "Wabah menyebar dari satu kota ke kota berikutnya.", "Stasiun evakuasi kewalahan menampung warga." },
    { "SETELAH GELOMBANG PERTAMA", "Distrik yang dulu padat kini nyaris kosong.", "Semakin sedikit penyintas yang menjawab panggilan radio." },
    { "JALUR MENUJU SHELTER", "Bu Sari menyelipkan daftar warga di bawah radio.", "Operator: Nanti panggil satu-satu, Bu. Sesudah gerbang ditutup." },
    { "PENUNJUKAN KOMANDAN", "Pos komando menyerahkan kendali pertahanan kepadamu.", "Tempatkan peleton dan jaga gerbang shelter." }
};

void begin_story(Story *story)
{
    *story = (Story){ .playing = true };
}

void begin_ending(Story *story, bool won)
{
    *story = (Story){ .kind = won ? EVACUATED : OVERRUN, .playing = true };
}

float story_scene_seconds(const Story *story)
{
    if (story->kind == OPENING) return STORY_SCENE_SECONDS;
    return story->kind == OVERRUN ? 4 : 5;
}

void advance_story(Story *story)
{
    if (!story->playing) return;
    story->scene++;
    story->elapsed = 0;
    if (story->scene == (story->kind == OPENING ? STORY_SCENES : 3)) story->playing = false;
}

void tick_story(Story *story, float dt)
{
    if (!story->playing) return;
    story->elapsed += dt;
    float duration = story_scene_seconds(story);
    while (story->playing && story->elapsed >= duration) {
        float carry = story->elapsed-duration;
        advance_story(story);
        if (story->playing) story->elapsed = carry;
    }
}

void draw_story(const Story *story, bool reduced)
{
    if (!story->playing) return;
    float t = reduced ? 0 : story->elapsed;
    BeginScissorMode(24, 20, 592, 245);
    if (story->kind != OPENING) ending_scene(story, t);
    else switch (story->scene) {
        case 0: earth(t); break;
        case 1: outbreak(t); break;
        case 2: ruins(t); break;
        case 3: refuge(t); desk(153,214,false,3); break;
        case 4: command(t, reduced); break;
    }
    if (!reduced && t < .55f) block(24, 20, 592, 245, Fade(INK, 1-t/.55f));
    EndScissorMode();
    DrawRectangleLines(24, 20, 592, 245, RIM);
    panel((Rectangle){ 24, 276, 592, 62 }, METAL);
    const Caption *caption = story->kind == OPENING ? &scenes[story->scene] :
        &endings[story->kind == OVERRUN][story->scene];
    label(caption->title, 39, 286, 12, AMBER);
    label(caption->line1, 39, 305, 10, CREAM);
    label(caption->line2, 39, 320, 10, CREAM);
    label(TextFormat("%02d / %02d", story->scene+1, story->kind == OPENING ? STORY_SCENES : 3), 539, 283, 10, MUTED);
}
