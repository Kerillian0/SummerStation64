#!/usr/bin/env python3
"""Fetch a title logo for every commercially released N64 game and make it
ready for the menu's Game info screen, where it takes the place of the
game's name.

  1. The list of games: libretro-database's No-Intro list, which gives each
     release's name and four-letter game code. Prototypes, demos, kiosk
     carts, betas, unlicensed carts, aftermarket homebrew and pirate copies
     are left out (414 games).
  2. The logos: the Named_Logos folder of libretro's N64 thumbnails, named
     the No-Intro way: see-through PNGs, mostly 512 pixels wide. A game whose
     releases have none there borrows a file of the same title.
  3. logo_overrides.txt, beside this script, fixes the few it gets wrong: a
     crop, a picture from somewhere else, or no logo at all.
  4. Each is cut down to its visible part, shrunk to fit the name's place on
     Game info (288 x 78, never enlarged) and stored in the console's 32-bit
     format with libdragon's mksprite, so its soft edges stay soft.

One logo serves every region of a game unless a region's own differs (a
Japanese title, say). The shared one goes in the folder named by the first
three letters of the code, which the menu looks in when the region's own
folder has none:

    build/logos/menu/metadata/N/S/M/logo.sprite      every region
    build/logos/menu/metadata/B/K/J/J/logo.sprite    only where Japan's differs

Copy build/logos/menu to the SD card's menu folder (it merges with the box
art). A game without a logo keeps its name as text; so do hacks (they carry
their original's code). build/logos/logos_report.txt lists what was made
and what wasn't found; build/logos/logos.json is the same for scripts.

    python3 scripts/fetch_logos.py              # everything
    python3 scripts/fetch_logos.py --only NSM,NKT
    python3 scripts/fetch_logos.py --dry-run    # just the matching

Downloads are kept in build/logo_sources/ so a second run only fetches what
is missing. Needs Pillow and libdragon's mksprite: run it in the dev
container (sudo apt-get install python3-pil).

Never commit the logos or the downloads: they belong to the games' owners.
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
import posixpath
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
THUMBS = "libretro-thumbnails/Nintendo_-_Nintendo_64"
TREE_URL = f"https://api.github.com/repos/{THUMBS}/git/trees/master?recursive=1"
RAW_URL = f"https://raw.githubusercontent.com/{THUMBS}/master/"
FOLDER = "Named_Logos"
LOGO_WIDTH, LOGO_HEIGHT = 288, 78   # TITLE_WIDTH and LOGO_HEIGHT in src/menu/game_info_ui.c
FAINT = 8                           # alpha at or below this doesn't count as part of the picture when cutting it down
LOGO_FILE = "logo.sprite"
DAT_URL = ("https://raw.githubusercontent.com/libretro/libretro-database/master/"
           "metadat/no-intro/Nintendo%20-%20Nintendo%2064.dat")
USER_AGENT = "SummerStation64-logo-fetch/1"

# Not commercial releases, or not the game as sold.
LEFT_OUT = re.compile(r"\((?:[^)]*\b(?:Proto|Prototype|Beta|Demo|Sample|Kiosk|Unl|Pirate|Aftermarket|"
                      r"Homebrew|Program|Test|Debug|Competition|Promo|Add-on)\b[^)]*)\)|\[BIOS\]", re.I)

# The same game under another name in the thumbnails.
ALIASES = {
    "007 goldeneye": "goldeneye 007",
    "nfl quarterback club 2001": "nfl qb club 2001",
    "michael owen s world league soccer 2000": "michael owen s wls 2000",
    "sin and punishment": "tsumi to batsu hoshi no keishousha",
}


def fetch(url, dest, tries=4):
    """Download url to dest (whole, or not at all), trying again after network trouble."""
    for attempt in range(tries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=120) as r:
                tmp = dest + ".part"
                with open(tmp, "wb") as f:
                    shutil.copyfileobj(r, f, 1 << 16)
                os.replace(tmp, dest)
                return dest
        except Exception as e:
            if attempt == tries - 1:
                raise
            print(f"  ({e}; trying again)", file=sys.stderr)
            time.sleep(3 * (attempt + 1))


def tidy_title(name):
    """'Legend of Zelda, The - Ocarina of Time (USA) (Rev 1)' -> 'legendofzeldaocarinaoftime'

    Spaces go too, so 'GT 64' is 'GT64' and 'Rakugakids' is 'Rakuga Kids'."""
    t = re.sub(r"\s*[\(\[][^)\]]*[\)\]]", "", name)
    t = re.sub(r",\s*the\b", "", t, flags=re.I)
    t = re.sub(r"^the\s+", "", t, flags=re.I)
    t = t.lower().replace("&", "and")
    t = re.sub(r"[^a-z0-9]+", " ", t).strip()
    t = ALIASES.get(t, t)
    return t.replace(" ", "")


def read_games(dat_text):
    """{first three letters of the code: {'names': [release names], 'codes': set(), 'title': str}}
    for the commercial releases. One entry is one game, whatever its regions."""
    games = {}
    for code, names in read_releases(dat_text).items():
        g = games.setdefault(code[:3], {"names": [], "codes": set(), "title": None})
        g["names"] += [n for n in names if n not in g["names"]]
        g["codes"].add(code)
    for g in games.values():
        g["title"] = sorted(g["names"], key=plain_first)[0]   # named by its US release if there is one
    return games


def read_releases(dat_text):
    """{four-letter code: [release names]} for the commercial releases."""
    codes = {}
    for block in re.finditer(r'^game \((.*?)^\)', dat_text, re.M | re.S):
        body = block.group(1)
        name = re.search(r'^\s*name "([^"]*)"', body, re.M)
        serial = re.search(r'^\s*serial "([^"]*)"', body, re.M)
        if not name or not serial:
            continue
        name, code = name.group(1), serial.group(1).strip().upper()
        if re.fullmatch(r"[A-Z0-9]{4}", code) and not LEFT_OUT.search(name):
            codes.setdefault(code, []).append(name)
    return codes


def plain_first(name):
    """Sort key: the US release, then Europe, then the rest; fewest extra tags first ("(Rev 1)" after none).
    The region is the first bracket: "(USA)", "(Japan, USA)", "(Europe)", "(Europe, Australia)"..."""
    regions = re.search(r"\(([^)]*)\)", name)
    regions = regions.group(1).split(", ") if regions else []
    region = 0 if "USA" in regions else 1 if "Europe" in regions else 2
    return (region, name.count("("), name)


class Thumbnails:
    """The Named_Logos folder: what is in it, and each file fetched once (cached by its git hash)."""

    def __init__(self, cache):
        self.cache = cache
        os.makedirs(cache, exist_ok=True)
        tree_path = os.path.join(cache, "tree.json")
        if not os.path.exists(tree_path):
            fetch(TREE_URL, tree_path)
        with open(tree_path, encoding="utf-8") as f:
            tree = json.load(f)
        if tree.get("truncated"):
            sys.exit("The thumbnail list came back cut short; delete build/logo_sources/tree.json and try again.")
        self.entries = {e["path"]: e for e in tree["tree"] if e["type"] == "blob"}
        self.names = {p[len(FOLDER) + 1:-4]: p for p in self.entries
                      if p.startswith(FOLDER + "/") and p.endswith(".png")}
        self.by_title = {}
        for name in sorted(self.names, key=plain_first):
            if not LEFT_OUT.search(name):
                self.by_title.setdefault(tidy_title(name), []).append(name)
        self.locks, self.locks_lock = {}, threading.Lock()

    def fetch_once(self, path, local):
        """Download path to local unless it is there; several regions link to one file, so one download at a time each."""
        with self.locks_lock:
            lock = self.locks.setdefault(local, threading.Lock())
        with lock:
            if not os.path.exists(local):
                fetch(RAW_URL + urllib.parse.quote(path), local)

    def resolve(self, path, depth=0):
        """The repository path of the picture itself: many files are git links to another region's copy."""
        entry = self.entries.get(path)
        if not entry or entry["mode"] != "120000":
            return path
        if depth > 4:
            raise RuntimeError(f"{path}: links go round in circles")
        link = os.path.join(self.cache, entry["sha"] + ".link")
        self.fetch_once(path, link)
        with open(link, encoding="utf-8") as f:
            target = f.read().strip()
        return self.resolve(posixpath.normpath(posixpath.join(posixpath.dirname(path), target)), depth + 1)

    def picture(self, path):
        """The file on disk holding this logo (fetched if need be), and its git hash, which says when two are the same."""
        real = self.resolve(path)
        entry = self.entries.get(real)
        if not entry:
            raise RuntimeError(f"{path} links to {real}, which isn't there")
        local = os.path.join(self.cache, entry["sha"] + ".png")
        self.fetch_once(real, local)
        return local, entry["sha"]


def pick(game, releases, thumbs):
    """The logo for every region ({'name', 'how'} or None) and the regions whose own differs from it
    ({code: name}, filled in once the files are known)."""
    names = sorted(game["names"], key=plain_first)
    shared = next(({"name": n, "how": "release"} for n in names if n in thumbs.names), None)
    if not shared:
        for n in names:
            found = thumbs.by_title.get(tidy_title(n))
            if found:
                shared = {"name": found[0], "how": "title"}
                break
    own = {}
    for code in sorted(game["codes"]):
        mine = sorted((n for n in releases.get(code, []) if n in thumbs.names), key=plain_first)
        if mine:
            own[code] = mine[0]
    return shared, own


def read_overrides(path):
    """logo_overrides.txt: {three-letter code: ('crop', (left, top, right, bottom)) | ('url', address) | ('none', None)}."""
    found = {}
    if not os.path.exists(path):
        return found
    with open(path, encoding="utf-8") as f:
        for number, line in enumerate(f, 1):
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split()
            code, kind, rest = parts[0].upper(), (parts[1].lower() if len(parts) > 1 else ""), parts[2:]
            if not re.fullmatch(r"[A-Z0-9]{3}", code):
                sys.exit(f"{path}, line {number}: {parts[0]} isn't the first three letters of a game code")
            if kind == "crop" and len(rest) == 4 and all(n.isdigit() for n in rest):
                found[code] = ("crop", tuple(int(n) for n in rest))
            elif kind == "url" and len(rest) == 1 and rest[0].startswith("https://"):
                found[code] = ("url", rest[0])
            elif kind == "none" and not rest:
                found[code] = ("none", None)
            else:
                sys.exit(f"{path}, line {number}: expected 'crop LEFT TOP RIGHT BOTTOM', 'url https://...' or 'none'")
    return found


def make_sprite(src, out_dir, mksprite, crop=None):
    """src (a PNG), cut to `crop` if given and then to its visible part, shrunk to fit, written as out_dir/logo.sprite."""
    from PIL import Image
    im = Image.open(src).convert("RGBA")
    if crop:
        im = im.crop(crop)
    box = im.getchannel("A").point(lambda a: 255 if a > FAINT else 0).getbbox()
    if not box:
        raise RuntimeError("the picture is empty")
    im = im.crop(box)
    scale = min(LOGO_WIDTH / im.width, LOGO_HEIGHT / im.height, 1.0)
    size = (max(1, round(im.width * scale)), max(1, round(im.height * scale)))
    if size != im.size:
        im = im.resize(size, Image.LANCZOS)    # Pillow weighs the colors by their alpha here, so the edges don't darken
    os.makedirs(out_dir, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        png = os.path.join(tmp, "logo.png")
        im.save(png)
        r = subprocess.run([mksprite, "--format", "RGBA32", "-o", tmp, png], capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(f"mksprite: {r.stderr.strip() or r.stdout.strip()}")
        shutil.move(os.path.join(tmp, "logo.sprite"), os.path.join(out_dir, LOGO_FILE))
    return size


def find_mksprite():
    found = shutil.which("mksprite")
    if found:
        return found
    path = os.path.join(os.environ.get("N64_INST", "/opt/libdragon"), "bin", "mksprite")
    return path if os.path.exists(path) else None


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", default=os.path.join(REPO, "build", "logos"))
    p.add_argument("--cache", default=os.path.join(REPO, "build", "logo_sources"))
    p.add_argument("--only", help="comma-separated codes (three or four letters) to do, such as NSM,NKTE")
    p.add_argument("--jobs", type=int, default=8, help="downloads at once (default 8)")
    p.add_argument("--dry-run", action="store_true", help="match games to logos and report; make nothing")
    p.add_argument("--overrides", default=os.path.join(HERE, "logo_overrides.txt"),
                   help="fixes for logos libretro gets wrong (default: logo_overrides.txt beside this script)")
    args = p.parse_args()
    overrides = read_overrides(args.overrides)

    mksprite = None
    if not args.dry_run:
        mksprite = find_mksprite()
        try:
            import PIL  # noqa: F401
        except ImportError:
            mksprite = None
        if not mksprite:
            sys.exit("Needs Pillow and libdragon's mksprite: run this in the dev container "
                     "(sudo apt-get install python3-pil).")

    os.makedirs(args.cache, exist_ok=True)
    dat_path = os.path.join(args.cache, "n64.dat")
    if not os.path.exists(dat_path):
        fetch(DAT_URL, dat_path)
    with open(dat_path, encoding="utf-8") as f:
        dat = f.read()
    games = read_games(dat)
    releases = read_releases(dat)
    thumbs = Thumbnails(args.cache)
    only = {c.strip().upper() for c in args.only.split(",")} if args.only else None

    plan = {}
    for base, game in sorted(games.items()):
        if only and base not in only and not (game["codes"] & only):
            continue
        plan[base] = pick(game, releases, thumbs)

    # Fetch every picture that might be used, a few at a time. Even a dry run
    # does: the links have to be followed to know which regions' logos differ.
    wanted = set()
    for shared, own in plan.values():
        if shared:
            wanted.add(FOLDER + "/" + shared["name"] + ".png")
        wanted.update(FOLDER + "/" + n + ".png" for n in own.values())
    files, failed = {}, {}

    def get(path):
        try:
            return path, thumbs.picture(path), None
        except Exception as e:
            return path, None, str(e)
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        for path, got, err in pool.map(get, sorted(wanted)):
            if got:
                files[path] = got
            else:
                failed[path] = err

    if not args.dry_run and not only:
        shutil.rmtree(os.path.join(args.out, "menu"), ignore_errors=True)    # a full run: nothing left over from an earlier one
    manifest, made, lines = [], 0, []
    for base, (shared, own) in plan.items():
        game = games[base]
        entry = {"base": base, "codes": sorted(game["codes"]), "title": game["title"],
                 "logo": None, "how": None, "regions": {}, "override": None}
        shared_src, crop = None, None
        if shared:
            shared_file = files.get(FOLDER + "/" + shared["name"] + ".png")
            if shared_file:
                entry["logo"], entry["how"] = shared["name"], shared["how"]
                shared_src = shared_file[0]
                for code, name in own.items():
                    mine = files.get(FOLDER + "/" + name + ".png")
                    if mine and mine[1] != shared_file[1]:
                        entry["regions"][code] = name

        # A fix from logo_overrides.txt replaces what was found for every region.
        kind, value = overrides.get(base, (None, None))
        if kind == "none":
            entry.update(logo=None, how="override", regions={}, override="none")
        elif kind == "url":
            local = os.path.join(args.cache, "url-" + hashlib.sha1(value.encode()).hexdigest()[:16] + ".png")
            try:
                if not os.path.exists(local):
                    fetch(value, local)
                entry.update(logo=value, how="override", override="url " + value)
                shared_src = local
            except Exception as e:
                failed[value] = str(e)
        elif kind == "crop" and entry["logo"]:
            crop = value
            entry["override"] = "crop " + " ".join(str(n) for n in value)
        manifest.append(entry)

        if not entry["logo"]:
            note = "   (logo_overrides.txt)" if entry["override"] else ""
            lines.append(f"{base}  NONE      {game['title']}{note}")
            continue
        note = {"release": "", "title": "   (same title, another name)", "override": "   (logo_overrides.txt)"}[entry["how"]]
        if crop:
            note += f"   (cut to {entry['override'][5:]}, logo_overrides.txt)"
        lines.append(f"{base}  {entry['logo']}{note}")
        for code, name in sorted(entry["regions"].items()):
            lines.append(f"  {code} {name}")
        if args.dry_run:
            continue

        meta = os.path.join(args.out, "menu", "metadata")
        jobs = [(os.path.join(meta, *base), shared_src, crop, entry["logo"])]
        jobs += [(os.path.join(meta, *code), files[FOLDER + "/" + name + ".png"][0], None, name)
                 for code, name in entry["regions"].items()]
        for out_dir, src, cut, name in jobs:
            try:
                make_sprite(src, out_dir, mksprite, cut)
                made += 1
            except Exception as e:
                lines.append(f"  could not convert {name}: {e}")

    missing = [e for e in manifest if not e["logo"]]
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "logos.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=1, ensure_ascii=False)
    report = os.path.join(args.out, "logos_report.txt")
    with open(report, "w", encoding="utf-8") as f:
        f.write(f"{len(manifest) - len(missing)} of {len(manifest)} games have a logo"
                f" ({sum(len(e['regions']) for e in manifest)} regions with their own).\n\n")
        f.write("\n".join(lines) + "\n")
        if failed:
            f.write("\nCould not fetch:\n" + "".join(f"  {p}: {e}\n" for p, e in sorted(failed.items())))
    if args.dry_run:
        print(f"{len(manifest) - len(missing)} of {len(manifest)} games matched. Report: {report}")
    else:
        print(f"{made} logos made for {len(manifest) - len(missing)} of {len(manifest)} games. Report: {report}")


if __name__ == "__main__":
    main()
