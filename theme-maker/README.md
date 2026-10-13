# Theme Maker

A single-page web tool for designing themes for the carousel menu. It shows a
preview of the menu's 640x480 layout and exports a `theme.ini`, or a share
code that can be pasted back into the tool.

- `theme-maker.html` is the whole tool: markup, styles and script in one file,
  with no build step.
- The live copy is published as a claude.ai artifact:
  https://claude.ai/artifact/HxMPCpbZTSRZURJfWswAma
  This file is the source for that page. To update the page, publish this file
  to that same address so the link stays the same.
- The file is the page's content only, without `<!doctype>`, `<html>`, `<head>`
  or `<body>` tags; the publishing step wraps it. It still opens directly in a
  browser for a quick look.
- The "Save as file" buttons only appear on the published page. Opened as a
  local file, use the Copy buttons instead.

When a feature switch or a color key is added to the menu, add it here too:

- `FRAME_KEYS` / `FRAME_INI` for `[colors]` keys.
- `FEATURES` for `[features]` keys. The order of that list is the bit order in
  share codes, so only ever add to the end. The first eight fit in a v2 code;
  later ones need v3, which has room for sixteen.
  Fifteen are used (2026-10-13). Leave out switches that are personal habits
  and not part of a look (Remember Position, Count Plays, Back Up Saves).
- `PRESETS` starts with the four themes built into the menu; keep them the
  same as `src/menu/builtin_themes.c`.
- `BG_TYPES` for background types (two bits in a share code: all four used).
- The preview (`drawTopbar`, `drawGames`, `drawFolders`, `drawHints`) uses the
  console's own layout numbers from `games_ui.c`, `browser.c` and
  `folders_ui.c`. When the menu's layout changes, change them here too.
- The ocean background (`drawOcean`) is the same sums as `theme.c`.

There is no browser in the dev container. `tests/run.sh` checks the share
codes with Node.js; the preview was last checked by running the page's
script against stand-in page objects and replaying what it drew with Pillow.

The `theme.ini` keys and the share code formats are documented in `CLAUDE.md`
at the top of the repository.
