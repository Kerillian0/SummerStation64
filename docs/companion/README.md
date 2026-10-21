# SummerStation64 Companion

One web page, `index.html`, with three tabs: **Theme Maker**, **Card Prep**
and **Guide**. Published with GitHub Pages from this folder
(Settings > Pages: branch `summerstation`, folder `/docs`), at
`https://kerillian0.github.io/SummerStation64/companion/`. Each tab has its
own address: `#themes`, `#prep`, `#guide` (and `#guide-setup`,
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

## Guide

Plain HTML in the `#guide` tab: SD card setup, hardware, tools and
troubleshooting. Keep it in step with the menu's own wording (menu entries
and settings are named exactly as on the console).

## Tests

There is no browser in the dev container. `sh tests/run.sh` runs the Theme
Maker's share codes and Card Prep's working code (between `// --- logic` and
`// --- end logic ---`) in Node.js, taken straight out of this page. The parts
that need a browser (the preview, picking a folder, saving) are checked by
opening the page.
