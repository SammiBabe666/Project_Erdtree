/* erdtree's T1 page: the Erdtree over the ruins of Leyndell in the OP-1 line style, v0.5's screen (source/erdscreen.c)
 * on the core's page service:
 *   blue  (choir)  : golden leaves fall from the crown: none at 0, a steady drift of 16 at full
 *   green (strings): the crown grows from 86 % to full size and sways, more with more strings
 *   white (space)  : the star field: more stars, and they twinkle (white / grey / gone) the further out it is
 *   orange (brass) : shafts of light break through under the crown: more of them and longer with more brass,
 *                    drifting slowly and fading out in dashes toward the ground
 * The still parts (the trunk, the ground, the ruins: erd_art.h, made by opus/gen_opus_art.py) are drawn once by the
 * OS's renderer and their pixels kept (opus_page_keep); every frame puts them back and draws the crown, the stars, the
 * shafts and the leaves over them with the core's smoothed lines. The crown, an SVG group on op1r that erdscreen.c
 * scaled and turned, is the same drawing kept as polylines and turned here. Its colours follow the encoders': the
 * crown the green one's, the leaves and the arcades' openings the blue one's, the shafts the orange one's; the trunk,
 * the ruins and the stars stay white as on v0.5. Integer only.
 * Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and
 * NOTICE). */
#include "opus_plugin.h"
#include "erd_page.h"
#include "erd_art.h"

/* geometry, in the drawing's units: must match source/gen_svg.py */
#define CROWN_X  160          /* top of the trunk: the crown's pivot */
#define CROWN_Y  66
#define CAN_X    160          /* crown ellipse (CANOPY) */
#define CAN_Y    58
#define CAN_RX   126
#define CAN_RY   40
#define GROUND   146
#define N_STARS  36
#define N_LEAVES 16
#define N_SHAFTS 6
#define SHAFT_Y  66           /* shafts start under the branches */

#define U(v)        ((int)((v) * 256))         /* units x 256 (constants only) */
#define PX          256                        /* a line one panel pixel wide */
#define LAYER_WORDS (20 * 1024)                /* the kept still art */

#define WHITE       0xFFFFFFu
#define GREY        0x6F7173u                  /* v0.5's dim star */

/* light shafts: top x (units), slope dx/dy (Q10: they fan out from a point high above the crown, clear of the
 * trunk), width (units), and the order they light up in as brass rises */
static const short shaft_x[N_SHAFTS]           = { 70, 100, 128, 192, 222, 252 };
static const short shaft_slope[N_SHAFTS]       = { -743, -495, -264, 264, 495, 760 };
static const signed char shaft_w[N_SHAFTS]     = { 5, 3, 6, 4, 6, 3 };
static const signed char shaft_rank[N_SHAFTS]  = { 2, 4, 0, 1, 3, 5 };

static int art = -1;
static unsigned char *white_slots[32], *blue_slots[32];
static int white_n = -1, blue_n = -1;
static unsigned short *layer;
static int layer_used = -1, made_rows;
static unsigned made_colours, made_blue;

static unsigned last_cycles, t_ms;                      /* screen time */
static short star_x[N_STARS], star_y[N_STARS];          /* units */
static unsigned short star_ph[N_STARS];                 /* twinkle phase, 1/8192 of a turn */

#ifdef ERD_PAGE_HOST_CYCLES               /* the PC preview (opus/test) steps the time itself */
static inline unsigned cycles(void) { return ERD_PAGE_HOST_CYCLES; }
#else
static inline unsigned cycles(void)
{
    unsigned c;
    __asm__ volatile ("%0 = CYCLES;" : "=d"(c));
    return c;
}
#endif

/* sine and cosine of a (1/8192 of a turn, any), x 32767, as v0.5 read them from the firmware's table */
static int tsin(int a)
{
    int t = (a & 8191) >> 1, s, c;
    opus_sincos(t > 2048 ? t - 4096 : t, &s, &c);
    s <<= 3;
    return s > 32767 ? 32767 : s < -32767 ? -32767 : s;
}
static int tcos(int a) { return tsin(a + 2048); }

/* a line from (x, y) by (dx, dy), units x 256; w x 256 in panel pixels */
static void seg(int x, int y, int dx, int dy, int w, unsigned rgb)
{
    opus_page_stroke_line(x, y, x + dx, y + dy, w, OPUS_RGB(rgb));
}

void erd_page_init(struct opl_self *self)
{
    layer = (unsigned short *)opus_alloc(self, 2 * LAYER_WORDS);
    art = opus_art_text(self, "erdtree.svg", erd_art, sizeof erd_art - 1);
    /* fixed star field (LCG): the sky outside the crown (with a margin) and above the ruins */
    unsigned s = 0x45524454u;                           /* "ERDT" */
    for (int i = 0; i < N_STARS; ) {
        s = s * 1664525u + 1013904223u; int x = 6 + (int)((s >> 8) % 308);
        s = s * 1664525u + 1013904223u; int y = 4 + (int)((s >> 8) % 94);
        int dx = (x - CAN_X) * 100 / (CAN_RX + 16), dy = (y - CAN_Y) * 100 / (CAN_RY + 16);
        if (dx * dx + dy * dy < 10000) continue;
        if (x > 186 && x < 214 && y > 78) continue;     /* the tall tower */
        star_x[i] = (short)x; star_y[i] = (short)y;
        star_ph[i] = (unsigned short)(s >> 19);
        i++;
    }
}

/* The still art in the user's colours, drawn and kept; put back when nothing changed. */
static void still(void)
{
    int rows;
    if (art < 0 || !layer || !opus_page_pixels(&rows) || !opus_art_ready(art))
        return;
    if (white_n < 0)
        white_n = opus_art_colour_slots(art, ART_WHITE_RGB, white_slots, 32);
    if (blue_n < 0)
        blue_n = opus_art_colour_slots(art, ART_BLUE_RGB, blue_slots, 32);
    const unsigned colours = opus_art_colours(), blue = opus_accent_now(0);
    if (layer_used < 0 || made_rows != rows || made_colours != colours || made_blue != blue) {
        opus_art_set_colour(white_slots, white_n, WHITE);
        opus_art_set_colour(blue_slots, blue_n, blue);
        opus_art_draw(art);
        layer_used = opus_page_keep(layer, LAYER_WORDS, 0, rows - 1, 0, rows / 2 - 1);
        made_rows = rows;
        made_colours = colours;
        made_blue = blue;
    } else if (layer_used > 0)
        opus_page_put(layer, layer_used);
}

/* green: the crown grows (0.86 .. 1.0) and sways up to +-2.5 degrees about the top of the trunk */
static void crown(int strings, int t, unsigned rgb)
{
    const int cs = 880 + ((strings * 144) >> 15);                       /* Q10 */
    const int ang = (tsin(t * 3) * ((strings * 57) >> 15)) >> 15;       /* 8192 = 360 deg: 57 = 2.5 deg */
    const int c = (tcos(ang) >> 5) * cs >> 10, s = (tsin(ang) >> 5) * cs >> 10;   /* Q10 */
    const int cx = CROWN_X * 64, cy = CROWN_Y * 64;
    int p = 0;
    for (int l = 0; l < CROWN_LINES; ++l) {
        int x0 = 0, y0 = 0;
        for (int j = 0; j < crown_n[l]; ++j, ++p) {
            const int dx = crown_pt[p][0] - cx, dy = crown_pt[p][1] - cy;
            const int x = (cx << 2) + ((c * dx - s * dy) >> 8), y = (cy << 2) + ((s * dx + c * dy) >> 8);   /* x 256 */
            if (j)
                opus_page_stroke_line(x0, y0, x, y, crown_w[l] * (PX / 4), OPUS_RGB(rgb));
            x0 = x;
            y0 = y;
        }
    }
}

void erd_page(void)
{
    short k[8];
    opus_engine_live(k);
    const int choir = k[0] < 0 ? 0 : k[0], strings = k[1] < 0 ? 0 : k[1], space = k[2] < 0 ? 0 : k[2],
              brass = k[3] < 0 ? 0 : k[3];
    const unsigned c = cycles();                        /* 400 MHz */
    unsigned dt = last_cycles ? (c - last_cycles) / 400000u : 0;
    last_cycles = c;
    t_ms += dt > 250 ? 250 : dt;
    const int t = (int)(t_ms & 0x3FFFFFF);

    still();
    crown(strings, t, opus_accent_now(1));

    /* white: stars. 14 at space 0, all 36 at full; twinkle depth follows space */
    const int n_st = 14 + ((space * (N_STARS - 14)) >> 15);
    const int tw = (space * 3) >> 2;                    /* 0..24575: how far below the top a star may fade */
    for (int i = 0; i < n_st; i++) {
        const int v = tsin(star_ph[i] + t * (2 + (i & 3)));                 /* -32767..32767 */
        const int lvl = 32767 - ((32767 - v) >> 1) * tw / 32767;            /* 1..32767, steady at space 0 */
        if (lvl < 12000) continue;                                          /* gone this moment */
        const int x = U(star_x[i]), y = U(star_y[i]);
        opus_page_stroke_line(x, y, x, y, (i % 5 == 0) ? 3 * PX : 2 * PX, OPUS_RGB(lvl < 22000 ? GREY : WHITE));
    }

    /* orange: light shafts. Each is two parallel edges from under the crown toward the ground: solid for the first
     * 60 % of its length, then dashes. Brass lights up to 6 and stretches them from 14 units down to the tops of the
     * ruins. Kept inside x 20..300. */
    const unsigned orange = opus_accent_now(3);
    int n_sh = (brass * 7) >> 15; if (n_sh > N_SHAFTS) n_sh = N_SHAFTS;
    for (int i = 0; i < N_SHAFTS; i++) {
        if (shaft_rank[i] >= n_sh) continue;
        int len = 14 + ((brass * 44) >> 15) + ((tsin(t + i * 1400) * 4) >> 15);   /* units down, breathing */
        if (SHAFT_Y + len > GROUND - 1) len = GROUND - 1 - SHAFT_Y;
        const int x0 = U(shaft_x[i]) + ((tsin((t >> 1) + i * 2100) * U(3)) >> 15);   /* slow drift */
        const int slope = shaft_slope[i] >> 2, solid = len * 3 / 5;            /* x 256 a unit down */
        for (int y = 0; y < len; y += 8) {
            const int h = y + 8 > len ? len - y : (y >= solid ? 5 : 8);        /* past 60 %: 5-unit dashes */
            if (h <= 0) break;
            const int xa = x0 + y * slope, yq = U(SHAFT_Y + y);
            const int xb = xa + h * slope;                                     /* lower end of this piece */
            if (xb < U(20) || xb + U(shaft_w[i]) > U(300)) break;
            for (int e = 0; e < 2; e++)
                seg(xa + e * U(shaft_w[i]), yq, h * slope, U(h), PX, orange);
        }
    }

    /* blue: falling leaves, small diamonds drifting down from the crown to the ground */
    const unsigned blue = opus_accent_now(0);
    const int n_lv = (choir * N_LEAVES + 16383) >> 15;
    for (int i = 0; i < n_lv; i++) {
        const int x0 = 44 + (i * 113) % 232;                                  /* spread under the crown */
        const int speed = 3 + (i * 7) % 5;                                    /* units per second x 3 */
        const int span = GROUND - 2 - 64;
        const int y = 64 + (int)(((t_ms / 100u) * (unsigned)speed / 3u + (unsigned)i * 37u) % (unsigned)span);
        const int x = U(x0) + ((tsin(t * 2 + i * 1100) * U(5)) >> 15);
        const int yq = U(y), h = U(1.6);
        seg(x - h, yq, h, -h, PX, blue);
        seg(x, yq - h, h, h, PX, blue);
        seg(x + h, yq, -h, h, PX, blue);
        seg(x, yq + h, -h, -h, PX, blue);
    }
}
