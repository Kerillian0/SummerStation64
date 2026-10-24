# SummerStation64 Companion

One web page, `index.html`, with five tabs: **Theme Maker**, **Card Prep**,
**Hacks**, **Missing Art** and **Guide**. Published with GitHub Pages from this folder
(Settings > Pages: branch `summerstation`, folder `/docs`), at
`https://kerillian0.github.io/SummerStation64/companion/`. Each tab has its
own address: `#themes`, `#prep`, `#hacks`, `#art`, `#guide` (and `#guide-setup`,
`#guide-hardware`, `#guide-tools`, `#guide-help`).

Markup, styles and scripts are all in the one file, with no build step. It
also opens straight from disk in a browser for a quick look.

## Theme Maker

Designs a theme for the menu with a preview of its 640x480 screens and exports
a `theme.ini` (or a share code that can be pasted back in). On GitHub Pages,
"Save as file" downloads `theme.ini`; inside a claude.ai page it uses the
host's save prompt (the old published copy:
https://claude.ai/artifact/HxMPCpbZTSRZURJfWswAma).

When a feature switch or a color key is added to the menu, add it here too:

- `FRAME_KEYS` / `FRAME_INI` for `[colors]` keys.
- `FEATURES` for `[features]` keys. The order of that list is the bit order in
  share codes, so only ever add to the end. The first eight fit in a v2 code;
  later ones need v3, which has room for sixteen. Fifteen are used. Leave out
  switches that are personal habits and not part of a look.
- `PRESETS` starts with the six themes built into the menu (Midnight, the
  default, first); keep them the same as `src/menu/builtin_themes.c`.
- `BG_TYPES` for background types (two bits in a share code: all four used).
- The preview (`drawTopbar`, `drawGames`, `drawFolders`, `drawHints`) uses the
  console's own layout numbers from `games_ui.c`, `browser.c` and
  `folders_ui.c`. When the menu's layout changes, change them here too.
- The ocean background (`drawOcean`) is the same sums as `theme.c`.

The `theme.ini` keys are read by `src/menu/theme_parse.c`; the share code
formats are written and read by `encodeTheme()` / `decodeTheme()`.

## Card Prep

Checks every game on the SD card against No-Intro's Nintendo 64 list and
writes `menu/verified.txt`, which the menu reads for its dump check marks
(`src/menu/dump_check.c`).

1. The player picks the card's top folder (Chrome and Edge can then save onto
   it; other browsers read it and offer the file as a download).
2. The list is downloaded from
   [libretro-database](https://github.com/libretro/libretro-database)
   (CC-BY-SA 4.0) when the page opens, or loaded from a `.dat` file.
3. Every `.z64`, `.v64` and `.n64` (not in `menu/` or `saves/`) is put in the
   console's byte order and its CRC-32 worked out. Nothing leaves the browser.
4. `menu/verified.txt`: the line `verified 1`, then one line per game:

```
<path key> <size> <crc32> <result> <No-Intro name>
5f3a9c21 8388608 3ce60709 V Super Mario 64 (USA)
```

- **path key**: FNV-1a of the full path as the menu writes it
  (`sd:/N64(US)/Game.z64`), the same sum as `path_key()` in
  `src/menu/game_index.c`.
- **result**: `V` a known good dump, `M` a game the list knows but not this
  file, `U` a game the list doesn't know (homebrew, prototypes).

A second run reuses the CRC of every file whose path and size are unchanged.

## Hacks

Gives a hack a game code of its own, so the menu shows it with its own name,
cover and description instead of those of the game it was made from (see
`src/menu/hack_games.h`).

1. The player picks the card's top folder. Each game's code is read from its
   header; `menu/hacks.txt`, `menu/verified.txt` and the hack folders already
   on the card (`menu/metadata/X/...`) are read too.
2. Games that share a code with another file are listed (or every game).
3. For the chosen file: name, maker, release date, players, short and full
   description, a code (suggested: X, two letters from the name, E; codes
   already on the card are refused) and a cover, cut in the browser to the
   pack's size (158x112, or 112x158 for a tall box).
4. Saving writes each hack's folder (`metadata.ini`, `description.txt`,
   `boxart_front.png`), then `menu/hacks.txt`:

```
hacks 1
; size crc32 code file name   (a CRC of 0: match by file name and size)
16777216 3b780fd0 XMLE Legend of Zelda, The Missing Link (U) (V1.3).z64
```

The CRC is the same as Card Prep's (the console's byte order), taken from
`verified.txt` when it has the file, otherwise worked out. Browsers that
can't write to the card get a `.zip` (stored, no packing) to unzip onto it.
Removing a hack takes its line out of `hacks.txt` and leaves its folder.

**Known hacks (work in progress).** Step 3 offers a list of popular hacks
(`assets/data/popular_hacks.txt`: one `[Name]` block each with its base game,
a fixed code, maker, year, players, Expansion Pak and real-N64 notes, and the
words its file names usually hold). A file whose name holds an entry's words,
made from the same game, gets that entry suggested; choosing one fills in the
name, maker, players and code. The page holds a copy of the list (so it works
opened from disk): after changing the list run
`python3 scripts/embed_popular_hacks.py`; the tests check the two match. The
compatibility notes are still being checked, and the page says so.

## Missing Art

Fills in what the menu's own check (Settings > Library > Find Missing Art)
found missing, listed in `menu/missingart.txt`: one line per game, tab-separated:
its folder under `menu/metadata/`, kind (game, hack, homebrew), what it lacks
(front, back, top, cartridge, description, date, developer) and its file. For
each game the page offers only what is missing: pictures chosen from the
player's own files (the front and back cut to the cover's size like the Hacks
tab's, the top label and cartridge fitted inside 158 x 158), the developer,
the release date and a description (written to `description.txt`, with the
start of it as `short-desc`). An existing `metadata.ini` keeps everything
else; a developer keeps the publisher after its `|`.

Everything goes in the folder with the region letter (`N/S/M/E`), the only
one the menu reads details from. If the game was using the folder without
the letter (`N/S/M`, with no single-region folders beside it), its files are
copied along, or the menu would stop finding them. Homebrew shares its game
code with unrelated games, so its pictures go in `homebrew/<title>/` and its
details in `Game.metadata.ini` beside its file (the only place Game info reads
them for homebrew), with the description as a longer `short-desc` (the menu
keeps 255 characters of a value). Pictures and details baked
into the menu file are not on the list (they count as there).

## Guide

Plain HTML in the `#guide` tab: SD card setup, hardware, tools and
troubleshooting. Keep it in step with the menu's own wording (menu entries
and settings are named exactly as on the console).

## Tests

There is no browser in the dev container. `sh tests/run.sh` runs the Theme
Maker's share codes, Card Prep's working code (between `// --- logic` and
`// --- end logic ---`), the Hacks tab's (`// --- hacks logic`) and the Missing Art
tab's (`// --- art logic`) in Node.js, taken straight out of this page. The parts
that need a browser (the preview, picking a folder, saving) are checked by
opening the page.
