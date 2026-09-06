"""EVERY STOCK MATERIAL, SIDE BY SIDE, IN RAKING LIGHT.

A material is judged against its NEIGHBOUR and never on its own: CLAUDE.md's rule is that the
ratio is what a viewer reads, and a ratio needs two things in one frame. So the card stands every
material in the stock as a tile, in one row of light, at a distance a player actually stands at --
and it is looked at as a SET, which is the only way an albedo that is out of band shows.

Each tile is a 1.2 m square laid flat plus a 1.2 m square standing up, because a laid surface
courses differently on the two and a bond that is right on a wall can be wrong on a pavement.
"""
import math
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import blend  # noqa: E402
import camera as lab_camera  # noqa: E402
import materials as stock  # noqa: E402
import publish  # noqa: E402

OUT = pathlib.Path(__import__("os").environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "look"
TILE_M = 1.20             # [SET] big enough that a 300 mm slab reads as a slab
GAP_M = 0.10
ROW_M = 2.60              # [SET] a tile lies flat AND stands up, so a row needs both depths


def centres(names, cols=6, back=None):
    """Where each tile's FLOOR centre stands, in the frame the render is given. The measurement
    reads the picture at these points and nowhere else."""
    rows = (len(names) + cols - 1) // cols
    pitch = TILE_M + GAP_M
    deep = rows * ROW_M
    back = 5.0 if back is None else back
    out = []
    for k, name in enumerate(names):
        cx = (k % cols - (cols - 1) / 2.0) * pitch
        cy = (rows - 1 - k // cols) * ROW_M
        out.append((name, (cx, cy + TILE_M * 0.5 + back, 0.0)))
    return out


def card(names=None, out=None, cols=6):
    """Every stock material as a tile in a GRID, each one laid flat AND standing up.

    A single ROW put twenty-nine tiles across thirty-eight metres and a camera that could hold
    them all made each one forty pixels wide -- a card nobody can read is a card nobody looks at
    (rendered and looked at, 2026-09-06). The grid is the same argument as a contact sheet's."""
    names = names or [n for n in sorted(stock.STOCK) if n != "water"]
    parts, looks = {}, {}
    rows = (len(names) + cols - 1) // cols
    pitch = TILE_M + GAP_M
    for k, name in enumerate(names):
        cx = (k % cols - (cols - 1) / 2.0) * pitch
        cy = (rows - 1 - k // cols) * ROW_M
        x0, x1 = cx - TILE_M / 2, cx + TILE_M / 2
        y0, y1 = cy, cy + TILE_M
        floor = [(x0, y0, 0.0), (x1, y0, 0.0), (x1, y1, 0.0), (x0, y1, 0.0)]
        wall = [(x0, y1, 0.0), (x1, y1, 0.0), (x1, y1, TILE_M), (x0, y1, TILE_M)]
        parts[name] = (floor + wall, [(0, 1, 2), (0, 2, 3), (4, 5, 6), (4, 6, 7)])
        looks[name] = stock.STOCK[name]
    # A CARD WITH NO GROUND UNDER IT IS A CARD IN THE DARK: the tiles had nothing to bounce off
    # and every albedo read as its own fraction of black
    r = max(cols, rows) * pitch * 3.0
    parts["_deck"] = ([(-r, -r, -0.002), (r, -r, -0.002), (r, r, -0.002), (-r, r, -0.002)],
                      [(0, 1, 2), (0, 2, 3)])
    looks["_deck"] = stock.STOCK["concrete"]
    # THE WHOLE GRID HAS TO BE IN THE FRAME, so the camera is placed from its own extent and
    # never from a guess: two of five rows in view is a card that shows what it happens to reach
    deep = rows * ROW_M
    back, high = 5.0, max(12.0, deep * 1.15)
    cam = lab_camera.Camera(lat=49.4, lon=8.7, agl_m=high, bearing_deg=0.0,
                            pitch_deg=-math.degrees(math.atan2(high, back + deep / 2.0)),
                            fov_deg=58.0, width=2000, height=1400)
    shifted = {r_: ([(x, y + back, z) for (x, y, z) in v], t) for r_, (v, t) in parts.items()}
    out = out or (OUT / "materials.png")
    out.parent.mkdir(parents=True, exist_ok=True)
    # the light RAKES: a relief that only shows at noon is a relief nobody sees
    blend.render(shifted, cam, np.array([0.62, 0.30, 0.72]), str(out), samples=128, looks=looks)
    return out, cam, names


def main(argv):
    names = argv or None
    shot, cam, names = card(names)
    red = []
    from PIL import Image
    img = np.asarray(Image.open(shot).convert("RGB")).astype(float) / 255.0
    # THE CONTROL THAT MATTERS HERE IS THAT THE TILES DIFFER. A card whose neighbours read the
    # same is a card that proves nothing, whatever each albedo says on paper.
    # THE CLAIM IS THAT THE TILES DIFFER FROM ONE ANOTHER, so the measurement reads THE TILES
    # and never the picture. A coarse block spread over the whole frame reads the GEOMETRY --
    # tile against background, wall against floor, shadow against light -- and scored 0.031 on a
    # control where every material was the same grey (measured 2026-09-06), which is a false
    # proof. Every tile's floor is lit the same and faces the same way, so the only thing that
    # can separate them is albedo: the camera projects each centre and the spread of THOSE is
    # the number.
    reads = []
    for (name, at) in centres(names):
        px = cam.project(at)
        if px is None:
            continue
        x, y = int(px[0]), int(px[1])
        if not (8 <= x < img.shape[1] - 8 and 8 <= y < img.shape[0] - 8):
            continue
        reads.append(float(img[y - 6:y + 7, x - 6:x + 7].mean()))
    flat = float(np.std(reads)) if len(reads) > 3 else 0.0
    if len(reads) < len(names):
        red.append(f"offscreen({len(names) - len(reads)} tiles)")
    if flat < 0.02:
        red.append(f"uniform({flat:.4f})")
    publish.take("look", "materials", shot, red)
    print(f"materials  {'RED ' + ','.join(red) if red else 'ok':20s} "
          f"{len(stock.STOCK)} materials  column contrast {flat:.4f}  -> {shot}")
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
