/* PC render of the Opus port for the demo video and the screenshots: the port's T1 page (erd_page.c) at 30 frames a
 * second and its sound (erdtree.c), along a knob script. The core's drawing calls print each line; the OS's voice loop
 * is stood in for: 128 samples at 88.2 kHz a block per voice (render's contract), a simple envelope (80 ms attack,
 * sustain 0.8, 600 ms release) multiplied in as the OS does, the sum taken down to 44.1 kHz by averaging pairs.
 *   cc -O2 -DERD_HOST -I<plugin-kit>/sdk -Iopus/sammi.erdtree opus/test/opus_video.c opus/sammi.erdtree/erdtree.c -lm
 *   ./a.out video FRAMES.txt AUDIO.raw          the demo: five scenes of 4 s, then a 2 s tail
 *   ./a.out still FRAMES.txt C S W B MS          one frame at these knobs, MS ms after the page first showed
 * FRAMES.txt: "F c s w b" per frame, then "L x0 y0 x1 y1 w rrggbb" per line (units x 256, width x 256 px). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "opus_plugin.h"
static unsigned fake_cycles;
#define ERD_PAGE_HOST_CYCLES fake_cycles
#include "erd_page.c"

static short knobs[8];
static FILE *fr;
int opus_engine_live(short par[8]) { for (int i = 0; i < 8; i++) par[i] = knobs[i]; return 1; }
unsigned opus_accent_now(int i) { static const unsigned c[4] = { 0x4D9EFF, 0x00ED95, 0xADADFF, 0xFF3A5D }; return c[i & 3]; }
void *opus_alloc(struct opl_self *s, unsigned n) { (void)s; return calloc(1, n); }
int opus_art_text(struct opl_self *s, const char *n, const char *svg, unsigned len) { (void)s; (void)n; (void)svg; (void)len; return -1; }
int opus_art_ready(int a) { (void)a; return 0; }
void opus_art_draw(int a) { (void)a; }
int opus_art_colour_slots(int a, unsigned rgb, unsigned char **s, int m) { (void)a; (void)rgb; (void)s; (void)m; return 0; }
void opus_art_set_colour(unsigned char **s, int n, unsigned rgb) { (void)s; (void)n; (void)rgb; }
unsigned opus_art_colours(void) { return 0; }
unsigned short *opus_page_pixels(int *rows) { *rows = 320; return 0; }
int opus_page_keep(unsigned short *o, unsigned c, int a, int b, int d, int e) { (void)o; (void)c; (void)a; (void)b; (void)d; (void)e; return -1; }
void opus_page_put(const unsigned short *r, int u) { (void)r; (void)u; }
void opus_sincos(int t, int *s, int *c)
{
    *s = (int)lrint(4096 * sin(2 * M_PI * t / 4096.0));
    *c = (int)lrint(4096 * cos(2 * M_PI * t / 4096.0));
}
void opus_page_stroke_line(int x0, int y0, int x1, int y1, int w, int colour)
{
    fprintf(fr, "L %d %d %d %d %d %06x\n", x0, y0, x1, y1, w, colour & 0xFFFFFF);
}

/* the engine, as the core gets it */
struct engine_head {
    unsigned size;
    const char *name;
    short knobs[8];
    unsigned voice;
    void (*init)(void *voice, int index);
    void (*note_on)(void *voice, const short *knobs);
    void (*release)(void *voice, const short *knobs);
    void (*render)(void *voice, short *out, const short *par, const unsigned *pitch);
};
static const struct engine_head *eng;
int16_t erd_host_env[128];
int opus_engine_add(struct opl_self *self, const struct opus_engine *e) { (void)self; eng = (const void *)e; return 0x100; }
int opl_init(struct opl_self *self);

static void frame(void)
{
    fprintf(fr, "F %d %d %d %d\n", knobs[0], knobs[1], knobs[2], knobs[3]);
    erd_page();
}

#define NV 4
#define SR 44100
static unsigned inc_of(double midi) { return (unsigned)llrint(4294967296.0 * 440.0 * pow(2, (midi - 69) / 12.0) / 88200); }

static void video(FILE *audio)
{
    /* blue choir, green strings, white space, orange brass; chords: C minor, Ab major, F minor / Ab, G */
    static const int chords[4][NV] = { { 48, 55, 60, 63 }, { 44, 56, 60, 63 }, { 41, 56, 60, 65 }, { 43, 55, 62, 67 } };
    static unsigned char vmem[NV][64];
    int32_t env[NV] = { 0 };
    int held[NV] = { 0 };
    const int scene_ms = 4000, total_ms = 5 * scene_ms + 2000;
    long samples = 0;
    int next_frame = 0;
    for (int v = 0; v < NV; v++) eng->init(vmem[v], v);
    for (int scene = -1; samples < (long)total_ms * SR / 1000; ) {
        const int ms = (int)(samples * 1000 / SR);
        const int sc = ms / scene_ms;
        const double t = (double)(ms % scene_ms) / scene_ms;
        short *k = knobs;
        switch (sc) {
        case 0: k[0] = 0; k[1] = (short)(32767 * t); k[2] = 6000; k[3] = 0; break;
        case 1: k[0] = 0; k[1] = 26000; k[2] = 6000; k[3] = (short)(32767 * t); break;
        case 2: k[0] = (short)(30000 * t); k[1] = 22000; k[2] = 6000; k[3] = 24000; break;
        case 3: k[0] = 24000; k[1] = 26000; k[2] = (short)(6000 + 26767 * t); k[3] = 28000; break;
        default: break;                                 /* scene 4: all held as they are; then the tail */
        }
        if (sc != scene && sc < 5) {                    /* a new chord each scene */
            scene = sc;
            for (int v = 0; v < NV; v++) { eng->note_on(vmem[v], knobs); held[v] = 1; }
        }
        if (sc >= 5) for (int v = 0; v < NV; v++) held[v] = 0;
        const int c = sc < 4 ? sc : 3;
        static int32_t acc[128];
        short out[128];
        unsigned pitch[128];
        memset(acc, 0, sizeof acc);
        for (int v = 0; v < NV; v++) {
            const int32_t target = held[v] ? 26214 : 0;
            const int32_t step = held[v] ? 32767 * 64 / (SR * 8 / 100) : 26214 * 64 / (SR * 6 / 10);
            if (env[v] < target) { env[v] += step; if (env[v] > target) env[v] = target; }
            else if (env[v] > target) { env[v] -= step; if (env[v] < target) env[v] = target; }
            for (int n = 0; n < 128; n++) { erd_host_env[n] = (int16_t)env[v]; pitch[n] = inc_of(chords[c][v]); }
            eng->render(vmem[v], out, knobs, pitch);
            for (int n = 0; n < 128; n++) acc[n] += (out[n] * env[v]) >> 15;
        }
        short o44[64];
        for (int n = 0; n < 64; n++) {
            int s = (acc[2 * n] + acc[2 * n + 1]) / 2;
            o44[n] = (short)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
        }
        fwrite(o44, 2, 64, audio);
        samples += 64;
        while ((long)next_frame * SR / 30 <= samples) {   /* frames at 30 a second */
            fake_cycles = (unsigned)((unsigned long long)next_frame * 400000000ull / 30);
            frame();
            next_frame++;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    fr = fopen(argv[2], "w");
    opl_init(0);
    if (!strcmp(argv[1], "video") && argc > 3) {
        FILE *a = fopen(argv[3], "wb");
        video(a);
        fclose(a);
    } else if (!strcmp(argv[1], "still") && argc > 7) {
        for (int i = 0; i < 4; i++) knobs[i] = (short)atoi(argv[3 + i]);
        erd_page();                                     /* the first frame starts the clock */
        fclose(fr);
        fr = fopen(argv[2], "w");
        fake_cycles = (unsigned)atoi(argv[7]) * 400000u;
        frame();
    } else
        return 2;
    fclose(fr);
    return 0;
}
