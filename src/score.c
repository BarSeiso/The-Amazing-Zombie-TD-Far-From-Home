#include "score.h"
#include "story.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TAU 6.28318530718f

enum { RATE = 22050, WAV_HEADER = 44 };
typedef enum { PAD, METALLOPHONE, BASS, DRUM, RIM } Voice;
typedef struct {
    float *samples;
    int frames;
    bool looping;
} Score;

static float voice_sample(Voice voice, float phase, float t)
{
    switch (voice) {
        case PAD:
            return .70f*sinf(phase)+.18f*sinf(phase*2.003f)+.08f*sinf(phase*3.006f);
        case METALLOPHONE:
            return expf(-2.3f*t)*(.64f*sinf(phase)+.24f*sinf(phase*2.76f)*expf(-4.7f*t)
                         +.12f*sinf(phase*5.11f)*expf(-11*t));
        case BASS:
            return .75f*sinf(phase)+.19f*sinf(phase*2)+.06f*sinf(phase*3);
        case DRUM:
            return expf(-10*t)*(.76f*sinf(phase+3*(1-expf(-32*t)))+.24f*sinf(phase*1.69f));
        case RIM:
            return expf(-35*t)*(.57f*sinf(phase*2.31f)+.28f*sinf(phase*5.93f)+.15f*sinf(phase*9.61f));
    }
    return 0;
}

static void note(Score *s, Voice voice, float start, float duration, int midi, float gain)
{
    int from = (int)lroundf(start*RATE);
    int length = (int)lroundf(duration*RATE);
    float pitch = TAU*440*powf(2, (midi-69)/12.0f);
    float attack = voice == PAD ? .34f : voice == BASS ? .025f : .004f;
    float release = voice == PAD ? .45f : .08f;
    for (int i = 0; i < length; i++) {
        int frame = from+i;
        if (frame >= s->frames) {
            if (!s->looping) break;
            frame %= s->frames;
        }
        float t = (float)i/RATE;
        float envelope = fminf(1, fminf(t/attack, (duration-t)/release));
        s->samples[frame] += gain*envelope*voice_sample(voice, pitch*t, t);
    }
}

static void arrange(Score *s, MusicCue cue, int bars, float beat)
{
    // Four-note D-F-G-A signature with different answers over a D minor progression.
    static const int chords[][4] = {
        {50, 57, 60, 65}, {46, 53, 57, 62}, {41, 53, 57, 60}, {48, 55, 58, 62}
    };
    static const int motif[] = {62, 65, 67, 69};
    static const int answer[] = {72, 69, 67, 65};
    static const float positions[] = {0, .75f, 1.5f, 2.75f};
    for (int bar = 0; bar < bars; bar++) {
        float at = bar*4*beat;
        int scene = bar/4;
        bool story = cue == MUSIC_STORY;
        bool crisis = cue == MUSIC_CRISIS;
        bool pulse = cue == MUSIC_WAVE || crisis || (story && (scene == 1 || scene >= 3));
        bool sparse = cue == MUSIC_PREPARE || (story && (scene == 0 || scene == 2));
        int chord = (bar/2)%4;
        if (crisis && bar%4 == 3) chord = 3;
        for (int voice = 0; voice < 4; voice++) {
            note(s, PAD, at, 4.23f*beat, chords[chord][voice],
                 (sparse ? .049f : .062f)*(voice == 0 ? 1.22f : 1));
        }
        if (pulse) {
            for (int step = 0; step < 8; step++) {
                if (step == 3 || (!crisis && step == 6)) continue;
                int pitch = chords[chord][step%4 == 2 ? 1 : 0]-12;
                note(s, BASS, at+.5f*step*beat, .36f*beat, pitch, crisis ? .23f : .17f);
            }
            note(s, DRUM, at, .29f, 37, .24f);
            note(s, DRUM, at+2.5f*beat, .27f, 39, .18f);
            note(s, RIM, at+1.5f*beat, .125f, 61, .10f);
            note(s, RIM, at+3.5f*beat, .125f, 65, crisis ? .15f : .078f);
            if (crisis || bar%4 == 3) note(s, DRUM, at+3.75f*beat, .22f, 43, .13f);
        }
        if (sparse) {
            if (bar%2 == 0) note(s, METALLOPHONE, at+beat, 1.9f*beat, motif[(bar/2)%4], .14f);
        } else if (bar%4 == 0 || bar%4 == 2) {
            const int *phrase = bar%4 == 0 ? motif : answer;
            float variation = bar >= 8 ? .25f*beat : 0;
            for (int i = 0; i < 4; i++) {
                note(s, METALLOPHONE, at+positions[i]*beat+variation, 1.28f*beat,
                     phrase[i]+(crisis ? 12 : 0), crisis ? .21f : .22f);
            }
        } else if (bar >= 8 || cue == MUSIC_MENU) {
            note(s, METALLOPHONE, at+2.5f*beat, 1.38f*beat, chords[chord][3]+12, .13f);
        }
        if (crisis && bar%4 == 0) note(s, DRUM, at, .55f, 30, .17f);
    }
}

static void stinger(Score *s, bool win)
{
    static const int victory[] = {62, 65, 67, 69, 74};
    static const int defeat[] = {69, 67, 65, 62};
    const int *phrase = win ? victory : defeat;
    for (int i = 0; i < (win ? 5 : 4); i++) {
        note(s, METALLOPHONE, .16f+.31f*i, .78f, phrase[i], .32f);
    }
    note(s, PAD, .04f, 2.6f, win ? 50 : 46, .17f);
    note(s, PAD, .4f, 2.2f, win ? 57 : 53, .12f);
    note(s, DRUM, .14f, .36f, win ? 39 : 34, .16f);
}

static void le16(unsigned char *dst, unsigned value)
{
    dst[0] = (unsigned char)value;
    dst[1] = (unsigned char)(value >> 8);
}

static void le32(unsigned char *dst, unsigned value)
{
    le16(dst, value);
    le16(dst+2, value >> 16);
}

unsigned char *render_score(MusicCue cue, int *size)
{
    static const int tempos[MUSIC_COUNT] = {72, 80, 72, 96, 112, 0, 0};
    static const int measures[MUSIC_COUNT] = {8, STORY_SCENES*4, 8, 16, 16, 0, 0};
    bool result = cue == MUSIC_WIN || cue == MUSIC_LOSS;
    float beat = result ? 0 : 60.0f/tempos[cue];
    float seconds = result ? 3 : cue == MUSIC_STORY ? STORY_SCENES*STORY_SCENE_SECONDS : measures[cue]*4*beat;
    Score mix = { .frames = (int)lroundf(seconds*RATE), .looping = cue != MUSIC_STORY && !result };
    *size = WAV_HEADER+mix.frames*2;
    unsigned char *wav = calloc(1, *size);
    mix.samples = calloc(mix.frames, sizeof *mix.samples);
    if (!wav || !mix.samples) {
        free(wav);
        free(mix.samples);
        *size = 0;
        return NULL;
    }

    if (result) stinger(&mix, cue == MUSIC_WIN);
    else arrange(&mix, cue, measures[cue], beat);
    memcpy(wav, "RIFF", 4);
    le32(wav+4, *size-8);
    memcpy(wav+8, "WAVEfmt ", 8);
    le32(wav+16, 16);
    le16(wav+20, 1);
    le16(wav+22, 1);
    le32(wav+24, RATE);
    le32(wav+28, RATE*2);
    le16(wav+32, 2);
    le16(wav+34, 16);
    memcpy(wav+36, "data", 4);
    le32(wav+40, mix.frames*2);
    for (int i = 0; i < mix.frames; i++) {
        float s = mix.samples[i];
        if (!mix.looping) s *= fminf(1, (mix.frames-1-i)/(RATE*.22f));
        // A soft ceiling keeps accents audible without clipping stacked voices.
        s /= 1+.38f*fabsf(s);
        le16(wav+WAV_HEADER+i*2, (uint16_t)(int16_t)lroundf(s*28500));
    }
    free(mix.samples);
    return wav;
}
