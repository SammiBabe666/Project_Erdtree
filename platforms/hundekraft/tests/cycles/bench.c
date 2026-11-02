/* Cycle bench: renders 6 voices x 20 blocks of erdtree (several knob settings) and supersaw in GDB's Blackfin
 * simulator and prints the cycles per voice-block. The simulator counts instructions and branch delays only: no
 * multi-cycle 32-bit multiplies, no cache or SDRAM waits, so compare engines with it rather than read it as hardware.
 *   build: see run.sh. Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission. */
#include "op1_plugin.h"
extern void sys_write(int fd, const char *buf, int len);
extern const op1_engine_t ENGINE_erdtree, ENGINE_supersaw;
static void puts_(const char *s) { int n = 0; while (s[n]) n++; sys_write(1, s, n); }
static void putu(uint32_t v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); puts_(b + i); }
static inline uint32_t cyc(void) { uint32_t c; __asm__ volatile("%0 = CYCLES;" : "=d"(c)); return c; }
static void bench(const char *name, const op1_engine_t *e, const int16_t *k)
{
    int16_t params[8] = { k[0], k[1], k[2], k[3], 0x5320, 0, 0, 0 };
    static int16_t out[OP1_BLOCK];
    for (int v = 0; v < OP1_VOICES; v++) {
        uint32_t *w = (uint32_t *)OP1_VOICE(v);
        for (unsigned i = 0; i < OP1_VOICE_SIZE / 4; i++) w[i] = 0;
        e->note_on(OP1_VOICE(v), params);
    }
    uint32_t best = 0xffffffffu, total = 0; int blocks = 20;
    for (int b = 0; b < blocks; b++)
        for (int v = 0; v < OP1_VOICES; v++) {
            uint32_t c0 = cyc();
            e->render(OP1_VOICE(v), out, params);
            uint32_t d = cyc() - c0;
            total += d; if (d < best) best = d;
        }
    puts_(name); puts_(": avg "); putu(total / (blocks * OP1_VOICES)); puts_(" cycles per voice-block, min ");
    putu(best); puts_(", six voices = "); putu(total / blocks); puts_("\n");
}
int main(void)
{
    int16_t *sine = (int16_t *)OP1_SINE_TABLE;
    for (int i = 0; i < 8192; i++) {  /* rough sine, the values don't change the timing */
        int x = (i & 4095) - 2048; int y = 32767 - (int)(((int64_t)x * x * 32767) >> 22);
        sine[i] = (int16_t)(i < 4096 ? y : -y);
    }
    int32_t *inc = (int32_t *)OP1_PITCH_INC; int16_t *env = (int16_t *)OP1_VOICE_ENV; int16_t *vel = (int16_t *)OP1_VOICE_VEL;
    for (int i = 0; i < OP1_BLOCK; i++) { inc[i] = 42852281; env[i] = 20000; }   /* 440 Hz */
    for (int i = 0; i < OP1_VOICES; i++) vel[i] = 100 << 8;
    static const int16_t all[4] = { 32767, 32767, 16000, 32767 }, def[4] = { 10000, 26000, 12000, 16000 },
                         strings[4] = { 0, 26000, 26000, 0 }, ss[4] = { 16000, 16000, 16000, 16000 };
    bench("erdtree all sections", &ENGINE_erdtree, all);
    bench("erdtree first grace ", &ENGINE_erdtree, def);
    bench("erdtree strings only", &ENGINE_erdtree, strings);
    static const int16_t choir[4] = { 32767, 0, 16000, 0 }, brass[4] = { 0, 0, 16000, 32767 }, none[4] = { 0, 0, 16000, 0 };
    bench("erdtree choir only  ", &ENGINE_erdtree, choir);
    bench("erdtree brass only  ", &ENGINE_erdtree, brass);
    bench("erdtree nothing     ", &ENGINE_erdtree, none);
    bench("supersaw            ", &ENGINE_supersaw, ss);
    return 0;
}
