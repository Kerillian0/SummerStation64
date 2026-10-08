#!/usr/bin/env python3
"""Draws the menu's small icons. Nothing here is a photo or anyone else's
artwork: every icon is drawn by the code below.

Needs Pillow (in the dev container: sudo apt-get install python3-pil).
Run it from this folder:

    python3 make_icons.py

then build as usual; the Makefile turns each .png here into a .sprite.

The icons are kept as small as the console allows:
- expansion_pak.png, jumper_pak.png: 24 x 26, at most 15 colors plus "see
  through" (4 bits a pixel: about 330 bytes each in memory). Only the one
  that matches the console is loaded.
- logo_holo.png (32 x 32) and logo_stripes.png (16 x 64): the two patterns
  on the intro logo's pillars, in thousands of colors (2 KB each). They are
  only in memory while the intro plays.
- heart.png: 16 x 14, white in 16 shades (112 bytes), tinted by the menu.
- button.png: a 20 x 20 white disc in 16 shades (4 bits a pixel: 200 bytes).
  The menu tints it for each button and stretches it into the longer
  shapes, so one picture serves every button.
"""
from PIL import Image, ImageDraw

SCALE = 8       # draw big, then shrink, for smooth edges


def shrink(picture, width, height):
    return picture.resize((width, height), Image.LANCZOS)


def save_few_colors(picture, name, colors=15):
    """Save with a see-through color and at most `colors` others."""
    alpha = picture.split()[3].point(lambda a: 255 if a >= 128 else 0)
    rgb = picture.convert("RGB").quantize(colors, method=Image.MEDIANCUT)
    palette = rgb.getpalette()[: colors * 3]
    out = Image.new("P", picture.size, 0)
    out.putpalette([255, 0, 255] + palette)             # color 0 is the see-through one
    pixels = [(p + 1) if a else 0 for p, a in zip(rgb.getdata(), alpha.getdata())]
    out.putdata(pixels)
    out.save(name, transparency=0)
    print(f"{name}: {picture.size[0]} x {picture.size[1]}, {len(set(pixels))} colors")


def pak(name, top, top_edge, top_side, vents):
    """A memory pak seen from above its front-left corner. Both paks use
    these same shapes, so they sit at the same angle."""
    W, H = 24, 26
    picture = Image.new("RGBA", (W * SCALE, H * SCALE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(picture)

    def shape(points, color):
        draw.polygon([(x * SCALE, y * SCALE) for x, y in points], fill=color)

    body_front = (66, 66, 74, 255)
    body_side = (44, 44, 50, 255)
    edge = (104, 104, 114, 255)

    # The body, under the lid: front face and the side that turns away.
    shape([(2.5, 9.5), (10, 13.5), (10, 25.5), (2.5, 20.5)], body_front)
    shape([(10, 13.5), (21.5, 8.5), (21.5, 19.5), (10, 25.5)], body_side)
    # A light edge down the corner, so the dark body shows on dark themes.
    shape([(9.6, 13.5), (10.4, 13.5), (10.4, 25.5), (9.6, 25.5)], edge)
    shape([(2.5, 20.1), (10, 25.1), (10, 25.5), (2.5, 20.5)], edge)
    shape([(10, 25.1), (21.5, 19.1), (21.5, 19.5), (10, 25.5)], edge)

    # The lid: its top, and the two edges we can see.
    shape([(0.5, 7), (14, 1.5), (23.5, 6), (10, 12)], top)
    shape([(0.5, 7), (10, 12), (10, 14.5), (0.5, 9.5)], top_edge)
    shape([(10, 12), (23.5, 6), (23.5, 8.5), (10, 14.5)], top_side)

    # Air holes in the lid, in rows that follow its slant.
    for row, col in vents:
        x = 6.0 + col * 3.3 + row * 2.3
        y = 7.0 - col * 1.35 + row * 1.2
        draw.ellipse([(x - 0.75) * SCALE, (y - 0.5) * SCALE, (x + 0.75) * SCALE, (y + 0.5) * SCALE], fill=top_side)

    save_few_colors(shrink(picture, W, H), name)


# Expansion Pak: red lid with air holes.
pak("expansion_pak.png", (224, 44, 44, 255), (176, 28, 32, 255), (140, 20, 26, 255),
    [(r, c) for r in range(3) for c in range(4)])
# Jumper Pak: the same block with a plain dark grey lid.
pak("jumper_pak.png", (128, 128, 138, 255), (96, 96, 104, 255), (74, 74, 82, 255), [])

# The button disc: white on black, 16 shades. White is "fully there".
SIZE = 20
disc = Image.new("L", (SIZE * SCALE, SIZE * SCALE), 0)
ImageDraw.Draw(disc).ellipse([0, 0, SIZE * SCALE - 1, SIZE * SCALE - 1], fill=255)
disc = shrink(disc, SIZE, SIZE).point(lambda v: (v // 17) * 17)
disc.save("button.png")
print(f"button.png: {SIZE} x {SIZE}")

# The heart that marks a favorite: two round lobes and a point.
HW, HH = 16, 14
heart = Image.new("L", (HW * SCALE, HH * SCALE), 0)
hd = ImageDraw.Draw(heart)
hd.ellipse([0.3 * SCALE, 0.3 * SCALE, 8.3 * SCALE, 8.3 * SCALE], fill=255)
hd.ellipse([7.7 * SCALE, 0.3 * SCALE, 15.7 * SCALE, 8.3 * SCALE], fill=255)
hd.polygon([(0.9 * SCALE, 6.4 * SCALE), (15.1 * SCALE, 6.4 * SCALE), (8 * SCALE, 13.7 * SCALE)], fill=255)
heart = shrink(heart, HW, HH).point(lambda v: (v // 17) * 17)
heart.save("heart.png")
print(f"heart.png: {HW} x {HH}")

# The two patterns on the intro logo's pillars, after the look of the
# reference picture the project owner supplied: a marbled lavender, cyan and
# pink "hologram", and pink-to-deep-blue stripes. Both are made from sums.
import math

def mix(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))

# Hologram: bands of three colors bent into swirls. It repeats without a seam.
HOLO = [(126, 118, 212), (112, 214, 228), (214, 146, 232), (150, 110, 218)]
holo = Image.new("RGB", (32, 32))
for y in range(32):
    for x in range(32):
        a = 2 * math.pi * x / 32
        b = 2 * math.pi * y / 32
        swirl = math.sin(b + 1.6 * math.sin(a) + 0.8 * math.sin(2 * b + a)) + 0.5 * math.sin(2 * a - b)
        t = (swirl / 3.0 + 0.5) * len(HOLO)
        i = int(math.floor(t)) % len(HOLO)
        holo.putpixel((x, y), mix(HOLO[i], HOLO[(i + 1) % len(HOLO)], t - math.floor(t)))
holo.save("logo_holo.png")
print("logo_holo.png: 32 x 32")

# Stripes: pink and red at the top sinking to deep blue, in bands 4 high.
stripes = Image.new("RGB", (16, 64))
for y in range(64):
    down = y / 63
    base = mix((236, 70, 130), (40, 20, 150), down)
    dark = mix((176, 0, 60), (2, 3, 85), down)
    color = base if (y // 4) % 2 == 0 else dark
    for x in range(16):
        stripes.putpixel((x, y), color)
stripes.save("logo_stripes.png")
print("logo_stripes.png: 16 x 64")
