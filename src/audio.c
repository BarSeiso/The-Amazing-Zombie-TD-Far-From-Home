#include "audio.h"
#include "score.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define TAU 6.28318530718f

/* Mensintesis suara sirene peringatan Krisis selama CRISIS_WARNING_SECONDS detik di RAM. */
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

/* Mensintesis efek ketukan pintu darurat + statis radio saat HP shelter kritis (<= 5). */
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

/*
 * Generator efek suara (SFX) prosedural untuk tembakan, ledakan, transaksi, dan UI.
 * Menggunakan kombinasi sapuan frekuensi (frequency sweep), harmonik, dan derau LCG.
 */
static Sound make_sfx(SfxKind kind)
{
    const int rate = 22050;
    static const float durations[SFX_COUNT] = {
        0.09f, 0.26f, 0.17f, 0.11f, 0.21f, 0.15f, 0.28f, 0.035f
    };
    int frames = (int)(durations[kind] * rate);
    int16_t *samples = calloc((size_t)frames, sizeof *samples);
    if (!samples) return (Sound){0};

    uint32_t rng = 1337u + (uint32_t)kind * 97u;
    float lowNoise = 0.0f;
    float phase = 0.0f;

    for (int i = 0; i < frames; i++) {
        float t = (float)i / rate;
        rng = rng * 1664525u + 1013904223u;
        float white = (float)(rng >> 16) / 32768.0f - 1.0f;
        lowNoise = lowNoise * 0.84f + white * 0.16f;
        float s = 0.0f;

        switch (kind) {
            case SFX_BOW: {
                float freq = 330.0f - 1500.0f * t;
                if (freq < 120.0f) freq = 120.0f;
                phase += TAU * freq / rate;
                s = (0.55f * sinf(phase) + 0.30f * white * expf(-45.0f * t)) * expf(-24.0f * t);
                break;
            }
            case SFX_CANNON: {
                float freq = 42.0f + 110.0f * expf(-22.0f * t);
                phase += TAU * freq / rate;
                s = (0.68f * sinf(phase) + 0.55f * lowNoise) * expf(-9.5f * t);
                break;
            }
            case SFX_SNIPER: {
                float freq = 105.0f + 540.0f * expf(-38.0f * t);
                phase += TAU * freq / rate;
                float crack = white * expf(-65.0f * t);
                s = 0.55f * sinf(phase) * expf(-15.0f * t) + 0.45f * crack;
                break;
            }
            case SFX_PLACE: {
                float step = t < 0.05f ? t : t - 0.05f;
                float freq = t < 0.05f ? 195.0f : 275.0f;
                s = 0.60f * sinf(TAU * freq * step) * expf(-32.0f * step);
                break;
            }
            case SFX_UPGRADE: {
                /* Arpeggio D5 - F5 - A5 selaras dengan tangga nada D minor musik latar. */
                float noteDur = 0.07f;
                int idx = (int)(t / noteDur);
                if (idx > 2) idx = 2;
                float local = t - idx * noteDur;
                static const float notes[3] = { 587.33f, 698.46f, 880.0f };
                s = (0.52f * sinf(TAU * notes[idx] * local) +
                     0.18f * sinf(TAU * notes[idx] * 2.0f * local)) * expf(-16.0f * local);
                break;
            }
            case SFX_SELL: {
                float local = t < 0.07f ? t : t - 0.07f;
                float freq = t < 0.07f ? 523.25f : 392.0f;
                s = 0.48f * sinf(TAU * freq * local) * expf(-20.0f * local);
                break;
            }
            case SFX_LEAK: {
                float impact = (0.6f * sinf(TAU * 78.0f * t) + 0.35f * sinf(TAU * 114.0f * t)) * expf(-8.0f * t);
                s = impact + 0.25f * lowNoise * expf(-10.0f * t);
                break;
            }
            case SFX_CLICK: {
                s = 0.38f * sinf(TAU * 680.0f * t) * expf(-85.0f * t);
                break;
            }
            case SFX_COUNT:
                break;
        }
        float fadeIn = fminf(1.0f, (float)i / 32.0f);
        float clamped = fmaxf(-1.0f, fminf(1.0f, s * fadeIn));
        samples[i] = (int16_t)(clamped * 24000.0f);
    }

    Wave wave = { .frameCount = (unsigned)frames, .sampleRate = rate, .sampleSize = 16, .channels = 1, .data = samples };
    Sound out = LoadSoundFromWave(wave);
    free(samples);
    return out;
}

void play_critical_call(GameAudio *a, int sfxVolume)
{
    if (!IsAudioDeviceReady() || sfxVolume <= 0 || !IsSoundValid(a->criticalCall)) return;
    SetSoundVolume(a->criticalCall, sfxVolume/100.f);
    PlaySound(a->criticalCall);
}

void play_sfx(GameAudio *a, SfxKind kind, int sfxVolume)
{
    if (!IsAudioDeviceReady() || sfxVolume <= 0 || kind < 0 || kind >= SFX_COUNT) return;
    if (a->sfxCooldown[kind] > 0.0f || !IsSoundValid(a->sfx[kind])) return;

    static const float baseGain[SFX_COUNT] = {
        0.34f, 0.56f, 0.48f, 0.46f, 0.46f, 0.42f, 0.65f, 0.32f
    };
    static const float minGap[SFX_COUNT] = {
        0.055f, 0.085f, 0.075f, 0.04f, 0.06f, 0.06f, 0.12f, 0.03f
    };
    a->sfxCooldown[kind] = minGap[kind];
    SetSoundVolume(a->sfx[kind], baseGain[kind] * (sfxVolume / 100.0f));
    PlaySound(a->sfx[kind]);
}

void play_game_events(GameAudio *a, unsigned int events, int sfxVolume)
{
    if (!events || sfxVolume <= 0) return;
    if (events & EV_LEAK)    play_sfx(a, SFX_LEAK, sfxVolume);
    if (events & EV_UPGRADE) play_sfx(a, SFX_UPGRADE, sfxVolume);
    if (events & EV_SELL)    play_sfx(a, SFX_SELL, sfxVolume);
    if (events & EV_PLACE)   play_sfx(a, SFX_PLACE, sfxVolume);
    if (events & EV_SNIPER)  play_sfx(a, SFX_SNIPER, sfxVolume);
    if (events & EV_CANNON)  play_sfx(a, SFX_CANNON, sfxVolume);
    if (events & EV_BOW)     play_sfx(a, SFX_BOW, sfxVolume);
}

GameAudio load_game_audio(void)
{
    GameAudio audio = {0};
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return audio;
    audio.siren = make_siren();
    audio.criticalCall = make_critical_call();
    if (IsSoundValid(audio.siren)) SetSoundVolume(audio.siren, .36f);
    for (int i = 0; i < SFX_COUNT; i++) {
        audio.sfx[i] = make_sfx((SfxKind)i);
    }

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
    /* Isi buffer awal decoder sebelum thread audio mengambil sampel pertama. */
    UpdateMusicStream(t->stream);
    PlayMusicStream(t->stream);
    t->started = true;
}

/* Mengatur crossfade antar-track musik latar, volume sirene, dan cooldown efek suara tiap frame. */
void update_game_audio(GameAudio *a, MusicCue cue, bool warning, bool paused,
                       int musicVolume, int sfxVolume, float dt, float storySeek)
{
    if (!IsAudioDeviceReady()) return;
    for (int i = 0; i < SFX_COUNT; i++) {
        a->sfxCooldown[i] = fmaxf(0.0f, a->sfxCooldown[i] - dt);
    }
    if (IsSoundValid(a->siren)) SetSoundVolume(a->siren, .36f*sfxVolume/100.0f);
    if (IsSoundValid(a->criticalCall)) {
        SetSoundVolume(a->criticalCall, sfxVolume/100.f);
        if (paused || cue == MUSIC_MENU || cue == MUSIC_LOSS || cue == MUSIC_WIN)
            StopSound(a->criticalCall);
    }
    set_paused(a, paused);
    if (paused) return;

    bool alert = warning && !a->warning;
    if (warning != a->warning && IsSoundValid(a->siren)) {
        if (warning && sfxVolume > 0) PlaySound(a->siren);
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
        SetMusicVolume(t->stream, (musicVolume/100.0f)*gain[i]/fmaxf(1, total));
        UpdateMusicStream(t->stream);
    }
}

void unload_game_audio(GameAudio *a)
{
    if (!IsAudioDeviceReady()) return;
    if (IsSoundValid(a->criticalCall)) UnloadSound(a->criticalCall);
    if (IsSoundValid(a->siren)) UnloadSound(a->siren);
    for (int i = 0; i < SFX_COUNT; i++) {
        if (IsSoundValid(a->sfx[i])) UnloadSound(a->sfx[i]);
    }
    for (int i = 0; i < MUSIC_COUNT; i++) {
        ScoreTrack *t = &a->tracks[i];
        if (IsMusicValid(t->stream)) UnloadMusicStream(t->stream);
        free(t->wav);
    }
    CloseAudioDevice();
}
