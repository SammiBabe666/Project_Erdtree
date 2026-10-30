/* Host demo for erdtree: a short "conducted" passage, 4 voices, the stock envelope stood in by a simple ADSR that is
 * also applied to the output (as the firmware does after render). Writes raw int16 mono 44.1 kHz to stdout.
 *   cc -DOP1_HOST -I<op1r-src>/synth_sdk -I.. demo.c ../erdtree.c -lm
 * Scenes (4 s each): strings swell in on green; brass enters on orange over the strings; choir joins on blue;
 * white (space) opens up with everything held. */
#include "op1_plugin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int16_t host_sine[8192]; int32_t host_pitch_inc[128]; uint8_t host_voice_mem[6 * 0x24]; int16_t host_voice_vel[6];
int16_t host_voice_env[128]; int16_t host_accel[3]; int16_t host_knobs[8][8]; int host_slot;
const uint8_t *host_sample_buffer[8]; int16_t host_fx_knobs[3][8]; int16_t host_fx_meter[3]; void *host_fx_scratch[3];
volatile uint8_t op1_fx_page[3][4]; volatile uint8_t op1_eng_page[4];

extern const op1_engine_t ENGINE_erdtree;
static int32_t inc_of(double midi) { return (int32_t)lrint(4294967296.0 * 440.0 * pow(2, (midi - 69) / 12.0) / 44100); }

#define SR 44100
#define NV 4
int main(void)
{
    for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
    /* chords: C minor, Ab major, F minor / Ab, G (sus -> major) */
    static const int chords[4][NV] = { { 48, 55, 60, 63 }, { 44, 56, 60, 63 }, { 41, 56, 60, 65 }, { 43, 55, 62, 67 } };
    const int blocks_per_scene = 4 * SR / OP1_BLOCK;
    int32_t env[NV] = { 0 }; int held[NV] = { 0 };
    int16_t out[OP1_BLOCK]; static int32_t acc[OP1_BLOCK];
    for (int scene = 0; scene < 5; scene++) {
        int c = scene < 4 ? scene : 3;
        for (int v = 0; v < NV; v++) {
            if (scene < 4) {
                host_voice_vel[v] = (int16_t)(100 << 8);
                ENGINE_erdtree.note_on(host_voice_mem + v * 0x24, host_knobs[0]);
                held[v] = 1;
            }
        }
        for (int b = 0; b < blocks_per_scene; b++) {
            double t = (double)b / blocks_per_scene;                   /* 0..1 through the scene */
            int16_t *k = host_knobs[0];
            switch (scene) {                                            /* blue choir, green strings, white space, red brass */
            case 0: k[0] = 0; k[1] = (int16_t)(32767 * t); k[2] = 6000; k[3] = 0; break;
            case 1: k[0] = 0; k[1] = 26000; k[2] = 6000; k[3] = (int16_t)(32767 * t); break;
            case 2: k[0] = (int16_t)(30000 * t); k[1] = 22000; k[2] = 6000; k[3] = 24000; break;
            case 3: k[0] = 24000; k[1] = 26000; k[2] = (int16_t)(6000 + 26767 * t); k[3] = 28000; break;
            default: break;                                             /* release tail */
            }
            if (scene == 3 && b == blocks_per_scene - 1) for (int v = 0; v < NV; v++) held[v] = 0;
            if (scene == 4) for (int v = 0; v < NV; v++) held[v] = 0;
            for (int n = 0; n < OP1_BLOCK; n++) acc[n] = 0;
            for (int v = 0; v < NV; v++) {
                /* ADSR stand-in: 80 ms attack, sustain 0.8, 600 ms release */
                int32_t target = held[v] ? 26214 : 0;
                int32_t step = held[v] ? 32767 * OP1_BLOCK / (SR * 8 / 100) : 26214 * OP1_BLOCK / (SR * 6 / 10);
                if (env[v] < target) { env[v] += step; if (env[v] > target) env[v] = target; }
                else if (env[v] > target) { env[v] -= step; if (env[v] < target) env[v] = target; }
                for (int n = 0; n < OP1_BLOCK; n++) { host_voice_env[n] = (int16_t)env[v]; host_pitch_inc[n] = inc_of(chords[c][v]); }
                ENGINE_erdtree.render(host_voice_mem + v * 0x24, out, host_knobs[0]);
                for (int n = 0; n < OP1_BLOCK; n++) acc[n] += (out[n] * env[v]) >> 15;
            }
            for (int n = 0; n < OP1_BLOCK; n++) out[n] = op1_sat16(acc[n]);
            fwrite(out, 2, OP1_BLOCK, stdout);
        }
    }
    return 0;
}
