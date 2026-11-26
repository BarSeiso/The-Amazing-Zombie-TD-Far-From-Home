#ifndef AUDIO_H
#define AUDIO_H

#include "game.h"

typedef enum {
    MUSIC_MENU, MUSIC_STORY, MUSIC_PREPARE, MUSIC_WAVE,
    MUSIC_CRISIS, MUSIC_WIN, MUSIC_LOSS, MUSIC_COUNT
} MusicCue;

typedef enum {
    SFX_BOW, SFX_CANNON, SFX_SNIPER,
    SFX_PLACE, SFX_UPGRADE, SFX_SELL, SFX_LEAK, SFX_CLICK, SFX_COUNT
} SfxKind;

typedef struct {
    Music stream;
    unsigned char *wav;
    float level;
    bool started;
} ScoreTrack;

typedef struct {
    ScoreTrack tracks[MUSIC_COUNT];
    Sound siren, criticalCall;
    Sound sfx[SFX_COUNT];
    float sfxCooldown[SFX_COUNT];
    bool warning, paused;
} GameAudio;

GameAudio load_game_audio(void);
void update_game_audio(GameAudio *audio, MusicCue cue, bool warning, bool paused,
                       int musicVolume, int sfxVolume, float dt, float storySeek);
void play_critical_call(GameAudio *audio, int sfxVolume);
void play_sfx(GameAudio *audio, SfxKind kind, int sfxVolume);
void play_game_events(GameAudio *audio, unsigned int events, int sfxVolume);
void unload_game_audio(GameAudio *audio);

#endif
