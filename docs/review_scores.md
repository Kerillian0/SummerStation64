# Review scores

The menu can show a review score out of 10 for each game: at the right of
its name under the covers, in a **Review score** box on its Game info
screen, and as **Review Score** in Settings > Library > Sort By (highest
first). Scores are green from 7.5, yellow from 5.0 and red below. Hacks
show none.

No scores come with the menu: they are read from a file on the SD card,
`menu/reviewscores.txt`, that you write yourself or get from a metadata
source you trust. Without it (or with a file that has no scores) nothing
shows. **Review Scores** in Settings > Display turns them on or off; it is
on by default. The file is read once, at start-up.

## The file

A plain text file. The first line is `reviewscores 1`. Then one game per
line: its four-letter game code, a space, and its score.

```text
reviewscores 1
# Lines starting with # are comments.
NSME 87
NSMJ 8.7
NZLE 9.9
```

- **The game code** is the one in the cartridge header (`NSME` is Super
  Mario 64 for the USA, `NSMJ` for Japan). Each region has its own code, so
  list every region you want scored. On a game's Game info screen, START
  (Details) shows its code.
- **The score** is in tenths (`87`) or out of 10 with a point (`8.7`), from
  0.1 to 10. A plain `10` means 1.0; write `100` or `10.0` for full marks.
- A code listed twice keeps its first score. Lines that don't read as a
  code and a score are skipped. Windows line endings are fine.
