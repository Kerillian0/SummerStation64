# Assistant skins

The menu's assistant is Spark, its own fairy (`assets/images/make_fairy.py`).
A skin swaps in another fairy: its picture, name, color and lines about
its own games. Skins are built into the menu file by
`make_skins.py`, and only into the Art Edition: `scripts/make_release.py`
leaves them out of the Standard edition.

**The skins' pictures and lines are never committed.** `source/` and
`filesystem/assistant/` are ignored by git, like the box art.

## A skin

A folder in `source/`, for example `source/navi/`, holding:

- `picture.png`: the fairy, with a see-through background. Any size; it is
  trimmed and fitted into 56 x 56. The menu turns it to face one way, then
  the other, now and then.
- `skin.txt`:

```
skin 1
name Navi
color 6CB4FF
games CZL NZL
e An eager line about its games.
s A snarky line about its games.
```

- `name`: shown before its words in the hint band.
- `color`: six hex digits; the edge of its text box and its sparkles.
- `games`: its home games, by the first three letters of their game codes
  (so every region and its hacks count). Resting on one of these for a few
  seconds, the fairy says one of its lines; while the menu is idle they
  join the lines it rotates through.
- `e` and `s` lines: what it says about its games in the Eager and the
  Snarky mood, one line each (about 110 characters fit in two rows of the
  band).
- `on <key> e|s <line>`: what it says about another game. The key is the
  first three letters of that game's code (`on NZS e ...`), or `zelda` for
  any game with "Zelda" in its name that isn't one of its own.
- `open e|s <line>`: said when one of its own games' info page opens.
- `back e|s <line>`: said when you come back from playing one of its own
  games.

At most 72 lines in all per skin. Games the skin has no line for get the
menu's own lines for well-known games (`game_lines` in
`src/menu/assistant.c`).

Then run `python3 assets/assistant/make_skins.py` and build. Settings >
Display > Assistant Character lists Spark first, then the skins in
folder-name order.
