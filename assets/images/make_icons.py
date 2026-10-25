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
- corner.png: a 16 x 16 white disc in 16 shades (128 bytes). Its four
  quarters are the rounded corners of the tabs, panels and badges.
- heart.png: 16 x 14, white in 16 shades (112 bytes), tinted by the menu.
- port.png: 24 x 20, a controller port in 16 shades (240 bytes), tinted by
  the menu (the controller marks under the clock).
- cart_back.png: 158 x 107, the back of a game cartridge in light grey
  (thousands of colors, 34 KB in memory while a cartridge shows its back).
  The menu tints it to each cartridge's plastic. Its outline comes from
  cart_outline.py.
- cart_bottom.png: 64 x 16, the slot in a cartridge's bottom with the
  circuit board's edge (2 KB, while cartridges are shown in 3D).
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

# A controller port, as on the front of the console: a dome-shaped
# opening (flat at the bottom, round at the top) with its three pins in a
# row. White in 16 shades, tinted by the menu: the accent color for a port
# with a controller in it, grey for an empty one. The opening is see-through.
PW, PH = 24, 20
port = Image.new("L", (PW * SCALE, PH * SCALE), 0)
pd = ImageDraw.Draw(port)
def dome(d, x0, y0, x1, y1, fill):
    r = (x1 - x0) / 2
    d.ellipse([x0, y0, x1, y0 + 2 * r * 0.9], fill=fill)
    d.rectangle([x0, y0 + r * 0.9, x1, y1], fill=fill)
k = SCALE
dome(pd, 0.5 * k, 0.5 * k, 23.5 * k, 19.5 * k, 255)    # the shell
dome(pd, 3.5 * k, 3.5 * k, 20.5 * k, 16.5 * k, 0)      # the opening
for i in range(3):                                      # the pins
    cx = (7.5 + i * 4.5) * k
    pd.ellipse([cx - 1.5 * k, 9.5 * k, cx + 1.5 * k, 12.5 * k], fill=255)
port = shrink(port, PW, PH).point(lambda v: (v // 17) * 17)
port.save("port.png")
print(f"port.png: {PW} x {PH}")

# The dump marks (Card Prep's verified.txt): white symbols drawn over the
# button disc, which the menu tints green, amber or red. Same size as the
# disc, 16 shades. Thick strokes: they are seen on a composite CRT.
def mark(name, draw):
    m = Image.new("L", (SIZE * SCALE, SIZE * SCALE), 0)
    draw(ImageDraw.Draw(m), SCALE)
    m = shrink(m, SIZE, SIZE).point(lambda v: (v // 17) * 17)
    m.save(name)
    print(f"{name}: {SIZE} x {SIZE}")

def check(d, k):
    d.line([(5.2 * k, 10.4 * k), (8.6 * k, 13.8 * k), (14.8 * k, 6.4 * k)], fill=255, width=int(2.8 * k), joint="curve")

def cross(d, k):
    d.line([(6.3 * k, 6.3 * k), (13.7 * k, 13.7 * k)], fill=255, width=int(2.8 * k))
    d.line([(13.7 * k, 6.3 * k), (6.3 * k, 13.7 * k)], fill=255, width=int(2.8 * k))

def question(d, k):
    d.arc([6.2 * k, 3.6 * k, 13.8 * k, 10.8 * k], start=190, end=80, fill=255, width=int(2.5 * k))
    d.line([(11.2 * k, 10.0 * k), (10.0 * k, 11.4 * k), (10.0 * k, 12.6 * k)], fill=255, width=int(2.5 * k))
    d.ellipse([8.6 * k, 14.2 * k, 11.4 * k, 17.0 * k], fill=255)

mark("mark_check.png", check)
mark("mark_cross.png", cross)
mark("mark_question.png", question)

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

# The disc whose quarters round off the corners of tabs, panels and badges.
CORNER = 16
corner = Image.new("L", (CORNER * SCALE, CORNER * SCALE), 0)
ImageDraw.Draw(corner).ellipse([0, 0, CORNER * SCALE - 1, CORNER * SCALE - 1], fill=255)
corner = shrink(corner, CORNER, CORNER).point(lambda v: (v // 17) * 17)
corner.save("corner.png")
print(f"corner.png: {CORNER} x {CORNER}")

# The back of a game cartridge, for the 3D cartridges (Show Games As:
# Cartridges), redrawn after a scan the project owner supplied: plastic, two
# screws, the raised "Nintendo" oval, the seams and notches, and the label
# with the N64 logo, NINTENDO 64 and GAME PAK (the owner's choice to keep
# those). The small print is made-up letters: at this size it can't be read.
# Light neutral grey plastic, so the menu can tint it to each cartridge's own.
# Its outline is the shared one in cart_outline.py.
import random
from PIL import ImageFont
from cart_outline import outline as cart_outline, WIDTH as CART_W, HEIGHT as CART_H

S = SCALE
W, H = CART_W * S, CART_H * S
FONT = "../fonts/Firple-Bold.ttf"
def font(height):
    return ImageFont.truetype(FONT, max(1, round(height * H)))
def at(x, y):
    return (x * (W - 1), y * (H - 1))
back = Image.new("RGBA", (W, H), (0, 0, 0, 0))
d = ImageDraw.Draw(back)
PLASTIC = (176, 176, 182, 255)
SHADE = (138, 138, 146, 255)
LIGHT = (206, 206, 212, 255)
LABEL = (86, 88, 106, 255)
INK = (236, 236, 240, 255)
d.polygon([at(x, y) for x, y in cart_outline()], fill=PLASTIC)
# the seams between the middle and the grips
for x in (0.198, 0.802):
    d.line([at(x, 0.05), at(x, 0.30)], fill=SHADE, width=S)
    d.line([at(x, 0.82), at(x, 1.0)], fill=SHADE, width=S)
# the notches at the bottom
for x0, x1 in ((0.063, 0.159), (0.841, 0.937)):
    d.rounded_rectangle([at(x0, 0.87), at(x1, 1.02)], radius=0.02 * H, fill=SHADE)
    d.line([at(x0 + 0.01, 0.875), at(x1 - 0.01, 0.875)], fill=LIGHT, width=S)
# the screws
for x in (0.198, 0.802):
    cx, cy = at(x, 0.131)
    r = 0.019 * W
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(160, 140, 80, 255), outline=(96, 86, 56, 255), width=S)
    d.line([cx - r * 0.6, cy, cx + r * 0.6, cy], fill=(96, 86, 56, 255), width=S)
    d.line([cx, cy - r * 0.6, cx, cy + r * 0.6], fill=(96, 86, 56, 255), width=S)
# the raised oval with "Nintendo" pressed into the plastic
d.rounded_rectangle([at(0.37, 0.10), at(0.63, 0.197)], radius=0.048 * H, outline=LIGHT, width=2 * S)
d.rounded_rectangle([at(0.374, 0.108), at(0.626, 0.19)], radius=0.04 * H, outline=SHADE, width=S)
f = font(0.07)
cx, cy = at(0.5, 0.149)
d.text((cx + S, cy + S), "Nintendo", font=f, fill=LIGHT, anchor="mm")
d.text((cx, cy), "Nintendo", font=f, fill=SHADE, anchor="mm")
# the label: a raised rim, then the dark label
d.rounded_rectangle([at(0.145, 0.288), at(0.855, 0.83)], radius=0.03 * H, fill=LIGHT)
d.rounded_rectangle([at(0.152, 0.298), at(0.848, 0.821)], radius=0.025 * H, fill=LABEL)
# the N64 logo: four blocks making an N, seen a little from the side
def quad(points):
    d.polygon([at(x, y) for x, y in points], fill=INK)
quad([(0.178, 0.345), (0.192, 0.335), (0.192, 0.41), (0.178, 0.418)])        # left pillar
quad([(0.194, 0.335), (0.204, 0.342), (0.204, 0.41), (0.194, 0.41)])
quad([(0.216, 0.338), (0.226, 0.331), (0.226, 0.405), (0.216, 0.413)])        # right pillar
quad([(0.204, 0.342), (0.216, 0.338), (0.216, 0.37), (0.204, 0.374)])         # the slant between
quad([(0.204, 0.374), (0.216, 0.37), (0.216, 0.4), (0.204, 0.37)])
# NINTENDO 64, a rule, GAME PAK
d.text(at(0.245, 0.333), "NINTENDO", font=font(0.04), fill=INK)
d.text(at(0.375, 0.326), "64", font=font(0.022), fill=INK)
d.line([at(0.245, 0.378), at(0.40, 0.378)], fill=INK, width=S)
d.text(at(0.245, 0.385), "GAME PAK", font=font(0.04), fill=INK)
# the small print, in made-up letters
random.seed(64)
def gibberish(n):
    return " ".join("".join(random.choice("ABCDEFGHIKLMNOPRSTUVW") for _ in range(random.randint(2, 8))) for _ in range(n))
small = font(0.02)
for i, y in enumerate((0.333, 0.358, 0.383)):
    d.text(at(0.425, y), gibberish(3 if i else 4), font=small, fill=INK)
d.text(at(0.70, 0.333), "NUS-EUR-1", font=font(0.03), fill=INK)
d.text(at(0.645, 0.365), "CE", font=font(0.10), fill=INK)
d.text(at(0.75, 0.43), "D-63760", font=small, fill=INK)
d.text(at(0.75, 0.455), gibberish(1), font=small, fill=INK)
heading = font(0.026)
for title, y0, lines in (("CAUTION", 0.43, 4), ("ATTENTION", 0.565, 4), ("WICHTIG", 0.698, 4)):
    d.text(at(0.177, y0), title, font=heading, fill=INK)
    for n in range(lines):
        y = y0 + 0.03 + n * 0.022
        words = 4 if (title == "CAUTION" and n == 0) else (9 if n % 2 == 0 else 7)    # the first line stops short of the CE mark
        d.text(at(0.177, y), "\u2022 " + gibberish(words), font=small, fill=INK)
for y in (0.557, 0.69):
    d.line([at(0.177, y), at(0.83, y)], fill=INK, width=max(1, S // 2))
back = shrink(back, CART_W, CART_H)
back.save("cart_back.png")
print(f"cart_back.png: {CART_W} x {CART_H}")

# The slot in the bottom of a cartridge, for the 3D cartridges tipped up: dark,
# with the green circuit board's edge and its gold contacts. The menu lays it
# on the cartridge's own plastic. 64 x 16, the slot's length across.
BOTTOM_W, BOTTOM_H = 64, 16
bottom = Image.new("RGB", (BOTTOM_W * S, BOTTOM_H * S), (30, 30, 34))
d = ImageDraw.Draw(bottom)
def bt(x, y):
    return (x * (BOTTOM_W * S - 1), y * (BOTTOM_H * S - 1))
d.rectangle([bt(0.03, 0.30), bt(0.97, 0.74)], fill=(34, 110, 60))                  # the board's edge
for i in range(25):                                                                 # gold contacts
    x = 0.05 + i * 0.0365
    d.rectangle([bt(x, 0.34), bt(x + 0.021, 0.70)], fill=(214, 176, 72))
bottom = shrink(bottom, BOTTOM_W, BOTTOM_H)
bottom.save("cart_bottom.png")
print(f"cart_bottom.png: {BOTTOM_W} x {BOTTOM_H}")
