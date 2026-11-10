#!/usr/bin/env python3
"""Bakes the default profile pictures into the menu file.

Put the pictures in assets/profiles/source/ (PNG or JPEG, any size; square
ones look best). That folder is ignored by git: the pictures are only ever
inside the menu files you build, never in the repository. Then run, from the
repository's top folder:

    python3 assets/profiles/make_avatars.py

and build as usual. Each picture is cut to a square from its middle, shrunk
to 64 x 64 and written to filesystem/avatars/ (also ignored by git) with a
list of their names. A picture's name in the menu is its file name without
the extension, so "Mario.png" is "Mario"; the menu shows them in file-name
order, so "01 Mario.png" puts it first (the number and its space are left
off the name). See-through parts show the player's color behind them.

Needs Pillow (in the dev container: sudo apt-get install python3-pil).
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SOURCE = os.path.join(HERE, "source")
OUT = os.path.join(ROOT, "filesystem", "avatars")
DFS = os.path.join(ROOT, "build", "N64FlashcartMenu.dfs")
MKSPRITE = os.path.join(os.environ.get("N64_INST", "/opt/libdragon"), "bin", "mksprite")
SIZE = 64
MOST = 48       # the menu's list holds this many

if not os.path.isdir(SOURCE):
    sys.exit(f"No pictures: make {SOURCE} and put them in it.")

files = sorted(f for f in os.listdir(SOURCE) if f.lower().endswith((".png", ".jpg", ".jpeg")))
if len(files) > MOST:
    print(f"Only the first {MOST} of {len(files)} pictures are baked.")
    files = files[:MOST]

shutil.rmtree(OUT, ignore_errors=True)
os.makedirs(OUT)
names = []
skipped = []
with tempfile.TemporaryDirectory() as work:
    for i, f in enumerate(files):
        name = re.sub(r"^\d+\s+", "", os.path.splitext(f)[0]).strip()[:31] or f"Picture {i + 1}"
        try:
            picture = Image.open(os.path.join(SOURCE, f)).convert("RGBA")
        except Exception as error:
            skipped.append(f"{f}: {error}")
            continue
        side = min(picture.width, picture.height)
        left = (picture.width - side) // 2
        top = (picture.height - side) // 2
        picture = picture.crop((left, top, left + side, top + side)).resize((SIZE, SIZE), Image.LANCZOS)
        stem = f"a{len(names):02d}"
        small = os.path.join(work, stem + ".png")
        picture.save(small)
        result = subprocess.run([MKSPRITE, "--format", "RGBA16", "--compress", "1", "-o", OUT, small],
                                capture_output=True, text=True)
        if result.returncode != 0:
            skipped.append(f"{f}: {result.stderr.strip()}")
            continue
        names.append((stem, name))

with open(os.path.join(OUT, "list.txt"), "w", encoding="utf-8", newline="\n") as out:
    for stem, name in names:
        out.write(f"{stem}\t{name}\n")

if os.path.exists(DFS):
    os.remove(DFS)  # the build only repacks the ROM's files when a listed one changes

total = sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT))
print(f"{len(names)} profile pictures baked into {OUT}, {total / 1024:.0f} KB added to the menu file.")
for line in skipped:
    print("skipped:", line)
