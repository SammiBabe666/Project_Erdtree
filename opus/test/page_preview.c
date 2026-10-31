/* PC preview of the port's T1 page (erd_page.c): the core's drawing calls are replaced by ones that print each line
 * and dot, and opus/test/page_preview.py draws them over the still art at the knob corners. The page reads the
 * Blackfin's cycle counter for its clock; here the preview steps the time itself.
 *   cc -DERD_HOST -Dasm=__asm__ -I<plugin-kit>/sdk -Iopus/sammi.erdtree opus/test/page_preview.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "opus_plugin.h"
static unsigned fake_cycles;
#define ERD_PAGE_HOST_CYCLES fake_cycles
#include "erd_page.c"
static short knobs[8];
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
    printf("L %d %d %d %d %d %06x\n", x0, y0, x1, y1, w, colour & 0xFFFFFF);
}
int main(int argc, char **argv)
{
    /* argv: choir strings space brass ms */
    for (int i = 0; i < 4 && i + 1 < argc; i++) knobs[i] = (short)atoi(argv[i + 1]);
    const unsigned ms = argc > 5 ? (unsigned)atoi(argv[5]) : 1000;
    erd_page_init(0);
    erd_page();
    fake_cycles += ms * 400000u;
    puts("FRAME");
    erd_page();
    return 0;
}
