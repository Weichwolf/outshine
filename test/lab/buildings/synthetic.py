"""The building bed: an OSM footprint plus tags plus the DEM, made into a closed body.

    python3 test/lab/buildings/synthetic.py [case ...]

What a building IS here follows OSM and nothing else: the footprint's way, `building:levels`
or `height` for how tall, `min_height` for where it starts, `roof:shape` and `roof:height` for
what sits on top. What the data does not say, a rule answers, and every rule carries its
origin. What comes out is a CLOSED body -- walls from the pad to the eaves, a roof, and a
skirt that reaches the ground on every side -- checked as a mesh, not as a picture.

THE ROOF IS A DISTANCE FUNCTION, and that is the whole of it: a hipped roof of equal pitch is
z(p) = tan(pitch) * dist(p, boundary), whose ridge set is the polygon's STRAIGHT SKELETON.
Every other shape is a small variation -- a gable clamps the distance along one axis, a
pyramid takes the maximum, a skillion is one plane, a mansard is two slopes in series. So the
roofs need no skeleton library and no special cases: they need one function and its level
sets, which is also why they mesh without cracks.
"""
import math
import os
import pathlib
import sys

import numpy as np
from shapely.geometry import Point, Polygon
from shapely.ops import unary_union

OUT = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "buildings"

LEVEL_M = 3.0             # [SET] OSM wiki's building:levels convention, and GHS-BUILT's storey
ROOF_PITCH = math.radians(35.0)   # [SET] the median pitch of a European gabled roof
EAVES_M = 0.4             # [SET] the eaves' overhang past the wall
WELD_M = 1e-3
FOOT_M = 0.5              # [SET] the wall is buried this far, which is what a foundation is
FREEBOARD_M = 0.3         # [SET] a deck over water stands this far above it


# ----------------------------------------------------------------------------- ground

class Ground:
    def __init__(self, fn, water=None):
        self.fn = fn
        self.water = water

    def at(self, x, y):
        return self.fn(x, y)


GROUNDS = {
    "G1-flat": lambda: Ground(lambda x, y: 100.0),
    "G2-cross15": lambda: Ground(lambda x, y: 100.0 + 0.15 * y),
    "G3-cross40": lambda: Ground(lambda x, y: 100.0 + 0.40 * y),
    "G4-terrace": lambda: Ground(lambda x, y: 100.0 + 3.0 * math.floor(y / 8.0)),
    "G5-water": lambda: Ground(lambda x, y: 96.0, water=100.0),
}


# ----------------------------------------------------------------------------- footprints

def f_rect(w=12.0, d=8.0):
    return Polygon([(-w / 2, -d / 2), (w / 2, -d / 2), (w / 2, d / 2), (-w / 2, d / 2)])


def f_l():
    return Polygon([(-10, -6), (6, -6), (6, 0), (10, 0), (10, 6), (-10, 6)])


def f_u():
    return Polygon([(-12, -8), (12, -8), (12, 8), (6, 8), (6, -2), (-6, -2), (-6, 8), (-12, 8)])


def f_courtyard():
    outer = Polygon([(-14, -10), (14, -10), (14, 10), (-14, 10)])
    inner = Polygon([(-6, -4), (6, -4), (6, 4), (-6, 4)])
    return outer.difference(inner)


def f_round(n=24, r=7.0):
    return Polygon([(r * math.cos(2 * math.pi * k / n), r * math.sin(2 * math.pi * k / n)) for k in range(n)])


def f_thin():
    return Polygon([(-20, -1.5), (20, -1.5), (20, 1.5), (-20, 1.5)])


def f_tower():
    return f_rect(14.0, 14.0)


FOOTPRINTS = {
    "F1-rect": f_rect,
    "F2-L": f_l,
    "F3-U": f_u,
    "F4-courtyard": f_courtyard,
    "F5-round": f_round,
    "F6-thin": f_thin,
    "F7-tower": f_tower,
}


# ----------------------------------------------------------------------------- the roof

def roof_height_at(poly, x, y, shape, eaves_h, ridge_h, axis=None):
    """The roof's height above the eaves at (x, y). One distance function and its variations."""
    d = poly.exterior.distance(Point(x, y))
    for ring in poly.interiors:
        d = min(d, ring.distance(Point(x, y)))
    if shape == "flat":
        return 0.0
    if shape == "pyramidal":
        # the apex over the centroid: the distance function normalised by its own maximum
        return (ridge_h - eaves_h) * d / max(roof_inradius(poly), 1e-6)
    if shape == "hipped":
        rise = min(d * math.tan(ROOF_PITCH), ridge_h - eaves_h)
        return rise
    if shape == "gabled":
        # the distance to the two LONG sides only: the roof rises to a ridge along the long axis
        u, v = axis
        across = abs((x - poly.centroid.x) * v[0] + (y - poly.centroid.y) * v[1])
        half = axis_half(poly, v)
        rise = (half - across) * math.tan(ROOF_PITCH)
        return max(0.0, min(rise, ridge_h - eaves_h))
    if shape == "skillion":
        u, v = axis
        across = (x - poly.centroid.x) * v[0] + (y - poly.centroid.y) * v[1]
        half = axis_half(poly, v)
        return (ridge_h - eaves_h) * (across + half) / max(2 * half, 1e-6)
    if shape == "mansard":
        # two pitches: steep to a third of the rise, then shallow -- the distance function again
        steep = min(d * math.tan(math.radians(70.0)), (ridge_h - eaves_h) * 0.6)
        shallow = (ridge_h - eaves_h) * 0.6 + max(0.0, d - (ridge_h - eaves_h) * 0.6 / math.tan(math.radians(70.0))) * math.tan(math.radians(20.0))
        return min(max(steep, 0.0) if d * math.tan(math.radians(70.0)) < (ridge_h - eaves_h) * 0.6 else shallow, ridge_h - eaves_h)
    if shape == "dome":
        r = roof_inradius(poly)
        return (ridge_h - eaves_h) * math.sqrt(max(0.0, 1.0 - ((r - d) / max(r, 1e-6)) ** 2))
    return 0.0


def roof_inradius(poly, tol=1e-4):
    """The largest distance from the boundary to a point inside: the roof's own scale, and the
    height of a hipped apex divided by tan(pitch). It is the largest d for which the inward
    offset is still non-empty, found by bisection -- a grid found it to half a cell and read the
    apex 0.13 m short (measured). Exact to `tol` and independent of the polygon's shape."""
    lo, hi = 0.0, max(poly.bounds[2] - poly.bounds[0], poly.bounds[3] - poly.bounds[1])
    for _ in range(40):
        mid = 0.5 * (lo + hi)
        if poly.buffer(-mid, join_style=2).is_empty:
            hi = mid
        else:
            lo = mid
        if hi - lo < tol:
            break
    return lo


def principal_axis(poly):
    """The footprint's long axis, from the minimum rotated rectangle -- what a ridge follows."""
    box = poly.minimum_rotated_rectangle
    pts = list(box.exterior.coords)[:-1]
    edges = [(math.dist(pts[k], pts[(k + 1) % 4]), pts[k], pts[(k + 1) % 4]) for k in range(4)]
    edges.sort(key=lambda e: -e[0])
    (_, a, b) = edges[0]
    d = (b[0] - a[0], b[1] - a[1])
    n = math.hypot(*d) or 1.0
    u = (d[0] / n, d[1] / n)
    return u, (-u[1], u[0])


def axis_half(poly, v):
    pts = list(poly.exterior.coords)
    c = poly.centroid
    return max(abs((p[0] - c.x) * v[0] + (p[1] - c.y) * v[1]) for p in pts)


# ----------------------------------------------------------------------------- the body

class Building:
    """Footprint + tags + ground -> a closed body: pad, walls, eaves, roof."""

    def __init__(self, poly, tags, ground, cell=1.0):
        self.poly = poly
        self.tags = tags
        self.ground = ground
        self.cell = cell
        self.levels = float(tags.get("building:levels", 0) or 0)
        self.height_m = float(tags.get("height", 0) or 0) or (self.levels * LEVEL_M if self.levels else 3 * LEVEL_M)
        self.min_m = float(tags.get("min_height", 0) or 0)
        self.roof = tags.get("roof:shape", "flat")
        self.axis = principal_axis(poly)
        self.roof_h = float(tags.get("roof:height", 0) or 0) or self._roof_height()
        self.corners = [(x, y, ground.at(x, y)) for (x, y) in list(poly.exterior.coords)[:-1]]
        self.lowest = min(c[2] for c in self.corners)
        self.highest = max(c[2] for c in self.corners)
        # THE PAD IS ONE HEIGHT and it is the ground's HIGHEST corner: a building does not float,
        # and it does not sink into the hill either -- it stands on a level pad cut into the
        # slope, and the skirt fills the gap on the downhill side (RAS-Q's Baugrube, and what
        # every terraced hillside in Wuppertal or San Francisco looks like)
        self.pad = self.highest
        if ground.water is not None and self.pad < ground.water:
            # over water the pad stands a FREEBOARD above the surface: a boathouse or a pile
            # dwelling has a deck, not a flooded floor
            self.pad = ground.water + FREEBOARD_M
        self.eaves = self.pad + max(self.height_m - self.roof_h, 1.0)
        self.ridge = self.pad + self.height_m
        self.vertices = []
        self.byKey = {}
        self.tris = []
        self.faces_of = {}
        self._build()

    def _roof_height(self):
        if self.roof in ("flat",):
            return 0.0
        if self.roof in ("pyramidal", "dome"):
            return roof_inradius(self.poly) * math.tan(ROOF_PITCH)
        if self.roof == "gabled":
            return axis_half(self.poly, self.axis[1]) * math.tan(ROOF_PITCH)
        return roof_inradius(self.poly) * math.tan(ROOF_PITCH)

    # -- mesh plumbing
    def vertex(self, x, y, z):
        key = (round(x / WELD_M), round(y / WELD_M), round(z / WELD_M))
        at = self.byKey.get(key)
        if at is None:
            at = len(self.vertices)
            self.byKey[key] = at
            self.vertices.append((x, y, z))
        return at

    def tri(self, a, b, c):
        if len({a, b, c}) < 3:
            return
        self.tris.append((a, b, c))
        for e in ((a, b), (b, c), (c, a)):
            k = tuple(sorted(e))
            self.faces_of[k] = self.faces_of.get(k, 0) + 1

    def _rings(self):
        return [list(self.poly.exterior.coords)[:-1]] + [list(r.coords)[:-1] for r in self.poly.interiors]

    def wall_top(self, x, y):
        """The wall's top is the ROOF's edge above that point, not a constant: a gable end rises
        to the ridge and a skillion's high side to its eaves, and a wall cut at one height leaves
        the body open there (measured: 32 open edges on the first gabled case)."""
        return self.eaves + roof_height_at(self.poly, x, y, self.roof, self.eaves, self.ridge, self.axis)

    def wall_foot(self, x, y):
        """The wall's foot FOLLOWS THE GROUND: a building on a slope has a tall wall downhill and
        a short one uphill, which is what a Wuppertal or San Francisco hillside looks like. On
        stilts (min_height) the foot is the pad plus that height and the ground stays untouched."""
        if self.min_m > 0:
            return self.pad + self.min_m
        return min(self.ground.at(x, y), self.pad) - FOOT_M

    def _build(self):
        # walls, every ring, densified so a wall meets the ground's own steps
        for at, ring in enumerate(self._rings()):
            outward = 1 if at == 0 else -1
            dense = []
            for a, b in zip(ring, ring[1:] + ring[:1]):
                steps = max(1, int(math.dist(a, b) / self.cell))
                for k in range(steps):
                    u = k / steps
                    dense.append((a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u))
            for a, b in zip(dense, dense[1:] + dense[:1]):
                la = self.vertex(a[0], a[1], self.wall_foot(*a))
                lb = self.vertex(b[0], b[1], self.wall_foot(*b))
                ua = self.vertex(a[0], a[1], self.wall_top(*a))
                ub = self.vertex(b[0], b[1], self.wall_top(*b))
                if outward > 0:
                    self.tri(la, lb, ub)
                    self.tri(la, ub, ua)
                else:
                    self.tri(la, ub, lb)
                    self.tri(la, ua, ub)
            # the floor (or the underside of a building on stilts), fanned from the centroid; its
            # rim follows the wall's foot so the two meet
            c = self.poly.representative_point()
            floor = self.vertex(c.x, c.y, min(self.wall_foot(*p) for p in dense))
            for a, b in zip(dense, dense[1:] + dense[:1]):
                ia = self.vertex(a[0], a[1], self.wall_foot(*a))
                ib = self.vertex(b[0], b[1], self.wall_foot(*b))
                if outward > 0:
                    self.tri(floor, ib, ia)
                else:
                    self.tri(floor, ia, ib)
        self._roof_mesh()

    def _roof_mesh(self):
        """The roof as a height field over the footprint, sampled on a grid whose cell follows
        from the tolerance and the pitch: a flat roof needs one quad, a hipped one needs the
        ridge, and the ridge is where the distance function's gradient turns -- so the grid is
        densified there by sampling the distance's own level sets."""
        minx, miny, maxx, maxy = self.poly.bounds
        cell = self.cell
        pts = []
        for ring in self._rings():
            for a, b in zip(ring, ring[1:] + ring[:1]):
                steps = max(1, int(math.dist(a, b) / cell))
                for k in range(steps):
                    u = k / steps
                    pts.append((a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u))
        # the roof's RIDGE is where the distance function's gradient turns, and a grid misses it
        # (measured: a 12 x 8 house read 0.28 m short of its apex). The level sets of the
        # distance function are the polygon's inward offsets, so the offsets ARE the ridge lines:
        # sampling them puts a vertex on every ridge and hip, which is the straight skeleton.
        step = max(cell / 2.0, 0.25)

        def ring_points(d):
            inner = self.poly.buffer(-d, join_style=2)
            if inner.is_empty:
                return None
            out = []
            for part in (inner.geoms if inner.geom_type == "MultiPolygon" else [inner]):
                for ring in [part.exterior] + list(part.interiors):
                    dense_ring = ring.segmentize(cell) if hasattr(ring, "segmentize") else ring
                    out += list(dense_ring.coords)[:-1]
            return out

        d = step
        last = 0.0
        while True:
            got = ring_points(d)
            if got is None:
                break
            pts += got
            last = d
            d += step
        # the RIDGE is the last non-empty offset, and a fixed step steps over it: bisect for it,
        # or the apex reads short by half a step times the pitch (measured: 0.28 m on a 12 x 8
        # house). The limit of these offsets IS the straight skeleton's ridge set.
        lo, hi = last, last + step
        for _ in range(20):
            mid = 0.5 * (lo + hi)
            if ring_points(mid) is None:
                hi = mid
            else:
                lo = mid
        top = ring_points(lo)
        if top:
            pts += top
        if self.roof in ("gabled", "skillion"):
            u, v = self.axis
            c = self.poly.centroid
            half_u = axis_half(self.poly, u)
            for s in np.arange(-half_u, half_u + cell, cell / 2.0):
                p = (c.x + u[0] * s, c.y + u[1] * s)
                if self.poly.contains(Point(*p)):
                    pts.append(p)
        for x in np.arange(minx + cell / 2, maxx, cell):
            for y in np.arange(miny + cell / 2, maxy, cell):
                if self.poly.contains(Point(x, y)):
                    pts.append((float(x), float(y)))
        pts = np.array(pts)
        if len(pts) < 3:
            return
        from scipy.spatial import Delaunay
        tri = Delaunay(pts)
        for simplex in tri.simplices:
            a, b, c = pts[simplex]
            cx, cy = (a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3
            if not self.poly.contains(Point(cx, cy)):
                continue
            ids = []
            for px, py in (a, b, c):
                z = self.eaves + roof_height_at(self.poly, px, py, self.roof, self.eaves, self.ridge, self.axis)
                ids.append(self.vertex(px, py, z))
            # counter-clockwise seen from above is outward for a roof
            ax, ay = a
            bx, by = b
            cxx, cyy = c
            if (bx - ax) * (cyy - ay) - (cxx - ax) * (by - ay) < 0:
                ids = [ids[0], ids[2], ids[1]]
            self.tri(*ids)

    # -- the checks
    def open_edges(self):
        return sum(1 for n in self.faces_of.values() if n == 1)

    def bad_edges(self):
        return sum(1 for n in self.faces_of.values() if n > 2)

    def watertight(self):
        return self.open_edges() == 0 and self.bad_edges() == 0

    def volume(self):
        v = 0.0
        for (ia, ib, ic) in self.tris:
            a, b, c = self.vertices[ia], self.vertices[ib], self.vertices[ic]
            v += (a[0] * (b[1] * c[2] - c[1] * b[2]) - a[1] * (b[0] * c[2] - c[0] * b[2])
                  + a[2] * (b[0] * c[1] - c[0] * b[1])) / 6.0
        return v

    def skirt_gap_m(self):
        """B1: the wall must reach the ground everywhere along the footprint -- the worst height
        of ground ABOVE the wall's foot (a gap under the building) is what this reports."""
        worst = 0.0
        for ring in self._rings():
            for a, b in zip(ring, ring[1:] + ring[:1]):
                steps = max(1, int(math.dist(a, b) / 0.5))
                for k in range(steps + 1):
                    u = k / steps
                    x, y = a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u
                    if self.min_m > 0:
                        continue        # on stilts the ground under it is meant to be there
                    # a GAP is a wall foot standing ABOVE the ground: the wall is buried by
                    # FOOT_M everywhere else, which is what a foundation is
                    worst = max(worst, self.wall_foot(x, y) - self.ground.at(x, y))
        return worst

    def ridge_error_m(self):
        """The roof's own arithmetic: a hipped roof's apex is tan(pitch) x the inradius, a gabled
        one's ridge is tan(pitch) x the half width across the long axis. Measured against the
        mesh's own highest vertex."""
        top = max(v[2] for v in self.vertices) - self.eaves
        if self.roof == "flat":
            want = 0.0
        elif self.roof == "gabled":
            want = min(axis_half(self.poly, self.axis[1]) * math.tan(ROOF_PITCH), self.ridge - self.eaves)
        elif self.roof in ("hipped", "pyramidal", "dome", "mansard"):
            want = min(roof_inradius(self.poly) * math.tan(ROOF_PITCH), self.ridge - self.eaves)
        else:
            want = self.ridge - self.eaves
        return abs(top - want)


CASES = [
    ("F1-rect", "G1-flat", {"building:levels": 3, "roof:shape": "gabled"}),
    ("F1-rect", "G2-cross15", {"building:levels": 3, "roof:shape": "gabled"}),
    ("F1-rect", "G3-cross40", {"building:levels": 2, "roof:shape": "hipped"}),
    ("F1-rect", "G4-terrace", {"building:levels": 2, "roof:shape": "hipped"}),
    ("F1-rect", "G5-water", {"building:levels": 1, "roof:shape": "flat", "min_height": 2.0}),
    ("F2-L", "G2-cross15", {"building:levels": 3, "roof:shape": "hipped"}),
    ("F3-U", "G1-flat", {"building:levels": 4, "roof:shape": "gabled"}),
    ("F4-courtyard", "G1-flat", {"building:levels": 5, "roof:shape": "flat"}),
    ("F4-courtyard", "G2-cross15", {"building:levels": 5, "roof:shape": "hipped"}),
    ("F5-round", "G1-flat", {"building:levels": 2, "roof:shape": "dome"}),
    ("F6-thin", "G2-cross15", {"building:levels": 2, "roof:shape": "gabled"}),
    ("F7-tower", "G1-flat", {"height": 60.0, "roof:shape": "flat"}),
    ("F7-tower", "G1-flat", {"height": 24.0, "roof:shape": "pyramidal"}),
    ("F1-rect", "G1-flat", {"building:levels": 3, "roof:shape": "mansard"}),
    ("F1-rect", "G1-flat", {"building:levels": 2, "roof:shape": "skillion"}),
    ("F1-rect", "G3-cross40", {"building:levels": 3, "roof:shape": "gabled", "min_height": 4.0}),
]


def run(case):
    fname, gname, tags = case
    poly = FOOTPRINTS[fname]()
    ground = GROUNDS[gname]()
    b = Building(poly, tags, ground)
    red = []
    if not b.watertight():
        red.append("B-closed")
    if b.volume() <= 0:
        red.append("B-wound")
    if b.skirt_gap_m() > 1e-6:
        red.append("B1-gap")
    if b.ridge_error_m() > 0.05:
        red.append("B-roof")
    if ground.water is not None and b.pad < ground.water:
        red.append("B3-water")
    return b, red


def main(argv):
    picked = [c for c in CASES if not argv or any(a in c[0] or a in c[1] or a in str(c[2]) for a in argv)]
    reds = 0
    for case in picked:
        b, red = run(case)
        flag = "RED " + ",".join(red) if red else "ok"
        print(f"{case[0]:14s} {case[1]:12s} {str(case[2].get('roof:shape')):10s} {flag:16s} "
              f"open {b.open_edges():4d}  bad {b.bad_edges():3d}  vol {b.volume():9.1f} m3  "
              f"gap {b.skirt_gap_m():.3f} m  roof {b.ridge_error_m():.3f} m  tris {len(b.tris):5d}")
        reds += bool(red)
    print(f"\n{len(picked)} cases, {reds} red")
    return 1 if reds else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
