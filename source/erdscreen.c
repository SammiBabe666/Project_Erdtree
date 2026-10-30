/* erdtree screen: the Erdtree over the ruins of Leyndell (erdtree.svg, made by gen_svg.py), OP-1 line style.
 *   blue  (choir)  : golden leaves fall from the crown: none at 0, a steady drift of 16 at full
 *   green (strings): the crown grows from 86 % to full size and sways, more with more strings
 *   white (space)  : the star field: more stars, and they twinkle (white / grey / gone) the further out it is
 *   red   (brass)  : shafts of light break through under the crown: more of them and longer with more brass,
 *                    drifting slowly and fading out in dashes toward the ground
 * Stars, shafts and leaves are raster lines drawn after the SVG (lines drawn before it are lost). Integer only.
 * Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and
 * NOTICE). */
#include "op1_plugin.h"

/* geometry: must match gen_svg.py */
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

static const uint8_t C_BLUE[4]  = { 0x4D, 0x9E, 0xFF, 0xFF };
static const uint8_t C_WHITE[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
static const uint8_t C_GREY[4]  = { 0x6F, 0x71, 0x73, 0xFF };
static const uint8_t C_RED[4]   = { 0xFF, 0x3A, 0x5D, 0xFF };

/* light shafts: top x (px), slope dx/dy (Q10: they fan out from a point high above the crown, clear of the trunk),
 * width (px), and the order they light up in as brass rises */
static const int16_t shaft_x[N_SHAFTS]     = { 70, 100, 128, 192, 222, 252 };
static const int16_t shaft_slope[N_SHAFTS] = { -743, -495, -264, 264, 495, 760 };
static const int8_t  shaft_w[N_SHAFTS]     = { 5, 3, 6, 4, 6, 3 };
static const int8_t  shaft_rank[N_SHAFTS]  = { 2, 4, 0, 1, 3, 5 };

static void *crown_node;
static uint32_t t_ms;                                   /* screen time */
static int16_t star_x[N_STARS], star_y[N_STARS];        /* px */
static uint16_t star_ph[N_STARS];                       /* twinkle phase, sine-table units */

static inline int32_t tsin(int32_t a) { return OP1_SINE_TABLE[a & 8191]; }
static inline int32_t tcos(int32_t a) { return tsin(a + 2048); }

/* a short line in Q10 from (x, y) + (dx, dy), all Q10 */
static void seg(void *r, int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t w, const uint8_t *c)
{
    op1_pt a = { x, y }, b = { x + dx, y + dy };
    op1_raster_line(r, &a, &b, w, c);
}

/* a dot: two short crossing lines (zero-length lines are not drawn) */
static void dot(void *r, int32_t x, int32_t y, int32_t half, const uint8_t *c)
{
    seg(r, x - half, y, 2 * half, 0, OP1_Q10(1), c);
    seg(r, x, y - half, 0, 2 * half, OP1_Q10(1), c);
}

static void erd_init(void *scr)
{
    crown_node = op1_find_node(scr, "green");
    /* fixed star field (LCG): the sky outside the crown (with a margin) and above the ruins */
    uint32_t s = 0x45524454u;                           /* "ERDT" */
    for (int i = 0; i < N_STARS; ) {
        s = s * 1664525u + 1013904223u; int32_t x = 6 + (int32_t)((s >> 8) % 308);
        s = s * 1664525u + 1013904223u; int32_t y = 4 + (int32_t)((s >> 8) % 94);
        int32_t dx = (x - CAN_X) * 100 / (CAN_RX + 16), dy = (y - CAN_Y) * 100 / (CAN_RY + 16);
        if (dx * dx + dy * dy < 10000) continue;
        if (x > 186 && x < 214 && y > 78) continue;     /* the tall tower */
        star_x[i] = (int16_t)x; star_y[i] = (int16_t)y;
        star_ph[i] = (uint16_t)(s >> 19);
        i++;
    }
    t_ms = 0;
}

static void erd_update(void *scr, int dt)
{
    int16_t k[4];
    op1_synth_knobs_live(OP1_SCREEN_SLOT & 7, k);
    int32_t choir = k[0], strings = k[1], space = k[2], brass = k[3];
    t_ms += (uint32_t)dt;
    int32_t t = (int32_t)(t_ms & 0x3FFFFFF);

    /* green: crown grows (0.86 .. 1.0) and sways up to +-2.5 degrees about the top of the trunk */
    if (crown_node) {
        int32_t cs = 880 + ((strings * 144) >> 15);
        int32_t ang = (tsin(t * 3) * ((strings * 57) >> 15)) >> 15;          /* 8192 = 360 deg: 57 = 2.5 deg */
        int32_t c = (tcos(ang) >> 5) * cs >> 10, s = (tsin(ang) >> 5) * cs >> 10;
        int32_t cx = OP1_Q10(CROWN_X), cy = OP1_Q10(CROWN_Y);
        int32_t m[9] = { c, s, 0, -s, c, 0, cx - ((cx * c - cy * s) >> 10), cy - ((cx * s + cy * c) >> 10), 1024 };
        op1_node_set_transform(crown_node, m);
    }

    op1_screen_draw(scr, dt);                           /* the SVG first: raster lines drawn before it are lost */
    void *r = op1_raster();

    /* white: stars. 14 at space 0, all 36 at full; twinkle depth follows space */
    int32_t n_st = 14 + ((space * (N_STARS - 14)) >> 15);
    int32_t tw = (space * 3) >> 2;                      /* 0..24575: how far below the top a star may fade */
    for (int i = 0; i < n_st; i++) {
        int32_t v = tsin(star_ph[i] + t * (2 + (i & 3)));                    /* -32767..32767 */
        int32_t lvl = 32767 - ((32767 - v) >> 1) * tw / 32767;               /* 1..32767, steady at space 0 */
        if (lvl < 12000) continue;                                           /* gone this moment */
        dot(r, OP1_Q10(star_x[i]), OP1_Q10(star_y[i]), (i % 5 == 0) ? OP1_Q10(1.5) : OP1_Q10(1),
            lvl < 22000 ? C_GREY : C_WHITE);
    }

    /* red: light shafts. Each is two parallel edges from under the crown toward the ground: solid for the first 60 %
     * of its length, then dashes. Brass lights up to 6 and stretches them from 14 px down to the tops of the ruins. Kept inside x 20..300. */
    int32_t n_sh = (brass * 7) >> 15; if (n_sh > N_SHAFTS) n_sh = N_SHAFTS;
    for (int i = 0; i < N_SHAFTS; i++) {
        if (shaft_rank[i] >= n_sh) continue;
        int32_t len = 14 + ((brass * 44) >> 15) + ((tsin(t + i * 1400) * 4) >> 15);   /* px down, breathing */
        if (SHAFT_Y + len > GROUND - 1) len = GROUND - 1 - SHAFT_Y;
        int32_t x0 = OP1_Q10(shaft_x[i]) + ((tsin((t >> 1) + i * 2100) * OP1_Q10(3)) >> 15);   /* slow drift */
        int32_t slope = shaft_slope[i], solid = len * 3 / 5;
        for (int y = 0; y < len; y += 8) {
            int32_t h = y + 8 > len ? len - y : (y >= solid ? 5 : 8);        /* past 60 %: 5 px dashes, 3 px gaps */
            if (h <= 0) break;
            int32_t xa = x0 + y * slope, yq = OP1_Q10(SHAFT_Y + y);
            int32_t xb = xa + h * slope;                                       /* lower end of this piece */
            if (xb < OP1_Q10(20) || xb + OP1_Q10(shaft_w[i]) > OP1_Q10(300)) break;
            for (int e = 0; e < 2; e++)
                seg(r, xa + e * OP1_Q10(shaft_w[i]), yq, h * slope, OP1_Q10(h), OP1_Q10(1), C_RED);
        }
    }

    /* blue: falling leaves, small diamonds drifting down from the crown to the ground */
    int32_t n_lv = (choir * N_LEAVES + 16383) >> 15;
    for (int i = 0; i < n_lv; i++) {
        int32_t x0 = 44 + (i * 113) % 232;                                   /* spread under the crown */
        int32_t speed = 3 + (i * 7) % 5;                                     /* px per second x 3 */
        int32_t span = GROUND - 2 - 64;
        int32_t y = 64 + (int32_t)(((t_ms / 100u) * (uint32_t)speed / 3u + (uint32_t)i * 37u) % (uint32_t)span);
        int32_t x = OP1_Q10(x0) + ((tsin(t * 2 + i * 1100) * OP1_Q10(5)) >> 15);
        int32_t yq = OP1_Q10(y), h = OP1_Q10(1.6);
        seg(r, x - h, yq, h, -h, OP1_Q10(1), C_BLUE);
        seg(r, x, yq - h, h, h, OP1_Q10(1), C_BLUE);
        seg(r, x + h, yq, -h, h, OP1_Q10(1), C_BLUE);
        seg(r, x, yq + h, -h, -h, OP1_Q10(1), C_BLUE);
    }
}

const op1_screen_t SCREEN_erdtree = { erd_init, erd_update };
