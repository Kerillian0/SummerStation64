#!/usr/bin/env python3
"""Make the short gameplay clips Game info plays in the cover's place.

A clip is 10 to 30 seconds of a video (a file, or an http(s) link that ffmpeg
can read, such as a longplay on archive.org), cut down to the part that
matters and encoded for the N64:

  - black borders cropped away and the picture made exactly 4:3,
  - 192x144 (12 x 9 macroblocks; the menu shows it at 168x126 in the cover's
    place), 20 frames a second, no sound,
  - H.264 as libdragon's RSP-accelerated decoder wants it (videoconv64),
    usually 250 to 450 KB for 25 seconds.

It is written where the menu looks for it, beside the game's box art:

    <out>/menu/metadata/N/S/M/E/preview.h264   (NSME: that release only)
    <out>/menu/metadata/N/S/M/preview.h264     (NSM: every region's)

with its seek table, preview.seek, beside it (it lets the menu loop the clip
without a pause; a clip without one still plays). <out>/clip_sources.txt
notes where each clip came from. Copy the menu folder inside <out> onto the
root of the SD card.

One clip:
    python3 scripts/make_preview_clip.py NSME https://.../longplay.mp4 --start 3:55 --length 25

Every clip in a list (one per line: CODE  SOURCE  START  [LENGTH]):
    python3 scripts/make_preview_clip.py --list scripts/preview_clips.txt

Needs ffmpeg and libdragon's videoconv64, so run it in the dev container:
videoconv64 comes with libdragon's tools (or is found in the libdragon
submodule once built), and ffmpeg is one install away (apt-get install ffmpeg).

Never commit the clips: like the box art, they belong to the games' owners.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VIDEOCONV_IN_TREE = "libdragon/tools/videoconv64/videoconv64"

MIN_SECONDS = 10
MAX_SECONDS = 30
MAX_BYTES = 3 * 1024 * 1024     # the menu won't load a bigger clip (video_preview.c)


def parse_time(text):
    """'95', '1:35' or '0:01:35.5' -> seconds."""
    seconds = 0.0
    for part in str(text).split(":"):
        seconds = seconds * 60 + float(part)
    return seconds


def game_folder(code):
    """menu/metadata/N/S/M/E for one region's release, menu/metadata/N/S/M for all of them."""
    if not re.fullmatch(r"[A-Za-z0-9]{3,4}", code):
        raise SystemExit(f"{code}: a game code is four letters or digits, such as NSME "
                         "(or its first three, NSM, for every region)")
    return os.path.join("menu", "metadata", *list(code))


class Tools:
    """Finds ffmpeg and videoconv64, and runs them."""

    def __init__(self):
        self.videoconv = shutil.which("videoconv64")
        in_tree = os.path.join(REPO, VIDEOCONV_IN_TREE)
        if not self.videoconv and os.path.isfile(in_tree) and os.access(in_tree, os.X_OK):
            self.videoconv = in_tree
        missing = [name for name, found in (("ffmpeg", shutil.which("ffmpeg")), ("videoconv64", self.videoconv))
                   if not found]
        if missing:
            raise SystemExit(f"Needs {' and '.join(missing)}. In the dev container: apt-get install ffmpeg; "
                             "videoconv64 comes with libdragon's tools (cd libdragon && make tools).")

    def run(self, args):
        if args[0] == "videoconv64":
            args = [self.videoconv] + args[1:]
        return subprocess.run(args, capture_output=True, text=True)


def make_clip(tools, code, source, start, length, out, width, fps, quality, label=None):
    """label: how clip_sources.txt names the source (default: the source itself)."""
    if not (MIN_SECONDS <= length <= MAX_SECONDS):
        raise SystemExit(f"{code}: a clip is {MIN_SECONDS} to {MAX_SECONDS} seconds long, not {length:g}")
    if width % 32 or not (64 <= width <= 320):
        raise SystemExit("--width must be a multiple of 32 from 64 to 320")

    if not re.match(r"https?://", source) and not os.path.isfile(source):
        raise SystemExit(f"{code}: no such file: {source}")

    with tempfile.TemporaryDirectory(prefix="clip-") as work:
        cut = ["-ss", f"{start:.3f}", "-t", f"{length:.3f}", "-i", source]
        cut_mp4 = os.path.join(work, "cut.mp4")
        seek_txt = os.path.join(work, "seek.txt")
        out_dir = os.path.join(work, "out")

        # 1. Where the picture is: most captures have black bars round it.
        found = tools.run(["ffmpeg", "-nostdin", "-hide_banner"] + cut +
                          ["-an", "-vf", "cropdetect=limit=24:round=2:reset=0", "-f", "null", "-"])
        crops = re.findall(r"crop=(\d+):(\d+):(\d+):(\d+)", found.stderr)
        if found.returncode != 0 or not crops:
            sys.stderr.write(found.stderr[-2000:])
            raise SystemExit(f"{code}: ffmpeg could not read {source}")
        w, h, x, y = (int(v) for v in crops[-1])
        crop = f"crop={w}:{h}:{x}:{y}," if w >= 64 and h >= 48 else ""

        # 2. That part, 4:3, sharp enough for the encoder to work from.
        step = tools.run(["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-y"] + cut +
                         ["-an", "-vf", crop + "scale=384:288:flags=lanczos,setsar=1",
                          "-c:v", "libx264", "-crf", "12", "-preset", "veryfast", cut_mp4])
        if step.returncode != 0:
            sys.stderr.write(step.stderr[-2000:])
            raise SystemExit(f"{code}: ffmpeg could not cut the clip")

        # 3. Encoded for the N64, with a seek table that marks frame 0 (the
        #    menu goes back there to loop without a stall). Only frame 0 is
        #    asked for, so the encoding itself is as without one.
        os.makedirs(out_dir, exist_ok=True)
        with open(seek_txt, "w") as f:
            f.write("0\n")
        step = tools.run(["videoconv64", "--codec", "h264", "--width", str(width), "--fps", str(fps),
                          "--quality", str(quality), "--no-audio", "--no-progress", "--seek", seek_txt,
                          "-o", out_dir, cut_mp4])
        encoded = os.path.join(out_dir, "cut.h264")
        seek = os.path.join(out_dir, "cut.seek")
        if step.returncode != 0 or not os.path.isfile(encoded) or not os.path.isfile(seek):
            sys.stderr.write(step.stdout[-2000:] + step.stderr[-2000:])
            raise SystemExit(f"{code}: videoconv64 failed")

        size = os.path.getsize(encoded)
        if size > MAX_BYTES:
            raise SystemExit(f"{code}: {size // 1024} KB is more than the menu loads "
                             f"({MAX_BYTES // 1024} KB); try a lower --quality or a shorter --length")

        folder = os.path.join(out, game_folder(code))
        os.makedirs(folder, exist_ok=True)
        dest = os.path.join(folder, "preview.h264")
        shutil.copyfile(encoded, dest)
        shutil.copyfile(seek, os.path.join(folder, "preview.seek"))
        note_source(out, code, label or source, start, length)
        print(f"{code}: {length:g} s from {start:g} s, cropped to {w}x{h}, {size // 1024} KB -> {dest}")


SOURCES_FILE = "clip_sources.txt"


def read_sources(out):
    """{code: 'source  start  length'} for every clip made into `out` (kept beside the menu folder)."""
    found = {}
    try:
        with open(os.path.join(out, SOURCES_FILE), encoding="utf-8") as f:
            for line in f:
                if line.strip() and not line.startswith("#"):
                    code, _, rest = line.rstrip("\n").partition("\t")
                    found[code] = rest
    except FileNotFoundError:
        pass
    return found


_sources_lock = threading.Lock()


def note_source(out, code, source, start, length):
    """Remember where a clip came from, so a report can say so later."""
    with _sources_lock:
        found = read_sources(out)
        found[code] = f"{source}\t{start:g}\t{length:g}"
        os.makedirs(out, exist_ok=True)
        with open(os.path.join(out, SOURCES_FILE), "w", encoding="utf-8") as f:
            f.write("# code\tsource\tstart (s)\tlength (s): written by make_preview_clip.py\n")
            for c in sorted(found):
                f.write(f"{c}\t{found[c]}\n")


def read_list(path):
    clips = []
    with open(path, encoding="utf-8") as f:
        for number, line in enumerate(f, 1):
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) not in (3, 4):
                raise SystemExit(f"{path}:{number}: want CODE SOURCE START [LENGTH]")
            clips.append((parts[0], parts[1], parse_time(parts[2]), parse_time(parts[3]) if len(parts) == 4 else 25.0))
    return clips


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("code", nargs="?", help="the game's four-letter code, such as NSME")
    p.add_argument("source", nargs="?", help="a video file, or an http(s) link to one")
    p.add_argument("--start", default="0", help="where the clip starts in the source (seconds, or m:ss)")
    p.add_argument("--length", default="25", help=f"how long it is ({MIN_SECONDS}-{MAX_SECONDS} seconds)")
    p.add_argument("--list", help="a file of clips to make, one per line: CODE SOURCE START [LENGTH]")
    p.add_argument("--out", default=os.path.join(REPO, "build", "previews"),
                   help="where to write the SD card's files (default: build/previews)")
    p.add_argument("--width", type=int, default=192, help="picture width, a multiple of 32 (default 192: 192x144)")
    p.add_argument("--fps", type=int, default=20, help="frames a second (default 20)")
    p.add_argument("--quality", type=int, default=60, help="videoconv64's quality, 0-100 (default 60)")
    args = p.parse_args()

    if args.list:
        clips = read_list(args.list)
    elif args.code and args.source:
        clips = [(args.code, args.source, parse_time(args.start), parse_time(args.length))]
    else:
        p.error("give a game code and a source, or --list")

    tools = Tools()
    for code, source, start, length in clips:
        make_clip(tools, code.upper(), source, start, length, args.out, args.width, args.fps, args.quality)
    print(f"\nCopy {os.path.join(args.out, 'menu')} onto the root of the SD card.")


if __name__ == "__main__":
    main()
