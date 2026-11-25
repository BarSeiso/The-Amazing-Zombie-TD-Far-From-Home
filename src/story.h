#ifndef STORY_H
#define STORY_H

#include "game.h"

enum { STORY_SCENES = 5, STORY_SCENE_SECONDS = 12 };

typedef enum { OPENING, EVACUATED, OVERRUN } StoryKind;

typedef struct {
    StoryKind kind;
    int scene;
    float elapsed;
    bool playing;
} Story;

void begin_story(Story *story);
void begin_ending(Story *story, bool won);
float story_scene_seconds(const Story *story);
void advance_story(Story *story);
void tick_story(Story *story, float dt);
void draw_story(const Story *story, bool reducedMotion);

#endif
