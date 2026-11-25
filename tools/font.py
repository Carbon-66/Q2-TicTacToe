#!/usr/bin/env python3
"""Bakes the game's fonts into C: Poppins (SIL Open Font License 1.1), printable ASCII, a few sizes,
each glyph as 8-bit coverage. Usage: font.py OUT.c

Poppins comes from $POPPINS_DIR (Poppins-Bold.ttf and Poppins-Medium.ttf; Google Fonts,
github.com/itfoundry/Poppins)."""
import os, sys
from PIL import Image, ImageDraw, ImageFont

DIR = os.environ.get('POPPINS_DIR', '/usr/share/fonts/truetype/google-fonts')
FONTS = [  # C name, file, px
    ('font_small', 'Poppins-Medium.ttf', 15),
    ('font_body', 'Poppins-Medium.ttf', 19),
    ('font_bold', 'Poppins-Bold.ttf', 22),
    ('font_big', 'Poppins-Bold.ttf', 34),
]
CHARS = [chr(c) for c in range(32, 127)]

out = [f'/* made by tools/font.py from Poppins (SIL Open Font License 1.1): do not edit */\n#include "tt.h"\n']
for name, file, px in FONTS:
    f = ImageFont.truetype(os.path.join(DIR, file), px)
    asc, desc = f.getmetrics()
    h = asc + desc
    cap = f.getbbox('H', anchor='ls')  # baseline at 0: cap[1] is minus the cap height
    glyphs, x = [], 0
    for ch in CHARS:
        b = f.getbbox(ch, anchor='la') if ch != ' ' else (0, 0, 0, 0)
        w = max(0, b[2] - b[0])
        glyphs.append((ch, x, w, b[0], round(f.getlength(ch))))
        x += w
    atlas = Image.new('L', (max(1, x), h), 0)
    d = ImageDraw.Draw(atlas)
    for ch, gx, w, lsb, adv in glyphs:
        if w:
            d.text((gx - lsb, 0), ch, font=f, fill=255, anchor='la')
    data = atlas.tobytes()
    out.append(f'static const uint8_t {name}_px[{len(data)}] = {{' +
               ','.join(str(v) for v in data) + '};\n')
    out.append(f'const Font {name} = {{ {atlas.width}, {h}, {asc}, {-cap[1]}, {name}_px, {{\n' +
               ''.join(f'    {{ {gx}, {w}, {lsb}, {adv} }},\n' for ch, gx, w, lsb, adv in glyphs) + '} };\n')
open(sys.argv[1], 'w').write(''.join(out))
