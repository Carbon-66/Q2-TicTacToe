#!/usr/bin/env python3
"""Bakes the game's fonts into C: printable ASCII plus the middle dot (as character 127), each glyph
as 8-bit coverage. Usage: font.py OUT.c

  Yellowtail          the red and teal script   Apache License 2.0   $YELLOWTAIL_TTF
  DM Serif Display    the scores                SIL OFL 1.1          $DMSERIF_TTF
  Josefin Sans (700)  the small capitals        SIL OFL 1.1          $JOSEFIN_TTF (the variable font)
All three are from Google Fonts (github.com/google/fonts)."""
import os, sys
from PIL import Image, ImageDraw, ImageFont

FONTS = os.environ.get('FONTS_DIR', '/home/claude/fonts/gf')
YELLOWTAIL = os.environ.get('YELLOWTAIL_TTF', f'{FONTS}/apache/yellowtail/Yellowtail-Regular.ttf')
DMSERIF = os.environ.get('DMSERIF_TTF', f'{FONTS}/ofl/dmserifdisplay/DMSerifDisplay-Regular.ttf')
JOSEFIN = os.environ.get('JOSEFIN_TTF', f'{FONTS}/ofl/josefinsans/JosefinSans[wght].ttf')
SPECS = [  # C name, file, px, weight (variable fonts)
    ('font_name_sm', YELLOWTAIL, 17, None),
    ('font_name', YELLOWTAIL, 24, None),
    ('font_script', YELLOWTAIL, 26, None),
    ('font_script_big', YELLOWTAIL, 31, None),
    ('font_score', DMSERIF, 40, None),
    ('font_caps', JOSEFIN, 13, 700),
    ('font_caps_sm', JOSEFIN, 11, 700),
    ('font_caps_xs', JOSEFIN, 10, 700),
]
CHARS = [chr(c) for c in range(32, 127)] + ['·']

out = ['/* made by tools/font.py from Yellowtail (Apache 2.0), DM Serif Display and Josefin Sans '
       '(SIL OFL 1.1): do not edit */\n#include "tt.h"\n']
for name, file, px, weight in SPECS:
    f = ImageFont.truetype(file, px)
    if weight:
        f.set_variation_by_axes([weight])
    asc, desc = f.getmetrics()
    h = asc + desc
    cap = f.getbbox('H', anchor='ls')
    glyphs, x = [], 0
    for ch in CHARS:
        b = f.getbbox(ch, anchor='la') if ch != ' ' else (0, 0, 0, 0)
        w = max(0, b[2] - b[0])
        glyphs.append((x, w, b[0], round(f.getlength(ch))))
        x += w
    atlas = Image.new('L', (max(1, x), h), 0)
    d = ImageDraw.Draw(atlas)
    for ch, (gx, w, lsb, adv) in zip(CHARS, glyphs):
        if w:
            d.text((gx - lsb, 0), ch, font=f, fill=255, anchor='la')
    data = atlas.tobytes()
    out.append(f'static const uint8_t {name}_px[{len(data)}] = {{' + ','.join(map(str, data)) + '};\n')
    out.append(f'const Font {name} = {{ {atlas.width}, {h}, {asc}, {-cap[1]}, {name}_px, {{\n' +
               ''.join(f'    {{ {gx}, {w}, {lsb}, {adv} }},\n' for gx, w, lsb, adv in glyphs) + '} };\n')
open(sys.argv[1], 'w').write(''.join(out))
