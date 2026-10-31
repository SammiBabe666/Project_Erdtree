/* PC test of the Opus port's sound against erdtree v0.5: both engines render the same notes (every MIDI note, knob
 * corners, the envelope's attack, sustain and release) and the samples are compared. v0.5 multiplied by the velocity
 * on top of the OS's envelope; the port leaves the velocity to the OS, so at full velocity they differ only by v0.5's
 * gain of 32766 / 32767 (a sample or two). Run under ASan + UBSan, so an overflow or a bad read in the port aborts.
 *   cc -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -DOP1_HOST -DERD_HOST \
 *      -I<op1r-src>/synth_sdk -I<plugin-kit>/sdk -Iopus/sammi.erdtree \
 *      opus/test/match_test.c source/erdtree.c opus/sammi.erdtree/erdtree.c -lm
 * (opus/test/run.sh does it.) */
#include "op1_plugin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* op1r's host harness, for v0.5 */
int16_t host_sine[8192]; int32_t host_pitch_inc[128]; uint8_t host_voice_mem[6 * 0x24]; int16_t host_voice_vel[6];
int16_t host_voice_env[128]; int16_t host_accel[3]; int16_t host_knobs[8][8]; int host_slot;
const uint8_t *host_sample_buffer[8]; int16_t host_fx_knobs[3][8]; int16_t host_fx_meter[3]; void *host_fx_scratch[3];
volatile uint8_t op1_fx_page[3][4]; volatile uint8_t op1_eng_page[4];
extern const op1_engine_t ENGINE_erdtree;

/* the core, for the port: opus_engine_add keeps the struct; its page is not drawn here */
struct opl_self;
struct opus_engine_head {
    unsigned size;
    const char *name;
    short knobs[8];
    unsigned voice;
    void (*init)(void *voice, int index);
    void (*note_on)(void *voice, const short *knobs);
    void (*release)(void *voice, const short *knobs);
    void (*render)(void *voice, short *out, const short *par, const unsigned *pitch);
};
int16_t erd_host_env[128];
static const struct opus_engine_head *port;
int opus_engine_add(struct opl_self *self, const void *e) { (void)self; port = e; return 0x100; }
void erd_page_init(struct opl_self *self) { (void)self; }
void erd_page(void) {}
int opl_init(struct opl_self *self);

int main(void)
{
    for (int i = 0; i < 8192; i++) host_sine[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 8192.0));
    if (opl_init(0) || !port || !port->render) { puts("the port did not register its engine"); return 1; }
    printf("port: engine \"%s\", %u bytes a voice\n", port->name, port->voice);
    static const int16_t corners[] = { 0, 1, 16384, 32766, 32767 };
    int16_t a[128], b[128];
    unsigned pitch[128];
    static unsigned char pv[6][64];
    long blocks = 0, samples = 0, differ = 0; int maxd = 0, peak = 0;
    for (int midi = 0; midi < 128; midi++)
        for (int c = 0; c < 625; c += 7) {
            int16_t k[8] = { corners[c % 5], corners[(c / 5) % 5], corners[(c / 25) % 5], corners[(c / 125) % 5], 0x5300 };
            int32_t inc = (int32_t)lrint(4294967296.0 * 440.0 * pow(2, (midi - 69) / 12.0) / 44100);
            for (int i = 0; i < 128; i++) host_pitch_inc[i] = inc, pitch[i] = (unsigned)inc;
            const int vi = midi % 6;
            void *self = host_voice_mem + vi * 0x24;
            host_voice_vel[vi] = 127 << 8;
            port->init(pv[vi], vi);
            ENGINE_erdtree.note_on(self, k);
            port->note_on(pv[vi], k);
            for (int bl = 0; bl < 400; bl++) {
                int16_t e = bl < 3 ? 0 : bl < 200 ? 32767 : (int16_t)(32767 - (bl - 200) * 160);
                for (int i = 0; i < 128; i++) host_voice_env[i] = erd_host_env[i] = e < 0 ? 0 : e;
                ENGINE_erdtree.render(self, a, k);
                port->render(pv[vi], b, k, pitch);
                blocks++;
                for (int i = 0; i < 128; i++) {
                    int d = abs(a[i] - b[i]);
                    maxd = d > maxd ? d : maxd;
                    differ += d != 0;
                    int p = abs(b[i]);
                    peak = p > peak ? p : peak;
                    samples++;
                }
            }
        }
    printf("%ld blocks, %ld samples: %ld differ, by %d at most; the port's peak %d\n", blocks, samples, differ, maxd, peak);
    if (maxd > 4) { puts("FAIL: the port's sound differs from v0.5"); return 1; }
    puts("match test ok");
    return 0;
}
