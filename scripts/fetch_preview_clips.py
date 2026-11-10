#!/usr/bin/env python3
"""Fetch a preview clip for every commercially released N64 game and make it
ready for the menu's Game info screen.

  1. The list of games: libretro-database's No-Intro list, which gives each
     release's name and four-letter game code. Prototypes, demos, kiosk
     carts, betas, unlicensed carts, aftermarket homebrew and pirate copies
     are left out.
  2. The clips: the ~30 second gameplay "video snaps" kept on archive.org,
     named the No-Intro way. Several collections are searched, in order,
     for the game's own release name, then for any release of the same game
     (the same first three letters of its code), then for the same title
     whatever the region.
  3. Each one is cut to 25 seconds and encoded for the N64 by
     make_preview_clip.py (192x144, 20 frames a second, no sound).

One clip serves every region of a game: it is written to the folder named by
the first three letters of the code, which the menu looks in when the
region's own folder has no clip:

    build/previews/menu/metadata/N/S/M/preview.h264

A game with no clip anywhere keeps its cover on Game info, as before; so do
hacks (they carry their original's code). build/previews/clips_report.txt
lists what was made and what wasn't found.

    python3 scripts/fetch_preview_clips.py              # everything (about 1.7 GB of downloads)
    python3 scripts/fetch_preview_clips.py --only NSM,NKT
    python3 scripts/fetch_preview_clips.py --dry-run    # just the matching

Downloads are kept in build/preview_sources/ so a second run only makes what
is missing. Needs what make_preview_clip.py needs: run it in the dev
container, with ffmpeg installed (apt-get install ffmpeg).

Never commit the clips or the downloads: they belong to the games' owners.
"""

import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import sys
import threading
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_preview_clip as mpc  # noqa: E402

REPO = os.path.dirname(HERE)
DAT_URL = ("https://raw.githubusercontent.com/libretro/libretro-database/master/"
           "metadat/no-intro/Nintendo%20-%20Nintendo%2064.dat")
COLLECTIONS = ["Nintendo64VideoSnaps", "n64snaps", "Nintendo64-HyperSpinVideos"]
USER_AGENT = "SummerStation64-preview-fetch/1"

# Not commercial releases, or not the game as sold.
LEFT_OUT = re.compile(r"\((?:[^)]*\b(?:Proto|Prototype|Beta|Demo|Sample|Kiosk|Unl|Pirate|Aftermarket|"
                      r"Homebrew|Program|Test|Debug|Competition|Promo|Add-on)\b[^)]*)\)|\[BIOS\]", re.I)
CLIP_SECONDS = 25.0
SKIP_SECONDS = 1.0      # snaps often fade in from black


def fetch(url, dest=None, tries=4):
    for attempt in range(tries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=120) as r:
                if dest is None:
                    return r.read()
                tmp = dest + ".part"
                with open(tmp, "wb") as f:
                    while True:
                        block = r.read(1 << 16)
                        if not block:
                            break
                        f.write(block)
                os.replace(tmp, dest)
                return dest
        except Exception as e:  # network trouble: try again a little later
            if attempt == tries - 1:
                raise
            print(f"  ({e}; trying again)", file=sys.stderr)
            time.sleep(3 * (attempt + 1))


# The same game under another name in the snap collections.
ALIASES = {
    "007 goldeneye": "goldeneye 007",
    "nfl quarterback club 2001": "nfl qb club 2001",
    "michael owen s world league soccer 2000": "michael owen s wls 2000",
    "sin and punishment": "tsumi to batsu hoshi no keishousha",
}


def tidy_title(name):
    """'Legend of Zelda, The - Ocarina of Time (USA) (Rev 1)' -> 'legendofzeldaocarinaoftime'

    Spaces go too, so 'GT 64' is 'GT64' and 'Rakugakids' is 'Rakuga Kids'."""
    name = re.sub(r"(\.ia| - Copy)$", "", name)
    t = re.sub(r"\s*[\(\[][^)\]]*[\)\]]", "", name)
    t = re.sub(r",\s*the\b", "", t, flags=re.I)
    t = re.sub(r"^the\s+", "", t, flags=re.I)
    t = t.lower().replace("&", "and")
    t = re.sub(r"[^a-z0-9]+", " ", t).strip()
    t = ALIASES.get(t, t)
    return t.replace(" ", "")


def read_games(dat_text):
    """{base code: {'names': [release names], 'codes': set(), 'title': str}} for commercial releases."""
    games = {}
    for block in re.finditer(r'^game \((.*?)^\)', dat_text, re.M | re.S):
        body = block.group(1)
        name = re.search(r'^\s*name "([^"]*)"', body, re.M)
        serial = re.search(r'^\s*serial "([^"]*)"', body, re.M)
        if not name or not serial:
            continue
        name, code = name.group(1), serial.group(1).strip().upper()
        if not re.fullmatch(r"[A-Z0-9]{4}", code) or LEFT_OUT.search(name):
            continue
        g = games.setdefault(code[:3], {"names": [], "codes": set(), "title": None})
        if name not in g["names"]:
            g["names"].append(name)
        g["codes"].add(code)
    for g in games.values():
        # Name the game by its US release if there is one, else its first.
        us = [n for n in g["names"] if "(USA" in n]
        g["title"] = (us or g["names"])[0]
    return games


def read_collections(cache):
    """[(collection, {file name without .mp4: size}), ...] in search order."""
    found = []
    for ident in COLLECTIONS:
        path = os.path.join(cache, ident + ".json")
        if not os.path.exists(path):
            fetch(f"https://archive.org/metadata/{ident}", path)
        with open(path, encoding="utf-8") as f:
            files = json.load(f).get("files", [])
        found.append((ident, {fl["name"][:-4]: int(fl.get("size", 0)) for fl in files
                              if fl["name"].lower().endswith(".mp4") and "/" not in fl["name"]}))
    return found


def pick_sources(game, collections):
    """The snaps that could serve a game, best first: [(collection, file name), ...].
    Later ones are tried when one turns out unusable (some snaps are 5 second stubs)."""
    names = sorted(game["names"], key=lambda n: (0 if "(USA" in n else 1 if "(Europe" in n else 2, n))
    found = []
    for ident, files in collections:
        for n in names:
            if n in files and (ident, n) not in found:
                found.append((ident, n))
    titles = {tidy_title(n) for n in names}
    for ident, files in collections:
        for f in sorted(files, key=lambda n: (0 if "(USA" in n else 1, n)):
            if not LEFT_OUT.search(f) and tidy_title(f) in titles and (ident, f) not in found:
                found.append((ident, f))
    return found


def probe_seconds(tools, path):
    r = tools.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", path])
    try:
        return float(r.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        return 0.0


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", default=os.path.join(REPO, "build", "previews"))
    p.add_argument("--cache", default=os.path.join(REPO, "build", "preview_sources"))
    p.add_argument("--only", help="comma-separated codes (three or four letters) to do, such as NSM,NKTE")
    p.add_argument("--jobs", type=int, default=3, help="clips made at once (default 3)")
    p.add_argument("--dry-run", action="store_true", help="match games to clips and report; download nothing")
    p.add_argument("--redo", action="store_true", help="make clips again even if they exist")
    args = p.parse_args()

    os.makedirs(args.cache, exist_ok=True)
    dat = os.path.join(args.cache, "n64.dat")
    if not os.path.exists(dat):
        print("Fetching the N64 game list (libretro-database, No-Intro)...")
        fetch(DAT_URL, dat)
    with open(dat, encoding="utf-8", errors="replace") as f:
        games = read_games(f.read())
    collections = read_collections(args.cache)

    # --only limits what is made; the report still covers every game.
    keep = {c.strip().upper()[:3] for c in args.only.split(",") if c.strip()} if args.only else None

    plan, missing = [], []
    for base in sorted(games):
        srcs = pick_sources(games[base], collections)
        (plan if srcs else missing).append((base, srcs))
    print(f"{len(games)} games; a clip found for {len(plan)}, none for {len(missing)}.")

    tools = None if args.dry_run else mpc.Tools()
    videos = os.path.join(args.cache, "videos")
    os.makedirs(videos, exist_ok=True)
    results, lock = {}, threading.Lock()

    def make_one(base, ident, name):
        # The first collection's snaps keep plain names; another's carry its name too
        # (the same file name can be a different, longer snap there).
        stem = name if ident == COLLECTIONS[0] else f"{name} [{ident}]"
        local = os.path.join(videos, re.sub(r'[<>:"/\\|?*]', "_", stem) + ".mp4")
        if not os.path.exists(local):
            fetch(f"https://archive.org/download/{ident}/{urllib.parse.quote(name)}.mp4", local)
        seconds = probe_seconds(tools, local)
        length = min(CLIP_SECONDS, seconds - SKIP_SECONDS - 0.5)
        start = SKIP_SECONDS
        if length < mpc.MIN_SECONDS:
            start, length = 0.0, seconds - 0.2
        if length < mpc.MIN_SECONDS:
            return "too short", f"{seconds:.1f} s"
        try:
            mpc.make_clip(tools, base, local, start, length, args.out, 192, 20, 60,
                          label=f"https://archive.org/download/{ident}/{urllib.parse.quote(name)}.mp4")
        except SystemExit as e:
            return "failed", str(e)
        return "made", ""

    def make(base, srcs):
        """(status, why, the source used)"""
        # A clip made before seek tables were written is made again.
        folder = os.path.join(args.out, mpc.game_folder(base))
        if (os.path.exists(os.path.join(folder, "preview.h264")) and os.path.exists(os.path.join(folder, "preview.seek"))
                and not args.redo):
            return "kept", "", None
        tried = []
        for ident, name in srcs:
            status, why = make_one(base, ident, name)
            if status == "made":
                return status, "", (ident, name)
            tried.append(f"{name}: {status} {why}".strip())
        return "failed", "; ".join(tried), None

    if not args.dry_run:
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
            work = [(base, srcs) for base, srcs in plan if keep is None or base in keep]
            futures = {pool.submit(make, base, srcs): base for base, srcs in work}
            for done, fut in enumerate(concurrent.futures.as_completed(futures), 1):
                base = futures[fut]
                try:
                    results[base] = fut.result()
                except Exception as e:
                    results[base] = ("failed", str(e), None)
                with lock:
                    print(f"[{done}/{len(futures)}] {base} {games[base]['title']}: {results[base][0]} {results[base][1]}")

    os.makedirs(args.out, exist_ok=True)
    report = os.path.join(args.out, "clips_report.txt")
    with open(report, "w", encoding="utf-8") as f:
        f.write("# Preview clips: one per game, in menu/metadata/<first three letters of the code>/preview.h264\n")
        f.write("# code  status  game  <-  source\n")
        made_from = mpc.read_sources(args.out)     # what each clip was really made from
        for base, srcs in plan:
            clip = os.path.join(args.out, mpc.game_folder(base), "preview.h264")
            status = results.get(base) or (("ready" if os.path.exists(clip) else "not made"), "", None)
            if os.path.exists(clip) and base in made_from:
                source = "  ".join(made_from[base].split("\t"))
            else:
                ident, name = srcs[0]
                source = f"{ident}/{name}.mp4"
            f.write(f"{base}  {status[0]}{' (' + status[1] + ')' if status[1] else ''}  "
                    f"{games[base]['title']}  <-  {source}\n")
        f.write(f"\n# No clip found ({len(missing)}): Game info shows the cover for these.\n")
        for base, _ in missing:
            f.write(f"{base}  {games[base]['title']}\n")
    made = sum(1 for base, _ in plan if os.path.exists(os.path.join(args.out, mpc.game_folder(base), "preview.h264")))
    print(f"\n{made} clips ready, {len(missing)} games without one. Report: {report}")
    print(f"Copy {os.path.join(args.out, 'menu')} onto the root of the SD card.")


if __name__ == "__main__":
    main()
