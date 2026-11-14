# SummerStation64 v0.5

<p align="center">
  <b>☀️ A themeable game menu for the SummerCart64, built on a real N64 and a CRT. ☀️</b><br>
  <img src="https://img.shields.io/badge/SummerCart64-menu-f2b134" alt="SummerCart64 menu">
  <img src="https://img.shields.io/badge/runs%20on-4MB%20%26%208MB-3fa34d" alt="Runs on 4MB and 8MB">
  <img src="https://img.shields.io/badge/licence-AGPL--3.0-1c54d8" alt="Licence AGPL-3.0">
</p>

**v0.5** is the polish release. Meet **Vee**, a fairy who welcomes you back
from your games, has a word about the ones you rest on, answers questions
about the menu and guides you through a new **setup wizard**. Starting a game
**opens its box** and spins its cartridge while it loads, the cover row
**deals itself in** at start-up, and the new **Showy** animations make
everything glide. Keep track of your games with **status marks** (Playing,
Beaten, 100%, Backlog), **Library Stats** and a **Year in Review**.
**Settings** has been rebuilt as four headings of short pages, with
**search**. There's also a **versions column**, **clips on the covers**,
**TV test patterns** and a **controller test**. And the box art now lives in
its own file on the card, so the menu file is about 3 MB and slow SD cards no
longer stop with **"Watchdog timeout"**.

New here? Everything v0.4a brought (themes made on the console, theme
folders with their own music and sounds, up to four players with profile
pictures, title logos and video previews) is in the
**[v0.4a release notes](RELEASE_NOTES_v0.4a.md)**.

> [!TIP]
> **Quick start:** from the Assets below, copy **`sc64menu.n64`** to the top
> folder of your SD card, and unzip
> **`SummerStation64-v0.5-Covers-Logos-and-Clips.zip`** there too (box art,
> title logos and preview clips). Switch on. On a new card, Vee walks you
> through the main choices.

> [!NOTE]
> **Coming from v0.4a?** There's one menu now, no Art Edition or Standard:
> the box art has moved out of `sc64menu.n64` into `menu/art.pak`, which comes
> in the Covers, Logos and Clips zip with the logos and clips. Copy the new
> `sc64menu.n64` over the old one **and** unzip the new pack (without
> `art.pak` you get your own card covers, or plain boxes). Your
> settings, profiles, favorites, collections and saves all stay. The menu no
> longer has profile pictures built in: a player who used one shows their
> initial until you give them a picture of your own (the Companion's
> **[Profile Pictures](https://kerillian0.github.io/SummerStation64/companion/#pics)**
> tab makes one from any image). At the first
> start, **What's new** lists this release's changes; A on one takes you
> straight to its setting. **Settings has moved around**: every setting is
> still there, under **Look**, **Games**, **Console** or **Help** (L and R switch), and
> **Z** finds any of them by name. **Vee** is on by default and talks now and
> then; **Settings > Look > Assistant** makes her quieter, chattier, or
> switches her off. The setup wizard is in **Settings > Console > About**
> if you'd like to run it.

---

## ✨ New in v0.5

### 🛠️ No more "Watchdog timeout"
- **The box art has its own file on the card**, `menu/art.pak`, instead of
  being inside `sc64menu.n64`. The SummerCart64 must load the whole menu file
  within 5 seconds, and at about 33 MB some cards couldn't. It's now about
  3 MB, and covers appear as fast as before (read one at a time, straight
  into the cart's memory).
- **One menu, one pack:** the Art Edition and Standard are gone. There's
  `sc64menu.n64`, and one zip for the SD card with the box art, title logos
  and preview clips together.

### 🧚 Vee, your guide
- **A fairy in the corner** of the cover rows: she greets you at power-on
  (by the time of day, or for a holiday), welcomes you back from the game you
  played, marks milestones, and now and then shares a tip about something you
  haven't tried yet. Her words appear in the hint band.
- **She knows your games:** rest on one and she may have a word about it
  (some 40 well-known games have lines of their own). Leave the menu idle and
  she chats: your games, your stats, a joke.
- **How often** (Off, Rarely, Now and Then, Chatty) and **her mood** (Eager
  or Snarky) in **Settings > Look > Assistant**.
- **Ask a Question:** 48 answers about the whole menu in eight topics, from
  **Settings > Help** or last in the **Z** menu
  on any cover.
- **Make her your own:** **Fairy Look...** gives her a new name, a color, a
  way to arrive (Fly In, Sparkle, Fade, Drop In), a way to move while she
  talks (Bob, Still, Wander, Bounce) and a glow that pulses, all shown live.
- **In the intro** she flies across the title, drawing it in with a trail of
  sparkles, and a glint of light sweeps it once it's done.

### 👋 Setup wizard and What's new
- **Setup wizard:** the first time the menu starts on a card, Vee asks a few
  questions, one a page: your console and TV, the picture (240p or 480i),
  preferred region, region tags and copies, the theme (shown as you pick it),
  animations, the start-up, and how she talks and what she's called. A
  answers, B goes back, START keeps the rest as it is. Run it again from
  **Settings > Console > About > Setup Wizard**.
- **What's new:** shown once after the menu file is updated, and from
  **Settings > Console > About > What's New**. A on an item opens its setting.

### 🎬 Animations
- **Settings > Look > Theme > Animations: Snappy or Showy.** Snappy is the
  menu as it was. On **Showy** covers fade in and pop into place, names and
  badges slide in, highlights glide between rows, pop-ups grow open and
  shrink closed, Settings pages slide, and the cover flies into Game info and
  back. Any button press finishes a running animation at once.
- **The box opens:** starting a game from Game info flies its cover to the
  middle, opens the box, and the cartridge pops out and spins while the game
  loads, then snaps home with a soft thunk (**Settings > Look > Start-up >
  Game Start**: Box Opens, or the plain Loading Bar).
- **The covers deal in** at start-up: side covers slide in from their sides
  and the selected game drops into place. **Back from a game**, the row opens
  on it, and its cartridge rises spinning from the bottom of the screen and
  turns into its cover. Both follow **Cover Slide**.

### 📊 Status, stats and your year
- **Status marks:** mark a game **Playing**, **Beaten**, **100%** or
  **Backlog** with **Z > Status**, or from Game info's Options. Each shows as
  a badge, has its own C-up / C-down step on the Games tab, and **Backlog**
  and **Beaten** turn up as collections. **Sort By Status** puts what you're
  playing first.
- **Collection progress:** each collection's card says how many of its games
  you've beaten, with a bar, and **Complete** once you've beaten them all
  (**Settings > Look > Badges**).
- **Sort By Time Played** (most played first), and a **Never Played** step
  on C-up / C-down for the games you've never started.
- **Library Stats** (**Settings > Games > Saves & Play**): how many games you
  have, their size, your time played, the SD card's free space, your five
  most played in a big frame, your top developers and genres, and your
  status counts.
- **Year in Review:** C-right on Library Stats steps to a page per year: time
  played, games, starts and games beaten, each against the year before, plus
  that year's most played and the games you beat in it.

### ⚙️ A new Settings
- **Four headings** on L and R: **Look**, **Games**, **Console** and
  **Help**, with 22 short pages in place of 9 long ones, so every page fits
  on screen. **Help** opens Vee's questions and answers straight away.
- **Rounded panels, icons** beside each page, **On / Off pills**, a dot per
  value on short choices, and a **help panel** that names the setting or
  lists what's on the page. Left and right step a value back and forth.
- **A bar beside every setting you've changed** from its default.
- **New defaults:** Sort By starts on **Recently Played** for anyone who
  hasn't picked an order, and on a new card the cover rows wrap round from
  the last game to the first (**Wrap File List**). The intro's title is now
  always the menu's name in plain letters; the drawn picture title is gone.
- **Search:** press **Z** on any page and type a word or two. Every matching
  setting is listed with where it lives, and A takes you straight there.
- With two or more players, the player's picture and name sit at the top
  right, so you know whose settings these are.

### 🖼️ Covers and clips
- **The versions column:** A on a cover standing for several files (Game
  Versions: Grouped, or Region Copies: One Cover) opens them as a column of
  covers over a blurred Games screen, with each one's name and when you last
  played it. On the left or in the middle (**Settings > Look > Covers >
  Versions Column**).
- **Clips on the covers:** with the Covers, Logos and Clips pack and the Expansion
  Pak, the selected game's clip plays in its cover's place after a 2-second
  rest, even on the front of a 3D box, and carries on into Game info when you
  press A (**Settings > Look > Game Info > Clips on Covers**).
- **Game info with a clip** runs at the clip's own steady 20 frames a second,
  and clips load in small pieces, so the page no longer stalls.
- **64DD disks** are read once ever instead of at every power-on, and a game
  with no cover picture is looked for once per power-on, so the row no longer
  stalls on them.
- **Trim Black Edges** also cuts black padding at both ends of top labels and
  spines scanned narrower than the box (Dark Rift, NHL Breakaway 98, Mortal
  Kombat Mythologies, Super Mario 64 PAL).

### 📺 Tools for your TV and controllers
- **Test Patterns** (**Settings > Console > Tools**): an overscan grid with
  borders at the edge and at 2.5, 5 and 10 percent, to see what your TV cuts
  off, and color bars with grey steps and a black-level check.
- **Controller Test:** every button lights up in its place, the stick's reach
  is measured in eight directions with a wear rating, the four ports and each
  one's accessory are shown, and Z + A tries a Rumble Pak.

### ⌨️ Typing names
- On the keyboard, **Z** closes it without keeping the name, **B** only
  erases, and **holding B** keeps erasing.

### ℹ️ Credits
- **Settings > Console > About > Credits** now names SummerStation64 and
  links to this page, credits the N64ever fork's ideas and says the menu is
  made with AI. Its L|Z pop-up fits the screen and B closes it first.

---

## 📥 Installing

| File in the Assets | What it is | Where it goes |
|---|---|---|
| `sc64menu.n64` | the menu (about 3 MB), game details built in | the top folder of the SD card |
| `SummerStation64-v0.5-Covers-Logos-and-Clips.zip` | box art (`menu/art.pak`), title logos and preview clips | unzip at the top of the SD card |

1. **Update your SummerCart64's firmware** if it is old, and **back up** your
   `sc64menu.n64`.
2. **Copy `sc64menu.n64`** to the top folder of your SD card.
3. **Unzip `SummerStation64-v0.5-Covers-Logos-and-Clips.zip`** at the top of
   the card, keeping its `menu` folder: `art.pak` goes into `menu/`, the
   logos and clips into `menu/metadata/` (nothing of yours is replaced). The
   clips need the Expansion Pak; the box art and logos work on every
   console. Prefer your own art? Leave `art.pak` out and put pictures in
   `menu/metadata/` (the Companion's Guide shows how).
4. **Switch the N64 on.**

> [!WARNING]
> **Still seeing "Watchdog timeout" at power-on?** The SummerCart64 gives the
> menu file 5 seconds to load. Since v0.5 it's about 3 MB (the art is in
> `menu/art.pak`), which even a slow card loads in time. If it still
> happens, check that `sc64menu.n64` is the new one (an older Art Edition is
> about 33 MB). If it is, the card itself may be failing or badly
> formatted: copy your files off, reformat it (FAT32 or exFAT, for example with
> the SD Association's *SD Card Formatter*) and copy everything back.

New to SummerStation64? The Companion's
**[Guide](https://kerillian0.github.io/SummerStation64/companion/#guide-setup)**
walks through setting up a card, including adding your game folders.

<details>
<summary><b>🎨 Sharing a theme</b></summary>

A theme made on the console or in the Theme Maker is a folder:

```
menu/themes/My Theme/theme.ini
                     music.wav64, intro.wav64        (optional)
                     move / select / back / setting / error.wav64   (optional)
                     font.font64                     (optional)
```

Zip the folder to share all of it; others unzip it into `menu/themes/`. A
share code from the Theme Maker carries the colors, background and pattern
only. Use sounds you made, or whose licence lets you share them.
</details>

<details>
<summary><b>🕹️ Retro games: where their pictures go</b></summary>

NES, SNES and Game Boy games need the menu's emulators in `menu/emulators/`
(see N64FlashcartMenu's documentation). Their pictures are read from the SD
card, one folder per game, named after the game's file without its
extension:

```
menu/metadata/nes/Super Mario Bros. (World)/boxart_front.png
                                            boxart_back.png
                                            boxart_spine.png
                                            gamepak_front.png   (see-through background)
                                            gamepak_back.png
                                            metadata.ini, description.txt
```

`snes`, `gb` and `gbc` work the same way. The Companion's Missing Metadata
tab puts everything in the right place for you.
</details>

<details>
<summary><b>🔐 Checksums</b></summary>

SHA-1 of each download (and of `art.pak` inside the zip), to check them:

```
0a38f471313367be381468c623e455932d5a98b8  sc64menu.n64
8b74c18dc9b50016f98b7c1119e25b015c97f4fc  SummerStation64-v0.5-Covers-Logos-and-Clips.zip
be89b732b492bee18d75ac85b5b74c2542f71f28  menu/art.pak (inside the zip)
```
</details>

---

## ⚠️ Known limitations

- **Character Set Full needs the Expansion Pak.** On a Jumper Pak the menu
  uses Latin Only even when Full is chosen (Japanese file names show gaps);
  **Settings > Console > Tools > Memory** says so.
- **Year in Review** counts play from v0.5 on: time played before this
  release is in Library Stats' *All time* page, not in a year. **Games
  beaten in a year** counts games marked Beaten or 100% from v0.5 on.
- **The box opening** plays for cartridge games started from Game info;
  64DD disks and retro games keep the loading bar.
- **Profiles:** 64DD disk saves and the Controller Pak are shared by all
  players. Renaming a game on the Folders tab updates the playing profile's
  Recent, Favorites, collections and status marks, not the other players'.
- **Edit Theme** changes colors, the background and the pattern; a theme's
  switches, sounds and font are set in the Theme Maker, the Sounds tab or by
  hand. **A theme's font** applies after a restart.
- **Going back to an older menu** (v0.3 or the stock N64FlashcartMenu) keeps
  only your first 8 favorites, and drops the rest the next time it saves;
  profiles 2-4's saves stay in `saves/<profile>/` beside your games. v0.4a
  reads v0.5's files but drops the per-year play time and the days games
  were beaten if it saves them.
- **Emulator games** (NES, SNES, Game Boy) can't be favorited yet.
- **Cheats:** codes that need a button held (starting D0 to D3) may not work
  yet, and the menu reads up to 64 code lines per game.
- **Tested on:** every build on an NTSC N64 on a CRT, with an Expansion Pak
  and with a Jumper Pak (4MB). Players report it working on the **Analogue 3D**,
  the **ModRetro M64** and **PAL** consoles, thank you!
- **Party mode** has been tested with a stand-in for extra controllers, not
  with four real ones yet. Reports welcome.
- Saves of 64DD disks are not backed up. Other flashcarts are not supported.

---

## 🤖 AI disclosure

SummerStation64 is made with **Claude**, an AI model by Anthropic, used
through **Claude Code** inside Visual Studio Code.

- **Model:** Claude Opus 5.5, as recorded on the commits.
- **How much, in v0.5:** all 67 commits since v0.4a are co-authored by
  Claude (November 10 to 13, 2026): about 11,000 changed lines of menu code
  (17 new source files), Vee's questions and answers, the What's new list,
  the Companion's updated Settings paths, the tests, and these notes.
- **What the owner (Kerillian) did:** decided what v0.5 should hold and how
  each part should behave (Vee and her personality, the Showy animations, the
  box opening and the cartridge's return, the Settings redesign and its
  search, status marks and Year in Review, the setup wizard's questions, the
  test patterns and controller test), chose among the designs offered, and
  **tested every build on a real N64 and CRT**. Nothing was committed until
  it passed on the console.
- **Not made by AI:** the music and interface sounds, the fonts, the box art
  and game details (a community pack), the game data (libretro), and
  N64FlashcartMenu and libdragon, which this is built on.

If something seems wrong, please open an issue.

---

## 💛 Free, always

SummerStation64 is free and always will be, from v0.1 to v1.0 and beyond. I
will never charge for it.

**If you'd like to give money,** please support the people who made it
possible:
- **Polprzewodnikowy**, creator of the SummerCart64:
  [ko-fi.com/polprzewodnikowy](https://ko-fi.com/polprzewodnikowy)
- **networkfusion**, maintainer of N64FlashcartMenu:
  [ko-fi.com/networkfusion](https://ko-fi.com/networkfusion)

Buying your SummerCart64 from an official seller supports Polprzewodnikowy
too.

**If you'd rather give time,** help with the box art and game details every
player sees. The
[N64 flashcart menu metadata](https://github.com/n64-tools/n64-flashcart-menu-metadata#description)
project welcomes new art, descriptions and fixes; its README shows how to
start.

---

## 🙏 Credits and licence

- Built on [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu)
  by Polprzewodnikowy and contributors, and on
  [libdragon](https://github.com/DragonMinded/libdragon).
- Title logos, video previews and review scores by **[stewx](https://github.com/stewx)**.
- Requests and reports from players on Reddit, especially **Dispacho**
  (homebrew marks, hidden hacks, padded label ends) and **Quest-Quest**
  (64DD covers, the intro's parts).
- Ideas from the [N64ever](https://github.com/bjerreman/N64FlashcartMenu-N64ever)
  fork.
- Genres, player counts, series, and the lists used by Card Prep and the
  Missing Metadata tab:
  [libretro-database](https://github.com/libretro/libretro-database)
  (CC-BY-SA 4.0).
- Music by Eric Matyas, www.soundimage.org ("Cyber-dream-loop").
  Interface sounds by Lokif (CC0). Title lettering drawn with the Anton and
  Mr Dafoe fonts (SIL Open Font License). Vee and the cartridge's thunk are
  the project's own.

Released under the **GNU Affero General Public License v3**, like
N64FlashcartMenu; the source is in this repository.

> [!NOTE]
> **About the covers and game details:** the covers and cartridges in
> `menu/art.pak`, and the game descriptions built into the menu, are
> not part of the source code and not covered by its licence. They belong to
> the games' publishers and writers, and are included (in reduced size) for
> use with the menu on your own console. They come from the community
> [n64-flashcart-menu-metadata](https://github.com/n64-tools/n64-flashcart-menu-metadata)
> pack (scans credited there to ABeezy, LaunchBox community). If you hold
> the rights to any of them and want them removed, open an issue and the
> release files will be replaced without them. The same goes for the
> **title logos and preview clips** in the
> Covers, Logos and Clips pack: they belong to the games' publishers, are included
> in reduced size for use with the menu on your own console, and are not
> part of the source code. The logos come from libretro's thumbnail
> collection ([libretro-thumbnails](https://github.com/libretro-thumbnails/Nintendo_-_Nintendo_64));
> the clips are cut from gameplay recordings kept on
> [archive.org](https://archive.org). Ask, and they are removed.

<sub>SummerStation64 is an unofficial fan project, not made or approved by
Nintendo. Nintendo 64 and the N64 logo are trademarks of Nintendo.</sub>
