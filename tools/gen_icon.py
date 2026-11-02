#!/usr/bin/env python3
"""Launcher icon, from the logo in the launcher art (assets/launcher/fondo.jpg).

Android 8+ shrinks old square icons and puts them on a white plate, so the icon is an adaptive one:
a dark background plus the logo as foreground, as big as the mask allows. The PNGs in mipmap-*dpi are
only for older launchers, and the banner for Android TV-like launchers.
usage: gen_icon.py [res folder] [preview.png]
"""
import os, sys
from PIL import Image, ImageChops, ImageDraw, ImageFilter
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
RES = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'android/app/src/main/res')
BG = (20, 12, 16)
BOX = (664, 16, 1258, 276)        # the logo with its flourishes, in fondo.jpg
PANEL = (48, 36, 548, 226)        # frame and black panel, inside BOX: always opaque

src = Image.open(os.path.join(ROOT, 'android/app/src/main/assets/launcher/fondo.jpg')).convert('RGB').crop(BOX)
# outside the panel only the gold of the flourishes stays: the painted sky around it goes away
r, g, b = src.split()
warm = ImageChops.subtract(r, b).point(lambda v: 255 if v > 45 else 0)
bright = r.point(lambda v: 255 if v > 95 else 0)
mask = ImageChops.multiply(warm, bright).filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(0.8))
ImageDraw.Draw(mask).rounded_rectangle(PANEL, radius=6, fill=255)
logo = src.convert('RGBA'); logo.putalpha(mask)

def on_canvas(w, h, logo_w, fill):
    im = Image.new('RGBA', (w, h), fill)
    lh = logo_w * logo.height // logo.width
    im.alpha_composite(logo.resize((logo_w, lh), Image.LANCZOS), ((w - logo_w) // 2, (h - lh) // 2))
    return im

DENS = {'mdpi': 1, 'hdpi': 1.5, 'xhdpi': 2, 'xxhdpi': 3, 'xxxhdpi': 4}
for k, f in DENS.items():
    p = os.path.join(RES, 'mipmap-' + k); os.makedirs(p, exist_ok=True)
    # adaptive foreground: 108 dp, of which the mask shows the central 72 dp
    on_canvas(int(108 * f), int(108 * f), int(68 * f), (0, 0, 0, 0)).save(os.path.join(p, 'ic_launcher_foreground.png'))
    on_canvas(int(48 * f), int(48 * f), int(47 * f), BG + (255,)).convert('RGB').save(os.path.join(p, 'ic_launcher.png'))
p = os.path.join(RES, 'drawable-xhdpi'); os.makedirs(p, exist_ok=True)
on_canvas(320, 180, 290, BG + (255,)).convert('RGB').save(os.path.join(p, 'banner.png'))
p = os.path.join(RES, 'mipmap-anydpi-v26'); os.makedirs(p, exist_ok=True)
with open(os.path.join(p, 'ic_launcher.xml'), 'w') as f:
    f.write('<?xml version="1.0" encoding="utf-8"?>\n<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
            '    <background android:drawable="@color/ic_launcher_background" />\n'
            '    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n</adaptive-icon>\n')
with open(os.path.join(RES, 'values', 'ic_launcher_background.xml'), 'w') as f:
    f.write('<?xml version="1.0" encoding="utf-8"?>\n<resources>\n    <color name="ic_launcher_background">#%02X%02X%02X</color>\n</resources>\n' % BG)
if len(sys.argv) > 2:   # what the launchers show: rounded square and circle
    S = 432
    fg = on_canvas(S, S, 272, BG + (255,)).crop((72, 72, 360, 360))
    out = Image.new('RGB', (640, 300), (235, 235, 235))
    for i, kind in enumerate(('sq', 'ci')):
        m = Image.new('L', fg.size, 0); d = ImageDraw.Draw(m)
        if kind == 'sq': d.rounded_rectangle((0, 0, 287, 287), radius=64, fill=255)
        else: d.ellipse((0, 0, 287, 287), fill=255)
        out.paste(fg.convert('RGB'), (20 + i * 310, 6), m)
    out.save(sys.argv[2])
