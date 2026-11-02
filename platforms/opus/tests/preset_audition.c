/* PC audition of one Opus preset, or a stress run of the engine. The Opus build's engine (src/sammi.erdtree/erdtree.c,
 * built with ERD_HOST) renders as the core calls it: six voices, 128 samples at 88.2 kHz a block (render's contract),
 * the stock envelope multiplied in after the render as the OS does, the sum taken down to 44.1 kHz by averaging pairs.
 * The OS's envelope is stood in for (time ~ 6 s x (word / 16320)^2, linear segments, as the Hundekraft harness does;
 * the OP-1's real curves differ), so are the velocity (100 of 127) and the effect (none here: preset_audition.py adds
 * a stand-in). Raw int16 mono at 44.1 kHz on stdout.
 *
 *   cc -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -DERD_HOST -I<plugin-kit>/sdk \
 *      -Isrc/sammi.erdtree tests/preset_audition.c src/sammi.erdtree/erdtree.c -lm -o preset_audition
 *   ./preset_audition play CHOIR STRINGS SPACE BRASS ATTACK DECAY SUSTAIN RELEASE OCTAVE > out.raw
 *       the test phrase: a low held note, a held minor chord, a fast high run, overlapping legato notes, a tail
 *   ./preset_audition stress SEED
 *       every block: knobs jumped to random values and the extremes, notes started, let go and stolen at random over
 *       every MIDI note, voices rebuilt mid-note from garbage memory, presets switched; prints the peak. Run it under
 *       the sanitizers: an overflow or a bad read aborts.
 * Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opus_plugin.h"

/* the core, as far as the engine sees it */
struct engine_head {
    unsigned size;
    const char *name;
    short knobs[8];
    unsigned voice;
    void (*init)(void *voice, int index);
    void (*note_on)(void *voice, const short *knobs);
    void (*release)(void *voice, const short *knobs);
    void (*render)(void *voice, short *out, const short *par, const unsigned *pitch);
};
static const struct engine_head *eng;
int16_t erd_host_env[128];
int opus_engine_add(struct opl_self *self, const struct opus_engine *e) { (void)self; eng = (const void *)e; return 0x100; }
void erd_page_init(struct opl_self *self) { (void)self; }
void erd_page(void) {}
void *opus_fast(struct opl_self *self, unsigned n) { (void)self; return malloc(n); }
void *opus_alloc_audio(struct opl_self *self, unsigned n) { (void)self; return calloc(1, n); }
int opl_init(struct opl_self *self);

#define NV 6
#define SR 44100
#define BT (64.0 / SR)                       /* one block: 128 samples at 88.2 kHz */
static unsigned inc_of(double midi) { return (unsigned)llrint(4294967296.0 * 440.0 * pow(2, (midi - 69) / 12.0) / 88200); }
static double secs(int w) { double x = w / 16320.0; return fmax(0.003, 6 * x * x); }

struct voice {
    unsigned char mem[64];
    int busy, held;
    double midi, on_t, off_t, from;          /* from: the level at the release */
    long age;
};
static struct voice V[NV];
static short knobs[8];
static double A, D, S, R;

static double level(const struct voice *v, double t)
{
    double e, u = (v->held ? t : v->off_t) - v->on_t;
    e = u < A ? u / A : u < A + D ? 1 - (1 - S) * (u - A) / D : S;
    if (v->held) return e;
    return fmax(0, v->from * (1 - (t - v->off_t) / R));
}

static long order;
static int note_on(double midi, double t)
{
    int best = 0;
    for (int i = 0; i < NV; i++) {           /* a free voice, else the oldest (the OS steals the same way) */
        if (!V[i].busy) { best = i; break; }
        if (V[i].age < V[best].age) best = i;
    }
    struct voice *v = &V[best];
    eng->init(v->mem, best);
    eng->note_on(v->mem, knobs);
    v->busy = 1; v->held = 1; v->midi = midi; v->on_t = t; v->age = ++order;
    return best;
}

static void note_off(int i, double t)
{
    struct voice *v = &V[i];
    if (!v->busy || !v->held) return;
    v->from = level(v, t);
    v->held = 0; v->off_t = t;
    eng->release(v->mem, knobs);
}

static void block(double t, short o44[64])
{
    static int32_t acc[128];
    short out[128];
    unsigned pitch[128];
    memset(acc, 0, sizeof acc);
    for (int i = 0; i < NV; i++) {
        struct voice *v = &V[i];
        if (!v->busy) continue;
        double e = level(v, t);
        if (!v->held && e <= 0) { v->busy = 0; continue; }
        int16_t ev = (int16_t)lrint(e * 32767 * 100 / 127);
        for (int n = 0; n < 128; n++) { erd_host_env[n] = ev; pitch[n] = inc_of(v->midi); }
        eng->render(v->mem, out, knobs, pitch);
        for (int n = 0; n < 128; n++) acc[n] += (out[n] * ev) >> 15;
    }
    for (int n = 0; n < 64; n++) {
        int s = (acc[2 * n] + acc[2 * n + 1]) / 2;
        o44[n] = (short)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
    }
}

/* the test phrase: (start s, length s, MIDI note before the octave) */
static const double phrase[][3] = {
    { 0.0, 2.0, 36 },                                                    /* low: a held C2 */
    { 2.4, 3.0, 48 }, { 2.4, 3.0, 55 }, { 2.4, 3.0, 60 }, { 2.4, 3.0, 63 },   /* mid: a held C minor */
    { 6.2, 0.15, 72 }, { 6.4, 0.15, 75 }, { 6.6, 0.15, 79 }, { 6.8, 0.15, 84 }, { 7.0, 0.4, 87 },   /* high run */
    { 8.2, 0.65, 60 }, { 8.7, 0.65, 62 }, { 9.2, 0.65, 63 }, { 9.7, 1.2, 67 },  /* legato: each overlaps the next */
};
#define PHRASE_END 13.5

static int play(int argc, char **argv)
{
    if (argc < 11) return 2;
    for (int i = 0; i < 4; i++) knobs[i] = (short)atoi(argv[2 + i]);
    A = secs(atoi(argv[6])); D = secs(atoi(argv[7])); S = atoi(argv[8]) / 32767.0; R = secs(atoi(argv[9]));
    int oct = atoi(argv[10]);
    const int np = sizeof phrase / sizeof phrase[0];
    int voice_of[sizeof phrase / sizeof phrase[0]], started[sizeof phrase / sizeof phrase[0]];
    memset(started, 0, sizeof started);
    for (int i = 0; i < NV; i++) eng->init(V[i].mem, i);
    short o[64];
    for (double t = 0; t < PHRASE_END; t += BT) {
        for (int p = 0; p < np; p++) {
            if (!started[p] && t >= phrase[p][0]) { voice_of[p] = note_on(phrase[p][2] + 12 * oct, t); started[p] = 1; }
            if (started[p] == 1 && t >= phrase[p][0] + phrase[p][1]) {
                if (V[voice_of[p]].midi == phrase[p][2] + 12 * oct) note_off(voice_of[p], t);
                started[p] = 2;
            }
        }
        block(t, o);
        fwrite(o, 2, 64, stdout);
    }
    return 0;
}

static unsigned rng = 1;
static unsigned rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }
static short rknob(void)
{
    switch (rnd() % 4) { case 0: return 0; case 1: return 32767; default: return (short)(rnd() % 32768); }
}

static int stress(int argc, char **argv)
{
    rng = argc > 2 ? (unsigned)atoi(argv[2]) : 1;
    int peak = 0;
    A = secs(64); D = secs(4000); S = 0.8; R = secs(2000);
    for (int i = 0; i < NV; i++) { memset(V[i].mem, 0xA5, sizeof V[i].mem); eng->init(V[i].mem, i); }
    short o[64];
    double t = 0;
    for (long b = 0; b < 400000; b++, t += BT) {
        if (rnd() % 8 == 0) for (int i = 0; i < 4; i++) knobs[i] = rknob();       /* a knob jump or a preset change */
        if (rnd() % 64 == 0) {                                                    /* a sweep from end to end */
            for (int i = 0; i < 4; i++) knobs[i] = (short)((b * 4096) & 32767);
        }
        if (rnd() % 3 == 0) note_on((double)(rnd() % 128), t);                   /* every MIDI note, chords, steals */
        if (rnd() % 3 == 0) note_off((int)(rnd() % NV), t);
        if (rnd() % 97 == 0) {                                                   /* a rebuild from garbage, mid-note */
            int i = (int)(rnd() % NV);
            for (unsigned k = 0; k < sizeof V[i].mem; k++) V[i].mem[k] = (unsigned char)rnd();
            eng->init(V[i].mem, i);
        }
        if (rnd() % 500 == 0) {                                                  /* a different envelope */
            A = secs((int)(64 + rnd() % 16257)); D = secs((int)(64 + rnd() % 16257));
            S = (rnd() % 32768) / 32767.0; R = secs((int)(64 + rnd() % 16257));
        }
        block(t, o);
        for (int n = 0; n < 64; n++) if (abs(o[n]) > peak) peak = abs(o[n]);
    }
    printf("stress ok: seed %u, 400000 blocks, peak %d\n", argc > 2 ? (unsigned)atoi(argv[2]) : 1, peak);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    opl_init(0);
    if (!eng) return 3;
    if (!strcmp(argv[1], "play")) return play(argc, argv);
    if (!strcmp(argv[1], "stress")) return stress(argc, argv);
    return 2;
}
