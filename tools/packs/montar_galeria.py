#!/usr/bin/env python3
"""Builds the optional gallery pack from the user's images. The file name inside the zip is the caption the
app shows (a leading number only sets the order).  usage: montar_galeria.py carpeta_imagenes salida.zip"""
import os, sys, tempfile, zipfile
from PIL import Image

src, out = sys.argv[1:3]
ITEMS = [
    ('Europea Fron.jpg', 'Portada Europa'), ('Europea Back.jpg', 'Contraportada Europa'), ('Spine Europe.jpg', 'Lomo Europa'),
    ('5785e7e3-7063-48fb-bce7-aff6a7216ec2.png', 'Caja Europa'), ('Reconstructed Front.jpg', 'Portada reconstruida'),
    ('Classic Front.jpg', 'Portada Classic'), ('Classic Back.jpg', 'Contraportada Classic'), ('Spine Classic.jpg', 'Lomo Classic'),
    ('Classico front.png', 'Arte Classic'), ('Latam Front.jpg', 'Portada Latinoamérica'), ('Latam Back.jpg', 'Contraportada Latinoamérica'),
    ('Sims front.png', 'Portada SIMS'), ('Physical.png', 'Cartucho'), ('Logo.png', 'Logo'), ('Logo Oled.png', 'Logo OLED'),
    ('Wallpaper.jpg', 'Fondo de pantalla'),
]
MAX_SIDE = 2160
with zipfile.ZipFile(out, 'w', zipfile.ZIP_STORED) as z, tempfile.TemporaryDirectory() as t:
    for n, (f, cap) in enumerate(ITEMS, 1):
        im = Image.open(os.path.join(src, f))
        alpha = im.mode in ('RGBA', 'LA', 'P') and f.endswith('.png')
        if max(im.size) > MAX_SIDE:
            k = MAX_SIDE / max(im.size)
            im = im.resize((round(im.width * k), round(im.height * k)), Image.LANCZOS)
        name = '%02d %s.%s' % (n, cap, 'png' if alpha else 'jpg')
        p = os.path.join(t, 'x')
        if alpha: im.convert('RGBA').save(p, 'PNG', optimize=True)
        else: im.convert('RGB').save(p, 'JPEG', quality=88, optimize=True, progressive=False)
        z.write(p, name)
        print(name, im.size, os.path.getsize(p) // 1024, 'KB')
print(os.path.getsize(out) // 1024, 'KB')
