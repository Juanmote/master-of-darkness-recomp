#!/usr/bin/env python3
"""Turns the photocopier scans of the manual (A4 sheets, one spread each, rotated) into single pages.
usage: montar_manual.py carpeta_escaneos carpeta_salida
The first sheet holds only the front cover; the others are spreads of two pages, numbered 2-3, 4-5..."""
import glob, os, sys
import cv2
import numpy as np

src, out = sys.argv[1:3]
os.makedirs(out, exist_ok=True)
PAGE_W = 1600

def clean(img):
    """Flat white paper: divides by the lit background (removes fold shadow and grey), then levels."""
    small = cv2.resize(img, None, fx=0.25, fy=0.25, interpolation=cv2.INTER_AREA)
    bg = cv2.morphologyEx(small, cv2.MORPH_CLOSE, np.ones((25, 25), np.uint8))
    bg = cv2.GaussianBlur(bg, (0, 0), 12)
    bg = cv2.resize(bg, (img.shape[1], img.shape[0]), interpolation=cv2.INTER_LINEAR).astype(np.float32)
    f = img.astype(np.float32) / np.maximum(bg, 60) * 255
    f = (f - 18) * 255 / (238 - 18)
    return np.clip(f, 0, 255).astype(np.uint8)

def deskew(img):
    grey = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    small = cv2.resize(grey, None, fx=0.25, fy=0.25, interpolation=cv2.INTER_AREA)
    ink = (small < 170).astype(np.float32)
    best, ang = -1, 0.0
    h, w = small.shape
    for a in np.arange(-2.0, 2.01, 0.1):
        m = cv2.getRotationMatrix2D((w / 2, h / 2), a, 1)
        s = cv2.warpAffine(ink, m, (w, h)).sum(axis=1)
        v = float(((s[1:] - s[:-1]) ** 2).sum())
        if v > best: best, ang = v, a
    h, w = img.shape[:2]
    m = cv2.getRotationMatrix2D((w / 2, h / 2), ang, 1)
    return cv2.warpAffine(img, m, (w, h), flags=cv2.INTER_CUBIC, borderValue=(255, 255, 255)), ang

def save(img, name):
    h, w = img.shape[:2]
    img = cv2.resize(img, (PAGE_W, round(h * PAGE_W / w)), interpolation=cv2.INTER_AREA)
    cv2.imwrite(os.path.join(out, name), img, [cv2.IMWRITE_JPEG_QUALITY, 82, cv2.IMWRITE_JPEG_OPTIMIZE, 1])
    print(name, img.shape[1], img.shape[0], os.path.getsize(os.path.join(out, name)) // 1024, 'KB')

files = sorted(glob.glob(os.path.join(src, '*.jpg')))
# spread size measured on the sheets: 3440 x 1215 at 300 dpi; one page is half of it
SPREAD_H, PAGE_LEN = 1216, 1720
page = 2
for n, f in enumerate(files):
    img = cv2.rotate(cv2.imread(f), cv2.ROTATE_90_CLOCKWISE)   # the sheets lie on their side
    if n == 0:
        save(img[22:SPREAD_H + 30, 1756:1756 + PAGE_LEN], '00 Portada.jpg')   # the right half of the sheet
        continue
    top = img[:SPREAD_H + 110]
    top, ang = deskew(top)
    # the fold is the darkest column near the middle
    grey = cv2.cvtColor(top, cv2.COLOR_BGR2GRAY)
    prof = grey[120:SPREAD_H - 120].mean(axis=0)
    mid = int(np.argmin(prof[1700:1810])) + 1700
    cl = clean(top)
    for k, (a, b) in enumerate(((max(0, mid - PAGE_LEN), mid - 4), (mid + 4, mid + PAGE_LEN))):
        save(cl[12:SPREAD_H + 12, a:b], '%02d Pagina %d.jpg' % (page + k, page + k))
    page += 2
    print(os.path.basename(f), 'ang %.1f mid %d' % (ang, mid))
