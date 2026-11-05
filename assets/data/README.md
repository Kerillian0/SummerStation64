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

## Series

`series.txt` is built into the menu file too, for the Collections tab: each
series (The Legend of Zelda, Mario Party, Banjo, ...) with the exact game
codes of its games in every region.

- **Which series there are, and how their games are recognised** (a regular
  expression on the game's name) is `series_list.txt`, written for this
  project. Edit it to add or change a series; a game in two series goes in
  the first, and a series needs two games or more.
- **The game names and codes** come from libretro-database's No-Intro list
  (`metadat/no-intro`), CC-BY-SA 4.0, so `series.txt` is shared under that
  licence as well.

Rebuild after changing `series_list.txt` (needs the internet):

```sh
python3 assets/data/make_series.py
```

The build copies it into the menu file as `rom:/series.txt`
(`src/menu/collections.c` reads it, about 2 KB in memory).
