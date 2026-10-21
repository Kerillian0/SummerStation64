# Game data

`gamedata.txt` is built into the menu file and gives, per game code, the genre,
the most players at once, and how they play together (co-op, versus).

- **Genre and players** come from
  [libretro-database](https://github.com/libretro/libretro-database)
  (`metadat/genre`, `metadat/maxusers`, joined to game codes through
  `metadat/no-intro`), licensed
  [CC-BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).
  `gamedata.txt` is a work adapted from it and is shared under the same
  licence. Credited on the menu's credits screen (the library pop-up) and in
  the README.
- **How they play together**, and a few corrections where libretro is plainly
  wrong, come from `play_modes.txt`, written for this project.

Rebuild after changing `play_modes.txt` (needs the internet):

```sh
python3 assets/data/make_game_data.py
```

The build copies `gamedata.txt` into the menu file as `rom:/gamedata.txt`
(`src/menu/game_data.c` reads it, about 6 KB in memory).
