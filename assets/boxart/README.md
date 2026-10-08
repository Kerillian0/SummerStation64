# Baking box art into the menu

The menu can carry box art inside its own file, so covers appear without
searching the SD card and without unpacking a PNG each time. Nothing is
baked in by default, and no art is kept in this repository: you bake your
own.

1. Copy your art folder from the SD card (`menu/metadata`) into
   `assets/boxart/source/`, so that you have, for example,
   `assets/boxart/source/N/Z/S/E/boxart_front.png`.
   The `source` folder is ignored by git.
2. In the dev container, from the project's top folder, run
   `python3 assets/boxart/make_baked_art.py`
   (needs Pillow: `sudo apt-get install python3-pil`).
   It writes one small file per picture under `filesystem/art/`
   (also ignored by git) and prints how many it made and their total size.
3. Build as usual: `make sc64`.

To go back to no baked art, delete `filesystem/art/` and
`build/N64FlashcartMenu.dfs`, then build again. (Without removing the
second one, the build does not notice the art is gone and keeps it.)

Art that is not baked in is still read from the SD card as before, so
hacks, homebrew and anything added later keep working.

Every baked cover makes the menu file bigger, and the cart loads the whole
menu file at each start, so more covers mean a slower start. The script's
total tells you by how much the file grows.
