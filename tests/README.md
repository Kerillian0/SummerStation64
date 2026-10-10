# PC-side tests

Checks that run on the PC, not the console, for the two pieces of the
project that read what people type or share:

- `theme_parse_test.c` — the `theme.ini` reader (`src/menu/theme_parse.c`):
  a full file as the Theme Maker writes it, bad values that must leave the
  defaults alone, number limits, comments, spacing, case, Windows line
  endings, keys in the wrong section and a missing file. It is built with
  the PC's own compiler; `stubs/libdragon.h` stands in for the console's
  library, which the reader only needs for its color type.
- `share_code_test.js` — the Theme Maker's share codes. The code is taken
  straight out of `theme-maker/theme-maker.html`, so the test checks the
  page itself: codes of all three versions load back the same, the right
  version is chosen, the check sum matches its published value, typing
  slips the alphabet allows are forgiven, and damaged codes are refused
  (every single-character typo is tried).

Run both with:

    sh tests/run.sh

Needs `gcc` (in the dev container already) and Node.js
(`sudo apt-get install -y nodejs`). Run them after changing either file.
