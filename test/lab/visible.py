"""WHAT IS SEEN, BEFORE ANYTHING IS BUILT -- Novalogic's Voxel Space, asked of an OSM town.

THE IDEA, and it is not about the representation. A twin builds five thousand bodies to draw
eighty. Comanche never had that problem: it marched each screen column outward from the eye
through a height field, keeping a running HORIZON, and drew only what raised it -- so everything
behind a nearer roof cost nothing at all. OSM gives exactly the field that march needs, and gives
nothing else: a footprint and a height, which is 2.5D by construction. There are no overhangs in
the data; the eaves, the cornice and the balcony are what the GENERATOR adds, and it only has to
add them for the bodies the march named.

MEASURED 2026-09-07 on Rothenburg's own extract, 1 054 bodies:

    from a street at 1.7 m      8 bodies ever raise the horizon      0.8 %
    from 26 m up               86                                    8.2 %

    the field itself           840 x 840 cells at 1 m = 5.6 MB, built in 0.6 s from footprints
    the march                  1 280 columns, 535 040 steps, 25 ms vectorised (26x the loop)

So the twin's `buildings` stage -- 137 s and 686 MB of peak, to build 5 709 bodies -- has to
build eight of them.

WHAT THE MARCH DOES AND DOES NOT ANSWER. It answers the MASS: a body whose prism never raises the
horizon is behind something. It cannot see a balcony poking out of a hidden facade, so the field
is DILATED by the deepest relief a generator may add before it is marched -- CLAUDE.md already
makes relief depth a number a generator owes, and this is the second thing that number is for.
It answers buildings; the street is flat and always seen, and vegetation is a scatter with its
own rule.

THE STEP IS THE PIXEL, which is the whole of why the cost is bounded: a column takes
`t * fov / width` at distance t, so the number of steps to the sight limit is logarithmic in it
and not linear. That is Voxel Space's own trick and it is why it ran on a 486.
"""
import math

import numpy as np
import shapely
from shapely.strtree import STRtree

CELL_M = 1.0              # [SET] the field's cell: one metre is a quarter pixel at 300 m
RELIEF_M = 1.20           # [SET] the deepest a generator's own detail may stand off the mass --
                          # a balcony is 1.2 m in RASt 06's own terms and nothing here is deeper


class Field:
    """THE TOWN AS A HEIGHT FIELD, from footprints and heights alone. No mesh is built to make it
    and none is needed to march it."""

    __slots__ = ("cell", "reach", "n", "ident", "top", "ground")

    def __init__(self, bodies, reach_m, cell_m=CELL_M, ground=0.0):
        self.cell, self.reach = float(cell_m), float(reach_m)
        self.n = int(2 * self.reach / self.cell)
        self.ident = np.full((self.n, self.n), -1, dtype=np.int32)
        self.top = np.zeros((self.n, self.n), dtype=np.float32)
        self.ground = float(ground)
        if not bodies:
            return
        step = (np.arange(self.n) + 0.5) * self.cell - self.reach
        gx, gy = np.meshgrid(step, step, indexing="xy")
        probe = shapely.points(gx.ravel(), gy.ravel())
        # THE FIELD IS DILATED BY THE DEEPEST RELIEF a generator may add, so a balcony on an
        # otherwise hidden facade still names its body.
        tree = STRtree([b.poly.buffer(RELIEF_M) for b in bodies])
        pairs = tree.query(probe, predicate="intersects")
        for k in range(pairs.shape[1]):
            cell, body = int(pairs[0, k]), int(pairs[1, k])
            h = float(bodies[body].ridge)
            j, i = divmod(cell, self.n)
            if h > self.top[j, i]:
                self.top[j, i] = h
                self.ident[j, i] = body

    def bytes(self):
        return int(self.ident.nbytes + self.top.nbytes)


def march(field, eye_xy, eye_z, bearing_deg, fov_deg=55.0, width=1280, near_m=2.0):
    """FRONT TO BACK, ONE HORIZON PER COLUMN. Returns the set of body indices that ever raised it.

    Every column steps in lockstep, which is what makes this a compute pass rather than a loop --
    and why the same march is 26 times faster written this way in Python alone."""
    n, cell, reach = field.n, field.cell, field.reach
    rad = math.radians(fov_deg)
    ang = math.radians(bearing_deg) + (np.arange(width) / width - 0.5) * rad
    dx, dy = np.sin(ang), np.cos(ang)
    horizon = np.full(width, -1e9)
    seen = set()
    t = float(near_m)
    while t < reach:
        i = ((eye_xy[0] + dx * t + reach) / cell).astype(np.int32)
        j = ((eye_xy[1] + dy * t + reach) / cell).astype(np.int32)
        ok = (i >= 0) & (i < n) & (j >= 0) & (j < n)
        ci, cj = np.clip(i, 0, n - 1), np.clip(j, 0, n - 1)
        who = np.where(ok, field.ident[cj, ci], -1)
        up = np.where(who >= 0, (field.top[cj, ci] - eye_z) / t, -1e9)
        rise = up > horizon
        horizon = np.where(rise, up, horizon)
        got = who[rise & (who >= 0)]
        if got.size:
            seen.update(int(v) for v in np.unique(got))
        # THE STEP IS THE PIXEL, so the march to the sight limit is logarithmic in it
        t += max(cell, t * rad / width)
    return seen


def rung_for(distance_m, fov_deg=55.0, width=1280, detail_m=(0.10, 0.30, 1.00)):
    """WHICH RUNG A BODY EARNS, from the size of a pixel where it stands. `detail_m` is what each
    rung's finest feature measures -- a reveal at L2 is 0.10 m, an element at L1 0.30 m, a mass
    at L0 1.00 m -- and a rung whose feature is under a pixel is a rung nobody can see."""
    px = max(distance_m, 1e-6) * math.radians(fov_deg) / width
    if px <= detail_m[0]:
        return 3
    if px <= detail_m[1]:
        return 2
    if px <= detail_m[2]:
        return 1
    return 0
