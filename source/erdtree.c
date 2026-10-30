/* erdtree: an orchestra under the Erdtree. Three independently generated sections and a room control.
 *   blue  : choir   - spectral voices: harmonics 1..6 of a slightly sharp tone, breathing shimmer on the top ones
 *   green : strings - a section of three saws (two detuned violins + celli an octave down), bowed swell that opens
 *                     the tone, vibrato that arrives once the bow has settled
 *   white : space   - distance: wider section detune, darker tone, slower bloom, deeper choir breathing
 *   red   : brass   - (the orange knob) the Brassophone horn: phase-warped sine whose timbre follows the stock
 *                     envelope; more brass is louder AND brighter, with a lip scoop into each note
 * Each section has its own level (squared knob curve). The levels are summed with headroom: when their sum passes
 * 1.5 x full scale the mix is scaled down by one reciprocal per block, so all three at maximum do not clip. A section at
 * zero is skipped (its phases still run, so bringing it in mid-note stays in tune with the others).
 * The brass voice and its warp table come from Brassophone by nick (nvmsocool), op1r-src commit 358d7e6.
 * Cost: each section runs its own loop over the block (no per-sample branches, few register spills) and adds into
 * the output buffer; the loops use 16 x 16 multiplies only. Memory: per sample the voice reads two small tables
 * (erd_sine and choir_wave, 4 KiB together, interpolated) and one 130-byte column of the warp table, all of which
 * stay in the data cache, and stores nothing but its output (erd_tables.h).
 * Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and
 * NOTICE). */
#include "op1_plugin.h"
#include "erd_tables.h"

/* Slow LFO per voice (advanced once per block): 0.34 Hz. Vibrato runs at 16x (5.4 Hz), choir breath at 3x.
 * inc = f * 2^32 * OP1_BLOCK / 44100. */
#define LFO_INC           4238000u
#define STR_VIB_MAX       131         /* strings vibrato depth, Q15 fraction of the pitch (0.4 %, ~7 cents) */
#define BRASS_VIB_MAX     60          /* brass vibrato, ~3 cents */
#define BRASS_SCOOP       700         /* lip scoop at note start: ~37 cents flat, rising into tune */
#define MIX_ROOM          49151       /* sum of the three section levels (Q15) above which the mix is scaled down:
                                       * 1.5, as the sections rarely peak together (all at max: -11 dBFS peak) */
#define OUT_TRIM          23197       /* -3 dB on the output (since v0.4) */

/* Voice state (the firmware gives us 0x24 bytes: vtable + 32 bytes). */
typedef struct {
    uint32_t vtable;                  /* firmware-owned */
    uint32_t ph_b;                    /* brass phase */
    uint32_t ph_s1, ph_s2, ph_s3;     /* strings: violins, violins detuned up (choir rides this one), celli */
    uint32_t lfo;                     /* slow LFO phase */
    int16_t  lp_b;                    /* brass one-pole */
    int16_t  lp_s1, lp_s2;            /* strings two-pole */
    int16_t  lp_c;                    /* choir one-pole */
    int16_t  prev_env;                /* stock envelope at the end of the previous block (brass timbre ramp) */
    int16_t  age;                     /* 0..32767 from note-on: bow swell, vibrato gate, brass scoop */
} erd_voice_t;
typedef char erd_voice_fits[sizeof(erd_voice_t) <= OP1_VOICE_SIZE ? 1 : -1];

static inline int32_t sq15(int32_t k) { return (k * k) >> 15; }
static inline int32_t knob(const int16_t *p, int i) { return p[i] < 0 ? 0 : p[i]; }   /* LFO can push a word below 0 */
static inline int32_t sin15(uint32_t ph) { return erd_sine[ph >> 22]; }          /* nearest step: per-block LFOs */
/* 16 x 16 -> 32 signed multiply of values that fit int16. gcc turns the plain C into a 32-bit multiply (*=, a
 * multi-cycle instruction) unless both operands come straight from 16-bit loads, so the target build says it. */
#ifdef OP1_HOST
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

static void erd_note_on(void *self, const int16_t *params)
{
    erd_voice_t *v = (erd_voice_t *)self; (void)params;
    v->ph_b = 0;
    v->ph_s1 = 0; v->ph_s2 = 0x55555555u; v->ph_s3 = 0xAAAAAAAAu;   /* spread: no phase-cancelled attack */
    v->lfo = (uint32_t)op1_voice_index(self) * 0x2AAAAAABu;         /* voices breathe out of step */
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
    for (int n = 0; n < OP1_BLOCK; n++) {
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

static void erd_render(void *self, int16_t *out, const int16_t *params)
{
    erd_voice_t *v = (erd_voice_t *)self;
    const int32_t *inc = OP1_PITCH_INC;
    int32_t vg = (op1_voice_vel_gain(self) * OUT_TRIM) >> 15;

    /* ---- per-block control values ---- */
    int32_t brass = knob(params, 3);
    int32_t lc = sq15(knob(params, 0)), ls = sq15(knob(params, 1)), space = knob(params, 2), lb = sq15(brass);
    int32_t total = lc + ls + lb;
    int32_t norm = total > MIX_ROOM ? (int32_t)(((uint32_t)MIX_ROOM << 15) / (uint32_t)total) : 32767;   /* per block */
    lc = op1_mul16(lc, norm); ls = op1_mul16(ls, norm); lb = op1_mul16(lb, norm);

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

    /* output gains: section level x bow swell / breath x velocity x trim. The strings sit 6 dB above the others
     * (x2, level with the brass); the firmware applies the envelope */
    int32_t g_s = (((s_amp * ls) >> 15) * vg) >> 15;
    int32_t g_b = (lb * vg) >> 15;
    int32_t g_c = (((c_amp * lc) >> 15) * vg) >> 15;

    int32_t env_end = OP1_VOICE_ENV[OP1_BLOCK - 1];

    uint32_t ph_b = v->ph_b;
    int32_t lp_b = v->lp_b, lp_s1 = v->lp_s1, lp_s2 = v->lp_s2, lp_c = v->lp_c;

    /* ---- strings and choir: one loop writes the block (the choir rides the detuned violin phase); the phases
     * always run, so a section brought in mid-note keeps its place ---- */
    sc_t sc = { v->ph_s1, v->ph_s2, v->ph_s3, lp_s1, lp_s2, lp_c };
    const sc_k_t k = { mod_s, det2, bright, g_s, w6, c_coef, g_c };
    if (ls && lc) strings_choir(1, 1, out, inc, &sc, &k);
    else if (ls)  strings_choir(1, 0, out, inc, &sc, &k);
    else if (lc)  strings_choir(0, 1, out, inc, &sc, &k);
    else          strings_choir(0, 0, out, inc, &sc, &k);

    /* ---- brass: Brassophone's warped phase, interpolated along the phase, nearest timbre column; the timbre
     * follows the stock envelope ramped across the block (as Brassophone: no timbre steps at block edges) ---- */
    if (lb) {
        int32_t env_q = v->prev_env << 7, env_step = env_end - v->prev_env;
        for (int n = 0; n < OP1_BLOCK; n++) {
            int32_t i = inc[n];
            env_q += env_step;
            ph_b += (uint32_t)(i + mul16s(i >> 16, mod_b) * 2);
            int32_t ia = b_start + (mul16s(b_sweep, env_q >> 7) >> 15);
            ia = ia > 32767 ? 32767 : ia;
            const int16_t *w = warp_cols[(ia + 64) >> 7];               /* nearest of 257 columns */
            int32_t r = ph_b >> 26, frac = (ph_b & 0x3FFFFFF) >> 11;
            int32_t a = w[r], ph = a + (mul16s(w[r + 1] - a, frac) >> 15);   /* 0..32767: one cycle */
            lp_b += (wave(erd_sine, (uint32_t)ph << 17) - lp_b) >> 1;
            out[n] = op1_sat16(out[n] + (mul16s(lp_b, g_b) >> 16));
        }
    } else {
        for (int n = 0; n < OP1_BLOCK; n++) {
            int32_t i = inc[n];
            ph_b += (uint32_t)(i + mul16s(i >> 16, mod_b) * 2);
        }
    }

    v->ph_b = ph_b; v->ph_s1 = sc.p1; v->ph_s2 = sc.p2; v->ph_s3 = sc.p3;
    v->lp_b = (int16_t)lp_b; v->lp_s1 = (int16_t)sc.lp1; v->lp_s2 = (int16_t)sc.lp2; v->lp_c = (int16_t)sc.lpc;
    v->prev_env = (int16_t)env_end;
}

const op1_engine_t ENGINE_erdtree = { erd_note_on, erd_render, 0, { OP1_PRESS_NONE, OP1_PRESS_NONE, OP1_PRESS_NONE, OP1_PRESS_NONE } };
