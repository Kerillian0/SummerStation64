#!/usr/bin/env python3
"""Builds a SummerStation64 release in one go.

    make release VERSION=0.2        (or: python3 scripts/make_release.py 0.2)

Writes output/release-v<VERSION>/:
  sc64menu.n64                          the menu (game details built in)
  SummerStation64-v<VERSION>-Covers-Logos-and-Clips.zip  everything for the SD
                                        card in one: menu/art.pak (the box
                                        art) and the title logos and preview
                                        clips (menu/metadata/...)
  SHA1SUMS.txt                          checksums, for the release notes

Since v0.5 there is one menu file and no editions (the owner, 2026-11-14:
a Standard edition only confused players). The covers are not inside the
menu file: the SummerCart64 must load that whole file within 5 seconds, and
with 32 MB of art slow cards stopped with "Watchdog timeout"; the release
refuses a menu file over MENU_MAX_BYTES. The art pack is made from the
owner's baked pictures (build/art, make_baked_art.py), or taken from
output/menu/art.pak. The details are baked again from assets/boxart/source
first. Afterwards the next plain `make sc64` builds the usual working copy.

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
ART_SPRITES = os.path.join(ROOT, "build", "art")                 # make_baked_art.py's pictures
ART_PACK = os.path.join(ROOT, "output", "menu", "art.pak")       # make_art_pack.py's pack
OLD_ART = os.path.join(ROOT, "filesystem", "art")                # where the art went before v0.5
# The cart's bootloader reads the whole menu file within 5 seconds: at 1.5 MB
# a second (a slow, fragmented card) 6 MB still fits. The menu was 3.3 MB at v0.5.
MENU_MAX_BYTES = 6 * 1024 * 1024
# The assistant's skins (assets/assistant/make_skins.py): the owner's own, never in a release
# (neither edition); they are set aside for both builds and put back afterwards.
SKINS = os.path.join(ROOT, "filesystem", "assistant")
SKINS_ASIDE = os.path.join(ROOT, "build", "assistant-set-aside")
# The owner's profile pictures (assets/profiles/make_avatars.py: Nintendo
# characters), likewise the owner's own builds only since v0.5 (2026-11-14).
AVATARS = os.path.join(ROOT, "filesystem", "avatars")
AVATARS_ASIDE = os.path.join(ROOT, "build", "avatars-set-aside")
BUILT = os.path.join(ROOT, "output", "sc64menu.n64")
PACK_SOURCES = [os.path.join(ROOT, "build", "logos", "menu"), os.path.join(ROOT, "build", "previews", "menu")]
PACK_NOTICE = """Covers, logos and clips for SummerStation64

Unzip this at the top of the SD card, beside sc64menu.n64 (the menu folder
inside goes over the card's menu folder; nothing of yours is replaced):
  menu/art.pak          box art for about 750 games, read a cover at a time
  menu/metadata/...     title logos (Game info shows them in place of the
                        name) and short preview clips (with the Expansion
                        Pak, in place of the cover)

Without them the menu still works: covers then come from your own
menu/metadata pictures, and Game info shows the plain name and cover.

These pictures and clips are not part of SummerStation64's source code and
not covered by its licence. They belong to the games' publishers and are
included, in reduced size, for use with the menu on your own console. The
box art comes from the community n64-flashcart-menu-metadata pack, the logos
from libretro's thumbnail collection (libretro-thumbnails), and the clips
are cut from gameplay recordings kept on archive.org. If you hold the rights
to any of them and want them removed, open an issue at
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
    """Stop if a skin or a profile picture made it into a release file: each
    skin.txt starts "skin 1" then its name; the pictures' list.txt names its
    first one "a00" followed by a tab."""
    with open(path, "rb") as f:
        data = f.read()
        if b"skin 1\nname " in data or b"skin 1\r\nname " in data:
            sys.exit(f"The assistant's skins ended up in {edition}: not releasing it.")
        if b"a00\t" in data:
            sys.exit(f"The owner's profile pictures ended up in {edition}: not releasing it.")


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
    pack = os.path.join(out, f"SummerStation64-v{version}-Covers-Logos-and-Clips.zip")
    menu_file = os.path.join(out, "sc64menu.n64")

    if os.path.isdir(OLD_ART):
        sys.exit(f"{OLD_ART} would put the art back inside the menu file: move it to {ART_SPRITES} first.")
    if not os.path.isdir(ART_SPRITES) and not os.path.isfile(ART_PACK):
        sys.exit("No baked art: run  python3 assets/boxart/make_baked_art.py  first.")
    if os.path.isdir(SKINS_ASIDE):
        sys.exit(f"The assistant's skins are in {SKINS_ASIDE} from an earlier run that stopped: move them back to {SKINS} first.")
    if os.path.isdir(AVATARS_ASIDE):
        sys.exit(f"The profile pictures are in {AVATARS_ASIDE} from an earlier run that stopped: move them back to {AVATARS} first.")
    changed = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"], cwd=ROOT,
                             capture_output=True, text=True).stdout.strip()
    if changed:
        print("Note: these files have changes not committed; the release is built from them as they are:\n" + changed)
    if os.path.isdir(out):
        sys.exit(f"{out} already exists: move it away or delete it first, so a published release isn't overwritten.")
    os.makedirs(out)

    say("Baking the game details")
    subprocess.run([sys.executable, "assets/boxart/make_baked_meta.py"], cwd=ROOT, check=True)

    if os.path.isdir(ART_SPRITES):
        say("Packing the box art")
        subprocess.run([sys.executable, "assets/boxart/make_art_pack.py", ART_SPRITES, ART_PACK], cwd=ROOT, check=True)

    skins = os.path.isdir(SKINS)
    if skins:
        os.rename(SKINS, SKINS_ASIDE)   # Navi and Tatl: the owner's builds only, never released
    avatars = os.path.isdir(AVATARS)
    if avatars:
        os.rename(AVATARS, AVATARS_ASIDE)   # the owner's profile pictures: likewise
    try:
        say(f"The menu, v{version}")
        build(version)
        no_skins(BUILT, "the menu file")
        size = os.path.getsize(BUILT)
        if size > MENU_MAX_BYTES:
            sys.exit(f"The menu file is {size / 1024 / 1024:.1f} MB, over {MENU_MAX_BYTES // (1024 * 1024)} MB: slow cards "
                     "would stop with \"Watchdog timeout\". Move what grew onto the card.")
        shutil.copyfile(BUILT, menu_file)
    finally:
        if skins:
            os.rename(SKINS_ASIDE, SKINS)
        if avatars:
            os.rename(AVATARS_ASIDE, AVATARS)
        forget_stale()      # the next plain build is the usual working copy (with the skins), not named v<version>

    say("Covers, logos and clips")
    count = 0
    with zipfile.ZipFile(pack, "w") as z:
        z.writestr("COVERS-LOGOS-AND-CLIPS-README.txt", PACK_NOTICE, compress_type=zipfile.ZIP_DEFLATED)
        # The art packs well; sprites and H.264 clips hardly shrink, so they are stored.
        z.write(ART_PACK, "menu/art.pak", compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
        sources = [source for source in PACK_SOURCES if os.path.isdir(source)]
        for source in sources:
            for folder, _, files in os.walk(source):
                for name in sorted(files):
                    path = os.path.join(folder, name)
                    z.write(path, os.path.join("menu", os.path.relpath(path, source)), compress_type=zipfile.ZIP_STORED)
                    count += 1
    print(f"  art.pak and {count} logo and clip files")
    if not sources:
        print("Note: no logos or clips made (build/logos, build/previews): the pack holds the box art only.")

    with open(os.path.join(out, "SHA1SUMS.txt"), "w", newline="\n") as f:
        f.write(f"{sha1(menu_file)}  sc64menu.n64\n")
        f.write(f"{sha1(pack)}  SummerStation64-v{version}-Covers-Logos-and-Clips.zip\n")
        f.write(f"{sha1(ART_PACK)}  menu/art.pak (inside the zip)\n")

    say("Done")
    for name in sorted(os.listdir(out)):
        print(f"  {name:45} {os.path.getsize(os.path.join(out, name)) / (1024 * 1024):6.1f} MB")
    print(f"\nIn {out}. output/sc64menu.n64 is the release menu (no skins or profile pictures); test it with output/menu/art.pak before publishing.")
    if skins:
        print("Your own build with Navi and Tatl: run  make sc64  again.")


main()
