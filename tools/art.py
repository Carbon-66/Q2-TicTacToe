#!/usr/bin/env python3
"""Bakes the still parts of the game's two screens into C, in the 50s look of the design mockup:
the teal tile pattern, the menu's cream sign with its title and starbursts, and the game's board
card, grid and bottom strip. What changes (buttons, marks, scores, banners) the game draws itself.
Each screen is drawn at 4x and scaled down, as 375 x 320 RGB. Usage: art.py OUT.c

Fonts: Yellowtail (Apache 2.0) from $YELLOWTAIL_TTF, DM Serif Display (OFL) from $DMSERIF_TTF."""
import math, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

W, H, S = 375, 320, 4
FONTS = os.environ.get('FONTS_DIR', '/home/claude/fonts/gf')
YELLOWTAIL = os.environ.get('YELLOWTAIL_TTF', f'{FONTS}/apache/yellowtail/Yellowtail-Regular.ttf')
DMSERIF = os.environ.get('DMSERIF_TTF', f'{FONTS}/ofl/dmserifdisplay/DMSerifDisplay-Regular.ttf')

CREAM, MUSTARD, RED, TEAL, TEAL_DARK = (244, 235, 207), (242, 189, 44), (200, 40, 30), (26, 143, 128), (26, 122, 110)
BG_DARK, BG_LIGHT = (27, 122, 110), (60, 196, 172)

def hexc(c, a=255):
    return c + (a,)

def pattern():
    """dark teal with light circles on a 34 px grid, just overlapping: four-point stars between"""
    ys, xs = np.mgrid[0:H * S, 0:W * S].astype(np.float32)
    x, y = (xs + 0.5) / S, (ys + 0.5) / S
    dx, dy = (x + 17) % 34 - 17, (y + 17) % 34 - 17
    r = 0.71 * math.hypot(17, 17)
    inside = (np.hypot(dx, dy) < r)[..., None]
    img = np.where(inside, np.array(BG_LIGHT, np.uint8), np.array(BG_DARK, np.uint8)).astype(np.uint8)
    return Image.fromarray(img, 'RGB').convert('RGBA')

def css_shape_mask(left, top, w, h, radii):
    """a box with CSS elliptical corners, radii (rx, ry) for TL, TR, BR, BL in px; at 4x"""
    ys, xs = np.mgrid[0:H * S, 0:W * S].astype(np.float32)
    x, y = (xs + 0.5) / S, (ys + 0.5) / S
    ok = (x >= left) & (x < left + w) & (y >= top) & (y < top + h)
    (a, b), (c, d), (e, f), (g, k) = radii
    for cx, cy, rx, ry, region in (
        (left + a, top + b, a, b, (x < left + a) & (y < top + b)),
        (left + w - c, top + d, c, d, (x > left + w - c) & (y < top + d)),
        (left + w - e, top + h - f, e, f, (x > left + w - e) & (y > top + h - f)),
        (left + g, top + h - k, g, k, (x < left + g) & (y > top + h - k)),
    ):
        if rx > 0 and ry > 0:
            ok &= ~region | (((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1)
    return Image.fromarray((ok * 255).astype(np.uint8), 'L')

def paint(base, mask, colour, dy=0):
    layer = Image.new('RGBA', base.size, colour)
    if dy:
        m = Image.new('L', base.size, 0)
        m.paste(mask, (0, dy * S))
        mask = m
    if len(colour) == 4 and colour[3] < 255:
        mask = mask.point(lambda v: v * colour[3] // 255)
    base.paste(Image.new('RGBA', base.size, colour[:3] + (255,)), (0, 0), mask)

def rounded(left, top, w, h, r):
    return css_shape_mask(left, top, w, h, [(r, r)] * 4)

def star(d, x, y, size, width, colour):
    """the atomic starburst of the mockup: four strokes through a 34-unit box at x, y"""
    k = size / 34 * S
    cx, cy = (x + size / 2) * S, (y + size / 2) * S
    for x0, y0, x1, y1 in ((0, -15, 0, 15), (-15, 0, 15, 0), (-9, -9, 9, 9), (-9, 9, 9, -9)):
        p0, p1 = (cx + x0 * k, cy + y0 * k), (cx + x1 * k, cy + y1 * k)
        wd = width * k
        d.line([p0, p1], fill=colour, width=round(wd))
        for p in (p0, p1):
            d.ellipse([p[0] - wd / 2, p[1] - wd / 2, p[0] + wd / 2, p[1] + wd / 2], fill=colour)

def css_baseline(font, top, line_height):
    asc, desc = font.getmetrics()
    return top + (line_height - (asc + desc)) / 2 + asc

def menu():
    im = pattern()
    # the sign: a boomerang, cream with a mustard edge, and its shadow
    L, T, w, h = 22, 14, 331, 292
    radii = [(0.58 * w, 0.34 * h), (0.42 * w, 0.30 * h), (0.46 * w, 0.70 * h), (0.54 * w, 0.66 * h)]
    outer = css_shape_mask(L, T, w, h, radii)
    paint(im, outer, (0, 0, 0, 46), dy=4)
    paint(im, outer, MUSTARD)
    inner = css_shape_mask(L + 5, T + 5, w - 10, h - 10, [(max(0, rx - 5), max(0, ry - 5)) for rx, ry in radii])
    paint(im, inner, CREAM)
    d = ImageDraw.Draw(im)
    star(d, 56, 30, 34, 3, MUSTARD)
    star(d, 300, 92, 26, 3.5, MUSTARD)
    star(d, 286, 38, 18, 4, RED)
    # the title: "Toe" in teal serif, "Tic-Tac-" in red script over it, tilted, with a cream edge
    toe = ImageFont.truetype(DMSERIF, 64 * S)
    x, base = 208.5 * S, css_baseline(toe, 39 * S, 64 * S)
    for ch in 'Toe':
        d.text((x, base), ch, font=toe, fill=TEAL, anchor='ls')
        x += toe.getlength(ch) + 1 * S
    tic = ImageFont.truetype(YELLOWTAIL, 50 * S)
    layer = Image.new('RGBA', im.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    tx, tb = 60.5 * S, css_baseline(tic, 43 * S, 56 * S)
    for ox, oy in ((2, 2), (-2, -2), (2, -2), (-2, 2)):
        ld.text((tx + ox * S, tb + oy * S), 'Tic-Tac-', font=tic, fill=CREAM, anchor='ls')
    ld.text((tx, tb), 'Tic-Tac-', font=tic, fill=RED, anchor='ls')
    centre = ((60.5 + 104) * S, (43 + 39.5) * S)
    layer = layer.rotate(5, resample=Image.BICUBIC, center=centre)  # CSS rotate(-5deg)
    im.alpha_composite(layer)
    return im

def play():
    im = pattern()
    # the board's card: cream, a mustard edge, its shadow
    card = rounded(75, 48, 225, 225, 22)
    paint(im, card, (0, 0, 0, 51), dy=4)
    paint(im, card, MUSTARD)
    paint(im, rounded(79, 52, 217, 217, 18), CREAM)
    # the grid: bars in the 209 px square at 83, 56
    for a in (67, 137):
        paint(im, rounded(83 + a, 56 + 6, 5, 197, 2.5), TEAL_DARK)
        paint(im, rounded(83 + 6, 56 + a, 197, 5, 2.5), TEAL_DARK)
    # the mustard strip along the bottom
    ImageDraw.Draw(im).rectangle([0, 284 * S, W * S, H * S], fill=MUSTARD)
    return im

def emit(name, im):
    small = im.convert('RGB').resize((W, H), Image.LANCZOS)
    data = small.tobytes()
    return f'const uint8_t {name}[{len(data)}] = {{' + ','.join(map(str, data)) + '};\n', small

if __name__ == '__main__':
    out = ['/* made by tools/art.py: do not edit */\n#include <stdint.h>\n']
    for name, fn in (('art_menu', menu), ('art_play', play)):
        src, small = emit(name, fn())
        out.append(src)
        if os.environ.get('ART_PNG'):
            small.save(os.path.join(os.environ['ART_PNG'], name + '.png'))
    open(sys.argv[1], 'w').write(''.join(out))
