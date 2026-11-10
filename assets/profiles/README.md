# Profile pictures

The pictures players can choose for their profile (Settings > Profiles >
Profile Picture) come from two places:

- **Built into the menu file:** put PNG or JPEG pictures in
  `assets/profiles/source/` and run `python3 assets/profiles/make_avatars.py`
  before building. The folder is ignored by git, so the pictures never go
  into the repository; only into the menu files you build. A build without
  them still works: players get the pictures on their SD card, or their
  initial in their color.
- **On the SD card:** any PNG in `sd:/menu/avatars/` (up to 32), shown after
  the built-in ones, named after the file.

Each picture is shown as a 64 x 64 square, cut from the middle of the
picture. Recommended sizes:

- **Built-in pictures (`source/`): square, about 256 x 256** (anything from
  128 x 128 up is fine). The script shrinks them smoothly on the PC, so a
  larger picture looks cleaner; a picture that isn't square loses its sides.
- **Card pictures (`menu/avatars/`): exactly 64 x 64.** The console shrinks
  a larger picture by skipping pixels, so it comes out jagged, and stretches
  a smaller one. A wide or tall picture is shrunk to fit 64 x 64 and then
  cut square, which leaves it small and soft. The Companion's Profile
  Pictures tab (`#pics`) frames and sizes any picture for you.

Either way, the picture is drawn as small as 24 pixels across (under the
tabs) and the circle cuts off its corners: a face, or a head and shoulders,
filling the square works best. In a built-in picture, see-through parts
show the player's color behind them. Settings > Profiles > Picture Shape
draws them round or as rounded squares, in a ring of the player's color.
