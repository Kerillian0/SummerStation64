#!/usr/bin/env python3
"""Turns a folder of box art PNGs into the small files the menu bakes in.

Reads   assets/boxart/source/<N>/<Z>/<S>/<E>/boxart_front.png  (and
        boxart_back, _top, _left and _right.png where there are any), the
        layout of the SD card's menu/metadata folder.
Writes  filesystem/art/Z/S/NZSE_f.sprite, NZSE_b, _t, _l and _r. The two
        folders are the code's middle letters: the console finds a file by
        walking through a folder's list, and a thousand files in one folder
        would make that walk slow.

A folder without the region letter that sits beside folders with one is
baked as the European (P) version: that is whose cover it is.

Each picture is shrunk to the size the menu shows it at (to fit 158 x 158)
(fronts and backs first lose the black frame many scans carry) and
stored in the console's own 16-bit format, so the console only has to
copy it in. Run from the project's top folder. See README.md here.
"""
import os
import subprocess
import sys
import tempfile

from PIL import Image

SOURCE = "assets/boxart/source"
OUT = "filesystem/art"
MAX_SIDE = 158          # BOXART_WIDTH_MAX / BOXART_HEIGHT_MAX in the menu
SIDES = {"boxart_front.png": "f", "boxart_back.png": "b",
         # the edges of the 3D Box (the top strip is 158 x 22 in the pack)
         "boxart_top.png": "t", "boxart_left.png": "l", "boxart_right.png": "r",
         # the cartridge, cut out (its see-through edges are kept)
         "gamepak_front.png": "p"}
# The black frame some scans carry (about half the pack's fronts and backs,
# 2-4 px): edge lines near-black almost all the way across are cut off, at
# most a twentieth of the picture per side, so covers black by design keep
# their shape. The same rule as src/menu/art_trim.c, for art read from the card.
DARK_SUM = 96
DARK_PERCENT = 97


def trim_black_frame(picture):
    w, h = picture.size
    px = picture.load()
    def dark_line(points):
        n = sum(1 for p in points if p[0] + p[1] + p[2] < DARK_SUM)
        return n * 100 >= DARK_PERCENT * len(points)
    most = lambda size: max(2, size // 20)
    top = bottom = left = right = 0
    while top < most(h) and dark_line([px[x, top] for x in range(w)]): top += 1
    while bottom < most(h) and dark_line([px[x, h - 1 - bottom] for x in range(w)]): bottom += 1
    while left < most(w) and dark_line([px[left, y] for y in range(h)]): left += 1
    while right < most(w) and dark_line([px[w - 1 - right, y] for y in range(h)]): right += 1
    return picture.crop((left, top, w - right, h - bottom))


MKSPRITE = os.path.join(os.environ.get("N64_INST", "/opt/libdragon"), "bin", "mksprite")

if not os.path.isdir(SOURCE):
    sys.exit(f"Nothing to do: {SOURCE} does not exist. See assets/boxart/README.md.")

import shutil
shutil.rmtree(OUT, ignore_errors=True)
os.makedirs(OUT, exist_ok=True)
# The build only repacks the menu's files when it sees one change, and it
# does not look inside these folders, so make it repack.
if os.path.exists("build/N64FlashcartMenu.dfs"):
    os.remove("build/N64FlashcartMenu.dfs")

made = 0
skipped = []
with tempfile.TemporaryDirectory() as work:
    for folder, _, files in sorted(os.walk(SOURCE)):
        parts = os.path.relpath(folder, SOURCE).split(os.sep)
        # A game's folder is three or four single characters deep: N/Z/S/E
        # (with the region) or N/Z/S (any region).
        if len(parts) not in (3, 4) or any(len(p) != 1 for p in parts):
            continue
        code = "".join(parts)
        # A folder without the region letter beside ones with it holds the
        # European cover (all 29 in the pack do): bake it as the P (Europe)
        # version, so a Japanese or US game sharing the code (Dezaemon 3D,
        # NDZJ, beside Duke Nukem: Zero Hour) doesn't take it for its own.
        if len(parts) == 3 and any(len(d) == 1 and os.path.isfile(os.path.join(folder, d, "boxart_front.png")) for d in os.listdir(folder)):
            if os.path.isdir(os.path.join(folder, "P")):
                continue    # the pack has a European folder already
            code += "P"
        for name, side in SIDES.items():
            if name not in files:
                continue
            try:
                picture = Image.open(os.path.join(folder, name)).convert("RGBA" if side == "p" else "RGB")
                if side in ("f", "b") and picture.width >= 16 and picture.height >= 16:
                    picture = trim_black_frame(picture)
                if side == "p":
                    # trim the see-through margin, so the 3D cartridge's edges meet its front
                    picture = picture.crop(picture.getchannel("A").getbbox() or (0, 0, picture.width, picture.height))
            except Exception as error:
                skipped.append(f"{code} {name}: {error}")
                continue
            scale = min(MAX_SIDE / picture.width, MAX_SIDE / picture.height, 1.0)
            size = (max(1, round(picture.width * scale)), max(1, round(picture.height * scale)))
            small = os.path.join(work, f"{code}_{side}.png")
            picture.resize(size, Image.LANCZOS).save(small)
            bucket = os.path.join(OUT, code[1], code[2])
            os.makedirs(bucket, exist_ok=True)
            result = subprocess.run([MKSPRITE, "--format", "RGBA16", "--compress", "1", "-o", bucket, small],
                                    capture_output=True, text=True)
            if result.returncode != 0:
                skipped.append(f"{code} {name}: {result.stderr.strip()}")
                continue
            made += 1

total = sum(os.path.getsize(os.path.join(folder, f)) for folder, _, files in os.walk(OUT) for f in files)
print(f"{made} pictures baked into {OUT}, {total / 1024 / 1024:.2f} MB added to the menu file.")
for line in skipped:
    print("skipped:", line)
