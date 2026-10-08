#!/usr/bin/env python3
"""Turns a folder of box art PNGs into the small files the menu bakes in.

Reads   assets/boxart/source/<N>/<Z>/<S>/<E>/boxart_front.png  (and
        boxart_back.png), the layout of the SD card's menu/metadata folder.
Writes  filesystem/art/Z/S/NZSE_f.sprite  and  NZSE_b.sprite. The two
        folders are the code's middle letters: the console finds a file by
        walking through a folder's list, and a thousand files in one folder
        would make that walk slow.

Each picture is shrunk to the size the menu shows it at (to fit 158 x 158)
and stored in the console's own 16-bit format, so the console only has to
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
SIDES = {"boxart_front.png": "f", "boxart_back.png": "b"}
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
        for name, side in SIDES.items():
            if name not in files:
                continue
            try:
                picture = Image.open(os.path.join(folder, name)).convert("RGB")
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
