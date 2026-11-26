#ifndef ART_H
#define ART_H

#include "game.h"

#define INK ((Color){ 16, 22, 19, 255 })
#define METAL ((Color){ 38, 48, 40, 255 })
#define RIM ((Color){ 99, 112, 82, 255 })
#define CREAM ((Color){ 235, 228, 201, 255 })
#define MUTED ((Color){ 172, 183, 155, 255 })
#define AMBER ((Color){ 235, 177, 87, 255 })
#define MINT ((Color){ 140, 204, 157, 255 })
#define ALERT_RED ((Color){ 246, 127, 112, 255 })

void panel(Rectangle rect, Color fill);
void label(const char *str, int x, int y, int size, Color color);
void load_map_art(void);
void unload_map_art(void);
void draw_map(const Game *g, bool reducedMotion);
void draw_map_preview(Rectangle bounds);
void draw_tower(const Tower *tower, float time, Color tint);
void draw_enemy_icon(EnemyKind kind, int x, int y);
void draw_combat(const Game *g, bool reducedMotion);
void draw_crisis_warning(const Game *g, bool reducedMotion);
void draw_portrait(int x, int y);
void draw_portrait_scaled(int x, int y, int scale);

#endif
