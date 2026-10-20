# SummerStation64 Art Edition

The [SummerStation64](https://github.com/Kerillian0/SummerStation64) menu
for the SummerCart64, with box covers built in. It is the same menu as the
Standard edition; the covers load faster because they are inside the menu
file instead of being read from the SD card.

This repository holds **release files only**. The menu's source code, its
release notes and the full install guide are in the
[main SummerStation64 repository](https://github.com/Kerillian0/SummerStation64).

## Installing

1. Download the `.zip` from the [Releases](../../releases) page and unzip
   it to get `sc64menu.n64` (about 28 MB).
2. Back up the `sc64menu.n64` on your SD card, then copy the new one to
   the top folder of the card, replacing it. The name must stay exactly
   `sc64menu.n64`.
3. Recommended: also put the
   [metadata pack](https://github.com/n64-tools/n64-flashcart-menu-metadata/releases)
   in `sd:/menu/metadata/`. It gives each game its description, release
   date, developer and player count.

The full step-by-step guide is in the main repository's release notes.

## Source code

The menu's code is licensed under the GNU Affero General Public License v3.
The source for each Art Edition release is the main repository at the same
version tag (for example `v0.1`). The covers are built in with
`assets/boxart/make_baked_art.py` from that repository.

## About the box art

The covers are not covered by the menu's licence. They belong to the games'
publishers and are included unchanged in size-reduced form, for use with
the menu on your own console. They come from the community
[n64-flashcart-menu-metadata](https://github.com/n64-tools/n64-flashcart-menu-metadata)
pack (scans credited there to ABeezy, LaunchBox community).

If you hold the rights to any of these pictures and want them removed,
open an issue in this repository and they will be taken out.

SummerStation64 is an unofficial fan project, not made or approved by
Nintendo. Nintendo 64 is a trademark of Nintendo.
