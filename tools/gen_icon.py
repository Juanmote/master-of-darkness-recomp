#!/usr/bin/env python3
"""Draws the (original) launcher icon: full moon, bat and 'MoD' lettering."""
import os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter
res = sys.argv[1] if len(sys.argv) > 1 else 'android/app/src/main/res'
S = 512
im = Image.new('RGBA', (S, S))
d = ImageDraw.Draw(im)
# rounded dark background with vertical gradient
bg = Image.new('RGBA', (S, S))
for y in range(S):
    t = y / S
    ImageDraw.Draw(bg).line([(0, y), (S, y)], fill=(int(40 + 30 * t), int(10 + 6 * t), int(40 - 10 * t), 255))
mask = Image.new('L', (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=96, fill=255)
im.paste(bg, (0, 0), mask)
# moon glow + moon
glow = Image.new('RGBA', (S, S))
ImageDraw.Draw(glow).ellipse([116, 46, 396, 326], fill=(255, 230, 160, 120))
glow = glow.filter(ImageFilter.GaussianBlur(28))
im.alpha_composite(glow)
d = ImageDraw.Draw(im)
d.ellipse([136, 66, 376, 306], fill=(250, 232, 180, 255))
for (x, y, r) in [(200, 140, 26), (300, 210, 18), (240, 250, 12), (320, 120, 10)]:
    d.ellipse([x - r, y - r, x + r, y + r], fill=(226, 204, 150, 255))
# bat silhouette
bat = [(256, 196), (236, 170), (210, 182), (184, 160), (150, 168), (176, 186), (160, 206), (200, 204),
       (228, 222), (256, 212), (284, 222), (312, 204), (352, 206), (336, 186), (362, 168), (328, 160),
       (302, 182), (276, 170)]
d.polygon(bat, fill=(20, 6, 16, 255))
d.polygon([(246, 176), (250, 160), (256, 172), (262, 160), (266, 176)], fill=(20, 6, 16, 255))
# lettering
font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf', 150)
txt = 'MoD'
w = d.textlength(txt, font=font)
x, y = (S - w) / 2, 318
for dx, dy in [(-4, 0), (4, 0), (0, -4), (0, 6), (4, 6), (-4, 6)]:
    d.text((x + dx, y + dy), txt, font=font, fill=(30, 4, 8, 255))
d.text((x, y), txt, font=font, fill=(214, 40, 34, 255))
sizes = {'mdpi': 48, 'hdpi': 72, 'xhdpi': 96, 'xxhdpi': 144, 'xxxhdpi': 192}
for k, v in sizes.items():
    p = os.path.join(res, 'mipmap-' + k)
    os.makedirs(p, exist_ok=True)
    im.resize((v, v), Image.LANCZOS).save(os.path.join(p, 'ic_launcher.png'))
im.save('icon_preview.png')
