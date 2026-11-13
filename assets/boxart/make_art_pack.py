#!/usr/bin/env python3
"""Pack baked box art into one file for the SD card: menu/art.pak.

The SummerCart64 must load the whole menu file within 5 seconds, so the art
no longer goes inside it. make_baked_art.py makes the pictures (one .sprite
each, in build/art/<2nd letter>/<3rd letter>/); this script puts them in one
file that the menu reads a cover at a time (baked_art.c).

    python3 assets/boxart/make_art_pack.py [sprites folder] [pack file]

Defaults: build/art -> output/menu/art.pak. Copy art.pak to the card's
menu folder (sd:/menu/art.pak).

Layout (all numbers big-endian, as the N64 reads them):
  0      header, 512 bytes: b"SS64ART\\0", version 2, count, index offset,
         record size 16
  512    index: count records of 16 bytes, sorted by key:
         key (8 bytes: the 3- or 4-letter code padded with zeros to 4, the
         side letter f/b/t/l/r/p, 3 zeros), offset (4), size (4)
  ...    each picture's .sprite file, unpacked (mkasset -c 0: unpacking
         took the console 6-9 ms a cover, reading the extra 10 KB about 1),
         starting on a 512-byte boundary and padded to one, so the cart
         reads whole card sectors
"""

import os
import re
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SECTOR = 512
RECORD = 16
VERSION = 2     # 1 held the pictures packed
MKASSET = os.path.join(os.environ.get("N64_INST", "/opt/libdragon"), "bin", "mkasset")
NAME = re.compile(r"^([0-9A-Z]{3,4})_([fbtlrp])\.sprite$")


def pad(n):
    return (n + SECTOR - 1) // SECTOR * SECTOR


def key_of(code, side):
    return code.encode("ascii").ljust(4, b"\0") + side.encode("ascii") + b"\0\0\0"


def make_pack(source, target):
    pictures = []
    for folder, _, files in os.walk(source):
        for name in files:
            found = NAME.match(name)
            if found:
                pictures.append((key_of(found.group(1), found.group(2)), os.path.join(folder, name)))
    if not pictures:
        sys.exit(f"No baked pictures in {source}: run  python3 assets/boxart/make_baked_art.py  first.")
    pictures.sort()
    keys = [k for k, _ in pictures]
    if len(set(keys)) != len(keys):
        sys.exit("Two pictures with the same code and side: " + str(sorted({k for k in keys if keys.count(k) > 1})))

    index_size = pad(len(pictures) * RECORD)
    offset = SECTOR + index_size
    index = bytearray()
    blobs = []
    work = tempfile.TemporaryDirectory()
    paths = [path for _, path in pictures]
    for start in range(0, len(paths), 200):
        subprocess.run([MKASSET, "-c", "0", "-o", work.name] + paths[start:start + 200], check=True)
    for key, path in pictures:
        with open(os.path.join(work.name, os.path.basename(path)), "rb") as f:
            data = f.read()
        if data[:3] == b"DCA":
            sys.exit(f"{path} is still packed after mkasset -c 0")
        index += key + struct.pack(">II", offset, len(data))
        blobs.append(data)
        offset += pad(len(data))

    header = b"SS64ART\0" + struct.pack(">IIII", VERSION, len(pictures), SECTOR, RECORD)
    os.makedirs(os.path.dirname(target) or ".", exist_ok=True)
    temp = target + ".tmp"
    with open(temp, "wb") as f:
        f.write(header.ljust(SECTOR, b"\0"))
        f.write(bytes(index).ljust(index_size, b"\0"))
        for data in blobs:
            f.write(data.ljust(pad(len(data)), b"\0"))
    os.replace(temp, target)
    work.cleanup()
    biggest = max(len(b) for b in blobs)
    print(f"{len(pictures)} pictures packed into {target}: {offset / 1024 / 1024:.2f} MB "
          f"(index {index_size // 1024} KB, largest picture {biggest // 1024} KB).")
    return len(pictures)


if __name__ == "__main__":
    source = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "art")
    target = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "output", "menu", "art.pak")
    make_pack(source, target)
