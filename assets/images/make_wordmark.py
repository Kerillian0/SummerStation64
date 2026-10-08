#!/usr/bin/env python3
"""Draws wordmark.png, the "SUMMERSTATION 64" title shown in the intro.

An 80s-style title: the name in chunky letters with a pink-to-yellow face,
a white rim and a slab of depth beneath, and "64" across it in neon
handwriting. Drawn here from two freely licensed fonts kept in wordmark/
(Anton and Mr Dafoe, both under the SIL Open Font License; see the .txt
files beside them). The reference the project owner gave was used for the
style only; nothing is copied from it.

Needs Pillow. Run from this folder:  python3 make_wordmark.py
"""
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

W, H = 448, 124         # size on the menu's screen
S = 3                   # draw this many times larger, then shrink
NAME = "SUMMERSTATION"
NUMBER = "64"


def gradient(size, stops):
    """Top-to-bottom blend through (position 0..1, color) stops."""
    w, h = size
    column = Image.new("RGB", (1, h))
    for y in range(h):
        t = y / max(1, h - 1)
        for (p0, c0), (p1, c1) in zip(stops, stops[1:]):
            if p0 <= t <= p1:
                k = (t - p0) / max(1e-6, p1 - p0)
                column.putpixel((0, y), tuple(round(c0[i] + (c1[i] - c0[i]) * k) for i in range(3)))
                break
    return column.resize((w, h))


def text_mask(text, font, size, spacing=0, stroke=0):
    """White-on-black picture of the text, letters `spacing` apart."""
    mask = Image.new("L", size, 0)
    draw = ImageDraw.Draw(mask)
    widths = [draw.textlength(ch, font=font) for ch in text]
    total = sum(widths) + spacing * (len(text) - 1)
    x = (size[0] - total) / 2
    for ch, w in zip(text, widths):
        draw.text((x, 0), ch, font=font, fill=255, stroke_width=stroke, stroke_fill=255)
        x += w + spacing
    return mask


canvas = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))

# --- the name ---------------------------------------------------------------
font = ImageFont.truetype("wordmark/Anton-Regular.ttf", 62 * S)
layer = (W * S, 90 * S)
face = text_mask(NAME, font, layer, spacing=2 * S)
box = face.getbbox()
top = 6 * S - box[1]            # where the layer goes so the letters start 6 down

def paste(color_image, mask, dx=0, dy=0):
    canvas.paste(color_image, (dx, top + dy), mask)

# A dark edge all round, so it stands out on any theme's background.
edge = text_mask(NAME, font, layer, spacing=2 * S, stroke=3 * S)
DEPTH = 9 * S
for d in range(DEPTH, -1, -1):
    paste(Image.new("RGB", layer, (44, 8, 70)), edge, d // 3, d)
# The slab of depth: orange under the face sinking to magenta.
for d in range(DEPTH, 0, -1):
    k = d / DEPTH
    color = (round(255 - 70 * k), round(170 - 130 * k), round(40 + 90 * k))
    paste(Image.new("RGB", layer, color), text_mask(NAME, font, layer, spacing=2 * S, stroke=1 * S), d // 3, d)
# White rim, then the face inside it.
paste(Image.new("RGB", layer, (255, 255, 255)), text_mask(NAME, font, layer, spacing=2 * S, stroke=1 * S))
y0, y1 = box[1] / layer[1], box[3] / layer[1]
mid = (y0 + y1) / 2
blend = gradient(layer, [
    (0.0, (255, 110, 200)), (y0, (255, 110, 200)),
    (mid - 0.02, (255, 214, 232)),      # pale at the "horizon"...
    (mid, (255, 255, 255)),
    (mid + 0.02, (255, 236, 150)),      # ...then warm below it
    (y1, (255, 150, 40)), (1.0, (255, 150, 40)),
])
inner = face.filter(ImageFilter.MinFilter(2 * (S // 2) + 3))   # the face, a little inside the rim
paste(blend, inner)

# --- the number, in neon ----------------------------------------------------
script = ImageFont.truetype("wordmark/MrDafoe-Regular.ttf", 78 * S)
num_layer = (150 * S, 110 * S)
num = Image.new("L", num_layer, 0)
ImageDraw.Draw(num).text((14 * S, 4 * S), NUMBER, font=script, fill=255)
num = num.rotate(7, resample=Image.BICUBIC)
where = (W * S - num_layer[0] - 6 * S, 26 * S)

glow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
wide = num.filter(ImageFilter.MaxFilter(4 * S + 1)).filter(ImageFilter.GaussianBlur(4 * S))
glow.paste(Image.new("RGBA", num_layer, (255, 40, 200, 255)), where, wide.point(lambda v: min(255, int(v * 0.9))))
canvas = Image.alpha_composite(canvas, glow)
dark = num.filter(ImageFilter.MaxFilter(2 * S + 1))
canvas.paste(Image.new("RGB", num_layer, (120, 0, 110)), where, dark)
tube = num.filter(ImageFilter.MaxFilter(S if S % 2 else S + 1))
canvas.paste(Image.new("RGB", num_layer, (255, 120, 230)), where, tube)
core = num.filter(ImageFilter.MinFilter(S if S % 2 else S + 1))
canvas.paste(Image.new("RGB", num_layer, (255, 240, 255)), where, core)

out = canvas.resize((W, H), Image.LANCZOS)
out.save("wordmark.png")
print(f"wordmark.png: {W} x {H}")
