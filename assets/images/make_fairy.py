#!/usr/bin/env python3
"""Draws Spark, the menu assistant's own fairy (assistant.c). Drawn from
scratch by the code below: a glowing coral orb with a soft halo and two
pale wings, in the sunset colors of the menu's name.

Needs Pillow (in the dev container: sudo apt-get install python3-pil).
Run it from this folder:

    python3 make_fairy.py

fairy.png: two 40 x 40 pictures side by side (wings up, wings down), full
color with soft edges (RGBA32 in the menu, 12.5 KB), loaded only while the
assistant is on screen. A skin's own picture (a fairy of the player's) can
take its place.
"""
import math

from PIL import Image, ImageDraw, ImageFilter

SIZE = 40
SCALE = 8
CORE = (255, 246, 236)      # nearly white in the middle
BODY = (255, 128, 104)      # coral
HALO = (255, 92, 120)       # sunset pink
WING = (255, 226, 214)      # pale peach


def frame(wings_up):
    big = SIZE * SCALE
    c = big / 2

    # The halo: a soft pink glow fading out to nothing.
    halo = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    px = halo.load()
    for y in range(big):
        for x in range(big):
            d = math.hypot(x - c, y - c) / (big / 2)
            if d < 1.0:
                a = (1.0 - d) ** 2.2
                px[x, y] = HALO + (int(200 * a),)

    # Two wings, as long ellipses tilted up or down from the middle.
    wings = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    w = ImageDraw.Draw(wings)
    tilt = -28 if wings_up else 18
    for side in (-1, 1):
        wing = Image.new("RGBA", (big, big), (0, 0, 0, 0))
        ImageDraw.Draw(wing).ellipse([c - 7 * SCALE, c - 3 * SCALE, c + 7 * SCALE, c + 3 * SCALE], fill=WING + (170,))
        wing = wing.rotate(-tilt * side, center=(c, c))
        dx = int(side * 9 * SCALE)
        dy = int((-3 if wings_up else 1) * SCALE)
        moved = Image.new("RGBA", (big, big), (0, 0, 0, 0))
        moved.paste(wing, (dx, dy), wing)
        wings = Image.alpha_composite(wings, moved)

    # The body: a coral orb, white at its heart.
    body = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    bp = body.load()
    radius = 7 * SCALE
    for y in range(big):
        for x in range(big):
            d = math.hypot(x - c, y - c) / radius
            if d < 1.0:
                t = d ** 1.5
                color = tuple(int(CORE[i] * (1 - t) + BODY[i] * t) for i in range(3))
                bp[x, y] = color + (255,)

    out = Image.alpha_composite(halo, wings)
    out = Image.alpha_composite(out, body.filter(ImageFilter.GaussianBlur(SCALE * 0.6)))
    return out.resize((SIZE, SIZE), Image.LANCZOS)


sheet = Image.new("RGBA", (SIZE * 2, SIZE), (0, 0, 0, 0))
sheet.paste(frame(True), (0, 0))
sheet.paste(frame(False), (SIZE, 0))
sheet.save("fairy.png")
print(f"fairy.png: two {SIZE} x {SIZE} pictures")
