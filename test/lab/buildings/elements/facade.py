"""A WALL IS A FACADE WITH HOLES IN IT, and that is the difference between a model and a building.

The relief elements put a reveal, a sill and a lintel around every opening and the openings were
still invisible, because the wall behind them is a SOLID QUAD: the glass sat inside solid geometry
and nothing could be seen of it (rendered and looked at, 2026-09-06). No amount of trim makes a
hole. A window is a HOLE with sides, and the sides are what the eye reads depth from.

So at the rung where a viewer is close enough to see it, the wall face is meshed as a polygon WITH
HOLES -- Shewchuk's constrained triangulation, the same tool the floor already uses -- and each
hole carries:

    the four REVEAL faces, running back into the wall by the frame's own depth
    the GLASS at the back of the recess, one quad
    the FRAME, a thin box around the glass

This is a REPLACEMENT for the wall's own quad and not an addition, and that is correct: a hole is
not something you add. The mass keeps its solid wall at the far rungs, which is what a game does
too -- the distant LOD is a textured quad and the near one is cut.
"""
import numpy as np

import sys as _sys, pathlib as _pl
_sys.path.insert(0, str(_pl.Path(__file__).resolve().parents[1]))
from base3 import cross3  # noqa: E402

from .base import _outward

FRAME_M = 0.06             # [SET] the sash's own face, what stands between the glass and the jamb


SHOP_EPOCHS = ("gruenderzeit", "jugendstil", "commercial", "hall", "interwar",
               "contemporary", "late20")


def shopfront(ctx):
    """WHETHER THE GROUND FLOOR IS A SHOP, and what its opening is. In every one of the
    references the street level is a different wall from the floors above it -- a stallriser, one
    wide opening per bay to the fascia, and a door. Cutting a WINDOW there and then drawing a
    shopfront behind it leaves the shopfront inside solid geometry, which is the same defect the
    reveal had (looked at, 2026-09-06)."""
    return ctx.epoch in SHOP_EPOCHS and ctx.street and int(ctx.levels) >= 2


def openings_of(ctx):
    """The rectangles this wall is holed by, in the wall's own frame: (s0, z0, s1, z1, storey).

    THE STOREYS ARE NOT ALL THE SAME. `storeys.of` gives the facade its hierarchy -- a ground
    floor of its own height, a BELETAGE taller than the rest with taller windows, the standard
    floors diminishing a few percent a floor, and a squat attic under the eaves. Five identical
    floors was the loudest thing left on a generated block, and no ornament repairs it."""
    from . import storeys as rhythm
    out = []
    n = max(1, int(ctx.bays))
    step = ctx.place.length / n
    shop = shopfront(ctx)
    door_bay = n // 2 if ctx.street else -1
    floors = rhythm.of(ctx)
    if ctx.street and floors:
        seat = max(0.0, ctx.floor_over_ground_m)
        high = min(2.35, floors[0].height * 0.72)
        mid = (door_bay + 0.5) * step
        out.append((mid - 0.70, seat + 0.02, mid + 0.70, seat + high, floors[0]))
    for bay in range(n):
        mid = (bay + 0.5) * step
        for floor in floors:
            w = min(floor.win_w, step * 0.68)
            if floor.at == 0 and shop:
                if bay == door_bay:
                    continue                      # the door's own bay, cut by the door element
                lo, hi = 0.60, floor.height - 0.55  # the stallriser and the fascia
                if hi - lo < 0.6:
                    continue
                out.append((mid - step * 0.42, lo, mid + step * 0.42, hi, floor))
                continue
            if floor.at == 0 and bay == door_bay and ctx.street:
                continue                          # the door stands here
            z = floor.z0 + floor.sill
            if z + floor.win_h > ctx.place.height - 0.25 or z < 0.12:
                continue
            out.append((mid - w / 2, z, mid + w / 2, z + floor.win_h, floor))
    return out


SPLASH_M = 0.50           # [SET] how high rain bounces off the ground and wets a wall


# SPLASH IS A SHADER FUNCTION AND NOT A VERTEX FIELD, and it took three goes to say so. A
# run-off STREAK is 0.2 m wide on a 15 m wall and interpolated across whole storeys when it was
# sampled at four points per sill. The SPLASH band is 0.5 m deep and looked carriable -- but I23
# counted 198 to 392 saturated edges on every one of the 41 building cases, because a wall quad
# runs from the ground to the eaves in one step and a party wall, a plinth box and a reveal have
# no rows in between. Both are FINE-SCALE surface features on COARSE geometry, which is the
# definition of a fragment-stage job. The height over the ground is what the shader needs and the
# building already knows it: one number per body, in the material.
SPLASH_M = 0.50           # [SET] how high rain bounces off the ground and wets what stands in it


def holed_wall(place, holes, depth):
    """The wall face with its holes, plus the reveal, the frame and the glass in each.

    Returns [(role, vertices, triangles, field)]; `field` is always None here -- see above.
    `depth` is how far the glass sits back."""
    import triangle as tri
    pts = [(0.0, 0.0), (place.length, 0.0), (place.length, place.height), (0.0, place.height)]
    segs = [(0, 1), (1, 2), (2, 3), (3, 0)]
    inside = []
    holes = [h[:4] for h in holes]
    for (s0, z0, s1, z1) in holes:
        base = len(pts)
        pts += [(s0, z0), (s1, z0), (s1, z1), (s0, z1)]
        segs += [(base, base + 1), (base + 1, base + 2), (base + 2, base + 3), (base + 3, base)]
        inside.append(((s0 + s1) / 2, (z0 + z1) / 2))

    holes = [h[:4] for h in holes]
    spec = {"vertices": np.array(pts, dtype=float), "segments": np.array(segs, dtype=np.int32)}
    if inside:
        spec["holes"] = np.array(inside, dtype=float)
    got = tri.triangulate(spec, "p")
    if "triangles" not in got:
        return []
    face_v = [place.at(float(s), float(z), 0.0) for (s, z) in got["vertices"]]
    face_t = []
    for (a, b, c) in got["triangles"]:
        p, q, r = (np.asarray(face_v[i]) for i in (a, b, c))
        n = cross3(q - p, r - p)
        face_t.append((int(a), int(b), int(c)) if float(np.dot(n, place.out)) > 0.0
                      else (int(a), int(c), int(b)))
    out = [("wall", face_v, face_t, None)]
    for (s0, z0, s1, z1) in holes:
        # the REVEAL: four faces running from the wall plane back to the glass
        v = [place.at(s, z, d) for (s, z, d) in
             ((s0, z0, 0.0), (s1, z0, 0.0), (s1, z1, 0.0), (s0, z1, 0.0),
              (s0, z0, -depth), (s1, z0, -depth), (s1, z1, -depth), (s0, z1, -depth))]
        t = [(0, 4, 5), (0, 5, 1), (1, 5, 6), (1, 6, 2),
             (2, 6, 7), (2, 7, 3), (3, 7, 4), (3, 4, 0)]
        mid = np.mean(v, axis=0)
        wound = []
        for (a, b, c) in t:
            n = cross3(np.asarray(v[b]) - v[a], np.asarray(v[c]) - v[a])
            # a reveal faces INTO the opening, so its normal points at the hole's own axis
            wound.append((a, b, c) if float(np.dot(n, mid - np.asarray(v[a]))) > 0.0 else (a, c, b))
        out.append(("wall", v, wound, None))
        f = FRAME_M
        out.append(("glass", *_quad(place, s0 + f, z0 + f, s1 - f, z1 - f, -depth), None))
        for (a0, b0, a1, b1) in ((s0, z0, s0 + f, z1), (s1 - f, z0, s1, z1),
                                 (s0 + f, z1 - f, s1 - f, z1), (s0 + f, z0, s1 - f, z0 + f)):
            out.append(("wood", *_quad(place, a0, b0, a1, b1, -depth + 0.01), None))
        # THE SASH ITSELF: a MEETING STILE down the middle, a TRANSOM across, and the glazing
        # bars in each light. A window read as one dark pane is the difference between a hole and
        # a window, and it is the cheapest detail on the whole facade -- four quads.
        if (s1 - s0) > 0.75 and (z1 - z0) > 0.9:
            mid = 0.5 * (s0 + s1)
            out.append(("wood", *_quad(place, mid - f * 0.55, z0 + f, mid + f * 0.55, z1 - f,
                                       -depth + 0.012), None))
            kaempfer = z0 + (z1 - z0) * 0.62
            out.append(("wood", *_quad(place, s0 + f, kaempfer - f * 0.5, s1 - f,
                                       kaempfer + f * 0.5, -depth + 0.012), None))
            for u in (0.5,):
                zb = z0 + f + (kaempfer - z0 - f) * u
                out.append(("wood", *_quad(place, s0 + f, zb - f * 0.35, s1 - f, zb + f * 0.35,
                                           -depth + 0.010), None))
    return out


def _quad(place, s0, z0, s1, z1, d):
    v = [place.at(s0, z0, d), place.at(s1, z0, d), place.at(s1, z1, d), place.at(s0, z1, d)]
    p, q, r = (np.asarray(x) for x in v[:3])
    n = cross3(q - p, r - p)
    t = [(0, 1, 2), (0, 2, 3)] if float(np.dot(n, place.out)) > 0.0 else [(0, 2, 1), (0, 3, 2)]
    return v, t
