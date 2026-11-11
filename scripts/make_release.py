#!/usr/bin/env python3
"""Builds both release editions of SummerStation64 in one go.

    make release VERSION=0.2        (or: python3 scripts/make_release.py 0.2)

Writes output/release-v<VERSION>/:
  sc64menu.n64                          Art Edition: covers and details built in
  SummerStation64-v<VERSION>-Standard.zip  sc64menu.n64 with the details only
  SummerStation64-v<VERSION>-Logos-and-Clips.zip  title logos and preview clips
                                        for the SD card (when they have been made)
  SHA1SUMS.txt                          checksums, for the release notes

Needs the owner's art already baked (filesystem/art, make_baked_art.py); the
details are baked again from assets/boxart/source first. The art folder is
set aside for the Standard build and always put back, even if a build fails.
Afterwards the next plain `make sc64` builds the usual working copy again.

The logos and clips (scripts/fetch_logos.py, scripts/fetch_preview_clips.py,
scripts/make_preview_clip.py, in build/logos and build/previews) are far too
big for the menu file, which the cart must load within 5 seconds, so they
go in a pack of their own that players unzip onto the card.
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
# The assistant's skins (assets/assistant/make_skins.py): the owner's own, never in a release
# (neither edition); they are set aside for both builds and put back afterwards.
SKINS = os.path.join(ROOT, "filesystem", "assistant")
SKINS_ASIDE = os.path.join(ROOT, "build", "assistant-set-aside")
BUILT = os.path.join(ROOT, "output", "sc64menu.n64")
PACK_SOURCES = [os.path.join(ROOT, "build", "logos", "menu"), os.path.join(ROOT, "build", "previews", "menu")]
PACK_NOTICE = """Title logos and preview clips for SummerStation64

Unzip this at the top of the SD card: the files go into menu/metadata/,
beside the box art. Game info then shows each game's logo in place of its
name, and (with the Expansion Pak) a short clip in place of its cover.

These pictures and clips are not part of SummerStation64's source code and
not covered by its licence. They belong to the games' publishers and are
included, in reduced size, for use with the menu on your own console. The
logos come from libretro's thumbnail collection (libretro-thumbnails); the
clips are cut from gameplay recordings kept on archive.org. If you hold the
rights to any of them and want them removed, open an issue at
https://github.com/Kerillian0/SummerStation64 and the pack will be replaced
without them.
"""
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


def no_skins(path, edition):
    """Stop if a skin made it into a release file: each skin.txt starts "skin 1" then its name."""
    with open(path, "rb") as f:
        data = f.read()
        if b"skin 1\nname " in data or b"skin 1\r\nname " in data:
            sys.exit(f"The assistant's skins ended up in {edition}: not releasing it.")


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
    if os.path.isdir(SKINS_ASIDE):
        sys.exit(f"The assistant's skins are in {SKINS_ASIDE} from an earlier run that stopped: move them back to {SKINS} first.")
    changed = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"], cwd=ROOT,
                             capture_output=True, text=True).stdout.strip()
    if changed:
        print("Note: these files have changes not committed; the release is built from them as they are:\n" + changed)
    if os.path.isdir(out):
        sys.exit(f"{out} already exists: move it away or delete it first, so a published release isn't overwritten.")
    os.makedirs(out)

    say("Baking the game details")
    subprocess.run([sys.executable, "assets/boxart/make_baked_meta.py"], cwd=ROOT, check=True)

    skins = os.path.isdir(SKINS)
    if skins:
        os.rename(SKINS, SKINS_ASIDE)   # Navi and Tatl: the owner's builds only, never released
    try:
        say(f"Standard edition (no covers), v{version}")
        os.rename(ART, ART_ASIDE)
        try:
            build(version)
            no_skins(BUILT, "the Standard edition")
            with zipfile.ZipFile(standard_zip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
                z.write(BUILT, "sc64menu.n64")
        finally:
            os.rename(ART_ASIDE, ART)

        say(f"Art Edition (covers built in), v{version}")
        build(version)
        no_skins(BUILT, "the Art Edition")
        shutil.copyfile(BUILT, art_rom)
    finally:
        if skins:
            os.rename(SKINS_ASIDE, SKINS)
        forget_stale()      # the next plain build is the usual working copy (with the skins), not named v<version>

    pack = os.path.join(out, f"SummerStation64-v{version}-Logos-and-Clips.zip")
    sources = [s for s in PACK_SOURCES if os.path.isdir(s)]
    if sources:
        say("Logos and clips pack")
        count = 0
        # Stored, not packed again: sprites and H.264 clips hardly shrink.
        with zipfile.ZipFile(pack, "w", zipfile.ZIP_STORED) as z:
            z.writestr("LOGOS-AND-CLIPS-NOTICE.txt", PACK_NOTICE)
            for source in sources:
                for folder, _, files in os.walk(source):
                    for name in sorted(files):
                        path = os.path.join(folder, name)
                        z.write(path, os.path.join("menu", os.path.relpath(path, source)))
                        count += 1
        print(f"  {count} files")
    else:
        print("Note: no logos or clips made (build/logos, build/previews): no pack this time.")

    with open(os.path.join(out, "SHA1SUMS.txt"), "w", newline="\n") as f:
        f.write(f"{sha1(art_rom)}  sc64menu.n64 (Art Edition)\n")
        with zipfile.ZipFile(standard_zip) as z:
            standard_sha = hashlib.sha1(z.read("sc64menu.n64")).hexdigest()
        f.write(f"{standard_sha}  sc64menu.n64 in SummerStation64-v{version}-Standard.zip\n")
        if os.path.exists(pack):
            f.write(f"{sha1(pack)}  SummerStation64-v{version}-Logos-and-Clips.zip\n")

    say("Done")
    for name in sorted(os.listdir(out)):
        print(f"  {name:45} {os.path.getsize(os.path.join(out, name)) / (1024 * 1024):6.1f} MB")
    print(f"\nIn {out}. output/sc64menu.n64 is the Art Edition (no skins); test it before publishing.")
    if skins:
        print("Your own build with Navi and Tatl: run  make sc64  again.")


main()
