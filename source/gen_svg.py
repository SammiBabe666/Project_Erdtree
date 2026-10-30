#!/usr/bin/env python3
"""Generate erdtree.svg: a simplified Erdtree over the ruins of Leyndell, OP-1 line style (320 x 160, thin coloured
lines on black). After the in-game view: a tall trunk of fibres, a crown that fans out into fine branches under a wide
cloud of leaves, broken walls, arcades and towers at its foot.

Layers, back to front (erdscreen.c animates the crown by id and draws stars, light shafts and falling leaves on top):
  green  the crown: branches forking twice from the top of the trunk, a scalloped canopy edge, hanging fringe
         (strings: grows and sways about the top of the trunk)
  white  the trunk (fibres flaring into roots and into the crown), the ground, the ruins, a small dead tree
  blue   the arcades' openings and the towers' windows
Native SVG subset only: groups, lines, polylines, M/L/C paths, at most 1 decimal.
    python3 gen_svg.py > erdtree.svg
Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
"""
import math

W, H = 320, 160
CX = 160                       # trunk axis
GROUND = 146
TOP = 66                       # top of the trunk: the crown's pivot (erdscreen.c CROWN_Y)
CANOPY = (160, 58, 126, 40)    # crown ellipse: centre and radii (branch tips land on it; erdscreen.c uses it too)
BLUE, GREEN, WHITE = '#4D9EFF', '#00ED95', '#FFFFFF'


def f(v):
    s = f'{v:.1f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def pts(p):
    return ' '.join(f'{f(x)},{f(y)}' for x, y in p)


def poly(p, col, w=1.0):
    return f'<polyline points="{pts(p)}" fill="none" stroke="{col}" stroke-width="{w}"/>'


def path(d, col, w=1.0):
    return f'<path d="{d}" fill="none" stroke="{col}" stroke-width="{w}"/>'


def ell(a_deg, grow=0.0):
    ex, ey, rx, ry = CANOPY
    a = math.radians(a_deg)
    return ex + (rx + grow) * math.cos(a), ey + (ry + grow) * math.sin(a)


def crown():
    out = []
    # 9 primary branches, each forking twice: 36 tips spread along the upper canopy ellipse (194..346 degrees).
    # Fork points sit part of the way to the mean of their tips, lifted a little so the branches arch upward.
    n1, a0, a1 = 9, 194.0, 346.0
    tips = [ell(a0 + (a1 - a0) * (i + .5) / (n1 * 4)) for i in range(n1 * 4)]
    mean = lambda ps: (sum(p[0] for p in ps) / len(ps), sum(p[1] for p in ps) / len(ps))
    for k in range(n1):
        t4 = tips[4 * k:4 * k + 4]
        u = (k - (n1 - 1) / 2) / ((n1 - 1) / 2)                       # -1 .. 1 across the crown
        b = (CX + 10 * u, TOP - 2 + 3 * abs(u))
        m = mean(t4)
        p1 = (b[0] + (m[0] - b[0]) * .3, b[1] + (m[1] - b[1]) * .62 - 4)
        forks = []
        for half in (t4[:2], t4[2:]):
            mh = mean(half)
            forks.append((p1[0] + (mh[0] - p1[0]) * .45, p1[1] + (mh[1] - p1[1]) * .5 - 3))
        w = 1.25 if abs(u) < .75 else 1.0
        out.append(poly([b, p1, forks[0], t4[0]], GREEN, w))
        out.append(poly([forks[0], t4[1]], GREEN))
        out.append(poly([p1, forks[1], t4[2]], GREEN))
        out.append(poly([forks[1], t4[3]], GREEN))
    # scalloped canopy edge: small outward bulges along a slightly larger ellipse, with gaps between them
    for a in range(188, 350, 10):
        p0, p1 = ell(a, 7), ell(a + 8, 7)
        c0, c1 = ell(a + 1.5, 12), ell(a + 6.5, 12)
        out.append(path(f'M{f(p0[0])},{f(p0[1])} C{f(c0[0])},{f(c0[1])} {f(c1[0])},{f(c1[1])} {f(p1[0])},{f(p1[1])}', GREEN))
    # fringe hanging under the sides of the crown
    for s in (-1, 1):
        for a, ln in ((190, 14), (200, 18), (210, 11)):
            x0, y0 = ell(a if s < 0 else 540 - a, 2)
            out.append(path(f'M{f(x0)},{f(y0)} C{f(x0 + s * 2)},{f(y0 + ln * .4)} {f(x0 - s)},{f(y0 + ln * .7)} {f(x0 + s * 1.5)},{f(y0 + ln)}', GREEN))
    return '<g id="green">' + ''.join(out) + '</g>'


def trunk():
    out = []
    # fibres: a tall bundle, narrow in the middle, flaring into roots at the ground and into the crown at the top
    n = 6
    for j in range(n):
        u = -1 + 2 * j / (n - 1)
        p = []
        for y in range(GROUND, TOP - 1, -5):
            half = 10 + 9 * math.exp(-(GROUND - y) / 8) + 6 * math.exp(-(y - TOP) / 10)
            p.append((CX + half * u + 0.7 * math.sin(y / 9 + j * 1.7), y))
        p[0] = (CX + u * 28, GROUND)                                    # roots run out along the ground
        out.append(poly(p, WHITE, 1.25 if j in (0, n - 1) else 1.0))
    out.append(f'<line x1="18" y1="{GROUND}" x2="302" y2="{GROUND}" stroke="{WHITE}" stroke-width="1.5"/>')
    return out


def ruins():
    white, blue = [], []
    # far left: a broken keep with a ragged top
    white.append(poly([(22, GROUND), (22, 110), (26, 106), (30, 109), (34, 101), (39, 105), (44, 98), (49, 103), (54, 100),
                       (58, 105), (60, GROUND)], WHITE))
    blue += [path('M30,128 L30,121 C30,117 36,117 36,121 L36,128', BLUE), path('M46,128 L46,119 C46,115 52,115 52,119 L52,128', BLUE)]
    # left arcade, its top broken off toward the trunk
    white.append(poly([(64, GROUND), (64, 116), (110, 116), (114, 121), (118, 118), (122, 124), (122, GROUND)], WHITE))
    for x in range(68, 118, 12):
        blue.append(path(f'M{x},{GROUND} L{x},130 C{x},123 {x + 8},123 {x + 8},130 L{x + 8},{GROUND}', BLUE))
    # small spire left of the trunk
    white.append(poly([(126, GROUND), (126, 112), (131, 100), (136, 112), (136, GROUND)], WHITE))
    # tall broken tower right of the trunk
    white.append(poly([(194, GROUND), (194, 90), (197, 86), (200, 89), (203, 84), (206, 88), (206, GROUND)], WHITE))
    blue.append(path('M198,112 L198,104 C198,101 202,101 202,104 L202,112', BLUE))
    # right arcade, lower and more ruined
    white.append(poly([(212, GROUND), (212, 122), (240, 122), (244, 127), (249, 124), (252, 130), (252, GROUND)], WHITE))
    for x in range(216, 246, 12):
        blue.append(path(f'M{x},{GROUND} L{x},134 C{x},128 {x + 8},128 {x + 8},134 L{x + 8},{GROUND}', BLUE))
    # a lone broken column
    white.append(poly([(264, GROUND), (264, 114), (267, 110), (269, 113), (270, GROUND)], WHITE))
    # small dead tree
    white.append(poly([(286, GROUND), (286, 132), (282, 124)], WHITE))
    white.append(poly([(286, 132), (290, 126), (293, 121)], WHITE))
    white.append(poly([(290, 126), (288, 120)], WHITE))
    return white, blue


def main():
    w2, b2 = ruins()
    body = crown() + '<g id="white">' + ''.join(trunk() + w2) + '</g>' + '<g id="blue">' + ''.join(b2) + '</g>'
    print(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">{body}</svg>')


if __name__ == '__main__':
    main()
