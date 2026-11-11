#!/usr/bin/env python3
"""Draws Spark, the menu assistant's own fairy (assistant.c). Drawn from
scratch by the code below: a glowing orb with two pale wings.

Needs Pillow (in the dev container: sudo apt-get install python3-pil).
Run it from this folder:

    python3 make_fairy.py

fairy.png: two 40 x 40 pictures side by side (wings up, wings down), in
shades of white with soft edges (IA16 in the menu: 6.4 KB). Each pixel says
how close to white it is: the menu paints it in the player's fairy colour
(Settings > Look > Fairy Look > Fairy Color), its darkest parts the colour
itself and its heart white. Loaded only while the assistant is on screen.

glow.png: a 32 x 32 soft round glow in 256 shades (I8, 1 KB), tinted and
drawn behind every fairy, Navi and Tatl too.

sparkle.png: a 16 x 16 four-pointed star with a soft glow, white in 16
shades (128 bytes), tinted by the menu: the sparkles where a new line
sweeps in while the assistant chats (Showy).
"""
import math

from PIL import Image, ImageDraw, ImageFilter

SIZE = 40
SCALE = 8
WING = 215      # how white the wings are (0: the fairy's colour, 255: white)


def frame(wings_up):
    """One picture: luminance = whiteness, alpha = coverage."""
    big = SIZE * SCALE
    c = big / 2

    # Two wings, as long ellipses tilted up or down from the middle.
    wings = Image.new("LA", (big, big), (0, 0))
    tilt = -28 if wings_up else 18
    for side in (-1, 1):
        wing = Image.new("LA", (big, big), (0, 0))
        ImageDraw.Draw(wing).ellipse([c - 7 * SCALE, c - 3 * SCALE, c + 7 * SCALE, c + 3 * SCALE], fill=(WING, 170))
        wing = wing.rotate(-tilt * side, center=(c, c))
        dx = int(side * 9 * SCALE)
        dy = int((-3 if wings_up else 1) * SCALE)
        moved = Image.new("LA", (big, big), (0, 0))
        moved.paste(wing, (dx, dy), wing)
        wings = Image.alpha_composite(wings.convert("RGBA"), moved.convert("RGBA")).convert("LA")

    # The body: an orb in the fairy's colour, white at its heart.
    body = Image.new("LA", (big, big), (0, 0))
    bp = body.load()
    radius = 7 * SCALE
    for y in range(big):
        for x in range(big):
            d = math.hypot(x - c, y - c) / radius
            if d < 1.0:
                bp[x, y] = (int(255 * (1 - d ** 1.5)), 255)

    out = Image.alpha_composite(wings.convert("RGBA"), body.convert("RGBA").filter(ImageFilter.GaussianBlur(SCALE * 0.6)))
    return out.resize((SIZE, SIZE), Image.LANCZOS).convert("LA")


sheet = Image.new("LA", (SIZE * 2, SIZE), (0, 0))
sheet.paste(frame(True), (0, 0))
sheet.paste(frame(False), (SIZE, 0))
sheet.save("fairy.png")
print(f"fairy.png: two {SIZE} x {SIZE} pictures")

# The glow behind a fairy: bright in the middle, fading to nothing.
GLOW = 32
glow = Image.new("L", (GLOW, GLOW), 0)
gp = glow.load()
for y in range(GLOW):
    for x in range(GLOW):
        d = math.hypot(x + 0.5 - GLOW / 2, y + 0.5 - GLOW / 2) / (GLOW / 2)
        gp[x, y] = int(255 * max(0.0, 1.0 - d) ** 2.0)
glow.save("glow.png")
print(f"glow.png: {GLOW} x {GLOW}")


# The sparkle: a four-pointed star (two thin diamonds crossed) over a soft glow.
SPARK = 16
big = SPARK * SCALE
c = big / 2
spark = Image.new("L", (big, big), 0)
sp = spark.load()
for y in range(big):
    for x in range(big):
        dx, dy = abs(x - c) / c, abs(y - c) / c
        glow = max(0.0, 1.0 - math.hypot(dx, dy) * 1.6) ** 2
        star = 1.0 if (dx + dy * 6 < 1.0 or dy + dx * 6 < 1.0) else 0.0
        sp[x, y] = int(255 * min(1.0, star + glow))
spark = spark.resize((SPARK, SPARK), Image.LANCZOS).point(lambda v: (v // 17) * 17)
spark.save("sparkle.png")
print(f"sparkle.png: {SPARK} x {SPARK}")
