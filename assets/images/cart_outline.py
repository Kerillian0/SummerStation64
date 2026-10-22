#!/usr/bin/env python3
"""The outline every N64 cartridge shares, for the 3D cartridges.

Traced from the cartridge pictures in the box art pack (all 303 are alike):
straight sides, a flat bottom and an arched top. Points run clockwise
across a 158 x 107 picture, from (0, 0) at its top left to (1, 1) at its
bottom right.

Used by make_icons.py (the cartridge back's shape) and copied into
src/menu/box3d.c, which prints with:

    python3 cart_outline.py
"""

WIDTH, HEIGHT = 158, 107

# The shape as traced: bottom left, up the left side,
# along the arch, down the right side to the bottom right.
TRACED = [(0.0, 1.0), (0.0, 0.16),
          (0.083, 0.094), (0.166, 0.066), (0.248, 0.038), (0.331, 0.019), (0.414, 0.009),
          (0.497, 0.0),
          (0.586, 0.009), (0.669, 0.019), (0.752, 0.038), (0.834, 0.066), (0.917, 0.094),
          (1.0, 0.16), (1.0, 1.0)]


def outline():
    """The outline, as (x, y) from 0 to 1."""
    return list(TRACED)


if __name__ == "__main__":
    points = outline()
    print(f"/* {len(points)} points, made by assets/images/cart_outline.py */")
    for i in range(0, len(points), 4):
        print("    " + " ".join(f"{{ {x:.4f}f, {y:.4f}f }}," for x, y in points[i:i + 4]))
