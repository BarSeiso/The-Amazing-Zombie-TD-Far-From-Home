#ifndef AUDIO_H
#define AUDIO_H

#include "game.h"

typedef enum {
    MUSIC_MENU, MUSIC_STORY, MUSIC_PREPARE, MUSIC_WAVE,
    MUSIC_CRISIS, MUSIC_WIN, MUSIC_LOSS, MUSIC_COUNT
} MusicCue;

typedef struct {
    Music stream;
    unsigned char *wav;
    float level;
    bool started;
} ScoreTrack;

typedef struct {
    ScoreTrack tracks[MUSIC_COUNT];
    Sound siren;
    bool warning, paused;
} GameAudio;

GameAudio load_game_audio(void);
void update_game_audio(GameAudio *audio, MusicCue cue, bool warning, bool paused,
                       int volume, float dt, float storySeek);
void unload_game_audio(GameAudio *audio);

#endif
