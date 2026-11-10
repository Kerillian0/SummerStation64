#!/usr/bin/env python3
"""Screenshots of Game info for every commercial N64 game, with its title
logo (scripts/fetch_logos.py), taken from the real menu running in ares.

How: a test build of the menu (FLAGS=-DLOGO_SHOTS, src/menu/logo_shots.c)
opens Game info for each game in turn and sends every finished screen out
through the emulator's debug log. In an emulator the menu reads its own
built-in files instead of an SD card, so those carry what the card would:
a placeholder file per game (just a ROM header with the game's code, no
game), its baked cover and details, and the logos.

  1. prepare  (PC)             placeholder games, covers, details, logos and the
                               list, under build/shots/fs
  2. build    (dev container)  the test ROM:
       FLAGS=-DLOGO_SHOTS make -j8 BUILD_DIR=build/shots/obj OUTPUT_DIR=build/shots/out \\
            N64_MKDFS_ROOT=build/shots/fs build/shots/out/N64FlashcartMenu.n64
  3. run      (PC)             ares plays it; its log becomes build/shots/png/*.png
                               and build/shots/shots.json (about 15-25 minutes)
  4. page     (PC)             build/shots/review/index.html to look through them

    python tests/logo_shots/shots.py prepare --art-from ../../filesystem
    python tests/logo_shots/shots.py run --ares C:/path/to/ares.exe
    python tests/logo_shots/shots.py page

prepare needs the logos (python3 scripts/fetch_logos.py, which also leaves
the game list in build/logo_sources/n64.dat) and a normal build first (for the
menu's own fonts and pictures in filesystem/). Covers and details come from
the baked art in --art-from (filesystem/art and filesystem/meta of a
checkout where they were made, see assets/boxart/README.md); without them
the screens show no cover. run and page need Pillow and numpy.

Nothing here is committed: the pictures belong to the games' owners.
"""

import argparse
import base64
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, "scripts"))
import fetch_logos as logos  # noqa: E402  (the game list and title matching)

SHOTS = os.path.join(REPO, "build", "shots")
FS = os.path.join(SHOTS, "fs")
ROM = os.path.join(SHOTS, "out", "N64FlashcartMenu.n64")
LOG = os.path.join(SHOTS, "ares.log")
PNG = os.path.join(SHOTS, "png")
ISVIEWER_LIMIT = 0x03FF0000      # ares only has the debug log for ROMs up to this size


plain_first = logos.plain_first   # US release first, then Europe, then the rest


def releases_by_code(dat):
    codes = {}
    for block in re.finditer(r'^game \((.*?)^\)', dat, re.M | re.S):
        body = block.group(1)
        name = re.search(r'^\s*name "([^"]*)"', body, re.M)
        serial = re.search(r'^\s*serial "([^"]*)"', body, re.M)
        if name and serial:
            codes.setdefault(serial.group(1).strip().upper(), []).append(name.group(1))
    return codes


def baked_front(art, code):
    """The baked front cover the menu would find for this code (baked_art.c), or None."""
    for c in (code, code[:3] + "P" if code[3] in "PDFISUXYHWL" else None, code[:3]):
        if c:
            path = os.path.join(art, code[1], code[2], c + "_f.sprite")
            if os.path.exists(path):
                return path
    return None


def stub_rom(title, code):
    """A ROM header and an empty boot area: enough for the menu to read the game's name and code."""
    header = bytearray(0x1000)
    header[0x00:0x10] = bytes.fromhex("80371240 0000000F 80000400 0000144B".replace(" ", ""))
    name = re.sub(r"\s*\(.*", "", title).upper().encode("ascii", "replace")[:20]
    header[0x20:0x34] = name.ljust(20, b" ")
    header[0x3B:0x3F] = code.encode("ascii")
    return bytes(header)


def prepare(args):
    dat_path = os.path.join(REPO, "build", "logo_sources", "n64.dat")
    manifest_path = os.path.join(REPO, "build", "logos", "logos.json")
    logo_dir = os.path.join(REPO, "build", "logos", "menu")
    for need in (dat_path, manifest_path, logo_dir, os.path.join(REPO, "filesystem", "gamedata.txt")):
        if not os.path.exists(need):
            sys.exit(f"Missing {need}: run scripts/fetch_logos.py and a normal build first.")
    art = os.path.join(args.art_from, "art")
    meta = os.path.join(args.art_from, "meta")
    if not os.path.isdir(art):
        print(f"No baked covers in {art}: the screens will have none.")

    with open(dat_path, encoding="utf-8") as f:
        dat = f.read()
    with open(manifest_path, encoding="utf-8") as f:
        manifest = json.load(f)
    by_code = releases_by_code(dat)
    only = {c.strip().upper() for c in args.only.split(",")} if args.only else None

    shutil.rmtree(FS, ignore_errors=True)
    # The menu's own files (fonts, pictures, tables), less any baked art: only the covers wanted go in.
    shutil.copytree(os.path.join(REPO, "filesystem"), FS, ignore=lambda d, names: [
        n for n in names if d == os.path.join(REPO, "filesystem") and n in ("art", "meta", "menu", "shots")])
    if os.path.isdir(meta):
        shutil.copytree(meta, os.path.join(FS, "meta"))
    shutil.copytree(logo_dir, os.path.join(FS, "menu"))
    os.makedirs(os.path.join(FS, "shots"))

    games, used_names = [], set()
    for entry in sorted(manifest, key=lambda e: logos.tidy_title(e["title"])):
        # The code a player most likely has: the US one, else the first, preferring one with a cover.
        codes = sorted(entry["codes"], key=lambda c: plain_first(sorted(by_code.get(c, [c]), key=plain_first)[0]))
        with_cover = [c for c in codes if os.path.isdir(art) and baked_front(art, c)]
        code = (with_cover or codes)[0]
        title = sorted(by_code.get(code, [entry["title"]]), key=plain_first)[0]
        file_name = re.sub(r'[\\/:*?"<>|]', "_", title) + ".z64"
        if file_name in used_names:
            file_name = f"{file_name[:-4]} [{code}].z64"
        used_names.add(file_name)
        # Every game is listed in games.json, so pictures keep their numbers; --only puts just some in the ROM.
        wanted = not only or entry["base"] in only or code in only
        if wanted:
            with open(os.path.join(FS, "shots", file_name), "wb") as f:
                f.write(stub_rom(title, code))
        front = baked_front(art, code) if os.path.isdir(art) else None
        if front and wanted:
            dest = os.path.join(FS, "art", os.path.relpath(front, art))
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copyfile(front, dest)
        logo_from = None
        if entry["logo"]:
            logo_from = entry["regions"].get(code, entry["logo"])
        games.append({"file": file_name, "title": title, "code": code, "base": entry["base"],
                      "logo": logo_from, "how": entry["how"] if logo_from else None,
                      "own_region_logo": code in entry["regions"], "cover": bool(front),
                      "override": entry.get("override") if logo_from == entry["logo"] else None,
                      "wanted": wanted})

    with open(os.path.join(FS, "shots", "list.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("".join(g["file"] + "\n" for g in games if g["wanted"]))
    os.makedirs(SHOTS, exist_ok=True)
    with open(os.path.join(SHOTS, "games.json"), "w", encoding="utf-8") as f:
        json.dump(games, f, indent=1, ensure_ascii=False)
    # The build only notices files it made itself; this one must be packed again.
    dfs = os.path.join(SHOTS, "obj", "N64FlashcartMenu.dfs")
    if os.path.exists(dfs):
        os.remove(dfs)

    size = sum(os.path.getsize(os.path.join(d, n)) for d, _, ns in os.walk(FS) for n in ns)
    taken = [g for g in games if g["wanted"]]
    print(f"{len(taken)} games ({sum(g['cover'] for g in taken)} with a cover, "
          f"{sum(bool(g['logo']) for g in taken)} with a logo); {size / 1e6:.1f} MB of files in {FS}.")
    if size > ISVIEWER_LIMIT - 4_000_000:
        print("Warning: the ROM may come out too big for ares's debug log (64 MB).")


def frame_to_png(data, w, h, path):
    import numpy as np
    from PIL import Image
    v = np.frombuffer(data, dtype=">u2").reshape(h, w)
    rgb = np.empty((h, w, 3), dtype=np.uint8)
    for i, shift in enumerate((11, 6, 1)):
        c = ((v >> shift) & 31).astype(np.uint8)
        rgb[..., i] = (c << 3) | (c >> 2)
    Image.fromarray(rgb, "RGB").save(path, optimize=False)


def decode(log_path):
    """Pictures out of the log. Returns how many."""
    os.makedirs(PNG, exist_ok=True)
    with open(os.path.join(SHOTS, "games.json"), encoding="utf-8") as f:
        number = {g["file"]: i for i, g in enumerate(json.load(f))}    # pictures are numbered by place in the whole list
    made = 0
    b64 = re.compile(r"^[A-Za-z0-9+/=]+$")
    shot, parts = None, []
    with open(log_path, encoding="latin-1") as f:
        for line in f:
            line = line.strip()
            if line.startswith("@@SHOT "):
                _, index, w, h, name = line.split(" ", 4)
                shot, parts = (int(index), int(w), int(h), name), []
            elif line.startswith("@@END ") and shot:
                index, w, h, name = shot
                data = b"".join(base64.b64decode(p) for p in parts)
                if len(data) == w * h * 2:
                    frame_to_png(data, w, h, os.path.join(PNG, f"{number.get(name, index):03d}.png"))
                    made += 1
                else:
                    print(f"  {name}: {len(data)} bytes, not {w * h * 2}")
                shot = None
            elif shot and b64.match(line):
                parts.append(line)
    return made


def run(args):
    if not os.path.exists(ROM):
        sys.exit(f"No test ROM at {ROM}: build it first (see the top of this file).")
    if os.path.getsize(ROM) > ISVIEWER_LIMIT:
        sys.exit("The test ROM is too big for ares's debug log: leave some covers out.")
    with open(os.path.join(FS, "shots", "list.txt"), encoding="utf-8") as f:
        count = sum(1 for line in f if line.strip())     # earlier pictures stay: a --only run replaces just its own
    # The settings given below are for this run only: ares's own file is put back afterwards.
    settings = os.path.join(os.path.dirname(os.path.abspath(args.ares)), "settings.bml")
    kept = open(settings, "rb").read() if os.path.exists(settings) else None
    print(f"ares is taking {count} pictures; its window stays open until it is done.")
    with open(LOG, "wb") as log:
        ares = subprocess.Popen([args.ares, "--system", "Nintendo 64", "--no-file-prompt",
                                 "--setting", "Input/Defocus=Allow",       # keeps going behind other windows
                                 "--setting", "Audio/Mute=true",
                                 "--setting", "Nintendo64/ExpansionPak=true",
                                 ROM], stdout=log, stderr=subprocess.STDOUT)
    began, read_to, last_change, taken, done = time.time(), 0, time.time(), 0, False
    try:
        while ares.poll() is None:
            time.sleep(5)
            with open(LOG, "rb") as f:
                f.seek(max(0, read_to - 16))   # a marker may straddle two reads
                new = f.read()
            if len(new) > 16:
                last_change = time.time()
            taken += new.count(b"@@END ") - (new[:16].count(b"@@END ") if read_to else 0)
            read_to = max(0, read_to - 16) + len(new)
            if b"@@DONE" in new:
                done = True
                break
            if args.limit and taken >= args.limit:
                print(f"\nStopping after {taken}, as asked.")
                break
            if time.time() - last_change > args.stall:
                print(f"\nNothing new in the log for {args.stall} seconds: stopping.")
                break
            print(f"\r  {int(time.time() - began)} s: {taken} of {count}", end="", flush=True)
    finally:
        if ares.poll() is None:
            ares.terminate()
            ares.wait(30)
        if kept is not None:
            with open(settings, "wb") as f:
                f.write(kept)
    print()
    made = decode(LOG)
    print(f"{made} of {count} pictures in {PNG}" + ("" if done else " (ares stopped before the end)"))


def page(args):
    from PIL import Image
    with open(os.path.join(SHOTS, "games.json"), encoding="utf-8") as f:
        games = json.load(f)
    out = os.path.join(SHOTS, "review")
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(os.path.join(out, "shots"))
    rows = []
    for i, g in enumerate(games):
        src = os.path.join(PNG, f"{i:03d}.png")
        if not os.path.exists(src):
            continue
        Image.open(src).save(os.path.join(out, "shots", f"{i:03d}.webp"), quality=args.quality, method=6)
        rows.append(dict(g, shot=f"shots/{i:03d}.webp"))
    # The list goes inside the page itself (review.html's /*GAMES*/ and /*INFO*/).
    as_js = lambda value: json.dumps(value, ensure_ascii=False).replace("</", "<\\/")
    captured = time.strftime("%Y-%m-%d", time.localtime(os.path.getmtime(LOG))) if os.path.exists(LOG) else None
    with open(os.path.join(HERE, "review.html"), encoding="utf-8") as f:
        html = f.read()
    html = html.replace("/*GAMES*/[]", as_js(rows), 1).replace("/*INFO*/{}", as_js({"captured": captured}), 1)
    with open(os.path.join(out, "index.html"), "w", encoding="utf-8", newline="\n") as f:
        f.write(html)
    size = sum(os.path.getsize(os.path.join(out, "shots", n)) for n in os.listdir(os.path.join(out, "shots")))
    print(f"{len(rows)} pictures ({size / 1e6:.1f} MB) in {out}; open index.html")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="step", required=True)
    s = sub.add_parser("prepare")
    s.add_argument("--only", help="put just these games in the ROM (codes, three or four letters, such as "
                                  "NSM,NPOE); their pictures replace the ones from an earlier run")
    s.add_argument("--art-from", default=os.path.join(REPO, "filesystem"),
                   help="folder holding the baked art/ and meta/ (default: this checkout's filesystem/)")
    s = sub.add_parser("run")
    s.add_argument("--ares", required=True, help="ares.exe (or ares on Linux/macOS)")
    s.add_argument("--stall", type=int, default=180, help="give up after this many seconds without output")
    s.add_argument("--limit", type=int, default=0, help="stop after this many pictures (a quick check)")
    sub.add_parser("decode", help="make the pictures again from build/shots/ares.log")
    s = sub.add_parser("page")
    s.add_argument("--quality", type=int, default=88, help="WebP quality of the pictures (default 88)")
    args = p.parse_args()
    if args.step == "prepare":
        prepare(args)
    elif args.step == "run":
        run(args)
    elif args.step == "decode":
        print(f"{decode(LOG)} pictures in {PNG}")
    else:
        page(args)


if __name__ == "__main__":
    main()
