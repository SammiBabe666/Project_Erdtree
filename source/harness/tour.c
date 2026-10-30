/* Host render of the preset tour film: tour.py writes the timeline (each preset's knobs, ADSR and octave, and the
 * notes it plays) on stdin; this drives the engine (erdtree.c) and the screen (erdscreen.c) through it, as film.c does.
 * Notes take a free voice of six, else the oldest. Each note uses the ADSR of the preset playing at its start, stood in
 * by a linear ADSR as in film.c; when the next preset loads, voices still sounding fade out over 0.25 s. No effects:
 * tour.py adds stand-ins.
 *   cc -DOP1_HOST -I<op1r-src>/synth_sdk -I.. tour.c ../erdtree.c ../erdscreen.c -lm
 *   ./a.out audio.raw < timeline.txt > frames.jsonl     (raw int16 mono 44.1 kHz; one JSON line per 30 fps frame)
 * timeline.txt: "L <seconds>", "S <n>" then n lines "start knob0..3 attack decay sustain release octave" (knobs and
 * ADSR as OP-1 words), "N <m>" then m lines "on off note" (seconds, MIDI note before the octave shift).
 * Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE). */
#include "op1_plugin.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int16_t host_sine[8192]; int32_t host_pitch_inc[128]; uint8_t host_voice_mem[6 * 0x24]; int16_t host_voice_vel[6];
int16_t host_voice_env[128]; int16_t host_accel[3]; int16_t host_knobs[8][8]; int host_slot;
const uint8_t *host_sample_buffer[8]; int16_t host_fx_knobs[3][8]; int16_t host_fx_meter[3]; void *host_fx_scratch[3];
volatile uint8_t op1_fx_page[3][4]; volatile uint8_t op1_eng_page[4];

/* screen mocks: record the node transforms and the raster lines of each frame (as film.c) */
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
#define MAXS 16
#define MAXN 256
static double secs(int w) { double x = w / 16320.0; return fmax(0.003, 6 * x * x); }

typedef struct { double start; int k[4], adsr[4], oct; } scene_t;
typedef struct { double on, off; int note; } note_t;
static scene_t S[MAXS]; static note_t N[MAXN]; static int ns, nn; static double len;

typedef struct { int on, note; double t_on, t_off, a, d, s, r, level; } env_t;
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
static int scene_at(double t)
{
    int i = 0;
    while (i + 1 < ns && t >= S[i + 1].start) i++;
    return i;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    if (scanf(" L %lf S %d", &len, &ns) != 2 || ns < 1 || ns > MAXS) return 3;
    for (int i = 0; i < ns; i++) {
        scene_t *s = &S[i];
        if (scanf("%lf %d %d %d %d %d %d %d %d %d", &s->start, &s->k[0], &s->k[1], &s->k[2], &s->k[3], &s->adsr[0],
                  &s->adsr[1], &s->adsr[2], &s->adsr[3], &s->oct) != 10) return 3;
    }
    if (scanf(" N %d", &nn) != 1 || nn < 0 || nn > MAXN) return 3;
    for (int i = 0; i < nn; i++)
        if (scanf("%lf %lf %d", &N[i].on, &N[i].off, &N[i].note) != 3) return 3;
    FILE *audio = fopen(argv[1], "wb");
    if (!audio) return 1;
    for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
    SCREEN_erdtree.init(0);
    int16_t *k = host_knobs[0];
    k[4] = 0x5320;
    int next = 0, frame = 0, cur = -1;
    int16_t out[OP1_BLOCK]; int32_t acc[OP1_BLOCK];
    for (long b = 0; (double)b * OP1_BLOCK / SR < len; b++) {
        double t = (double)b * OP1_BLOCK / SR;
        int sc = scene_at(t);
        if (sc != cur) {                                         /* the next preset loads: knobs jump, notes fade */
            cur = sc;
            for (int j = 0; j < 4; j++) k[j] = (int16_t)S[sc].k[j];
            for (int v = 0; v < 6; v++)
                if (ev[v].on) {
                    double cv = env_at(&ev[v], t);
                    ev[v].level = cv; ev[v].t_off = t; ev[v].r = fmin(ev[v].r, 0.25);
                }
        }
        while (next < nn && t >= N[next].on) {                    /* notes start on block edges, as on the OP-1 */
            const note_t *n = &N[next++];
            const scene_t *s = &S[scene_at(n->on)];
            int v = -1;
            for (int i = 0; i < 6 && v < 0; i++) if (!ev[i].on) v = i;
            if (v < 0) { v = 0; for (int i = 1; i < 6; i++) if (ev[i].t_on < ev[v].t_on) v = i; }
            ev[v] = (env_t){ 1, n->note + 12 * s->oct, t, n->off, secs(s->adsr[0]), secs(s->adsr[1]),
                             s->adsr[2] / 32767.0, secs(s->adsr[3]), 0 };
            host_voice_vel[v] = 100 << 8;
            ENGINE_erdtree.note_on(host_voice_mem + v * OP1_VOICE_SIZE, k);
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
        /* the frames that fall in this block: the preset playing at the frame's own time, the real screen code */
        while (frame < len * FPS && (double)frame / FPS < t + (double)OP1_BLOCK / SR) {
            double tf = (double)frame / FPS;
            int sf = scene_at(tf);
            for (int j = 0; j < 4; j++) k[j] = (int16_t)S[sf].k[j];
            printf("{\"t\":%.4f,\"scene\":%d,\"k\":[%d,%d,%d,%d],\"lines\":[", tf, sf, k[0], k[1], k[2], k[3]);
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
            for (int j = 0; j < 4; j++) k[j] = (int16_t)S[cur].k[j];
        }
    }
    fclose(audio);
    return 0;
}
