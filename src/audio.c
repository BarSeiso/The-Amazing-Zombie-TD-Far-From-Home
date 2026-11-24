#include "audio.h"
#include "score.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define TAU 6.28318530718f

static Sound make_siren(void)
{
    const unsigned rate = 22050;
    unsigned frames = (unsigned)(CRISIS_WARNING_SECONDS*rate);
    int16_t *data = malloc(frames*sizeof *data);
    if (!data) return (Sound){0};
    for (unsigned i = 0; i < frames; i++) {
        float t = (float)i/rate;
        float edge = fminf(1, fminf(t, CRISIS_WARNING_SECONDS-t)*45);
        float pulse = .5f-.5f*cosf(TAU*.73f*t);
        float pitch = TAU*(415*t-35*sinf(TAU*.73f*t));
        data[i] = (int16_t)((.72f*sinf(pitch)+.18f*sinf(2*pitch))*pulse*edge*25000);
    }
    Wave wave = { .frameCount = frames, .sampleRate = rate, .sampleSize = 16, .channels = 1, .data = data };
    Sound sound = LoadSoundFromWave(wave);
    free(data);
    return sound;
}

GameAudio load_game_audio(void)
{
    GameAudio audio = {0};
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return audio;
    audio.siren = make_siren();
    if (IsSoundValid(audio.siren)) SetSoundVolume(audio.siren, .36f);

    for (int i = 0; i < MUSIC_COUNT; i++) {
        int size;
        ScoreTrack *t = &audio.tracks[i];
        t->wav = render_score((MusicCue)i, &size);
        if (!t->wav) continue;
        t->stream = LoadMusicStreamFromMemory(".wav", t->wav, size);
        t->stream.looping = i != MUSIC_STORY && i != MUSIC_WIN && i != MUSIC_LOSS;
    }
    return audio;
}

static void set_paused(GameAudio *a, bool paused)
{
    if (paused == a->paused) return;
    a->paused = paused;
    if (a->warning && IsSoundValid(a->siren)) {
        if (paused) PauseSound(a->siren);
        else ResumeSound(a->siren);
    }
    for (int i = 0; i < MUSIC_COUNT; i++) {
        ScoreTrack *t = &a->tracks[i];
        if (!t->started) continue;
        if (paused) PauseMusicStream(t->stream);
        else ResumeMusicStream(t->stream);
    }
}

static void start_track(ScoreTrack *t, float time)
{
    SetMusicVolume(t->stream, 0);
    if (time >= 0) {
        StopMusicStream(t->stream);
        SeekMusicStream(t->stream, time);
    }
    // Prime the decoder before the audio thread asks for the first sub-buffer.
    UpdateMusicStream(t->stream);
    PlayMusicStream(t->stream);
    t->started = true;
}

void update_game_audio(GameAudio *a, MusicCue cue, bool warning, bool paused,
                       int volume, float dt, float storySeek)
{
    if (!IsAudioDeviceReady()) return;
    set_paused(a, paused);
    if (paused) return;

    bool alert = warning && !a->warning;
    if (warning != a->warning && IsSoundValid(a->siren)) {
        if (warning) PlaySound(a->siren);
        else StopSound(a->siren);
    }
    a->warning = warning;

    ScoreTrack *incoming = &a->tracks[cue];
    if (IsMusicValid(incoming->stream) && (!incoming->started || (cue == MUSIC_STORY && storySeek >= 0)))
        start_track(incoming, cue == MUSIC_STORY ? storySeek : -1);

    float blend = dt/.4f;
    float gain[MUSIC_COUNT] = {0};
    float total = 0;
    for (int i = 0; i < MUSIC_COUNT; i++) {
        ScoreTrack *t = &a->tracks[i];
        if (!t->started) continue;
        if (i == (int)cue) {
            t->level = alert ? 1 : fminf(1, t->level+blend);
        } else {
            t->level = alert ? 0 : fmaxf(0, t->level-blend);
            if (t->level == 0) {
                StopMusicStream(t->stream);
                t->started = false;
                continue;
            }
        }
        gain[i] = t->level*(warning && i == MUSIC_CRISIS ? .44f : 1);
        total += gain[i];
    }
    for (int i = 0; i < MUSIC_COUNT; i++) {
        ScoreTrack *t = &a->tracks[i];
        if (!t->started) continue;
        SetMusicVolume(t->stream, (volume/100.0f)*gain[i]/fmaxf(1, total));
        UpdateMusicStream(t->stream);
    }
}

void unload_game_audio(GameAudio *a)
{
    if (!IsAudioDeviceReady()) return;
    if (IsSoundValid(a->siren)) UnloadSound(a->siren);
    for (int i = 0; i < MUSIC_COUNT; i++) {
        ScoreTrack *t = &a->tracks[i];
        if (IsMusicValid(t->stream)) UnloadMusicStream(t->stream);
        free(t->wav);
    }
    CloseAudioDevice();
}
