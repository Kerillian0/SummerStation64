# SummerStation64 v0.1

> **Draft, not published.** Release notes and setup instructions for the
> first release, for players downloading it from GitHub.

SummerStation64 is an unofficial, themeable game menu for the
**SummerCart64**, made as a fork of
[N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu)
(based on its V0.3.4). It turns the file list into a row of game covers,
with tabs, themes and an intro, and it is built and tested on a real N64
with a CRT television.

It is a first release. Back up your SD card before trying it, and keep the
stock `sc64menu.n64` somewhere safe so you can go back.

## What's new compared with N64FlashcartMenu

**Browsing**
- Your games as a row of box covers that slide left and right, with
  optional smaller covers either side.
- Turn a box over with up/down to see the back.
- Tabs along the top: **Games**, **Favorites**, **Folders**, and
  optionally **Recent**. Choose which tabs appear, and in what order, under
  Settings > Tabs. L and R switch between them.
- **Games** shows only games, gathered from as many folders as you like
  (see "Adding your games" below).
- **C-up and C-down** filter the Games tab: All, USA, Japan, Europe or
  64DD.
- **Folders** is a plain file browser for everything else: pictures,
  music, text, zips.
- Each tab goes back to the game it was left on.
- Optional wrap-around: the last game sits left of the first.
- Sort the games by name, recently played, release date or developer
  (Settings > Library > Sort By). Dates and developers come from the
  metadata pack.

**Game info**
- The title panel under the covers shows badges for each game: players,
  Expansion Pak, and whether a save was found.
- A screen per game with the cover over a blurred copy of itself, a large
  title, badges (Expansion Pak, Rumble Pak, Controller Pak, Transfer Pak),
  times played and when, and the description.
- The ring round the cover takes the colour of the box art.
- Japanese (tall) and 64DD covers keep their own shape.
- START shows the technical details.

**Look and sound**
- Four built-in summer themes (Sunset, Night Drive, Beach, Ocean), or
  your own `theme.ini`.
- An intro with a spinning logo and a tune, and a fade-in at start-up.
  Both can be changed or turned off.
- Rounded corners, coloured button icons, a favourite heart, a memory
  badge showing whether an Expansion Pak is fitted.
- Background music, chosen to suit the menu.

**Comfort and safety**
- Quick Launch (tap A to start a game) or Hold A To Launch, if you want
  either.
- Names tidied for display: no file endings, "Legend of Zelda, The" shown
  as "The Legend of Zelda". Your files are never renamed.
- Play counts and "last played".
- **Save backups:** each time you start a game, the menu keeps its save
  from before, by date. Up to five per game, never the same one twice.
  To go back to one, open the game's Options and choose **Restore save**;
  the save you have is kept too, so you can change your mind.
- Settings are saved safely, so switching off at the wrong moment can't
  leave a broken settings file.
- **Safe mode:** hold **Z** while switching on to start without your
  theme and settings, if something goes wrong.
- A friendly screen instead of a crash dump if the menu ever fails.

## Installing

1. Make sure your SummerCart64 has up-to-date firmware.
2. Download `sc64menu.n64` from this release.
3. Copy it to the **top folder of your SD card**, replacing the one that
   is there. The name must stay exactly `sc64menu.n64`.
4. Put the SD card back in the cart and switch the N64 on.

To go back to the stock menu, copy the stock `sc64menu.n64` back the same
way.

## First steps

### Adding your games
The Games tab starts with your **start folder** (the folder the stock menu
opened in). To add more folders, for example one per region and one for
64DD disks:

1. Press **R** until you reach the **Folders** tab.
2. Go into the folder that holds the games.
3. Press **Z** and choose **Add this folder to Games**.

To take a folder out again, choose **Remove this folder from Games** in
the same place. Settings > Library shows how many folders are added and
can forget them all. To change the start folder itself, go into it on the
Folders tab, press Z and choose **Set current directory as default**.

### Box art
Covers come from the same metadata pack as the stock menu. Extract it so
the pictures sit in `sd:/menu/metadata/`, for example
`sd:/menu/metadata/N/G/E/E/boxart_front.png`. The
[N64FlashcartMenu box art page](https://github.com/Polprzewodnikowy/N64FlashcartMenu/blob/main/docs/19_gamepak_boxart.md)
explains the layout and links to a ready-made pack. Without it the covers
show the game's name.

The first time you browse your games, the menu notes which file is which
game (in `sd:/menu/gameindex.txt`), so covers and badges come up faster
on later visits.

### Region filter
A game's region is read from its game code, or from tags in the file name
such as `(U)`, `(J)`, `(E)`, `(USA)` or `(Japan)`. The menu learns the
codes by itself: the first time you leave the Games tab still for a few
seconds, it reads each game it doesn't know yet. Until then, a game with
no tag in its name shows only under "All".

## Controls

| Button | Cover tabs (Games, Recent, Favorites) | Folders tab |
|---|---|---|
| Left / Right | previous / next game | |
| Up / Down | turn the box over | move up / down the list |
| C-left / C-right | Games tab: jump 10 games | |
| C-up / C-down | Games tab: change the region filter | jump 10 entries |
| A | game info (or start the game, with Quick Launch) | open |
| B | | go up a folder |
| L / R | previous / next tab | previous / next tab |
| Z | options for the selected game | options |
| START | settings | settings |

On the game info screen: A starts the game, B goes back, R opens the
game's options, START shows the details, left/right show the game's other
pictures.

## Settings

START opens Settings, with these pages:
- **Display:** theme, covers, animations and on-screen extras.
- **Controls:** launch style, wrap-around and more.
- **Sound:** music and sound effects.
- **Tabs:** which tabs appear and in what order.
- **Library:** sort order, how names are shown, play counts, game folders.
- **Files:** what the file browser shows.
- **System:** intro, fade-in, character set, and a performance overlay for
  testing.

Every page explains the selected setting at the bottom.

## Files the menu keeps on your SD card

All under `sd:/menu/`. Any of them can be deleted to go back to the
defaults.

| File | What it holds |
|---|---|
| `theme/theme.ini` | your own theme (optional) |
| `features.ini`, `options.ini` | your settings |
| `folders.ini` | the selected game in each folder |
| `gamefolders.txt` | the folders added to the Games tab |
| `playstats.txt` | play counts and last played |
| `gameindex.txt` | which file is which game; rebuilt by itself if deleted |

## Making your own theme

Put a `theme.ini` in `sd:/menu/theme/`, then choose **From SD Card**
under Settings > Display > Theme. The Theme Maker in this repository
(`theme-maker/theme-maker.html`, open it in any web browser) builds one
with a live preview and share codes. It doesn't know the newest options
yet (the Ocean background, built-in themes).

## Known limitations

- **Tested on one console:** an NTSC N64 with the Expansion Pak, on a CRT.
  - Without an Expansion Pak (4MB) the menu is designed to fit, but this
    hasn't been tried on real hardware yet.
  - PAL consoles haven't been tried.
- Saving settings can cause one short pause.
- Saves of 64DD disks are not backed up.
- Other flashcarts (64drive, ED64) are not supported by this fork.

## Credits and licence

- Built on [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu)
  by Polprzewodnikowy and contributors, and on
  [libdragon](https://github.com/DragonMinded/libdragon).
- Ideas from the [N64ever](https://github.com/bjerreman/N64FlashcartMenu-N64ever)
  fork.
- Music by Eric Matyas, www.soundimage.org ("Cyber-dream-loop").
- Interface sounds by Lokif (CC0), from opengameart.org.
- Title lettering drawn with the Anton and Mr Dafoe fonts (SIL Open Font
  License).
- Made with the help of Claude, an AI assistant by Anthropic.

Released under the GNU Affero General Public License v3, like
N64FlashcartMenu; the source is in this repository.

SummerStation64 is an unofficial fan project, not made or approved by
Nintendo. Nintendo 64 and the N64 logo are trademarks of Nintendo.
