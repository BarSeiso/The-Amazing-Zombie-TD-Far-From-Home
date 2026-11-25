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

static Sound make_critical_call(void)
{
    enum { rate = 22050, frames = rate*2 };
    int16_t *samples = calloc(frames, sizeof *samples);
    if (!samples) return (Sound){0};
    uint32_t noise = 7;
    for (int i=0; i<frames; i++) {
        float t=(float)i/rate, signal=0;
        if (t < .75f) {
            float knock=fmodf(t,.25f);
            signal=.65f*expf(-32*knock)*sinf(TAU*83*knock);
        } else if (t>1 && t<1.8f) {
            noise=noise*1664525u+1013904223u;
            float hiss=(float)(noise>>16)/32768-1;
            signal=(.15f*hiss+.20f*sinf(TAU*741*t))*expf(-4*fmodf(t-1,.24f));
        }
        samples[i]=(int16_t)(signal*23000);
    }
    Wave wave={.frameCount=frames,.sampleRate=rate,.sampleSize=16,.channels=1,.data=samples};
    Sound call=LoadSoundFromWave(wave);
    free(samples);
    return call;
}

void play_critical_call(GameAudio *a, int volume)
{
    if (!IsSoundValid(a->criticalCall)) return;
    SetSoundVolume(a->criticalCall, volume/100.f);
    PlaySound(a->criticalCall);
}

GameAudio load_game_audio(void)
{
    GameAudio audio = {0};
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return audio;
    audio.siren = make_siren();
    audio.criticalCall = make_critical_call();
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
    if (IsSoundValid(a->siren)) SetSoundVolume(a->siren, .36f*volume/100);
    if (IsSoundValid(a->criticalCall)) {
        SetSoundVolume(a->criticalCall, volume/100.f);
        if (paused || cue == MUSIC_MENU || cue == MUSIC_LOSS || cue == MUSIC_WIN)
            StopSound(a->criticalCall);
    }
    set_paused(a, paused);
    if (paused) return;

    bool alert = warning && !a->warning;
    if (warning != a->warning && IsSoundValid(a->siren)) {
        if (warning) PlaySound(a->siren);
        else StopSound(a->siren);
    }
    a->warning = warning;

    ScoreTrack *incoming = &a->tracks[cue];
    if (IsMusicValid(incoming->stream) && (!incoming->started || storySeek >= 0))
        start_track(incoming, storySeek);

    float blend = dt/.4f;
    float gain[MUSIC_COUNT] = {0};
    float total = 0;
    for (int i = 0; i < MUSIC_COUNT; i++) {
        ScoreTrack *t = &a->tracks[i];
        if (!t->started) continue;
        if (i == (int)cue) {
            t->level = alert ? 1 : fminf(1, t->level+blend);
        } else {
            t->level = (alert || cue == MUSIC_LOSS) ? 0 : fmaxf(0, t->level-blend);
            if (t->level == 0) {
                StopMusicStream(t->stream);
                t->started = false;
                continue;
            }
        }
        gain[i] = t->level*(warning && i == MUSIC_CRISIS ? 0 : 1);
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
    if (IsSoundValid(a->criticalCall)) UnloadSound(a->criticalCall);
    if (IsSoundValid(a->siren)) UnloadSound(a->siren);
    for (int i = 0; i < MUSIC_COUNT; i++) {
        ScoreTrack *t = &a->tracks[i];
        if (IsMusicValid(t->stream)) UnloadMusicStream(t->stream);
        free(t->wav);
    }
    CloseAudioDevice();
}
