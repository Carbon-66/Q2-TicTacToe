#!/usr/bin/env python3
"""The card's icons, drawn: a little game won by X, on the game's navy, in a rounded square.
card/icon.png (52 px, Library's Games row) and card/icon-40.png (40 px). Usage: tools/icon.py"""
import pathlib
from PIL import Image, ImageDraw

X, O, GRID = (255, 107, 129), (79, 214, 200), (88, 100, 138)

def icon(size):
    S = size * 8  # drawn large, then scaled down smooth
    bg = Image.new('RGB', (S, S))
    px = bg.load()
    for y in range(S):
        t = y / S
        top, bot = (36, 42, 62), (16, 19, 28)
        c = tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3))
        for x in range(S):
            px[x, y] = c
    d = ImageDraw.Draw(bg)
    m, cs = S * 0.14, S * 0.24
    w = int(S * 0.035)
    for k in (1, 2):
        d.line([m + k * cs, m + S * 0.02, m + k * cs, m + 3 * cs - S * 0.02], fill=GRID, width=w)
        d.line([m + S * 0.02, m + k * cs, m + 3 * cs - S * 0.02, m + k * cs], fill=GRID, width=w)

    def xmark(i):
        cx, cy, h = m + (i % 3 + 0.5) * cs, m + (i // 3 + 0.5) * cs, cs * 0.27
        for a, b in (((cx - h, cy - h), (cx + h, cy + h)), ((cx + h, cy - h), (cx - h, cy + h))):
            d.line([a, b], fill=X, width=int(S * 0.055))
            for p in (a, b):
                r = S * 0.0275
                d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=X)

    def omark(i):
        cx, cy, r = m + (i % 3 + 0.5) * cs, m + (i // 3 + 0.5) * cs, cs * 0.3
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=O, width=int(S * 0.055))

    for i in (0, 4, 8):
        xmark(i)
    for i in (2, 3):
        omark(i)
    a, b = (m + 0.3 * cs, m + 0.3 * cs), (m + 2.7 * cs, m + 2.7 * cs)
    d.line([a, b], fill=(255, 255, 255), width=int(S * 0.03))
    mask = Image.new('L', (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=S * 0.22, fill=255)
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    im.paste(bg, (0, 0), mask)
    return im.resize((size, size), Image.LANCZOS)

card = pathlib.Path(__file__).resolve().parent.parent / 'card'
icon(52).save(card / 'icon.png')
icon(40).save(card / 'icon-40.png')
