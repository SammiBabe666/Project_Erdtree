/* Host range test: every MIDI note 0..127, knob corners, envelope 0 / full / ramps, under ASan + UBSan (no recovery):
 * any overflow or out-of-bounds read aborts. Also checks the voice vtable word is never written.
 *   cc -fsanitize=address,undefined -fno-sanitize-recover=undefined -DOP1_HOST -I<op1r-src>/synth_sdk -I.. range_test.c ../erdtree.c -lm */
#include "op1_plugin.h"
#include <math.h>
#include <stdio.h>
int16_t host_sine[8192]; int32_t host_pitch_inc[128]; uint8_t host_voice_mem[6 * 0x24]; int16_t host_voice_vel[6];
int16_t host_voice_env[128]; int16_t host_accel[3]; int16_t host_knobs[8][8]; int host_slot;
const uint8_t *host_sample_buffer[8]; int16_t host_fx_knobs[3][8]; int16_t host_fx_meter[3]; void *host_fx_scratch[3];
volatile uint8_t op1_fx_page[3][4]; volatile uint8_t op1_eng_page[4];
extern const op1_engine_t ENGINE_erdtree;
int main(void)
{
    for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
    static const int16_t corners[] = { 0, 1, 16384, 32766, 32767 };
    int16_t out[OP1_BLOCK]; long blocks = 0; int peak = 0;
    for (int v = 0; v < 6; v++) *(uint32_t *)(host_voice_mem + v * 0x24) = 0xdeadbeef;
    for (int midi = 0; midi < 128; midi += 1)
        for (int c = 0; c < 625; c += 7) {
            int16_t k[8] = { corners[c % 5], corners[(c / 5) % 5], corners[(c / 25) % 5], corners[(c / 125) % 5], 0x5301 };
            int32_t inc = (int32_t)lrint(4294967296.0 * 440.0 * pow(2, (midi - 69) / 12.0) / 44100);
            for (int i = 0; i < 128; i++) host_pitch_inc[i] = inc;
            void *self = host_voice_mem + (midi % 6) * 0x24;
            host_voice_vel[midi % 6] = (int16_t)((1 + (c % 127)) << 8);
            ENGINE_erdtree.note_on(self, k);
            for (int b = 0; b < 400; b++) {               /* 1.16 s: age reaches full at any space */
                int16_t e = b < 3 ? 0 : b < 200 ? 32767 : (int16_t)(32767 - (b - 200) * 160);
                for (int i = 0; i < 128; i++) host_voice_env[i] = e < 0 ? 0 : e;
                ENGINE_erdtree.render(self, out, k); blocks++;
                for (int i = 0; i < 128; i++) { int a = out[i] < 0 ? -out[i] : out[i]; if (a > peak) peak = a; }
            }
        }
    for (int v = 0; v < 6; v++) if (*(uint32_t *)(host_voice_mem + v * 0x24) != 0xdeadbeef) { puts("vtable overwritten"); return 1; }
    printf("range test ok: %ld blocks, all notes, knob corners, peak %d\n", blocks, peak);
    return 0;
}
