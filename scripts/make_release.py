#!/usr/bin/env python3
"""Builds both release editions of SummerStation64 in one go.

    make release VERSION=0.2        (or: python3 scripts/make_release.py 0.2)

Writes output/release-v<VERSION>/:
  sc64menu.n64                          Art Edition: covers and details built in
  SummerStation64-v<VERSION>-Standard.zip  sc64menu.n64 with the details only
  SHA1SUMS.txt                          checksums of both, for the release notes

Needs the owner's art already baked (filesystem/art, make_baked_art.py); the
details are baked again from assets/boxart/source first. The art folder is
set aside for the Standard build and always put back, even if a build fails.
Afterwards the next plain `make sc64` builds the usual working copy again.
Never run `make clean` near a release: it deletes output/.
"""
import hashlib
import os
import shutil
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "filesystem", "art")
ART_ASIDE = os.path.join(ROOT, "build", "art-set-aside")
BUILT = os.path.join(ROOT, "output", "sc64menu.n64")
STALE = [os.path.join(ROOT, "build", "N64FlashcartMenu.dfs"),        # the ROM's files, packed again
         os.path.join(ROOT, "build", "menu", "views", "credits.o")]  # carries the version name


def say(text):
    print(f"\n== {text}", flush=True)


def forget_stale():
    for path in STALE:
        if os.path.exists(path):
            os.remove(path)


def build(version):
    forget_stale()
    subprocess.run(["make", "sc64", f'MENU_VERSION="SummerStation64 v{version}"'], cwd=ROOT, check=True)
    if not os.path.isfile(BUILT):
        sys.exit("The build left no output/sc64menu.n64.")


def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for piece in iter(lambda: f.read(1 << 20), b""):
            h.update(piece)
    return h.hexdigest()


def main():
    if len(sys.argv) != 2 or not sys.argv[1].strip():
        sys.exit("Which version? e.g.  make release VERSION=0.2")
    version = sys.argv[1].strip().lstrip("vV")
    out = os.path.join(ROOT, "output", f"release-v{version}")
    standard_zip = os.path.join(out, f"SummerStation64-v{version}-Standard.zip")
    art_rom = os.path.join(out, "sc64menu.n64")

    if not os.path.isdir(ART):
        if os.path.isdir(ART_ASIDE):
            sys.exit(f"The art is in {ART_ASIDE} from an earlier run that stopped: move it back to {ART} first.")
        sys.exit("No baked art in filesystem/art: run  python3 assets/boxart/make_baked_art.py  first.")
    changed = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"], cwd=ROOT,
                             capture_output=True, text=True).stdout.strip()
    if changed:
        print("Note: these files have changes not committed; the release is built from them as they are:\n" + changed)
    if os.path.isdir(out):
        sys.exit(f"{out} already exists: move it away or delete it first, so a published release isn't overwritten.")
    os.makedirs(out)

    say("Baking the game details")
    subprocess.run([sys.executable, "assets/boxart/make_baked_meta.py"], cwd=ROOT, check=True)

    say(f"Standard edition (no covers), v{version}")
    os.rename(ART, ART_ASIDE)
    try:
        build(version)
        with zipfile.ZipFile(standard_zip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            z.write(BUILT, "sc64menu.n64")
    finally:
        os.rename(ART_ASIDE, ART)

    say(f"Art Edition (covers built in), v{version}")
    build(version)
    shutil.copyfile(BUILT, art_rom)
    forget_stale()      # the next plain build is the usual working copy, not named v<version>

    with open(os.path.join(out, "SHA1SUMS.txt"), "w", newline="\n") as f:
        f.write(f"{sha1(art_rom)}  sc64menu.n64 (Art Edition)\n")
        with zipfile.ZipFile(standard_zip) as z:
            standard_sha = hashlib.sha1(z.read("sc64menu.n64")).hexdigest()
        f.write(f"{standard_sha}  sc64menu.n64 in SummerStation64-v{version}-Standard.zip\n")

    say("Done")
    for name in sorted(os.listdir(out)):
        print(f"  {name:45} {os.path.getsize(os.path.join(out, name)) / (1024 * 1024):6.1f} MB")
    print(f"\nIn {out}. output/sc64menu.n64 is the Art Edition; test it before publishing.")


main()
