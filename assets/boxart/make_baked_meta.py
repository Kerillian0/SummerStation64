#!/usr/bin/env python3
"""Packs the metadata pack's game details into the menu file.

Reads   assets/boxart/source/<N>/<Z>/<S>/<E>/metadata.ini, the layout of the
        SD card's menu/metadata folder (the same source as the box art).
Writes  filesystem/meta/Z/S/NZSE.ini: the same file with its comments and
        blank lines left out (about a tenth of the size). Folders are named
        after the code's middle letters, as for the baked art.

The menu reads a game's details (name, developer, release date, players,
age rating, description) from here first and from the SD card only when a
game has none here. Run from the project's top folder. See README.md here.
"""
import os
import shutil
import sys

SOURCE = "assets/boxart/source"
OUT = "filesystem/meta"

if not os.path.isdir(SOURCE):
    sys.exit(f"Nothing to do: {SOURCE} does not exist. See assets/boxart/README.md.")

shutil.rmtree(OUT, ignore_errors=True)
os.makedirs(OUT, exist_ok=True)
# The build does not look inside these folders for changes, so make it repack.
if os.path.exists("build/N64FlashcartMenu.dfs"):
    os.remove("build/N64FlashcartMenu.dfs")

made = 0
bytes_in = 0
bytes_out = 0
for folder, _, files in sorted(os.walk(SOURCE)):
    if "metadata.ini" not in files:
        continue
    parts = os.path.relpath(folder, SOURCE).split(os.sep)
    if len(parts) != 4 or any(len(p) != 1 for p in parts):
        continue    # the menu only looks up four-letter codes
    code = "".join(parts)
    source = os.path.join(folder, "metadata.ini")
    with open(source, encoding="utf-8", errors="replace") as f:
        text = f.read()
    kept = [line.rstrip() for line in text.splitlines()
            if line.strip() and not line.lstrip().startswith((";", "#"))]
    target = os.path.join(OUT, code[1], code[2], f"{code}.ini")
    os.makedirs(os.path.dirname(target), exist_ok=True)
    with open(target, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(kept) + "\n")
    made += 1
    bytes_in += len(text.encode("utf-8"))
    bytes_out += os.path.getsize(target)

print(f"Baked the details of {made} games: {bytes_in // 1024} KB of files packed into {bytes_out // 1024} KB.")
