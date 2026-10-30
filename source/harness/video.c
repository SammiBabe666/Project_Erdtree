/* Host frame dump for the demo video: runs the real screen code at 30 fps along the same knob moves as demo.c and
 * prints one JSON line per frame (knobs, crown transform, raster lines) for video.py.
 *   cc -DOP1_HOST -I<op1r-src>/synth_sdk -I.. video.c ../erdscreen.c -lm
 * Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE). */
#include "op1_plugin.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int16_t host_sine[8192]; int32_t host_pitch_inc[128]; uint8_t host_voice_mem[6 * 0x24]; int16_t host_voice_vel[6];
int16_t host_voice_env[128]; int16_t host_accel[3]; int16_t host_knobs[8][8]; int host_slot;
const uint8_t *host_sample_buffer[8]; int16_t host_fx_knobs[3][8]; int16_t host_fx_meter[3]; void *host_fx_scratch[3];
volatile uint8_t op1_fx_page[3][4]; volatile uint8_t op1_eng_page[4];

static char node_id[8][16]; static int32_t node_m[8][9]; static int nnodes;
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
static int nlines, first;
void host_screen_draw(void *scr, int dt) { (void)scr; (void)dt; }
void *host_raster(void) { return &nlines; }
void host_raster_line(void *r, const void *p0, const void *p1, int32_t w, const uint8_t *c)
{
    const op1_pt *a = p0, *b = p1; (void)r;
    printf("%s[%d,%d,%d,%d,%d,%d,%d,%d]", first ? "" : ",", a->x, a->y, b->x, b->y, w, c[0], c[1], c[2]);
    first = 0; nlines++;
}
extern const op1_screen_t SCREEN_erdtree;

int main(void)
{
    for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
    SCREEN_erdtree.init(0);
    int16_t *k = host_knobs[0];
    const int fps = 30, total = 23 * fps;
    for (int f = 0; f < total; f++) {
        double s = (double)f / fps; int scene = (int)(s / 4); double t = s / 4 - scene;
        switch (scene) {                                   /* as demo.c: blue choir, green strings, white space, red brass */
        case 0: k[0] = 0; k[1] = (int16_t)(32767 * t); k[2] = 6000; k[3] = 0; break;
        case 1: k[0] = 0; k[1] = 26000; k[2] = 6000; k[3] = (int16_t)(32767 * t); break;
        case 2: k[0] = (int16_t)(30000 * t); k[1] = 22000; k[2] = 6000; k[3] = 24000; break;
        case 3: k[0] = 24000; k[1] = 26000; k[2] = (int16_t)(6000 + 26767 * t); k[3] = 28000; break;
        default: break;                                    /* release and the reverb tail: knobs stay */
        }
        printf("{\"k\":[%d,%d,%d,%d],\"lines\":[", k[0], k[1], k[2], k[3]);
        first = 1;
        SCREEN_erdtree.update(0, 1000 / fps);
        printf("],\"nodes\":{");
        for (int i = 0; i < nnodes; i++) {
            printf("%s\"%s\":[", i ? "," : "", node_id[i]);
            for (int j = 0; j < 9; j++) printf("%s%d", j ? "," : "", node_m[i][j]);
            printf("]");
        }
        printf("}}\n");
    }
    return 0;
}
