/* Host vs target check: renders erdtree's six voices over 40 blocks at several knob settings, with a pitch glide,
 * an envelope ramp and every velocity, and prints a hash of all samples. Built for the PC (-DOP1_HOST) and for the
 * Blackfin (run in GDB's simulator, see run.sh), the two hashes must be equal: the target's 16 x 16 multiplies
 * (inline assembly) and its integer code give the same samples as the host build the other tests use.
 * Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE). */
#include "op1_plugin.h"
extern const op1_engine_t ENGINE_erdtree;
#ifdef OP1_HOST
#include <stdio.h>
int16_t host_sine[8192]; int32_t host_pitch_inc[128]; uint8_t host_voice_mem[6 * 0x24]; int16_t host_voice_vel[6];
int16_t host_voice_env[128]; int16_t host_accel[3]; int16_t host_knobs[8][8]; int host_slot;
const uint8_t *host_sample_buffer[8]; int16_t host_fx_knobs[3][8]; int16_t host_fx_meter[3]; void *host_fx_scratch[3];
volatile uint8_t op1_fx_page[3][4]; volatile uint8_t op1_eng_page[4];
#define VOICE(v) ((void *)(host_voice_mem + (v) * OP1_VOICE_SIZE))
static void report(uint32_t h) { printf("hash %08x\n", (unsigned)h); }
#else
extern void sys_write(int fd, const char *buf, int len);
#define VOICE(v) OP1_VOICE(v)
static void report(uint32_t h)
{
    char b[15] = "hash ........\n";
    for (int i = 0; i < 8; i++) b[5 + i] = "0123456789abcdef"[(h >> (28 - 4 * i)) & 15];
    sys_write(1, b, 14);
}
#endif
int main(void)
{
    static const int16_t knobs[6][4] = { { 10000, 26000, 12000, 16000 }, { 32767, 32767, 32767, 32767 },
        { 32767, 0, 0, 0 }, { 0, 0, 32767, 32767 }, { -3000, 20000, 5000, -100 }, { 0, 0, 0, 0 } };
    int32_t *inc = (int32_t *)OP1_PITCH_INC; int16_t *env = (int16_t *)OP1_VOICE_ENV;
    int16_t *vel = (int16_t *)OP1_VOICE_VEL;
    int16_t out[OP1_BLOCK];
    uint32_t h = 2166136261u;
    for (int s = 0; s < 6; s++) {
        int16_t params[8] = { knobs[s][0], knobs[s][1], knobs[s][2], knobs[s][3], 0x5320, 0, 0, 0 };
        for (int v = 0; v < OP1_VOICES; v++) {
            uint32_t *w = (uint32_t *)VOICE(v);
            for (unsigned i = 1; i < OP1_VOICE_SIZE / 4; i++) w[i] = 0;
            vel[v] = (int16_t)((1 + 21 * v + 9 * s) % 128 << 8);
            ENGINE_erdtree.note_on(VOICE(v), params);
        }
        for (int b = 0; b < 40; b++)
            for (int v = 0; v < OP1_VOICES; v++) {
                for (int n = 0; n < OP1_BLOCK; n++) {
                    inc[n] = 2000000 + 9000000 * v + 3000 * (b * OP1_BLOCK + n) + 400000000 * (s == 1 && v == 5);
                    env[n] = (int16_t)(b < 20 ? b * 1600 + n : 32767 - b * 700);
                }
                ENGINE_erdtree.render(VOICE(v), out, params);
                for (int n = 0; n < OP1_BLOCK; n++) h = (h ^ (uint16_t)out[n]) * 16777619u;
            }
    }
    report(h);
    return 0;
}
