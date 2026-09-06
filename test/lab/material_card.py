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


def card(names=None, out=None, eye_m=1.65, back_m=4.6):
    names = names or [n for n in sorted(stock.STOCK) if n != "water"]
    parts, looks = {}, {}
    span = len(names) * (TILE_M + GAP_M)
    for k, name in enumerate(names):
        x0 = -span / 2 + k * (TILE_M + GAP_M)
        x1 = x0 + TILE_M
        floor = [(x0, 0.0, 0.0), (x1, 0.0, 0.0), (x1, TILE_M, 0.0), (x0, TILE_M, 0.0)]
        wall = [(x0, TILE_M, 0.0), (x1, TILE_M, 0.0), (x1, TILE_M, TILE_M), (x0, TILE_M, TILE_M)]
        parts[name] = (floor + wall, [(0, 1, 2), (0, 2, 3), (4, 5, 6), (4, 6, 7)])
        looks[name] = stock.STOCK[name]
    cam = lab_camera.Camera(lat=49.4, lon=8.7, agl_m=eye_m, bearing_deg=0.0,
                            pitch_deg=-math.degrees(math.atan2(eye_m - TILE_M / 2, back_m)),
                            fov_deg=64.0, width=2200, height=900)
    shifted = {r: ([(x, y - back_m, z) for (x, y, z) in v], t) for r, (v, t) in parts.items()}
    out = out or (OUT / "materials.png")
    out.parent.mkdir(parents=True, exist_ok=True)
    # the light RAKES: a relief that only shows at noon is a relief nobody sees
    blend.render(shifted, cam, np.array([0.86, 0.36, 0.36]), str(out), samples=128, looks=looks)
    return out


def main(argv):
    names = argv or None
    shot = card(names)
    red = []
    from PIL import Image
    img = np.asarray(Image.open(shot).convert("RGB")).astype(float) / 255.0
    # THE CONTROL THAT MATTERS HERE IS THAT THE TILES DIFFER. A card whose neighbours read the
    # same is a card that proves nothing, whatever each albedo says on paper.
    cols = img.mean(axis=0).mean(axis=1)
    flat = float(np.abs(np.diff(cols)).mean())
    if flat < 1e-3:
        red.append(f"uniform({flat:.4f})")
    publish.take("look", "materials", shot, red)
    print(f"materials  {'RED ' + ','.join(red) if red else 'ok':20s} "
          f"{len(stock.STOCK)} materials  column contrast {flat:.4f}  -> {shot}")
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
