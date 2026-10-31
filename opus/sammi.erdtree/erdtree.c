/* sammi.erdtree: an orchestra under the Erdtree, as an Opus 2 synth engine. The sound is erdtree v0.5's
 * (source/erdtree.c, written for op1r's and kennel's engine API), ported to the core's engine service:
 *   blue  : choir   - spectral voices: harmonics 1..6 of a slightly sharp tone, breathing shimmer on the top ones
 *   green : strings - a section of three saws (two detuned violins + celli an octave down), bowed swell that opens
 *                     the tone, vibrato that arrives once the bow has settled
 *   white : space   - distance: wider section detune, darker tone, slower bloom, deeper choir breathing
 *   orange: brass   - the Brassophone horn: phase-warped sine whose timbre follows the stock envelope; more brass
 *                     is louder AND brighter, with a lip scoop into each note
 * Each section has its own level (squared knob curve). The levels are summed with headroom: when their sum passes
 * 1.5 x full scale the mix is scaled down by one reciprocal per block, so all three at maximum do not clip. A section at
 * zero is skipped (its phases still run, so bringing it in mid-note stays in tune with the others).
 * The brass voice and its warp table come from Brassophone by nick (nvmsocool), op1r-src commit 358d7e6.
 *
 * What changed in the port (the DSP itself is v0.5's, line for line):
 *  - the voice is the core's (struct erd_voice, no vtable word); its index comes from init, not from its address;
 *  - the pitch comes as render's argument (the same 128 phase steps the op1r SDK read at 0xFF801950);
 *  - the velocity is the OS's alone: Opus's OS multiplies every engine's render by the envelope, velocity included
 *    (api.md, Engines), so the extra velocity gain v0.5 applied on top is gone. At full velocity (the OP-1's own
 *    keys) the sound is the same;
 *  - the brass timbre still follows the stock envelope: the OS's envelope gains for the voice being rendered, which
 *    the OS computes just before render (op1fw.h, fw_voice_envelope: "env = 128 gains (0xFF801D50) the OS multiplies
 *    the render by"). A raw firmware address, read only, inside render only: the one deep part of this plugin;
 *  - the screen is the core's page (erd_page.c) instead of an OS screen.
 * Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and
 * NOTICE). */
#include <stdint.h>                   /* the compiler's own (freestanding) header, no C library */
#include "opus_plugin.h"
#include "erd_tables.h"
#include "erd_page.h"

#define BLOCK             128         /* render: 128 samples at 88.2 kHz */
/* the OS's envelope gains for the voice being rendered, Q15, velocity and level in (op1fw.h fw_voice_envelope) */
#ifdef ERD_HOST                       /* the PC test (opus/test): the envelope is the test's */
extern int16_t erd_host_env[];
#define VOICE_ENV         erd_host_env
#else
#define VOICE_ENV         ((const volatile int16_t *)0xFF801D50)
#endif

/* Slow LFO per voice (advanced once per block): 0.34 Hz. Vibrato runs at 16x (5.4 Hz), choir breath at 3x.
 * inc = f * 2^32 * BLOCK / 44100 (v0.5's figures, kept as they were). */
#define LFO_INC           4238000u
#define STR_VIB_MAX       131         /* strings vibrato depth, Q15 fraction of the pitch (0.4 %, ~7 cents) */
#define BRASS_VIB_MAX     60          /* brass vibrato, ~3 cents */
#define BRASS_SCOOP       700         /* lip scoop at note start: ~37 cents flat, rising into tune */
#define MIX_ROOM          49151       /* sum of the three section levels (Q15) above which the mix is scaled down:
                                       * 1.5, as the sections rarely peak together (all at max: -11 dBFS peak) */
#define OUT_TRIM          23197       /* -3 dB on the output (since v0.4) */

struct erd_voice {
    uint32_t ph_b;                    /* brass phase */
    uint32_t ph_s1, ph_s2, ph_s3;     /* strings: violins, violins detuned up (choir rides this one), celli */
    uint32_t lfo;                     /* slow LFO phase */
    int16_t  lp_b;                    /* brass one-pole */
    int16_t  lp_s1, lp_s2;            /* strings two-pole */
    int16_t  lp_c;                    /* choir one-pole */
    int16_t  prev_env;                /* stock envelope at the end of the previous block (brass timbre ramp) */
    int16_t  age;                     /* 0..32767 from note-on: bow swell, vibrato gate, brass scoop */
    int16_t  index;                   /* the voice, 0-5 (init) */
};

static inline int32_t sq15(int32_t k) { return (k * k) >> 15; }
static inline int32_t knob(const int16_t *p, int i) { return p[i] < 0 ? 0 : p[i]; }   /* LFO can push a word below 0 */
static inline int32_t sin15(uint32_t ph) { return erd_sine[ph >> 22]; }          /* nearest step: per-block LFOs */
static inline int32_t mul15(int32_t a, int32_t b) { return (a * b) >> 15; }
static inline int16_t sat16(int32_t x) { return x > 32767 ? 32767 : (x < -32768 ? -32768 : (int16_t)x); }
/* 16 x 16 -> 32 signed multiply of values that fit int16. gcc turns the plain C into a 32-bit multiply (a
 * multi-cycle instruction) unless both operands come straight from 16-bit loads, so the target build says it. */
#ifdef ERD_HOST
static inline int32_t mul16s(int32_t a, int32_t b) { return (int32_t)(int16_t)a * (int16_t)b; }
#else
static inline int32_t mul16s(int32_t a, int32_t b)
{
    int32_t r;
    __asm__("%0 = %1.L * %2.L (IS);" : "=d"(r) : "d"(a), "d"(b));
    return r;
}
#endif
/* one cycle of tab (1024 steps + guard) at the top 10 bits of ph, interpolated with the next 15 */
static inline int32_t wave(const int16_t *tab, uint32_t ph)
{
    const int16_t *t = tab + (ph >> 22);
    int32_t a = t[0];
    return a + (mul16s(t[1] - a, (ph >> 7) & 0x7FFF) >> 15);
}

static void erd_init(void *voice, int index)
{
    struct erd_voice *v = (struct erd_voice *)voice;
    v->ph_b = v->ph_s1 = v->ph_s2 = v->ph_s3 = v->lfo = 0;
    v->lp_b = v->lp_s1 = v->lp_s2 = v->lp_c = 0;
    v->prev_env = 0;
    v->age = 0;
    v->index = (int16_t)index;
}

static void erd_note_on(void *voice, const short *knobs)
{
    struct erd_voice *v = (struct erd_voice *)voice; (void)knobs;
    v->ph_b = 0;
    v->ph_s1 = 0; v->ph_s2 = 0x55555555u; v->ph_s3 = 0xAAAAAAAAu;   /* spread: no phase-cancelled attack */
    v->lfo = (uint32_t)v->index * 0x2AAAAAABu;                      /* voices breathe out of step */
    v->lp_b = 0; v->lp_s1 = 0; v->lp_s2 = 0; v->lp_c = 0;
    v->prev_env = 0;
    v->age = 0;
}

/* The strings and the choir over one block, into out. do_s / do_c are constants at each call, so each of the four
 * calls is its own loop with no per-sample branch. */
typedef struct { uint32_t p1, p2, p3; int32_t lp1, lp2, lpc; } sc_t;
typedef struct { int32_t mod_s, det2, bright, g_s, w6, c_coef, g_c; } sc_k_t;
static inline __attribute__((always_inline)) void strings_choir(const int do_s, const int do_c, int16_t *out,
                                                                const int32_t *inc, sc_t *st, const sc_k_t *k)
{
    uint32_t p1 = st->p1, p2 = st->p2, p3 = st->p3;
    int32_t lp1 = st->lp1, lp2 = st->lp2, lpc = st->lpc;
    const int32_t mod_s = k->mod_s, det2 = k->det2, bright = k->bright, g_s = k->g_s, w6 = k->w6, c_coef = k->c_coef,
                  g_c = k->g_c;
    for (int n = 0; n < BLOCK; n++) {
        int32_t i = inc[n];
        int32_t is = i + mul16s(i >> 16, mod_s) * 2;                   /* i * (1 + mod_s / 2^15); i >> 16 < 32768 */
        int32_t dd = mul16s(is >> 16, det2);                            /* (is >> 15) * det */
        p1 += (uint32_t)is;
        p2 += (uint32_t)(is + dd);
        p3 += (uint32_t)((is - (dd >> 1)) >> 1);
        int32_t y = 0;
        if (do_s) {
            int32_t s = (mul16s((int32_t)p1 >> 16, 8000) + mul16s((int32_t)p2 >> 16, 8000)
                       + mul16s((int32_t)p3 >> 16, 5600)) >> 15;
            lp1 += mul16s((s - lp1) >> 1, bright) >> 14;
            lp2 += mul16s((lp1 - lp2) >> 1, bright) >> 14;
            y = mul16s(lp2, g_s) >> 15;                                 /* |lp2| < 21600 */
        }
        if (do_c) {
            int32_t c = wave(choir_wave, p2) + (mul16s(wave(erd_sine, p2 * 6u), w6) >> 15);
            lpc += mul16s((c - lpc) >> 1, c_coef) >> 14;
            y += mul16s(lpc, g_c) >> 16;
        }
        out[n] = (int16_t)y;                                            /* both at full: under 25000 */
    }
    st->p1 = p1; st->p2 = p2; st->p3 = p3; st->lp1 = lp1; st->lp2 = lp2; st->lpc = lpc;
}

static void erd_render(void *voice, short *out, const short *params, const unsigned *pitch)
{
    struct erd_voice *v = (struct erd_voice *)voice;
    const int32_t *inc = (const int32_t *)pitch;
    const int32_t vg = OUT_TRIM;                                    /* the OS applies the velocity */

    /* ---- per-block control values ---- */
    int32_t brass = knob(params, 3);
    int32_t lc = sq15(knob(params, 0)), ls = sq15(knob(params, 1)), space = knob(params, 2), lb = sq15(brass);
    int32_t total = lc + ls + lb;
    int32_t norm = total > MIX_ROOM ? (int32_t)(((uint32_t)MIX_ROOM << 15) / (uint32_t)total) : 32767;   /* per block */
    lc = mul15(lc, norm); ls = mul15(ls, norm); lb = mul15(lb, norm);

    /* age: 0.2 s (dry) .. 0.95 s (far) from note-on to full */
    int32_t age = v->age + 480 - ((space * 380) >> 15);
    if (age > 32767) age = 32767;
    v->age = (int16_t)age;
    int32_t gate = age * 2 - 32767; if (gate < 0) gate = 0;          /* vibrato waits for the second half */
    int32_t scoop = age * 8; if (scoop > 32767) scoop = 32767;       /* brass lands 8x faster than the bow */

    v->lfo += LFO_INC;
    int32_t vib = sin15(v->lfo * 16u);
    int32_t breath = (sin15(v->lfo * 3u) + 32767) >> 1;              /* 0..32767 */
    int32_t shimmer = (sin15(v->lfo) + 32767) >> 1;

    /* strings: detune 5..18 cents with space, tone opens with the bow and darkens with space */
    int32_t det = 95 + ((space * 246) >> 15);
    int32_t mod_s = (vib * ((STR_VIB_MAX * gate) >> 15)) >> 15;
    int32_t bright = 5000 + ((age * 9000) >> 15);
    bright = (bright * (32767 - ((space * 14000) >> 15))) >> 15;
    int32_t s_amp = 13000 + ((age * 19767) >> 15);
    int32_t det2 = det * 2;

    /* brass: Brassophone's interpolation start + sweep, both raised by the brass level, a little darker with space */
    int32_t b_start = 3000 + ((brass * 9000) >> 15);
    int32_t b_sweep = 7000 + ((brass * 14000) >> 15);
    b_sweep = (b_sweep * (32767 - space / 3)) >> 15;
    int32_t mod_b = -((BRASS_SCOOP * (32767 - scoop)) >> 15) + ((vib * ((BRASS_VIB_MAX * gate) >> 15)) >> 15);

    /* choir: breathing tremolo and top-harmonic shimmer, both deeper with space */
    int32_t c_amp = 32767 - ((breath * (3000 + ((space * 6000) >> 15))) >> 15);
    int32_t w6 = 1500 + ((shimmer * (2000 + ((space * 3000) >> 15))) >> 15);
    int32_t c_coef = 22000 - ((space * 9000) >> 15);

    /* output gains: section level x bow swell / breath x trim. The strings sit 6 dB above the others (x2, level
     * with the brass); the OS applies the envelope and the velocity */
    int32_t g_s = (((s_amp * ls) >> 15) * vg) >> 15;
    int32_t g_b = (lb * vg) >> 15;
    int32_t g_c = (((c_amp * lc) >> 15) * vg) >> 15;

    int32_t env_end = VOICE_ENV[BLOCK - 1];

    uint32_t ph_b = v->ph_b;
    int32_t lp_b = v->lp_b;

    /* ---- strings and choir: one loop writes the block (the choir rides the detuned violin phase); the phases
     * always run, so a section brought in mid-note keeps its place ---- */
    sc_t sc = { v->ph_s1, v->ph_s2, v->ph_s3, v->lp_s1, v->lp_s2, v->lp_c };
    const sc_k_t k = { mod_s, det2, bright, g_s, w6, c_coef, g_c };
    if (ls && lc) strings_choir(1, 1, out, inc, &sc, &k);
    else if (ls)  strings_choir(1, 0, out, inc, &sc, &k);
    else if (lc)  strings_choir(0, 1, out, inc, &sc, &k);
    else          strings_choir(0, 0, out, inc, &sc, &k);

    /* ---- brass: Brassophone's warped phase, interpolated along the phase, nearest timbre column; the timbre
     * follows the stock envelope ramped across the block (as Brassophone: no timbre steps at block edges) ---- */
    if (lb) {
        int32_t env_q = v->prev_env << 7, env_step = env_end - v->prev_env;
        for (int n = 0; n < BLOCK; n++) {
            int32_t i = inc[n];
            env_q += env_step;
            ph_b += (uint32_t)(i + mul16s(i >> 16, mod_b) * 2);
            int32_t ia = b_start + (mul16s(b_sweep, env_q >> 7) >> 15);
            ia = ia > 32767 ? 32767 : ia;
            const int16_t *w = warp_cols[(ia + 64) >> 7];               /* nearest of 257 columns */
            int32_t r = ph_b >> 26, frac = (ph_b & 0x3FFFFFF) >> 11;
            int32_t a = w[r], ph = a + (mul16s(w[r + 1] - a, frac) >> 15);   /* 0..32767: one cycle */
            lp_b += (wave(erd_sine, (uint32_t)ph << 17) - lp_b) >> 1;
            out[n] = sat16(out[n] + (mul16s(lp_b, g_b) >> 16));
        }
    } else {
        for (int n = 0; n < BLOCK; n++) {
            int32_t i = inc[n];
            ph_b += (uint32_t)(i + mul16s(i >> 16, mod_b) * 2);
        }
    }

    v->ph_b = ph_b; v->ph_s1 = sc.p1; v->ph_s2 = sc.p2; v->ph_s3 = sc.p3;
    v->lp_b = (int16_t)lp_b; v->lp_s1 = (int16_t)sc.lp1; v->lp_s2 = (int16_t)sc.lp2; v->lp_c = (int16_t)sc.lpc;
    v->prev_env = (int16_t)env_end;
}

static const struct opus_engine ERDTREE = {
    .size = sizeof(struct opus_engine),
    .name = "erdtree",
    /* a new sound: the "first grace" preset (choir, strings, space, brass); knob 4 is the core's */
    .knobs = { 10000, 26000, 12000, 16000, 0, 0, 0, 0 },
    .voice = sizeof(struct erd_voice),
    .init = erd_init,
    .note_on = erd_note_on,
    .render = erd_render,
    .page = erd_page,
    /* sounds saved with erdtree on op1r or the Hundekraft Workshop carry "type": "erdtree" (the preset pack's too):
     * they load as this engine */
    .type = "erdtree",
};

int opl_init(struct opl_self *self)
{
    erd_page_init(self);                   /* without its art or memory: the plain page */
    return opus_engine_add(self, &ERDTREE) < 0 ? -1 : 0;
}
