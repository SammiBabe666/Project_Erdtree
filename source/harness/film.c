/* Host render of the 30-second demo film: one timeline drives the engine (erdtree.c) and the screen (erdscreen.c).
 * Part one conducts a patch knob by knob; part two plays four of the presets (presets.py), with the knobs set to
 * each preset's values. Chords are three-note voicings on alternating voice triples, so no more than six voices
 * sound at once, as on the OP-1. The stock envelope is stood in by a linear ADSR (times as audition.c; the releases
 * between scenes are shortened to fit), applied to the output as the firmware does after render. No effects: film.py
 * adds a stand-in reverb.
 *   cc -DOP1_HOST -I<op1r-src>/synth_sdk -I.. film.c ../erdtree.c ../erdscreen.c -lm
 *   ./a.out audio.raw > frames.jsonl     (raw int16 mono 44.1 kHz; one JSON line per 30 fps frame)
 * Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE). */
#include "op1_plugin.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int16_t host_sine[8192]; int32_t host_pitch_inc[128]; uint8_t host_voice_mem[6 * 0x24]; int16_t host_voice_vel[6];
int16_t host_voice_env[128]; int16_t host_accel[3]; int16_t host_knobs[8][8]; int host_slot;
const uint8_t *host_sample_buffer[8]; int16_t host_fx_knobs[3][8]; int16_t host_fx_meter[3]; void *host_fx_scratch[3];
volatile uint8_t op1_fx_page[3][4]; volatile uint8_t op1_eng_page[4];

/* screen mocks: record the node transforms and the raster lines of each frame (as video.c) */
static char node_id[8][16]; static int32_t node_m[8][9]; static int nnodes, first;
void *host_find_node(void *scr, const char *id)
{
    (void)scr;
    snprintf(node_id[nnodes], 16, "%s", id);
    static const int32_t ident[9] = { 1024, 0, 0, 0, 1024, 0, 0, 0, 1024 };
    memcpy(node_m[nnodes], ident, sizeof ident);
    return (void *)(intptr_t)(++nnodes);
}
void host_node_set_transform(void *node, const int32_t *m9) { memcpy(node_m[(intptr_t)node - 1], m9, 36); }
void host_node_set_visible(void *node, int on) { (void)node; (void)on; }
uint8_t *host_node_style_rgba(void *scr, const char *id) { static uint8_t c[4]; (void)scr; (void)id; return c; }
void host_screen_draw(void *scr, int dt) { (void)scr; (void)dt; }
void *host_raster(void) { return &first; }
void host_raster_line(void *r, const void *p0, const void *p1, int32_t w, const uint8_t *c)
{
    const op1_pt *a = p0, *b = p1; (void)r;
    printf("%s[%d,%d,%d,%d,%d,%d,%d,%d]", first ? "" : ",", a->x, a->y, b->x, b->y, w, c[0], c[1], c[2]);
    first = 0;
}
extern const op1_engine_t ENGINE_erdtree;
extern const op1_screen_t SCREEN_erdtree;

#define SR 44100
#define FPS 30
#define LEN 30.0
static double secs(int w) { double x = w / 16320.0; return fmax(0.003, 6 * x * x); }

/* knobs: choir (blue), strings (green), space (white), brass (orange). A scene glides k0 -> k1 over [g0, g1] s from
 * its start, plays the chord on voices base..base+2 from its start until off (s from its start), then releases. */
typedef struct {
    double start, dur; const char *caption;
    int16_t k0[4], k1[4]; double g0, g1;
    int notes[3], base; double off; int adsr[3]; double rel;   /* adsr: attack, decay, sustain words */
} scene_t;
static const scene_t S[] = {
    { 0.0, 3.5, "green: the strings swell in", { 0, 0, 6000, 0 }, { 0, 26000, 6000, 0 }, 0.2, 3.3,
      { 48, 55, 63 }, 0, 6.6, { 2500, 12000, 30000 }, 0.6 },
    { 3.5, 3.5, "orange: the brass comes in", { 0, 26000, 6000, 0 }, { 0, 26000, 6000, 26000 }, 0.1, 3.3,
      { 0 }, -1, 0, { 0 }, 0 },
    { 7.0, 3.5, "blue: the choir joins", { 0, 26000, 6000, 26000 }, { 26000, 24000, 6000, 26000 }, 0.1, 3.3,
      { 44, 51, 60 }, 3, 3.1, { 2500, 12000, 30000 }, 0.6 },
    { 10.5, 3.5, "white: the space opens up", { 26000, 24000, 6000, 26000 }, { 26000, 24000, 30000, 26000 }, 0.1, 3.3,
      { 43, 50, 59 }, 0, 2.9, { 2500, 12000, 30000 }, 0.6 },
    { 14.0, 4.0, "preset: golden order", { 8000, 20000, 9000, 30000 }, { 8000, 20000, 9000, 30000 }, 0, 0,
      { 48, 55, 64 }, 3, 3.2, { 600, 10000, 30000 }, 0.7 },
    { 18.0, 4.0, "preset: malenia lament", { 0, 26000, 26000, 0 }, { 0, 26000, 26000, 0 }, 0, 0,
      { 53, 60, 68 }, 0, 3.2, { 7000, 14000, 30000 }, 0.7 },
    { 22.0, 4.0, "preset: roundtable", { 32767, 6000, 8000, 0 }, { 32767, 6000, 8000, 0 }, 0, 0,
      { 56, 60, 63 }, 3, 3.2, { 3000, 12000, 32767 }, 0.7 },
    { 26.0, 4.0, "preset: leyndell hall", { 18000, 24000, 18000, 20000 }, { 18000, 24000, 18000, 20000 }, 0, 0,
      { 48, 55, 63 }, 0, 2.0, { 3000, 12000, 30000 }, 1.6 },
};
#define NS (int)(sizeof S / sizeof S[0])

typedef struct { int on; double t_on, t_off, a, d, s, r, level; int note; } env_t;
static env_t ev[6];
static double env_at(env_t *e, double t)
{
    if (!e->on) return 0;
    if (t < e->t_off) {
        double u = t - e->t_on;
        e->level = u < e->a ? u / e->a : u < e->a + e->d ? 1 - (1 - e->s) * (u - e->a) / e->d : e->s;
        return e->level;
    }
    double v = e->level * (1 - (t - e->t_off) / e->r);
    if (v <= 0) { e->on = 0; return 0; }
    return v;
}
static void knobs_at(double t, int16_t *k, const char **cap)
{
    int i = 0;
    while (i + 1 < NS && t >= S[i + 1].start) i++;
    const scene_t *s = &S[i];
    double u = t - s->start, f = s->g1 > s->g0 ? (u - s->g0) / (s->g1 - s->g0) : 1;
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    f = f * f * (3 - 2 * f);                                     /* a hand on the knob: ease in and out */
    for (int j = 0; j < 4; j++) k[j] = (int16_t)lrint(s->k0[j] + (s->k1[j] - s->k0[j]) * f);
    *cap = s->caption;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    FILE *audio = fopen(argv[1], "wb");
    if (!audio) return 1;
    for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
    SCREEN_erdtree.init(0);
    int16_t *k = host_knobs[0];
    k[4] = 0x5320;
    const char *cap;
    int next_scene = 0, frame = 0;
    int16_t out[OP1_BLOCK]; int32_t acc[OP1_BLOCK];
    for (long b = 0; (double)b * OP1_BLOCK / SR < LEN; b++) {
        double t = (double)b * OP1_BLOCK / SR;
        knobs_at(t, k, &cap);
        while (next_scene < NS && t >= S[next_scene].start) {        /* chords start on block edges, as on the OP-1 */
            const scene_t *s = &S[next_scene++];
            if (s->base < 0) continue;
            for (int i = 0; i < 3; i++) {
                env_t *e = &ev[s->base + i];
                *e = (env_t){ 1, t, t + s->off, secs(s->adsr[0]), secs(s->adsr[1]), s->adsr[2] / 32767.0, s->rel, 0,
                              s->notes[i] };
                host_voice_vel[s->base + i] = 100 << 8;
                ENGINE_erdtree.note_on(host_voice_mem + (s->base + i) * OP1_VOICE_SIZE, k);
            }
        }
        for (int n = 0; n < OP1_BLOCK; n++) acc[n] = 0;
        for (int v = 0; v < 6; v++) {
            if (!ev[v].on) continue;
            int32_t e = (int32_t)lrint(env_at(&ev[v], t) * 32767);
            int32_t inc = (int32_t)lrint(4294967296.0 * 440.0 * pow(2, (ev[v].note - 69) / 12.0) / SR);
            for (int n = 0; n < OP1_BLOCK; n++) { host_voice_env[n] = (int16_t)e; host_pitch_inc[n] = inc; }
            ENGINE_erdtree.render(host_voice_mem + v * OP1_VOICE_SIZE, out, k);
            for (int n = 0; n < OP1_BLOCK; n++) acc[n] += (out[n] * e) >> 15;
        }
        for (int n = 0; n < OP1_BLOCK; n++) out[n] = op1_sat16(acc[n]);
        fwrite(out, 2, OP1_BLOCK, audio);
        /* the frames that fall in this block: knobs at the frame's own time, the real screen code's drawing */
        while (frame < LEN * FPS && (double)frame / FPS < t + (double)OP1_BLOCK / SR) {
            int16_t kf[4]; const char *cf;
            knobs_at((double)frame / FPS, kf, &cf);
            for (int j = 0; j < 4; j++) k[j] = kf[j];
            printf("{\"t\":%.4f,\"caption\":\"%s\",\"k\":[%d,%d,%d,%d],\"lines\":[", (double)frame / FPS, cf,
                   k[0], k[1], k[2], k[3]);
            first = 1;
            SCREEN_erdtree.update(0, 1000 / FPS);
            printf("],\"nodes\":{");
            for (int i = 0; i < nnodes; i++) {
                printf("%s\"%s\":[", i ? "," : "", node_id[i]);
                for (int j = 0; j < 9; j++) printf("%s%d", j ? "," : "", node_m[i][j]);
                printf("]");
            }
            printf("}}\n");
            frame++;
            knobs_at(t, k, &cap);
        }
    }
    fclose(audio);
    return 0;
}
