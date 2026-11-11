#!/usr/bin/env python3
"""Bakes the assistant's skins (other fairies) into the menu file.

Each skin is a folder in assets/assistant/source/ (ignored by git: skins
are only ever inside the menu files you build, never in the repository)
holding:

    picture.png   the fairy, with a see-through background (any size)
    skin.txt      its name, color, home games and lines (see README.md)

Run, from the repository's top folder:

    python3 assets/assistant/make_skins.py

and build as usual. Each picture is trimmed to what isn't see-through,
fitted into 56 x 56 and written to filesystem/assistant/ (also ignored by
git) as <folder>.sprite, with its skin.txt as <folder>.txt and a list of
the folders in list.txt. The menu offers them after its own fairy, Vee,
in folder-name order. They are for your own builds only: make_release.py
leaves them out of both released editions.

Needs Pillow (in the dev container: sudo apt-get install python3-pil).
"""
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SOURCE = os.path.join(HERE, "source")
OUT = os.path.join(ROOT, "filesystem", "assistant")
DFS = os.path.join(ROOT, "build", "N64FlashcartMenu.dfs")
MKSPRITE = os.path.join(os.environ.get("N64_INST", "/opt/libdragon"), "bin", "mksprite")
SIZE = 56
MOST = 8        # the menu offers this many

if not os.path.isdir(SOURCE):
    sys.exit(f"No skins: make {SOURCE} and put a folder per skin in it.")

folders = sorted(f for f in os.listdir(SOURCE) if os.path.isdir(os.path.join(SOURCE, f)))[:MOST]
shutil.rmtree(OUT, ignore_errors=True)
os.makedirs(OUT)
baked = []
with tempfile.TemporaryDirectory() as work:
    for folder in folders:
        picture_path = os.path.join(SOURCE, folder, "picture.png")
        skin_path = os.path.join(SOURCE, folder, "skin.txt")
        if not os.path.exists(picture_path) or not os.path.exists(skin_path):
            print(f"skipped {folder}: needs picture.png and skin.txt")
            continue
        stem = "".join(c for c in folder.lower() if c.isalnum())[:16] or f"skin{len(baked)}"
        picture = Image.open(picture_path).convert("RGBA")
        box = picture.getbbox()
        if box:
            picture = picture.crop(box)
        scale = SIZE / max(picture.width, picture.height)
        fitted = picture.resize((max(1, round(picture.width * scale)), max(1, round(picture.height * scale))), Image.LANCZOS)
        square = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
        square.paste(fitted, ((SIZE - fitted.width) // 2, (SIZE - fitted.height) // 2), fitted)
        small = os.path.join(work, stem + ".png")
        square.save(small)
        result = subprocess.run([MKSPRITE, "--format", "RGBA32", "--compress", "1", "-o", OUT, small],
                                capture_output=True, text=True)
        if result.returncode != 0:
            print(f"skipped {folder}: {result.stderr.strip()}")
            continue
        shutil.copyfile(skin_path, os.path.join(OUT, stem + ".txt"))
        baked.append(stem)

with open(os.path.join(OUT, "list.txt"), "w", encoding="utf-8", newline="\n") as out:
    for stem in baked:
        out.write(stem + "\n")

if os.path.exists(DFS):
    os.remove(DFS)  # the build only repacks the ROM's files when a listed one changes

total = sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT))
print(f"{len(baked)} assistant skins baked into {OUT} ({', '.join(baked)}), {total / 1024:.0f} KB.")
