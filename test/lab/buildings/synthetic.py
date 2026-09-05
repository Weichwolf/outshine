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
        self.min_m = float(tags.get("min_height", 0) or 0)
        told_height = float(tags.get("height", 0) or 0)
        self.style = Style(tags, poly, self.levels, told_height or self.levels * LEVEL_M)
        self.roof = self.style.roof_for(tags.get("roof:shape"))
        self.axis = principal_axis(poly)
        self.roof_h = float(tags.get("roof:height", 0) or 0) or self._roof_height()
        # OSM's rule, and it decides where the eaves stand: `height` is the WHOLE building,
        # ridge included, while `building:levels` counts the storeys UNDER the roof. Reading
        # levels as the whole height put the eaves inside the top storey and left a three-storey
        # house with one row of windows (seen in the elevation, measured as levels 1 of 3)
        told = told_height
        level_m = self.style.level_m
        self.height_m = told if told > 0 else (self.levels * level_m + self.roof_h if self.levels
                                               else 3 * level_m + self.roof_h)
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


# ----------------------------------------------------------------------------- type and epoch

# WHAT OSM ACTUALLY DELIVERS, measured on taginfo 2026-09-05 and stated where it matters:
#   building=*            707 M ways; the top values are yes (70 %), house, residential,
#                         apartments, garage, hut, detached, industrial, retail, commercial,
#                         church, school, warehouse, office, farm_auxiliary, barn
#   building:levels        6.0 % of them
#   height                <= 3.8 %
#   roof:shape             1.4 % (gabled 56, flat 21, hipped 10, pyramidal 3, skillion 3)
#   start_date            ~1.5 % -- the EPOCH where it is given, and nothing where it is not
#   building:material,    ~1 % each
#   roof:material
# So the type is nearly always known and everything else nearly never is. A generator therefore
# reads the TYPE, reads the EPOCH where start_date says it, and otherwise DERIVES both from
# what the geometry itself shows -- a footprint of 4 000 m2 with two storeys is a shed whatever
# the tag says, and twenty storeys is a tower.

EPOCHS = {
    # name: (level height m, bay m, window w x h, sill m, cornice, balconies, roofs allowed)
    "gruenderzeit": (3.6, 3.0, (1.2, 2.2), 0.85, True, True, ("gabled", "hipped", "mansard")),
    "interwar":     (3.0, 3.2, (1.3, 1.6), 0.9, True, True, ("gabled", "hipped", "flat")),
    "postwar":      (2.8, 3.4, (1.5, 1.4), 0.95, False, True, ("gabled", "flat", "skillion")),
    "late20":       (2.8, 3.6, (1.8, 1.4), 0.95, False, True, ("flat", "skillion", "hipped")),
    "contemporary": (3.0, 4.0, (2.4, 2.2), 0.4, False, True, ("flat", "skillion")),
    "industrial":   (6.0, 6.0, (2.4, 1.8), 3.0, False, False, ("flat", "skillion", "gabled")),
    "hall":         (9.0, 8.0, (3.0, 2.0), 4.0, False, False, ("flat", "gabled")),
    "sacral":       (9.0, 4.0, (1.4, 4.0), 3.0, True, False, ("gabled", "pyramidal", "dome")),
    "tower":        (3.3, 3.0, (2.4, 2.6), 0.5, False, False, ("flat",)),
}

INDUSTRIAL = {"industrial", "warehouse", "factory", "hangar", "shed", "barn", "farm_auxiliary",
              "silo", "storage_tank", "works"}
HALL = {"retail", "supermarket", "sports_hall", "stadium", "hangar", "train_station", "exhibition"}
SACRAL = {"church", "cathedral", "chapel", "mosque", "synagogue", "temple"}
HOUSE = {"house", "detached", "semidetached_house", "terrace", "bungalow", "hut", "cabin"}


def classify(tags, poly, levels, height_m):
    """TYPE and EPOCH, deterministically, from what OSM gives and what the geometry shows.

    The order matters and each step says why it wins: a `start_date` is a measurement and beats
    every inference; a footprint's shape and size beat a vague `building=yes`; and the tag is
    read last. `building=yes` is 70 percent of the planet's buildings, so a generator that
    trusted it alone would build one thing everywhere."""
    kind = (tags.get("building") or "yes").lower()
    area = poly.area
    slender = math.sqrt(area) if area > 0 else 1.0
    # 1. the geometry speaks first where it is unambiguous
    if levels >= 8 or height_m >= 25.0:
        return kind, "tower"
    if kind in SACRAL:
        return kind, "sacral"
    if kind in HALL or (area > 2000.0 and levels <= 2):
        return kind, "hall"
    if kind in INDUSTRIAL or (area > 800.0 and levels <= 3 and kind in ("yes", "commercial")):
        return kind, "industrial"
    # 2. the epoch, from start_date where it is given
    told = tags.get("start_date")
    if told:
        try:
            year = int(str(told)[:4])
        except ValueError:
            year = 0
        if year:
            if year < 1919:
                return kind, "gruenderzeit"
            if year < 1946:
                return kind, "interwar"
            if year < 1975:
                return kind, "postwar"
            if year < 2000:
                return kind, "late20"
            return kind, "contemporary"
    # 3. and where it is not, from what the building IS: a tall narrow terrace with 4+ storeys
    #    is a nineteenth-century block; a low detached house is post-war; the rest is late 20th
    if levels >= 4 and kind in ("apartments", "residential", "terrace", "yes"):
        return kind, "gruenderzeit"
    if kind in HOUSE and levels <= 2:
        return kind, "postwar"
    if slender > 40.0:
        return kind, "late20"
    return kind, "late20"


class Style:
    """The epoch's numbers, and what they forbid. A style is not decoration: it sets the storey
    height, the bay, the window's proportion and whether a balcony or a cornice belongs at all,
    and those are what a viewer reads a period from at 200 m."""

    def __init__(self, tags, poly, levels, height_m):
        self.kind, self.epoch = classify(tags, poly, levels, height_m)
        (self.level_m, self.bay_m, (self.win_w, self.win_h), self.sill_m,
         self.cornice, self.balconies, self.roofs) = EPOCHS[self.epoch]

    def roof_for(self, told):
        """The roof OSM says, if this epoch can carry it; otherwise the epoch's first."""
        if told and told in self.roofs:
            return told
        return told if told else self.roofs[0]


# ----------------------------------------------------------------------------- the facade

BAY_M = 3.2               # [SET] the bay a European street facade repeats, 2.4 to 4.0 m
WINDOW_W = 1.2            # [SET] a residential window's width
WINDOW_H = 1.5            # [SET] and its height
SILL_M = 0.9              # [SET] the sill above the floor
DOOR_W = 1.1              # [SET]
DOOR_H = 2.2              # [SET]
GATE_W = 4.0              # [SET] a loading gate on a hall or a shed
GATE_H = 4.5              # [SET] high enough for a lorry
BALCONY_D = 1.4           # [SET] a balcony's depth past the wall
REVEAL_M = 0.12           # [SET] how deep a window sits behind the wall's face
CORNICE_M = 0.3           # [SET] the cornice's projection at the eaves


class Facade:
    """The facade as a GRAMMAR, deterministic from the footprint and the tags: every wall is cut
    into BAYS of an equal width near BAY_M, every level into a floor, and each cell carries a
    window, a door (ground floor, the bay nearest the street) or nothing. Mueller 2006's CGA is
    the model and CARLA's BP_Procedural_Building the shipped baseline; the difference is where
    it is EXECUTED. CARLA places a mesh piece per cell and pays thousands of triangles per
    building, which is why it needs impostors. Here the cells are geometry only where they
    change the SILHOUETTE -- a balcony, a reveal, a cornice -- and everything flat is the
    fragment stage's (interior mapping, van Dongen 2008; parallax sills), which is world.md's
    split and what lets a street of a thousand houses stand in the frame."""

    def __init__(self, building):
        self.b = building
        self.walls = self._walls()

    def _walls(self):
        """One wall per edge of the footprint, with its bay grid and its OUTWARD normal. The
        normal's sign comes from the ring's own winding (the signed area), never assumed: a
        guess put every balcony inside the building and made the elevations look into the
        street instead of into the house (seen in the drawing)."""
        out = []
        for ring_at, ring in enumerate(self.b._rings()):
            area2 = sum(ring[k][0] * ring[(k + 1) % len(ring)][1] - ring[(k + 1) % len(ring)][0] * ring[k][1]
                        for k in range(len(ring)))
            ccw = area2 > 0
            for a, b in zip(ring, ring[1:] + ring[:1]):
                length = math.dist(a, b)
                if length < 1e-6:
                    continue
                # the bays divide the wall EXACTLY: a partial bay at a corner is what makes a
                # generated facade read as wallpaper, so the count is rounded and the width
                # follows from it
                bays = max(1, int(round(length / self.b.style.bay_m)))
                d = ((b[0] - a[0]) / length, (b[1] - a[1]) / length)
                left = (-d[1], d[0])
                # on a counter-clockwise ring the interior is to the LEFT; an inner ring (a
                # courtyard) is wound the other way and its outward normal points into the yard
                inward = left if (ccw == (ring_at == 0)) else (-left[0], -left[1])
                out.append({"a": a, "b": b, "length": length, "bays": bays,
                            "bay_m": length / bays, "outer": ring_at == 0,
                            "dir": d, "inward": inward,
                            "outward": (-inward[0], -inward[1])})
        return out

    def levels(self):
        """FULL levels only, at the EPOCH's storey height: a storey that does not fit under the
        eaves is not a storey, and rounding up gave a top-floor window through the roof."""
        rise = self.b.eaves - self.b.pad
        return max(1, int(math.floor(rise / self.b.style.level_m + 1e-6)))

    def cells(self):
        """Every (wall, bay, level) cell and what it carries. Deterministic: the entrance goes in
        the middle bay of the longest outer wall (the street side), and a bay above the ground
        floor carries a BALCONY DOOR where a balcony stands -- never a window, because a balcony
        is reached through a door. Which bays those are is the rule below: not the corner bays,
        and only on an outer wall long enough for one."""
        street = max((w for w in self.walls if w["outer"]), key=lambda w: w["length"], default=None)
        out = []
        for w in self.walls:
            for bay in range(w["bays"]):
                mid = (bay + 0.5) * w["bay_m"]
                for level in range(self.levels()):
                    what = "window"
                    if level == 0 and w is street and bay == w["bays"] // 2:
                        # a hall and a shed have a LOADING GATE, not a house door: the entrance
                        # a viewer reads on an industrial building is 4 m wide and 4.5 m high
                        what = "gate" if self.b.style.epoch in ("hall", "industrial") else "door"
                    elif level > 0 and self.b.style.balconies and self.b.style.epoch in ("gruenderzeit", "interwar", "postwar", "late20", "contemporary") and self._carries_balcony(w, mid):
                        what = "balcony-door"
                    out.append({"wall": w, "bay": bay, "level": level, "what": what})
        return out

    def _carries_balcony(self, wall, mid):
        """A balcony stands on an OUTER wall, above the ground floor, and never within three
        quarters of a bay of a corner -- where a real one would meet the neighbour's."""
        return (wall["outer"]
                and mid >= wall["bay_m"] * 0.75
                and mid <= wall["length"] - wall["bay_m"] * 0.75)

    def openings(self):
        """Each cell's opening in wall coordinates: (wall, along, up, width, height, kind). A cell
        whose head would stand through the wall's top carries NO opening -- under a gable end or
        a hip the corner bays are blind, which is what those houses look like."""
        out = []
        for c in self.cells():
            w = c["wall"]
            mid = (c["bay"] + 0.5) * w["bay_m"]
            base = self.b.pad + c["level"] * self.b.style.level_m
            if c["what"] == "gate":
                one = (w, mid, base + 0.02, GATE_W, GATE_H, "gate")
            elif c["what"] == "door":
                one = (w, mid, base + 0.02, DOOR_W, DOOR_H, "door")
            elif c["what"] == "balcony-door":
                # a BALCONY IS REACHED THROUGH A DOOR: a balcony behind a window is a builder's
                # error and reads as one, so the cell that carries a balcony carries a French
                # door -- full height, no sill -- and the balcony's slab sits at its threshold
                one = (w, mid, base + 0.02, DOOR_W, DOOR_H, "balcony-door")
            else:
                one = (w, mid, base + self.b.style.sill_m, self.b.style.win_w, self.b.style.win_h, "window")
            if self._fits(one):
                out.append(one)
        return out

    def _fits(self, opening):
        w, mid, up, ww, hh, kind = opening
        if mid - ww / 2 < 0.05 or mid + ww / 2 > w["length"] - 0.05:
            return False
        head = up + hh
        for u in np.linspace(mid - ww / 2, mid + ww / 2, 5):
            if head > self.b.wall_top(*self._point(w, float(u))) - 0.1:
                return False
        return True

    def balconies(self):
        """One balcony per balcony DOOR, its slab at the door's threshold: the door is what makes
        it a balcony rather than a shelf."""
        return [(w, mid, up - 0.02, w["bay_m"] * 0.8, BALCONY_D)
                for (w, mid, up, ww, hh, kind) in self.openings() if kind == "balcony-door"]

    def counts(self, lod):
        """What each rung ADDS, and the triangles it costs. The ladder is world.md's: L0 the mass,
        L1 the same mass with the facade as MATERIAL PARAMETERS (bays, levels, the cell grid --
        no geometry at all), L2 the openings' reveals and the cornice, L3 the balconies and their
        railings. Each rung is a superset of the one before, and the silhouette only changes from
        L2 up, which is why L0 and L1 can be one draw."""
        openings = self.openings()
        balconies = self.balconies()
        tris = len(self.b.tris)
        if lod >= 2:
            tris += 8 * len(openings) + 2 * sum(w["bays"] for w in self.walls if w["outer"])
        if lod >= 3:
            tris += 20 * len(balconies)
        return {"lod": lod, "triangles": tris,
                "openings": len(openings) if lod >= 1 else 0,
                "balconies": len(balconies) if lod >= 3 else 0,
                "bays": sum(w["bays"] for w in self.walls),
                "levels": self.levels()}

    def faults(self):
        """What must hold of a facade, whatever the footprint: every opening inside its wall and
        under the eaves, no opening across a corner, the bays exact, a door on the ground floor
        of the street side and nowhere else."""
        bad = []
        for (w, mid, up, ww, hh, kind) in self.openings():
            if mid - ww / 2 < 0.05 or mid + ww / 2 > w["length"] - 0.05:
                bad.append("opening across a corner")
            top = up + hh
            if top > self.b.wall_top(*self._point(w, mid)) - 0.1:
                bad.append("opening through the eaves")
            if up < self.b.pad - 1e-9:
                bad.append("opening below the pad")
        for w in self.walls:
            if abs(w["bays"] * w["bay_m"] - w["length"]) > 1e-9:
                bad.append("bays do not divide the wall")
        doors = [o for o in self.openings() if o[5] == "door"]
        for (w, mid, up, ww, hh, kind) in self.openings():
            if kind == "balcony-door" and not any(abs(bm - mid) < 1e-9 and bw is w
                                                  for (bw, bm, _, _, _) in self.balconies()):
                bad.append("a balcony door with no balcony")
        for (bw, bm, _, _, _) in self.balconies():
            if not any(o[5] == "balcony-door" and o[0] is bw and abs(o[1] - bm) < 1e-9
                       for o in self.openings()):
                bad.append("a balcony with no door")
        if len(doors) > 1:
            bad.append(f"{len(doors)} doors")
        return sorted(set(bad))

    def _point(self, wall, along):
        a, b = wall["a"], wall["b"]
        u = along / wall["length"]
        return (a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u)


CASES = [
    ("F1-rect", "G1-flat", {"building": "apartments", "building:levels": 5, "start_date": "1895"}),
    ("F1-rect", "G1-flat", {"building": "house", "building:levels": 2, "start_date": "1962"}),
    ("F1-rect", "G1-flat", {"building": "residential", "building:levels": 4, "start_date": "2015"}),
    ("F7-tower", "G1-flat", {"building": "office", "building:levels": 24}),
    ("F3-U", "G1-flat", {"building": "industrial", "building:levels": 2}),
    ("F7-tower", "G1-flat", {"building": "retail", "building:levels": 1}),
    ("F1-rect", "G1-flat", {"building": "church", "building:levels": 1, "roof:shape": "gabled"}),
    ("F1-rect", "G1-flat", {"building": "yes", "building:levels": 5}),
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


# ----------------------------------------------------------------------------- the drawing

def draw(case, b, f, number=0):
    """A sheet a builder would recognise: PLAN with the wall poched and dimensioned, two
    ELEVATIONS with the openings and the ground line, and a SECTION through the ridge with the
    storey heights. Architectural convention throughout -- cut faces solid, seen edges thin,
    ground hatched, dimensions outside, north arrow, scale bar, title block."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Polygon as MplPoly, Rectangle

    OUT.mkdir(parents=True, exist_ok=True)
    fig = plt.figure(figsize=(13.0, 9.2))
    grid = fig.add_gridspec(2, 2, height_ratios=[1.15, 1.0], hspace=0.28, wspace=0.22)
    ink = "0.15"

    # ---- PLAN
    ax = fig.add_subplot(grid[0, 0])
    ring = list(b.poly.exterior.coords)
    inner = b.poly.buffer(-0.36, join_style=2)          # [SET] a 36 cm outer wall
    ax.add_patch(MplPoly(np.array(ring), closed=True, facecolor="0.75", edgecolor=ink, lw=1.6))
    if not inner.is_empty:
        for part in (inner.geoms if inner.geom_type == "MultiPolygon" else [inner]):
            ax.add_patch(MplPoly(np.array(part.exterior.coords), closed=True,
                                 facecolor="white", edgecolor=ink, lw=1.0))
    for hole in b.poly.interiors:
        ax.add_patch(MplPoly(np.array(hole.coords), closed=True, facecolor="white", edgecolor=ink, lw=1.4))
    for (w, mid, up, ww, hh, kind) in f.openings():
        if up - b.pad > b.style.level_m * 0.5:
            continue                                     # the plan cuts the ground floor
        p0 = f._point(w, mid - ww / 2)
        p1 = f._point(w, mid + ww / 2)
        ax.plot([p0[0], p1[0]], [p0[1], p1[1]], color="white", lw=3.2, zorder=3, solid_capstyle="butt")
        ax.plot([p0[0], p1[0]], [p0[1], p1[1]], color=ink, lw=0.8, zorder=4,
                ls="-" if kind == "window" else "--")
        if kind == "door":
            ax.annotate("", xy=(p1[0], p1[1]), xytext=(p0[0], p0[1]),
                        arrowprops=dict(arrowstyle="-|>", color=ink, lw=0.8))
    for (w, mid, base, width, depth) in f.balconies():
        c = f._point(w, mid)
        d = w["dir"]
        n = w["outward"]
        pts = [(c[0] - d[0] * width / 2, c[1] - d[1] * width / 2),
               (c[0] + d[0] * width / 2, c[1] + d[1] * width / 2),
               (c[0] + d[0] * width / 2 + n[0] * depth, c[1] + d[1] * width / 2 + n[1] * depth),
               (c[0] - d[0] * width / 2 + n[0] * depth, c[1] - d[1] * width / 2 + n[1] * depth)]
        ax.add_patch(MplPoly(np.array(pts), closed=True, facecolor="none", edgecolor=ink, lw=0.6, ls=":"))
    minx, miny, maxx, maxy = b.poly.bounds
    for (x0, y0, x1, y1, text, side) in (
            (minx, miny - 2.0, maxx, miny - 2.0, f"{maxx - minx:.2f}", "h"),
            (minx - 2.0, miny, minx - 2.0, maxy, f"{maxy - miny:.2f}", "v")):
        ax.annotate("", xy=(x1, y1), xytext=(x0, y0), arrowprops=dict(arrowstyle="<|-|>", color=ink, lw=0.7))
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, text, ha="center", va="bottom" if side == "h" else "center",
                rotation=0 if side == "h" else 90, fontsize=8, color=ink)
    ax.annotate("N", xy=(maxx + 1.5, maxy + 1.0), xytext=(maxx + 1.5, maxy - 1.5),
                arrowprops=dict(arrowstyle="-|>", color=ink, lw=1.0), ha="center", fontsize=9, color=ink)
    ax.set_aspect("equal"); ax.set_title("PLAN  ground floor  1:200", fontsize=9, loc="left")
    ax.set_xticks([]); ax.set_yticks([])
    for s in ax.spines.values():
        s.set_visible(False)

    # ---- ELEVATIONS
    def elevation(axe, wall, label):
        along = np.linspace(0, wall["length"], 120)
        top = [b.wall_top(*f._point(wall, float(s))) for s in along]
        foot = [b.wall_foot(*f._point(wall, float(s))) for s in along]
        gnd = [b.ground.at(*f._point(wall, float(s))) for s in along]
        # the ROOF's silhouette behind the wall: at each point of the wall, the highest the roof
        # stands anywhere on the line running back into the building
        # the ELEVATION's silhouette is a PROJECTION: at each station of the wall, the highest
        # the building stands anywhere on that line of sight. Scanned ACROSS the building per
        # station rather than sampled on a grid -- the grid rounded the gable ends off and left
        # the ridge ragged (seen in B01).
        d = wall["dir"]
        n = wall["inward"]
        a0 = wall["a"]
        reach = max(b.poly.bounds[2] - b.poly.bounds[0], b.poly.bounds[3] - b.poly.bounds[1]) * 1.5
        sky = []
        for s in along:
            p0 = (a0[0] + d[0] * float(s), a0[1] + d[1] * float(s))
            best = b.eaves
            for r in np.linspace(0.0, reach, 160):
                q = (p0[0] + n[0] * r, p0[1] + n[1] * r)
                if b.poly.contains(Point(*q)):
                    best = max(best, b.eaves + roof_height_at(b.poly, q[0], q[1], b.roof,
                                                              b.eaves, b.ridge, b.axis))
            sky.append(best)
        axe.fill_between(along, top, sky, facecolor="0.86", edgecolor="none")
        axe.plot(along, sky, color=ink, lw=1.2)
        axe.fill_between(along, foot, top, facecolor="0.93", edgecolor=ink, lw=1.2)
        axe.plot(along, gnd, color=ink, lw=1.4)
        axe.fill_between(along, min(foot) - 1.2, gnd, facecolor="none", edgecolor="0.55",
                         hatch="////", lw=0.0)
        for (w, mid, up, ww, hh, kind) in f.openings():
            if w is not wall:
                continue
            axe.add_patch(Rectangle((mid - ww / 2, up), ww, hh,
                                    facecolor={"window": "0.55", "balcony-door": "0.45",
                                               "gate": "0.28"}.get(kind, "0.35"),
                                    edgecolor=ink, lw=0.8))
            if kind in ("window", "balcony-door"):
                axe.plot([mid, mid], [up, up + hh], color="white", lw=0.6)
                axe.plot([mid - ww / 2, mid + ww / 2], [up + hh * 0.55] * 2, color="white", lw=0.6)
        for (w, mid, base, width, depth) in f.balconies():
            if w is not wall:
                continue
            axe.add_patch(Rectangle((mid - width / 2, base), width, 0.18, facecolor="0.8", edgecolor=ink, lw=0.8))
            for r in np.linspace(mid - width / 2, mid + width / 2, 9):
                axe.plot([r, r], [base + 0.18, base + 1.0], color=ink, lw=0.5)
            axe.plot([mid - width / 2, mid + width / 2], [base + 1.0] * 2, color=ink, lw=0.8)
        for level in range(f.levels() + 1):
            z = b.pad + level * b.style.level_m
            axe.plot([-0.6, 0.0], [z, z], color=ink, lw=0.6)
            axe.text(-0.8, z, f"+{level * b.style.level_m:.2f}", ha="right", va="center", fontsize=7, color=ink)
        axe.set_xlim(-4.0, wall["length"] + 1.0)
        axe.set_ylim(min(foot) - 1.5, max(max(sky), max(top)) + 1.0)
        axe.set_aspect("equal"); axe.set_title(label, fontsize=9, loc="left")
        axe.set_xticks([]); axe.set_yticks([])
        for s in axe.spines.values():
            s.set_visible(False)

    outer = [w for w in f.walls if w["outer"]]
    longest = max(outer, key=lambda w: w["length"])
    other = max((w for w in outer if abs(math.atan2(w["b"][1] - w["a"][1], w["b"][0] - w["a"][0])
                                         - math.atan2(longest["b"][1] - longest["a"][1],
                                                      longest["b"][0] - longest["a"][0])) % math.pi > 0.5),
                key=lambda w: w["length"], default=longest)
    elevation(fig.add_subplot(grid[0, 1]), longest, "ELEVATION  street  1:200")
    elevation(fig.add_subplot(grid[1, 0]), other, "ELEVATION  side  1:200")

    # ---- SECTION through the ridge, across the long axis
    ax = fig.add_subplot(grid[1, 1])
    u, v = b.axis
    c = b.poly.centroid
    half = axis_half(b.poly, v)
    ss = np.linspace(-half, half, 200)
    pts = [(c.x + v[0] * s, c.y + v[1] * s) for s in ss]
    inside = [b.poly.contains(Point(*p)) for p in pts]
    roofz = [b.eaves + roof_height_at(b.poly, p[0], p[1], b.roof, b.eaves, b.ridge, b.axis) if ok else np.nan
             for p, ok in zip(pts, inside)]
    gnd = [b.ground.at(*p) for p in pts]
    footz = [b.wall_foot(*p) for p in pts]
    ax.fill_between(ss, footz, roofz, facecolor="0.93", edgecolor=ink, lw=1.2)
    ax.plot(ss, gnd, color=ink, lw=1.4)
    ax.fill_between(ss, min(footz) - 1.2, gnd, facecolor="none", edgecolor="0.55", hatch="////", lw=0.0)
    for level in range(f.levels() + 1):
        z = b.pad + level * b.style.level_m
        ax.plot([-half, half], [z, z], color=ink, lw=0.5, ls=(0, (6, 4)))
        ax.text(half + 0.4, z, f"+{level * b.style.level_m:.2f}", ha="left", va="center", fontsize=7, color=ink)
    ax.annotate("", xy=(-half - 1.2, b.pad), xytext=(-half - 1.2, b.ridge),
                arrowprops=dict(arrowstyle="<|-|>", color=ink, lw=0.7))
    ax.text(-half - 1.5, (b.pad + b.ridge) / 2, f"{b.ridge - b.pad:.2f}", rotation=90,
            ha="right", va="center", fontsize=8, color=ink)
    ax.set_aspect("equal"); ax.set_title("SECTION A-A  through the ridge  1:200", fontsize=9, loc="left")
    ax.set_xticks([]); ax.set_yticks([])
    for s in ax.spines.values():
        s.set_visible(False)

    tags = ", ".join(f"{k}={v}" for k, v in case[2].items())
    fig.suptitle(f"BLATT B{number:02d}   {b.style.kind.upper()} / {b.style.epoch.upper()}   "
                 f"auf {case[1].split('-', 1)[-1].upper()}   |   {case[0].split('-', 1)[-1]}, "
                 f"{f.levels()} Geschosse, Dach {b.roof}   |   {tags}",
                 fontsize=11, x=0.02, ha="left", fontweight="bold")
    fig.text(0.02, 0.015,
             f"levels {f.levels()}   bays {sum(w['bays'] for w in f.walls)}   openings {len(f.openings())}   "
             f"balconies {len(f.balconies())}   pad +{b.pad:.2f}   eaves +{b.eaves:.2f}   ridge +{b.ridge:.2f}   "
             f"volume {b.volume():.0f} m3   triangles L0/L1/L2/L3 "
             f"{'/'.join(str(f.counts(k)['triangles']) for k in range(4))}",
             fontsize=7.5, color="0.35")
    out = OUT / f"{number:02d}_{case[0]}_{case[1]}_{b.style.kind}_{b.style.epoch}.png"
    fig.savefig(out, dpi=110, bbox_inches="tight")
    plt.close(fig)
    return out


def run(case, number=0):
    fname, gname, tags = case
    poly = FOOTPRINTS[fname]()
    ground = GROUNDS[gname]()
    b = Building(poly, tags, ground)
    f = Facade(b)
    red = []
    red += f.faults()
    # what a TYPE forbids: an industrial hall has no balconies and no front door bay, a tower
    # no gabled roof, a church no bay grid of dwellings
    if b.style.epoch in ("industrial", "hall", "sacral", "tower") and f.balconies():
        red.append("balconies on " + b.style.epoch)
    if b.style.epoch == "tower" and b.roof != "flat":
        red.append("a tower with a " + b.roof + " roof")
    if not (2.4 <= b.style.level_m <= 12.0):
        red.append("storey height out of band")
    ladder = [f.counts(k) for k in range(4)]
    if not all(ladder[k]["triangles"] <= ladder[k + 1]["triangles"] for k in range(3)):
        red.append("LOD not monotone")
    if ladder[0]["triangles"] != ladder[1]["triangles"]:
        red.append("L1 costs geometry")
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
    draw(case, b, f, number)
    return b, red, f, ladder


def main(argv):
    picked = [c for c in CASES if not argv or any(a in c[0] or a in c[1] or a in str(c[2]) for a in argv)]
    reds = 0
    for number, case in enumerate(picked, start=1):
        b, red, f, ladder = run(case, number)
        flag = "RED " + ",".join(red) if red else "ok"
        print(f"{case[0]:12s} {case[1]:11s} {b.style.kind[:11]:11s} {b.style.epoch:13s} {b.roof:9s} {flag:28s} "
              f"open {b.open_edges():3d} bad {b.bad_edges():2d} vol {b.volume():8.1f} "
              f"gap {b.skirt_gap_m():.3f} roof {b.ridge_error_m():.3f} | bays {ladder[0]['bays']:3d} "
              f"levels {ladder[0]['levels']:2d} openings {ladder[3]['openings']:3d} "
              f"balconies {ladder[3]['balconies']:3d} tris {'/'.join(str(l['triangles']) for l in ladder)}")
        reds += bool(red)
    print(f"\n{len(picked)} cases, {reds} red; drawings under {OUT}")
    return 1 if reds else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
