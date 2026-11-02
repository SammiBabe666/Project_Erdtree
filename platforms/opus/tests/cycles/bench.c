/* Cycle bench for the Opus port: six voices x 20 blocks of the engine at several knob settings, in GDB's Blackfin
 * simulator (instructions and branch delays only: no SDRAM or cache waits). See run.sh. */
#include "opus_plugin.h"
extern void sys_write(int fd, const char *buf, int len);
static void puts_(const char *s) { int n = 0; while (s[n]) n++; sys_write(1, s, n); }
static void putu(unsigned v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); puts_(b + i); }
static inline unsigned cyc(void) { unsigned c; __asm__ volatile("%0 = CYCLES;" : "=d"(c)); return c; }

static const struct opus_engine *eng;
static unsigned char pool[256 * 1024] __attribute__((aligned(32)));
static unsigned pool_used;
int opus_engine_add(struct opl_self *self, const struct opus_engine *e) { (void)self; eng = e; return 0x100; }
static void *take(unsigned n) { void *p = pool + pool_used; pool_used += (n + 31) & ~31u; return p; }
void *opus_alloc(struct opl_self *s, unsigned n) { (void)s; return take(n); }
void *opus_alloc_audio(struct opl_self *s, unsigned n) { (void)s; return take(n); }
void *opus_fast(struct opl_self *s, unsigned n) { (void)s; return take(n); }
void erd_page_init(struct opl_self *self) { (void)self; }
void erd_page(void) {}
int opl_init(struct opl_self *self);
short erd_bench_env[128];

static void bench(const char *name, const short *k)
{
    static unsigned char voice[6][64] __attribute__((aligned(8)));
    short par[8] = { k[0], k[1], k[2], k[3], 0, 0, 0, 0 };
    static short out[128];
    static unsigned pitch[128];
    for (int i = 0; i < 128; i++) pitch[i] = 21426140u;     /* 440 Hz at 88.2 kHz */
    for (int v = 0; v < 6; v++) { eng->init(voice[v], v); eng->note_on(voice[v], par); }
    unsigned total = 0, worst = 0;
    for (int b = 0; b < 20; b++) {
        unsigned blk = 0;
        for (int v = 0; v < 6; v++) {
            unsigned c0 = cyc();
            eng->render(voice[v], out, par, pitch);
            blk += cyc() - c0;
        }
        total += blk;
        if (blk > worst) worst = blk;
    }
    puts_(name); puts_(": six voices avg "); putu(total / 20); puts_(", worst "); putu(worst);
    puts_(" cycles a block ("); putu(total / 120); puts_(" a voice)\n");
}

#include "bench_presets.h"

int main(void)
{
    for (int i = 0; i < 128; i++) erd_bench_env[i] = 20000;
    opl_init(0);
    static const short all[4] = { 32767, 32767, 16000, 32767 }, def[4] = { 10000, 26000, 12000, 16000 },
                       strings[4] = { 0, 26000, 26000, 0 }, choir[4] = { 32767, 0, 16000, 0 },
                       brass[4] = { 0, 0, 16000, 32767 }, none[4] = { 0, 0, 16000, 0 };
    bench("all sections", all);
    bench("first grace ", def);
    bench("strings only", strings);
    bench("choir only  ", choir);
    bench("brass only  ", brass);
    bench("nothing     ", none);
    /* the Opus presets' knobs (run.sh writes bench_presets.h from each preset folder's sound.json) */
    for (unsigned i = 0; i < sizeof bench_presets / sizeof bench_presets[0]; i++)
        bench(bench_presets[i].name, bench_presets[i].k);
    return 0;
}
