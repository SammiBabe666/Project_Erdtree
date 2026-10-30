/* Host audition of one preset: a 4-voice C minor chord held 3 s, then 2 s of release, with the preset's ADSR roughly
 * stood in for (time ~ 6 s * (word / 16320)^2, linear segments; the OP-1's real curve differs) and applied to the
 * output as the firmware does after render. No FX: video/audition scripts add a stand-in reverb. Raw int16 mono on
 * stdout.  args: choir strings space brass attack decay sustain release octave
 *   cc -DOP1_HOST -I<op1r-src>/synth_sdk -I.. audition.c ../erdtree.c -lm
 * Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE). */
#include "op1_plugin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int16_t host_sine[8192]; int32_t host_pitch_inc[128]; uint8_t host_voice_mem[6 * 0x24]; int16_t host_voice_vel[6];
int16_t host_voice_env[128]; int16_t host_accel[3]; int16_t host_knobs[8][8]; int host_slot;
const uint8_t *host_sample_buffer[8]; int16_t host_fx_knobs[3][8]; int16_t host_fx_meter[3]; void *host_fx_scratch[3];
volatile uint8_t op1_fx_page[3][4]; volatile uint8_t op1_eng_page[4];
extern const op1_engine_t ENGINE_erdtree;
static double secs(int w) { double x = w / 16320.0; return fmax(0.003, 6 * x * x); }
int main(int argc, char **argv)
{
    if (argc < 10) return 2;
    for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
    int16_t *k = host_knobs[0];
    for (int i = 0; i < 4; i++) k[i] = (int16_t)atoi(argv[1 + i]);
    k[4] = 0x5320;
    double a = secs(atoi(argv[5])), d = secs(atoi(argv[6])), s = atoi(argv[7]) / 32767.0, r = secs(atoi(argv[8]));
    int oct = atoi(argv[9]);
    static const int chord[4] = { 48, 55, 60, 63 };
    const double bt = 128.0 / 44100, hold = 3.0, total = 5.0;
    double rel_from[4] = { 0 };
    int16_t out[128]; int32_t acc[128];
    for (int v = 0; v < 4; v++) { host_voice_vel[v] = 100 << 8; ENGINE_erdtree.note_on(host_voice_mem + v * 0x24, k); }
    for (double t = 0; t < total; t += bt) {
        for (int n = 0; n < 128; n++) acc[n] = 0;
        for (int v = 0; v < 4; v++) {
            double e;
            if (t < hold) { e = t < a ? t / a : t < a + d ? 1 - (1 - s) * (t - a) / d : s; rel_from[v] = e; }
            else e = fmax(0, rel_from[v] * (1 - (t - hold) / r));
            int16_t ev = (int16_t)lrint(e * 32767 * 100 / 127);
            for (int n = 0; n < 128; n++) {
                host_voice_env[n] = ev;
                host_pitch_inc[n] = (int32_t)lrint(4294967296.0 * 440.0 * pow(2, (chord[v] + 12 * oct - 69) / 12.0) / 44100);
            }
            ENGINE_erdtree.render(host_voice_mem + v * 0x24, out, k);
            for (int n = 0; n < 128; n++) acc[n] += (out[n] * ev) >> 15;
        }
        for (int n = 0; n < 128; n++) out[n] = op1_sat16(acc[n]);
        fwrite(out, 2, 128, stdout);
    }
    return 0;
}
