# SummerStation64 v0.1

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
- **C-up and C-down** filter the Games tab: All, USA, Japan, Europe,
  64DD or Hacks.
- **Random game:** press Z on Games, Recent or Favorites and choose
  Random game to jump to any other game in the row (within the filter in
  use).
- **Folders** is a plain file browser for everything else: pictures,
  music, text, zips.
- Each tab goes back to the game it was left on.
- Optional wrap-around: the last game sits left of the first.
- Sort the games by name, recently played, release date or developer
  (Settings > Library > Sort By). Dates and developers come from the
  metadata pack.

**Game info**
- The title panel under the covers shows badges for each game: players,
  Expansion Pak, whether a save was found, and the accessories it uses
  (Controller Pak, Rumble Pak, Transfer Pak). Settings > Display >
  Accessory Badges shows those in blue boxes, as plain grey words, or not
  at all.
- A screen per game with the cover over a blurred copy of itself, a large
  title, badges (Expansion Pak, Rumble Pak, Controller Pak, Transfer Pak),
  times played and when, and the description.
- The ring round the cover takes the colour of the box art.
- Japanese (tall) and 64DD covers keep their own shape.
- START shows the technical details.

**ROM hacks**
- Hacks get an orange **Hack** badge and their own step in the filter.
- A file counts as a hack when its name says so ("hack", "redux", or a
  `[h1]`-style tag), or when it shares its game code with the original
  game but its name is a different game ("Super Mario 64 - Return to
  Yoshi's Island" next to "Super Mario 64"). Two copies of the same game
  under slightly different names are not called hacks.

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
- When memory runs short (mostly without an Expansion Pak), the menu
  leaves a few extras out instead of stopping: art on the side covers,
  the intro's title picture. Settings > System > **Memory** shows how much
  is free and what, if anything, was left out.

## Installing

### Choose a download
There are two editions. They are the same menu; only where the box covers
come from differs. **Most people want the Art Edition:** copy one file and
everything works.

| | **Art Edition** (recommended) | **Standard** |
|---|---|---|
| File to download (Assets, below) | `sc64menu.n64` | `SummerStation64-v0.1-Standard.zip` |
| File size | about 28 MB | about 3 MB |
| Box covers | built in, so they appear faster | read from the metadata pack on your SD card |
| Game details (descriptions, dates, developers, players) | built in | built in |
| Extra download needed | none | the metadata pack, for covers |

Choose Standard if you want to use your own box art, or prefer the small
download. You can switch at any time by copying the other file over it.

### Art Edition: steps
1. **Update your SummerCart64's firmware** if it is old (see the
   SummerCart64 documentation).
2. **Back up the menu you have now.** Copy `sc64menu.n64` from the top
   folder of your SD card to your computer, so you can go back.
3. **Download `sc64menu.n64`** from the Assets at the bottom of this page.
4. **Copy it to the top folder of your SD card**, replacing the one there.
   The name must stay exactly `sc64menu.n64`.
5. **Put the SD card back in the cart and switch the N64 on.** The intro
   plays, then the Games tab opens on your start folder.

### Standard: steps
1. and 2. As above: update the firmware and back up your menu.
3. **Download `SummerStation64-v0.1-Standard.zip`** from the Assets and
   unzip it. Inside is a file already named `sc64menu.n64`.
4. **Copy that `sc64menu.n64` to the top folder of your SD card**,
   replacing the one there.
5. **Add the metadata pack for box covers.** Download it from the
   [n64-flashcart-menu-metadata releases](https://github.com/n64-tools/n64-flashcart-menu-metadata/releases)
   and extract it so its folders sit in `sd:/menu/metadata/` (for example
   `sd:/menu/metadata/N/G/E/E/`). If you already use box art with the
   stock menu, it is already there.
6. **Put the SD card back in the cart and switch the N64 on.**

Optional for either edition: with the metadata pack on the card, the game
info screen also shows the box's sides and top.

To go back to the stock menu, copy your backed-up `sc64menu.n64` onto the
SD card the same way.

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
Without box art a cover shows the game's name. The
[N64FlashcartMenu box art page](https://github.com/Polprzewodnikowy/N64FlashcartMenu/blob/main/docs/19_gamepak_boxart.md)
explains how the metadata folder is laid out.

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
| Z | options (Random game, and more) | options |
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

Kept saves sit beside your saves, in a `backups` folder with one folder
per game (`saves/backups/<game>/` when the menu keeps saves in a `saves`
folder). Delete any you no longer want.

## Making your own theme

Put a `theme.ini` in `sd:/menu/theme/`, then choose **From SD Card**
under Settings > Display > Theme. The Theme Maker in this repository
(`theme-maker/theme-maker.html`, open it in any web browser) builds one
with a live preview of the Games and Folders screens, the four built-in
themes as starting points, and share codes.

## Known limitations

- **Tested on one console:** an NTSC N64 on a CRT, with an Expansion Pak
  and with a Jumper Pak (4MB).
  - On 4MB the menu uses a smaller character set by itself (Settings >
    System > Character Set: Auto), which leaves plenty of room. With the
    full set on 4MB it still works, but leaves extras out (see Memory
    above), and Japanese names need the full set.
  - Once on 4MB, opening a full-screen picture straight after a lot of
    cover browsing said "PNG decode failed due to insufficient memory".
    Nothing was lost; opening it again works.
  - PAL consoles haven't been tried.
- Accessory badges come from the menu's built-in game list; a game it
  doesn't list shows none.
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

**About the built-in covers and game details:** the Art Edition's covers,
and the game descriptions built into both editions, are not part of the
source code and not covered by its licence. They belong to the games'
publishers and writers, and are included (covers in reduced size) for
use with the menu on your own console. They come from the community
[n64-flashcart-menu-metadata](https://github.com/n64-tools/n64-flashcart-menu-metadata)
pack (scans credited there to ABeezy, LaunchBox community). If you hold
the rights to any of them and want them removed, open an issue and the
release files will be replaced without them.

SummerStation64 is an unofficial fan project, not made or approved by
Nintendo. Nintendo 64 and the N64 logo are trademarks of Nintendo.
