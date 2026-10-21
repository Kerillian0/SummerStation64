# Card Prep

A web page that prepares an SD card for SummerStation64. One file, no build
step, nothing installed: open `prep-tool.html` in Chrome or Edge.

What it does now:

1. You pick the card's top folder.
2. It downloads No-Intro's Nintendo 64 list from
   [libretro-database](https://github.com/libretro/libretro-database)
   (CC-BY-SA 4.0), or you load the `.dat` file yourself.
3. It reads every `.z64`, `.v64` and `.n64` on the card (not `menu/` or
   `saves/`), puts it in the console's byte order and works out its CRC-32.
   Nothing leaves the browser.
4. It writes `menu/verified.txt` on the card. Firefox and Safari can't write
   to a folder, so they offer the file as a download to copy over.

`menu/verified.txt`: the line `verified 1`, then one line per game:

```
<path key> <size> <crc32> <result> <No-Intro name>
5f3a9c21 8388608 3ce60709 V Super Mario 64 (USA)
```

- **path key**: FNV-1a of the full path as the menu writes it
  (`sd:/N64(US)/Game.z64`), the same sum as `path_key()` in
  `src/menu/game_index.c`.
- **result**: `V` a known good dump, `M` a game the list knows but not this
  file (hack, translation, patched or bad dump), `U` a game the list
  doesn't know (homebrew, prototypes).

A second run reuses the CRC of every file whose path and size are unchanged,
so only new games are read.

Tests: the page's working code sits between `// --- logic` and
`// --- end logic ---` and is run in Node.js by `tests/prep_tool_test.js`
(`sh tests/run.sh`). The parts that need a browser (picking the folder,
writing the file) can only be checked by opening the page.

Planned: building `gameindex.txt` ahead of time, and baking box art for the
Art Edition. Later it becomes part of the companion app.
