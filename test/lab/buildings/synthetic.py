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
import shapely
import shapely.geometry.polygon
import triangle
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
    """The roof's height above the eaves at (x, y). One distance function and its variations.

    THE DISTANCE IS SIGNED AND THE FUNCTION IS ZERO OUTSIDE. `distance` to a ring is positive on
    both sides of it, so a point OUTSIDE the footprint reads as one deep inside and the roof was
    evaluated there: the elevation of a round building grew two horns rising past the eaves at
    the stations beyond its own tangent (seen in B29). A roof exists over its footprint only."""
    here = Point(x, y)
    if not poly.covers(here):
        return 0.0
    d = poly.exterior.distance(here)
    for ring in poly.interiors:
        d = min(d, ring.distance(here))
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
    if shape == "half-hipped":
        # Krueppelwalm: a gable whose top is cut back by a small hip
        u, v = axis
        across = abs((x - poly.centroid.x) * v[0] + (y - poly.centroid.y) * v[1])
        half = axis_half(poly, v)
        along = abs((x - poly.centroid.x) * u[0] + (y - poly.centroid.y) * u[1])
        halfu = axis_half(poly, u)
        rise = (half - across) * math.tan(ROOF_PITCH)
        clip = max(0.0, (halfu - along)) * math.tan(ROOF_PITCH) + (ridge_h - eaves_h) * 0.55
        return max(0.0, min(rise, clip, ridge_h - eaves_h))
    if shape == "gambrel":
        # Mansarddach mit zwei Neigungen ueber die BREITE, nicht ueber den Abstand zum Rand
        u, v = axis
        across = abs((x - poly.centroid.x) * v[0] + (y - poly.centroid.y) * v[1])
        half = axis_half(poly, v)
        knee = half * 0.55
        steep = math.tan(math.radians(65.0))
        shallow = math.tan(math.radians(28.0))
        if across > knee:
            return max(0.0, (half - across) * steep)
        return (half - knee) * steep + (knee - across) * shallow
    if shape == "sawtooth":
        # Sheddach: the roof a factory hall wears, its glazing facing north
        u, v = axis
        along = (x - poly.centroid.x) * u[0] + (y - poly.centroid.y) * u[1]
        halfu = axis_half(poly, u)
        bay = max(6.0, 2 * halfu / max(1, round(2 * halfu / 12.0)))
        phase = ((along + halfu) % bay) / bay
        return (ridge_h - eaves_h) * phase
    if shape == "barrel":
        u, v = axis
        across = abs((x - poly.centroid.x) * v[0] + (y - poly.centroid.y) * v[1])
        half = axis_half(poly, v)
        return (ridge_h - eaves_h) * math.sqrt(max(0.0, 1.0 - (across / max(half, 1e-6)) ** 2))
    if shape == "spire":
        # Turmhelm: a steep pyramid, the church's own
        return (ridge_h - eaves_h) * d / max(roof_inradius(poly), 1e-6)
    if shape == "onion":
        # a ZWIEBELHAUBE has three parts and a cone has none of them: the BULB rises almost
        # vertically off the drum, a long SHOULDER carries it in, and a LANTERN spikes at the
        # top. A height field cannot hold the bulb's overhang -- the drum's radius is its widest
        # -- so the flare is spent on the rate instead: steep at the rim, flat through the
        # shoulder, steep again at the lantern. Drawn as a cone it read as a rocket (seen in B02)
        r = roof_inradius(poly)
        u = min(1.0, d / max(r, 1e-6))
        return (ridge_h - eaves_h) * (0.60 * math.sin(math.pi / 2 * u ** 0.42) + 0.40 * u ** 7)
    if shape == "butterfly":
        u, v = axis
        across = abs((x - poly.centroid.x) * v[0] + (y - poly.centroid.y) * v[1])
        half = axis_half(poly, v)
        return (ridge_h - eaves_h) * (across / max(half, 1e-6))
    if shape == "dome":
        r = roof_inradius(poly)
        return (ridge_h - eaves_h) * math.sqrt(max(0.0, 1.0 - ((r - d) / max(r, 1e-6)) ** 2))
    return 0.0


_INRADIUS = {}
_AXIS_HALF = {}


def roof_inradius(poly, tol=1e-4):
    """MEMOISED on the polygon: the bisection buffers the ring some twenty times, and
    `roof_height_at` asks for it once PER SAMPLED POINT. On the Koelner Dom that was six
    hundred thousand buffers of a 535-node ring -- 33 minutes on one building, measured."""
    held = _INRADIUS.get(id(poly))
    if held is not None and held[0] is poly:
        return held[1]
    got = _roof_inradius(poly, tol)
    _INRADIUS[id(poly)] = (poly, got)
    return got


def _roof_inradius(poly, tol=1e-4):
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
        # ORIENTATION IS THIS TREE'S CONVENTION AND NOT THE SOURCE'S: counter-clockwise seen from
        # outside, holes the other way. An OSM way is wound whichever way the surveyor drew it,
        # and a clockwise footprint turned the whole body inside out -- every wall's outward
        # normal pointed in, the volume came out negative, and B-wound went red on the Koelner
        # Dom. So the winding is normalised HERE, once, the way an importer converts a format
        poly = shapely.geometry.polygon.orient(poly, 1.0)
        self.poly = poly
        self.tags = tags
        self.ground = ground
        self.cell = cell
        self.levels = float(tags.get("building:levels", 0) or 0)
        self.min_m = float(tags.get("min_height", 0) or 0)
        told_height = float(tags.get("height", 0) or 0)
        self.style = Style(tags, poly, self.levels, told_height or self.levels * LEVEL_M)
        self.roof = self.style.roof_for(tags.get("roof:shape"))
        # a ROOF OF REVOLUTION needs a COMPACT plan. An onion, a dome, a spire and a pyramid are
        # turned about one axis, so a long nave under one of them is a 34 m onion -- which is
        # what OSM's `roof:shape=onion` on a whole church way asked for, and what B02 drew. The
        # tag means the TOWER's roof; the nave takes the period's own pitched roof
        if self.roof in ("onion", "dome", "spire", "pyramidal"):
            box = self.poly.minimum_rotated_rectangle
            side = [Point(box.exterior.coords[i]).distance(Point(box.exterior.coords[i + 1]))
                    for i in range(4)] if box.geom_type == "Polygon" else [1.0, 1.0]
            aspect = max(side) / max(min(side), 1e-6)
            if aspect > 1.7:
                self.roof = next((r for r in self.style.roofs
                                  if r not in ("onion", "dome", "spire", "pyramidal")), "gabled")
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
        if self.roof == "spire":
            return roof_inradius(self.poly) * 3.2          # [SET] a steeple is far steeper
        if self.roof == "onion":
            return roof_inradius(self.poly) * 2.2
        if self.roof == "sawtooth":
            return 2.5                                     # [SET] a shed's tooth
        if self.roof == "butterfly":
            return axis_half(self.poly, self.axis[1]) * math.tan(math.radians(12.0))
        if self.roof == "barrel":
            return axis_half(self.poly, self.axis[1]) * 0.5
        if self.roof in ("pyramidal", "dome"):
            return roof_inradius(self.poly) * math.tan(ROOF_PITCH)
        if self.roof == "gambrel":
            # ONE arithmetic for the roof's height, or the mesh and the check disagree: the
            # ridge is the steep slope up to the knee plus the shallow one above it (measured
            # 6.30 against 11.32 when the two formulas drifted apart)
            half = axis_half(self.poly, self.axis[1])
            knee = half * 0.55
            return (half - knee) * math.tan(math.radians(65.0)) + knee * math.tan(math.radians(28.0))
        if self.roof in ("gabled", "half-hipped"):
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

    def _dense_ring(self, ring):
        """ONE densifier for the wall and for the roof's boundary: two different ones put two
        different vertex sets on the same edge and the body was never closed (measured on the
        Reichstag, a fifteen-node footprint: 100+ open edges)."""
        dense = []
        for a, b in zip(ring, ring[1:] + ring[:1]):
            steps = max(1, int(math.dist(a, b) / self.cell))
            for k in range(steps):
                u = k / steps
                dense.append((a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u))
        return dense

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
            dense = self._dense_ring(ring)
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
        # the footprint's boundary is a CONSTRAINT and never a hint: an unconstrained Delaunay
        # triangulation clipped by a centroid test cuts corners and leaves holes wherever the
        # shape is narrow (measured: 46 open edges on the Reichstag's real 15-node footprint).
        # So the rings go in as SEGMENTS and Shewchuk's constrained Delaunay keeps every one of
        # them, which makes the roof closed by construction rather than by repair.
        rings = [self._dense_ring(ring) for ring in self._rings()]
        pts = []
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
        if pts:
            import shapely
            probe = shapely.points(np.asarray(pts, dtype=float))
            keep = shapely.distance(probe, self.poly.boundary) > 1e-6
            pts = [p for p, k in zip(pts, keep) if k]
        verts, segs, seen = [], [], {}

        def put(p):
            key = (round(p[0], 6), round(p[1], 6))
            if key not in seen:
                seen[key] = len(verts)
                verts.append([float(p[0]), float(p[1])])
            return seen[key]

        for ring in rings:
            ids = [put(p) for p in ring]
            segs += [[ids[i], ids[(i + 1) % len(ids)]] for i in range(len(ids))
                     if ids[i] != ids[(i + 1) % len(ids)]]
        for p in pts:
            put(p)
        if len(verts) < 3 or not segs:
            return
        job = {"vertices": np.array(verts), "segments": np.array(segs)}
        holes = [list(Polygon(r).representative_point().coords)[0] for r in self.poly.interiors]
        if holes:
            job["holes"] = np.array(holes)
        # 'p' is the planar straight-line graph; no 'q' and no 'a', so Triangle inserts NO point
        # of its own and the vertex set is exactly the one the roof's height field was sampled on
        out = triangle.triangulate(job, "p")
        pts = out["vertices"]
        for simplex in out["triangles"]:
            a, b, c = pts[simplex]
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
        elif self.roof in ("gabled", "half-hipped"):
            want = min(axis_half(self.poly, self.axis[1]) * math.tan(ROOF_PITCH), self.ridge - self.eaves)
        elif self.roof == "gambrel":
            # a Mansarddach's ridge is its knee's rise plus the shallow slope above it
            half = axis_half(self.poly, self.axis[1])
            knee = half * 0.55
            want = min((half - knee) * math.tan(math.radians(65.0)) + knee * math.tan(math.radians(28.0)),
                       self.ridge - self.eaves)
        elif self.roof in ("sawtooth", "butterfly", "barrel", "spire", "onion"):
            want = self.ridge - self.eaves
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

# each epoch also carries its FACADE ELEMENTS -- what a viewer names the period by
ELEMENTS = {
    "gothic":       ("Strebepfeiler", "Spitzbogenfenster", "Masswerk", "Wasserspeier"),
    "baroque":      ("Pilaster", "Gesims", "Segmentbogen", "Kartusche"),
    "gruenderzeit": ("Sockel", "Gurtgesims", "Fensterverdachung", "Erker", "Kranzgesims"),
    "jugendstil":   ("Erker", "Stuckband", "geschweifter Giebel"),
    "interwar":     ("Klinkerband", "Fensterbank", "Loggia"),
    "postwar":      ("Waschbeton", "Fensterband", "Balkonbrueste"),
    "late20":       ("Fensterband", "Vordach"),
    "contemporary": ("Pfosten-Riegel", "franzoesischer Balkon", "Attika"),
    "industrial":   ("Sheddach", "Stahlfenster", "Rampe", "Schornstein"),
    "hall":         ("Schaufenster", "Vordach", "Werbeband"),
    "commercial":   ("Schaufenster", "Arkade", "Gesims", "Attika"),
    "sacral":       ("Turm", "Rosette", "Strebepfeiler", "Portal"),
    "tower":        ("Vorhangfassade", "Attika", "Sockelgeschoss"),
    "farm":         ("Tor", "Fachwerk", "Krueppelwalm"),
}

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
    "gothic":       (12.0, 3.5, (1.6, 6.0), 4.0, True, False, ("gabled", "spire", "pyramidal")),
    "baroque":      (5.0, 4.0, (1.5, 3.0), 1.0, True, False, ("mansard", "onion", "hipped", "dome")),
    "jugendstil":   (3.5, 3.4, (1.3, 2.4), 0.8, True, True, ("mansard", "gabled", "half-hipped")),
    "commercial":   (4.2, 5.0, (2.6, 2.4), 0.9, True, False, ("flat", "barrel", "butterfly")),
    "farm":         (3.0, 4.5, (1.0, 1.2), 1.1, False, False, ("gabled", "half-hipped", "gambrel")),
}

INDUSTRIAL = {"industrial", "warehouse", "factory", "hangar", "shed",
              "silo", "storage_tank", "works"}
FARM = {"barn", "farm", "farm_auxiliary", "stable", "cowshed", "greenhouse"}
COMMERCIAL = {"commercial", "office", "retail_park", "kiosk", "department_store"}
GOTHIC = {"cathedral", "minster"}
HALL = {"retail", "supermarket", "sports_hall", "stadium", "hangar", "train_station", "exhibition"}
SACRAL = {"church", "cathedral", "chapel", "mosque", "synagogue", "temple"}
HOUSE = {"house", "detached", "semidetached_house", "terrace", "bungalow", "hut", "cabin"}


def _year(tags):
    told = tags.get("start_date")
    if not told:
        return None
    try:
        return int(str(told)[:4])
    except ValueError:
        return None


def classify(tags, poly, levels, height_m):
    """TYPE and EPOCH, deterministically, from what OSM gives and what the geometry shows.

    The order matters and each step says why it wins: a `start_date` is a measurement and beats
    every inference; a footprint's shape and size beat a vague `building=yes`; and the tag is
    read last. `building=yes` is 70 percent of the planet's buildings, so a generator that
    trusted it alone would build one thing everywhere."""
    kind = (tags.get("building") or "yes").lower()
    area = poly.area
    slender = math.sqrt(area) if area > 0 else 1.0
    # 1. a NAMED use wins over the geometry where the name is unambiguous: a cathedral is a
    # cathedral at any height, and Cologne's 157 m read as an office tower (measured)
    if kind in GOTHIC or (kind in SACRAL and (tags.get("building:architecture") == "gothic"
                                              or (_year(tags) and _year(tags) < 1550))):
        return kind, "gothic"
    if kind in SACRAL:
        if _year(tags) and 1600 <= _year(tags) < 1780:
            return kind, "baroque"
        return kind, "sacral"
    # 2. then the geometry, where it is unambiguous. A TOWER IS SLENDER and not merely tall:
    # the Bundeshaus in Bern is 64 m to its dome over a 62 m wide footprint and came out as a
    # curtain-walled tower (measured). The test is the ratio a builder uses -- height against
    # the footprint's own width -- and 2.2 is where a mass stops reading as a block
    if levels >= 8 or height_m >= 25.0:
        if height_m >= 2.2 * slender or levels >= 12:
            return kind, "tower"
        year_here = _year(tags)
        if not year_here:
            # a DOME OR AN ONION over a CIVIC use dates itself: nobody has built a domed
            # parliament, palace or museum since the 1920s, so the mass belongs to the
            # historicist period that built almost all of them. The Bundeshaus in Bern is 1902
            # and came out `late20` because no `start_date` stands on the way (measured)
            if (tags.get("roof:shape") in ("dome", "onion")
                    and kind in ("government", "palace", "castle", "museum", "townhall",
                                 "civic", "public")):
                return kind, "gruenderzeit"
            return kind, "postwar" if height_m < 40.0 else "late20"
    if kind in FARM:
        return kind, "farm"
    # the TAG wins over the area wherever it is specific: a 2 000 m2 shed tagged industrial is
    # industry and not a shop, and it was drawn with a shopfront across its whole front
    if kind in INDUSTRIAL:
        return kind, "industrial"
    if kind in HALL or (area > 2000.0 and levels <= 2 and kind in ("yes", "commercial", "retail")):
        return kind, "hall"
    if area > 800.0 and levels <= 3 and kind in ("yes",):
        return kind, "industrial"
    if kind in COMMERCIAL and levels <= 6:
        return kind, "commercial"
    # 2. the epoch, from start_date where it is given
    year = _year(tags)
    if year:
        if year < 1550:
            return kind, "gothic"
        if year < 1780:
            return kind, "baroque"
        if year < 1890:
            return kind, "gruenderzeit"
        if year < 1919:
            return kind, "jugendstil" if tags.get("building:architecture") == "art_nouveau" \
                else "gruenderzeit"
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
        # A STOREY HEIGHT FOLLOWS THE USE where the use says it plainly. A parliament, a palace,
        # a museum or a station has rooms twice a flat's height, and dividing the Reichstag's
        # 48 m by a dwelling's 3.6 m gave it thirteen floors (measured). [SET] from the German
        # building types: representative 5.5 m, a concourse 7 m
        tall = {"government": 5.5, "palace": 5.5, "castle": 5.5, "museum": 5.5, "civic": 5.5,
                "townhall": 5.5, "public": 5.0, "train_station": 7.0, "transportation": 7.0}
        if self.kind in tall:
            self.level_m = max(self.level_m, tall[self.kind])

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

    def _once(self, key, make):
        """MEMOISED per facade. `openings()` walks every wall, bay and storey and the sheet asks
        for it a dozen times -- once for the plan, once per elevation, once for the counts, once
        for the checks. On a Speicherstadt block with 500 wall segments that is the whole cost of
        the drawing, and it is the same answer every time."""
        held = getattr(self, "_held", None)
        if held is None:
            held = self._held = {}
        if key not in held:
            held[key] = make()
        return held[key]

    def levels(self):
        """FULL levels only, at the EPOCH's storey height: a storey that does not fit under the
        eaves is not a storey, and rounding up gave a top-floor window through the roof."""
        rise = self.b.eaves - self.b.pad
        return max(1, int(math.floor(rise / self.b.style.level_m + 1e-6)))

    def cells(self):
        return self._once("cells", self._cells)

    def _cells(self):
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
                    elif level == 0 and "Schaufenster" in ELEMENTS.get(self.b.style.epoch, ()):
                        # a SHOPFRONT is the whole ground floor: it replaces the windows there
                        # rather than standing behind them (they were drawn as crosses on it).
                        # It is decided AFTER the entrance, or a shop has no door and a hall no
                        # gate -- which is how a 60 m retail shed came out with no opening at all
                        what = "shopfront" if self.b.style.level_m < 5.5 else "clerestory"
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
        return self._once("openings", self._openings)

    def _openings(self):
        """Each cell's opening in wall coordinates: (wall, along, up, width, height, kind). A cell
        whose head would stand through the wall's top carries NO opening -- under a gable end or
        a hip the corner bays are blind, which is what those houses look like."""
        out = []
        for c in self.cells():
            w = c["wall"]
            mid = (c["bay"] + 0.5) * w["bay_m"]
            base = self.b.pad + c["level"] * self.b.style.level_m
            if c["what"] == "shopfront":
                continue                      # drawn once per wall, not per bay
            if c["what"] == "gate":
                one = (w, mid, base + 0.02, GATE_W, GATE_H, "gate")
            elif c["what"] == "door":
                one = (w, mid, base + 0.02, DOOR_W, DOOR_H, "door")
            elif c["what"] == "balcony-door":
                # a BALCONY IS REACHED THROUGH A DOOR: a balcony behind a window is a builder's
                # error and reads as one, so the cell that carries a balcony carries a French
                # door -- full height, no sill -- and the balcony's slab sits at its threshold
                one = (w, mid, base + 0.02, DOOR_W, DOOR_H, "balcony-door")
            elif c["what"] == "clerestory" or self.b.style.level_m >= 5.5:
                # a HALL's storey is nine metres and a window at a house's sill height reads as a
                # doll's house: the light comes in HIGH, in a band under the eaves
                one = (w, mid, base + self.b.style.level_m * 0.66,
                       min(w["bay_m"] * 0.7, 3.2), min(2.4, self.b.style.level_m * 0.22), "window")
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
        return self._once("balconies", self._balconies)

    def _balconies(self):
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


PARTS = {}          # a footprint's parts, by identity: shapely's geometries take no attributes


def f_church():
    """A nave with a tower at the west end: the plan every parish church has. The tower is a
    PART with its own height and roof -- OSM's Simple 3D Buildings calls it `building:part`, and
    without parts a cathedral is one uniform block with its tower flattened into the nave (seen
    in the first sheet)."""
    nave = Polygon([(-18, -7), (10, -7), (10, 7), (-18, 7)])
    tower = Polygon([(-24, -5), (-18, -5), (-18, 5), (-24, 5)])
    whole = unary_union([nave, tower])
    PARTS[id(whole)] = [(tower, {"building:part": "yes", "height": 54.0, "roof:shape": "spire",
                                 "roof:height": 22.0})]
    return whole


def f_hall_shed():
    return Polygon([(-30, -18), (30, -18), (30, 18), (-30, 18)])


def f_terrace():
    """A terraced house: narrow, deep, two party walls."""
    return Polygon([(-4.5, -11), (4.5, -11), (4.5, 11), (-4.5, 11)])


def f_farm():
    """A farmstead: house and barn under one long roof, the Lower Saxon way."""
    return Polygon([(-9, -22), (9, -22), (9, 22), (-9, 22)])


FOOTPRINTS.update({"F8-church": f_church, "F9-shed": f_hall_shed,
                   "F10-terrace": f_terrace, "F11-farm": f_farm})


CASES = [
    ("F8-church", "G1-flat", {"building": "cathedral", "start_date": "1290",
                              "building:levels": 1, "roof:shape": "gabled"}),
    ("F8-church", "G2-cross15", {"building": "church", "start_date": "1720",
                                 "building:levels": 1, "roof:shape": "onion"}),
    ("F8-church", "G1-flat", {"building": "church", "building:levels": 1, "roof:shape": "spire"}),
    ("F10-terrace", "G1-flat", {"building": "terrace", "building:levels": 4, "start_date": "1888"}),
    ("F1-rect", "G1-flat", {"building": "apartments", "building:levels": 4, "start_date": "1908",
                            "building:architecture": "art_nouveau"}),
    ("F11-farm", "G2-cross15", {"building": "barn", "building:levels": 2,
                                "roof:shape": "half-hipped"}),
    ("F11-farm", "G1-flat", {"building": "farm", "building:levels": 2, "roof:shape": "gambrel"}),
    ("F9-shed", "G1-flat", {"building": "industrial", "building:levels": 1,
                            "roof:shape": "sawtooth"}),
    ("F9-shed", "G1-flat", {"building": "retail", "building:levels": 1, "roof:shape": "barrel"}),
    ("F1-rect", "G1-flat", {"building": "commercial", "building:levels": 4, "start_date": "1955"}),
    ("F1-rect", "G1-flat", {"building": "office", "building:levels": 5, "roof:shape": "butterfly"}),
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

def vaulted_kind(b):
    """A nave is one room to its vault: no floor slab is drawn across it."""
    return (any(k in b.style.kind for k in ("church", "cathedral", "chapel", "basilica"))
            or b.style.epoch in ("gothic", "sacral"))


def draw(case, b, f, number=0):
    """A sheet a builder would recognise: PLAN with the wall poched and dimensioned, two
    ELEVATIONS with the openings and the ground line, and a SECTION through the ridge with the
    storey heights. Architectural convention throughout -- cut faces solid, seen edges thin,
    ground hatched, dimensions outside, north arrow, scale bar, title block."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.lines
    import matplotlib.patches
    import matplotlib.pyplot as plt
    from matplotlib.patches import Polygon as MplPoly, Rectangle

    OUT.mkdir(parents=True, exist_ok=True)
    # A2 LANDSCAPE, 594 x 420 mm -- the format a builder's plan is printed on. On the 13-inch
    # sheet this bed started with, a 17 m house needed 1:500 to fit its panel and was drawn the
    # size of a stamp; the scale is not free, it follows from the paper
    fig = plt.figure(figsize=(594 / 25.4, 420 / 25.4))
    grid = fig.add_gridspec(2, 2, height_ratios=[1.10, 1.0], hspace=0.16, wspace=0.14,
                            left=0.05, right=0.97, top=0.93, bottom=0.10)
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
    for part in getattr(b, "parts", []):
        ax.add_patch(MplPoly(np.array(part.poly.exterior.coords), closed=True,
                             facecolor="0.62", edgecolor=ink, lw=1.6))
        ax.annotate(f"+{part.ridge - part.pad:.1f}", (part.poly.centroid.x, part.poly.centroid.y),
                    fontsize=7, color="white", ha="center", va="center", fontweight="bold")
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
    off = max(2.0, 0.09 * max(maxx - minx, maxy - miny))
    for (x0, y0, x1, y1, text, side) in (
            (minx, miny - off, maxx, miny - off, f"{maxx - minx:.2f}", "h"),
            (minx - off, miny, minx - off, maxy, f"{maxy - miny:.2f}", "v")):
        ax.annotate("", xy=(x1, y1), xytext=(x0, y0), arrowprops=dict(arrowstyle="<|-|>", color=ink, lw=0.7))
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, text, ha="center", va="bottom" if side == "h" else "center",
                rotation=0 if side == "h" else 90, fontsize=8, color=ink)
    # the sheet names a SECTION A-A, so the plan has to say WHERE it is cut and which way it
    # looks -- a section title with no line in the plan is a drawing that cannot be checked
    cq = b.poly.centroid
    v = b.axis[1]
    # the section line runs just past the building and not far into the margin: at 0.75 of the
    # extent it made the PLAN's own drawing twice as tall as the building and pushed the panel
    # from 1:500 to 1:1000, so the plan was drawn at half the size of its own elevations
    reach_a = 0.5 * max(abs(v[0]) * (maxx - minx), abs(v[1]) * (maxy - miny)) \
        + 0.10 * max(maxx - minx, maxy - miny)
    aa = [(cq.x - v[0] * reach_a, cq.y - v[1] * reach_a), (cq.x + v[0] * reach_a, cq.y + v[1] * reach_a)]
    ax.plot([aa[0][0], aa[1][0]], [aa[0][1], aa[1][1]], color=ink, lw=1.0, ls=(0, (9, 3, 2, 3)))
    for (px, py), sgn in ((aa[0], 1), (aa[1], -1)):
        ax.annotate("", xy=(px + v[0] * sgn * 1.6 - b.axis[0][0] * 0.0,
                            py + v[1] * sgn * 1.6),
                    xytext=(px, py), arrowprops=dict(arrowstyle="-|>", color=ink, lw=1.1))
        ax.text(px - v[0] * sgn * 0.9, py - v[1] * sgn * 0.9, "A", fontsize=9, color=ink,
                ha="center", va="center", fontweight="bold")
    ax.annotate("N", xy=(maxx + off * 0.6, maxy + off * 0.5), xytext=(maxx + off * 0.6, maxy - off * 0.6),
                arrowprops=dict(arrowstyle="-|>", color=ink, lw=1.0), ha="center", fontsize=9, color=ink)
    ax.set_aspect("equal", adjustable="datalim"); ax.set_title("PLAN  ground floor  1:200", fontsize=9, loc="left")
    ax.set_xticks([]); ax.set_yticks([])
    for s in ax.spines.values():
        s.set_visible(False)

    # ---- ELEVATIONS
    def elevation(axe, wall, label):
        # an ELEVATION shows everything the eye meets, so it runs over the whole building's
        # projection onto the wall's direction and not only over that wall's own length -- a
        # tower beyond the nave's end was simply missing from the street elevation
        d0 = wall["dir"]
        a00 = wall["a"]
        allpts = list(b.poly.exterior.coords) + [c for pp in getattr(b, "parts", [])
                                                 for c in pp.poly.exterior.coords]
        proj = [(px - a00[0]) * d0[0] + (py - a00[1]) * d0[1] for (px, py) in allpts]
        along = np.linspace(min(proj) - 0.5, max(proj) + 0.5, 140)
        # THE DRAWING'S ORIGIN IS THE LEFT EDGE OF WHAT IS DRAWN, not the reference wall's own
        # start. Every element below is written in 0..extent, so the sampling keeps the wall's
        # frame and the plotting shifts into the sheet's
        shift = float(min(along))
        alongp = along - shift
        extent = float(max(along)) - shift
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
        # the line of sight is sampled as one GRID and tested in ONE call: a point-by-point
        # `contains` on a 535-node ring is six hundred thousand shapely calls per sheet
        import shapely
        Sg, Rg = np.meshgrid(np.asarray(along, float), np.linspace(0.0, reach, 40), indexing="ij")
        QX = a0[0] + d[0] * Sg + n[0] * Rg
        QY = a0[1] + d[1] * Sg + n[1] * Rg
        insideq = shapely.contains_xy(b.poly, QX, QY)
        sky = []
        for i in range(len(along)):
            best = b.eaves
            for j in np.nonzero(insideq[i])[0]:
                best = max(best, b.eaves + roof_height_at(b.poly, QX[i, j], QY[i, j], b.roof,
                                                          b.eaves, b.ridge, b.axis))
            sky.append(best)
        # a PART standing behind this wall rises in the elevation too
        for part in getattr(b, "parts", []):
            insidep = shapely.contains_xy(part.poly, QX, QY)
            hits = list(insidep.any(axis=1))
            if any(hits):
                # the part's own ROOF silhouette, from its own roof function -- a flat band
                # from eaves to ridge turned a spire into a box (seen in B01)
                crest = []
                for i in range(len(along)):
                    best = np.nan
                    for j in np.nonzero(insidep[i])[0]:
                        z = part.eaves + roof_height_at(part.poly, QX[i, j], QY[i, j], part.roof,
                                                        part.eaves, part.ridge, part.axis)
                        best = z if np.isnan(best) else max(best, z)
                    crest.append(best)
                mid = [part.eaves if h else np.nan for h in hits]
                axe.fill_between(alongp, mid, crest, facecolor="0.80", edgecolor=ink, lw=1.0)
                axe.fill_between(alongp, [b.pad] * len(along), mid, where=hits,
                                 facecolor="0.90", edgecolor=ink, lw=1.2)
                for level in range(1, int((part.eaves - part.pad) / part.style.level_m) + 1):
                    z = part.pad + level * part.style.level_m
                    axe.plot([min(np.array(alongp)[np.array(hits)]), max(np.array(alongp)[np.array(hits)])],
                             [z, z], color=ink, lw=0.4, alpha=0.5)
        axe.fill_between(alongp, top, sky, facecolor="0.86", edgecolor="none")
        axe.plot(alongp, sky, color=ink, lw=1.2)
        # the ROOF is a roof and not a band: it oversails the wall, its eaves line is where the
        # covering starts, and its ridge is a line. Without these three an elevation reads as a
        # parapet with a grey stripe on it (seen in B12)
        oversail = 0.45 if b.roof != "flat" else 0.0
        if b.roof != "flat" and b.ridge > b.eaves + 0.05:
            x0, x1 = 0.0, extent
            axe.plot([x0 - oversail, x1 + oversail], [b.eaves] * 2, color=ink, lw=1.1)
            axe.plot([x0 - oversail, x1 + oversail], [b.eaves - 0.22] * 2, color=ink, lw=0.7)
            crestz = max(sky)
            flat_at = [s for s, z in zip(alongp, sky) if z > crestz - 0.02]
            if len(flat_at) > 3:
                axe.plot([min(flat_at), max(flat_at)], [crestz] * 2, color=ink, lw=1.4)
            if b.roof in ("gabled", "hipped", "mansard", "gambrel", "half-hipped", "skillion"):
                for s in np.linspace(x0, x1, max(6, int((x1 - x0) / 1.6))):
                    zz = float(np.interp(s + shift, along, sky))
                    if zz > b.eaves + 0.15:
                        axe.plot([s, s], [b.eaves + 0.05, zz - 0.05], color="0.62", lw=0.4)
        axe.fill_between(alongp, foot, top, facecolor="0.93", edgecolor=ink, lw=1.2, zorder=1)
        axe.plot(alongp, gnd, color=ink, lw=1.4, zorder=6)
        axe.fill_between(alongp, min(foot) - 1.2, gnd, facecolor="white", edgecolor="0.55",
                         hatch="////", lw=0.0, zorder=6)
        el_of = ELEMENTS.get(b.style.epoch, ())

        def proj(p):
            return (p[0] - a0[0]) * d[0] + (p[1] - a0[1]) * d[1] - shift

        # AN ELEVATION SHOWS EVERY WALL THAT FACES THE VIEWER, not the one it was named after.
        # The Reichstag is 104 m across and its longest single wall is a fraction of that, so the
        # drawing carried one strip of windows and 80 m of blank render (measured). A wall is
        # seen when its outward normal points back along the line of sight
        seen_walls = [w for w in f.walls
                      if w["outer"] and (w["outward"][0] * n[0] + w["outward"][1] * n[1]) < -0.25]
        if wall not in seen_walls:
            seen_walls.append(wall)
        for (w, mid, up, ww, hh, kind) in f.openings():
            if w not in seen_walls:
                continue
            mid = proj(f._point(w, mid))
            ww = ww * abs(w["dir"][0] * d[0] + w["dir"][1] * d[1])
            if ww < 0.2:
                continue                      # a wall seen edge-on carries no visible opening
            axe.add_patch(Rectangle((mid - ww / 2, up), ww, hh,
                                    facecolor={"window": "0.55", "balcony-door": "0.45",
                                               "gate": "0.28"}.get(kind, "0.35"),
                                    edgecolor=ink, lw=0.8))
            if kind in ("window", "balcony-door"):
                axe.plot([mid, mid], [up, up + hh], color="white", lw=0.6)
                axe.plot([mid - ww / 2, mid + ww / 2], [up + hh * 0.55] * 2, color="white", lw=0.6)
            # a REVEAL is what makes a wall read as thick: the opening sits back from the face,
            # so its head and one jamb carry a shadow. This is the cheapest depth on a facade and
            # the one every epoch has (0.10 to 0.25 m; [SET] 0.14 m here)
            reveal = 0.14
            axe.plot([mid - ww / 2, mid + ww / 2], [up + hh - reveal] * 2, color="0.25", lw=0.7)
            axe.plot([mid - ww / 2 + reveal] * 2, [up, up + hh], color="0.25", lw=0.7)
            if kind in ("window", "balcony-door"):
                axe.add_patch(Rectangle((mid - ww / 2 - 0.09, up - 0.10), ww + 0.18, 0.10,
                                        facecolor="0.88", edgecolor=ink, lw=0.6))   # Fensterbank
            head = up + hh
            if "Spitzbogenfenster" in el_of or "Spitzbogen" in el_of:
                # a POINTED ARCH is the one thing a viewer reads gothic from, and a rectangle
                # reads as a warehouse. Two arcs struck from the opposite springing, so the rise
                # is 0.87 of the span -- the equilateral arch, which is what the period built
                rise = ww * 0.87
                for sgn in (-1, 1):
                    axe.add_patch(matplotlib.patches.Arc((mid + sgn * ww / 2, head), 2 * ww,
                                                         2 * ww, theta1=60 if sgn < 0 else 90,
                                                         theta2=90 if sgn < 0 else 120,
                                                         color=ink, lw=1.0))
                axe.add_patch(MplPoly(np.array([(mid - ww / 2, head), (mid, head + rise),
                                                (mid + ww / 2, head)]), closed=True,
                                      facecolor="0.55", edgecolor="none", zorder=1))
                if "Masswerk" in el_of and hh > 2.2:                       # tracery
                    for frac in (-0.34, 0.0, 0.34):
                        axe.plot([mid + frac * ww] * 2, [up + 0.2, head + rise * 0.35],
                                 color="white", lw=0.7)
                    axe.add_patch(matplotlib.patches.Circle((mid, head + rise * 0.45),
                                                            ww * 0.16, facecolor="none",
                                                            edgecolor="white", lw=0.7))
            elif "Segmentbogen" in el_of and kind in ("window", "door", "gate"):
                axe.add_patch(matplotlib.patches.Wedge((mid, head), ww / 2 + 0.12, 0, 180,
                                                       width=0.16, facecolor="0.84",
                                                       edgecolor=ink, lw=0.6))
            elif "Rundbogen" in el_of or "Portal" in el_of and kind in ("door", "gate"):
                axe.add_patch(matplotlib.patches.Wedge((mid, head), ww / 2 + 0.14, 0, 180,
                                                       width=0.18, facecolor="0.84",
                                                       edgecolor=ink, lw=0.7))
            if "Fensterverdachung" in el_of and kind == "window":
                head = up + hh
                axe.add_patch(Rectangle((mid - ww / 2 - 0.16, head + 0.04), ww + 0.32, 0.16,
                                        facecolor="0.84", edgecolor=ink, lw=0.7))
                if up - b.pad < b.style.level_m * 1.8:          # the piano nobile gets a pediment
                    axe.add_patch(MplPoly(np.array([(mid - ww / 2 - 0.16, head + 0.20),
                                                    (mid, head + 0.62),
                                                    (mid + ww / 2 + 0.16, head + 0.20)]),
                                          closed=True, facecolor="0.86", edgecolor=ink, lw=0.7))
        # --- the epoch's own elements, drawn where the period puts them ---------------------
        el = ELEMENTS.get(b.style.epoch, ())
        top_of_wall = max(top)
        base_of_wall = min(foot)
        named_wall = wall
        # a SOCKEL, a GURTGESIMS and a KRANZGESIMS run round the whole building, so on the sheet
        # they span what is DRAWN and not the reference wall's own length
        wall = dict(wall)
        wall["length"] = extent
        joints = sorted({proj(f._point(w, k * w["bay_m"]))
                         for w in seen_walls for k in range(w["bays"] + 1)})
        if b.style.cornice:
            # a KRANZGESIMS is a PROFILE and not a stripe: a bed mould, a row of dentils, the
            # corona that throws the shadow, and a cyma above it. Four courses is what makes a
            # cornice read at 1:200 and it is the single element a viewer reads a period from
            oversail = 0.55
            axe.add_patch(Rectangle((-0.10, b.eaves - 0.62), wall["length"] + 0.20, 0.12,
                                    facecolor="0.88", edgecolor=ink, lw=0.6))       # Bettgesims
            if b.style.epoch in ("gruenderzeit", "baroque", "commercial"):
                for x in np.arange(0.0, wall["length"], 0.42):                       # Zahnschnitt
                    axe.add_patch(Rectangle((x + 0.08, b.eaves - 0.50), 0.22, 0.20,
                                            facecolor="0.80", edgecolor=ink, lw=0.4))
            axe.add_patch(Rectangle((-oversail, b.eaves - 0.30), wall["length"] + 2 * oversail,
                                    0.24, facecolor="0.78", edgecolor=ink, lw=0.9))  # Corona
            axe.add_patch(Rectangle((-oversail * 0.7, b.eaves - 0.06),
                                    wall["length"] + 1.4 * oversail, 0.10,
                                    facecolor="0.86", edgecolor=ink, lw=0.6))        # Sima
        if "Sockel" in el or b.style.epoch in ("gruenderzeit", "baroque", "gothic", "jugendstil"):
            hs = b.style.level_m * 0.32
            axe.add_patch(Rectangle((-0.12, b.pad), wall["length"] + 0.24, hs,
                                    facecolor="0.86", edgecolor=ink, lw=0.9))        # Sockel
            for z in np.arange(b.pad + 0.34, b.pad + hs - 0.05, 0.34):               # Bossierung
                axe.plot([-0.12, wall["length"] + 0.12], [z, z], color="0.55", lw=0.45)
            axe.add_patch(Rectangle((-0.20, b.pad + hs), wall["length"] + 0.40, 0.14,
                                    facecolor="0.80", edgecolor=ink, lw=0.7))        # Sockelgesims
        if "Gurtgesims" in el or "Gesims" in el:
            for level in range(1, f.levels()):
                z = b.pad + level * b.style.level_m
                axe.add_patch(Rectangle((-0.16, z - 0.24), wall["length"] + 0.32, 0.18,
                                        facecolor="0.84", edgecolor=ink, lw=0.7))
                axe.plot([-0.16, wall["length"] + 0.16], [z - 0.30] * 2, color="0.55", lw=0.5)
        if "Pilaster" in el:
            for bay in range(wall["bays"] + 1):
                x = bay * wall["bay_m"]
                w_p = 0.42
                axe.add_patch(Rectangle((x - w_p / 2, b.pad), w_p, b.eaves - b.pad,
                                        facecolor="0.90", edgecolor=ink, lw=0.7))    # Schaft
                axe.add_patch(Rectangle((x - w_p * 0.8, b.pad), w_p * 1.6, 0.34,
                                        facecolor="0.84", edgecolor=ink, lw=0.7))    # Basis
                axe.add_patch(Rectangle((x - w_p * 0.85, b.eaves - 1.05), w_p * 1.7, 0.42,
                                        facecolor="0.82", edgecolor=ink, lw=0.7))    # Kapitell
                for z in np.arange(b.pad + 0.7, b.eaves - 1.2, 0.55):                # Kannelur
                    axe.plot([x - w_p * 0.22, x + w_p * 0.22], [z, z], color="0.6", lw=0.3)
        if "Strebepfeiler" in el:
            # a STREBEPFEILER carries the vault's thrust and is therefore THICKEST AT THE FOOT,
            # set back in stages, each stage shedding water on a weathering. A plain rectangle is
            # a pilaster with the wrong name and carries nothing (seen in B01)
            H = b.eaves - b.pad
            for bay in range(wall["bays"] + 1):
                x = bay * wall["bay_m"]
                for k, (wd, z0, z1) in enumerate(((0.62, 0.00, 0.42), (0.48, 0.42, 0.74),
                                                  (0.34, 0.74, 0.94))):
                    axe.add_patch(Rectangle((x - wd, b.pad + H * z0), 2 * wd, H * (z1 - z0),
                                            facecolor="0.90", edgecolor=ink, lw=0.9, zorder=5))
                    axe.add_patch(MplPoly(np.array([(x - wd, b.pad + H * z1),
                                                    (x + wd, b.pad + H * z1),
                                                    (x + wd * 0.72, b.pad + H * z1 + 0.34),
                                                    (x - wd * 0.72, b.pad + H * z1 + 0.34)]),
                                          closed=True, facecolor="0.80", edgecolor=ink,
                                          lw=0.8, zorder=5))                   # Wasserschlag
                axe.add_patch(MplPoly(np.array([(x - 0.30, b.pad + H * 0.94),
                                                (x + 0.30, b.pad + H * 0.94),
                                                (x, b.pad + H * 0.94 + 1.4)]), closed=True,
                                      facecolor="0.86", edgecolor=ink, lw=0.8, zorder=5))  # Fiale
        if "Vorhangfassade" in el:
            # a CURTAIN WALL is a grid hung in front of the frame: a spandrel band at every
            # floor, a mullion at every module, a SOCKELGESCHOSS of double height at the foot
            # and an ATTIKA that hides the plant. Drawn as 480 loose window rectangles it read
            # as graph paper, which is what a tower without these three reads as
            base_h = b.style.level_m * 1.7
            axe.add_patch(Rectangle((0, b.pad), wall["length"], base_h,
                                    facecolor="0.40", edgecolor=ink, lw=1.0, zorder=3))
            axe.add_patch(Rectangle((-0.25, b.pad + base_h), wall["length"] + 0.5, 0.30,
                                    facecolor="0.86", edgecolor=ink, lw=0.8, zorder=3))
            for level in range(2, f.levels() + 1):
                z = b.pad + level * b.style.level_m
                if z > b.eaves:
                    break
                axe.add_patch(Rectangle((0, z - 0.75), wall["length"], 0.75,
                                        facecolor="0.72", edgecolor="none", zorder=3))
                axe.plot([0, wall["length"]], [z - 0.75, z - 0.75], color=ink, lw=0.4, zorder=3)
            for m in np.arange(0.0, wall["length"] + 1e-6, wall["bay_m"] / 2.0):
                axe.plot([m, m], [b.pad + base_h + 0.3, b.eaves], color=ink, lw=0.4, zorder=3)
            axe.add_patch(Rectangle((-0.30, b.eaves - 1.1), wall["length"] + 0.60, 1.1,
                                    facecolor="0.80", edgecolor=ink, lw=1.0, zorder=3))  # Attika
        if "Fachwerk" in el and f.levels() >= 2:
            # a FACHWERK is a frame and not a texture: a SCHWELLE and a RÄHM close each storey,
            # a STÄNDER stands on every bay joint, and the corner bays are braced with a STREBE
            # -- without the brace the frame is a mechanism and the drawing says so
            for level in range(1, f.levels()):
                z0 = b.pad + level * b.style.level_m
                z1 = min(b.eaves, z0 + b.style.level_m)
                axe.add_patch(Rectangle((0, z0), wall["length"], 0.16,
                                        facecolor="0.72", edgecolor=ink, lw=0.6))   # Schwelle
                axe.add_patch(Rectangle((0, z1 - 0.16), wall["length"], 0.16,
                                        facecolor="0.72", edgecolor=ink, lw=0.6))   # Raehm
                for bay in range(wall["bays"] + 1):
                    x = bay * wall["bay_m"]
                    axe.add_patch(Rectangle((x - 0.09, z0), 0.18, z1 - z0,
                                            facecolor="0.72", edgecolor=ink, lw=0.6))
                for bay in (0, wall["bays"] - 1):
                    x0s = bay * wall["bay_m"]
                    sgn = 1.0 if bay == 0 else -1.0
                    axe.plot([x0s + sgn * 0.09, x0s + sgn * (wall["bay_m"] * 0.55)],
                             [z1 - 0.16, z0 + 0.16], color=ink, lw=1.4)              # Strebe
        if "Schaufenster" in el:
            # a SHOPFRONT is about three metres tall whatever the storey height is: tied to
            # `level_m` it grew to eight metres on a retail shed and swallowed the whole facade
            hs_ = min(3.2, b.style.level_m - 0.9)
            axe.add_patch(Rectangle((0.6, b.pad + 0.3), wall["length"] - 1.2, hs_,
                                    facecolor="0.45", edgecolor=ink, lw=1.0, zorder=3))
            axe.add_patch(Rectangle((-0.4, b.pad + hs_ + 0.45), wall["length"] + 0.8, 0.35,
                                    facecolor="0.75", edgecolor=ink, lw=0.8, zorder=4))  # Vordach
        if "Arkade" in el:
            # an ARCADE is a row of PIERS with arches between them, springing at the head of the
            # ground floor and dying into the first floor's band. Struck at mid-storey with a
            # 0.7-bay radius it read as two pencil scribbles over the shopfront (seen in B10)
            spring = b.pad + b.style.level_m * 0.62
            pier = min(0.6, wall["bay_m"] * 0.18)
            for bay in range(wall["bays"] + 1):
                x = bay * wall["bay_m"]
                axe.add_patch(Rectangle((x - pier / 2, b.pad), pier, spring - b.pad,
                                        facecolor="0.86", edgecolor=ink, lw=0.9, zorder=4))
            for bay in range(wall["bays"]):
                x = (bay + 0.5) * wall["bay_m"]
                clear = wall["bay_m"] - pier
                axe.add_patch(matplotlib.patches.Wedge((x, spring), clear / 2, 0, 180,
                                                       width=0.22, facecolor="0.86",
                                                       edgecolor=ink, lw=0.9, zorder=4))
        if "Sheddach" in el or b.roof == "sawtooth":
            pass
        if "Schornstein" in el and b.roof in ("flat", "sawtooth", "skillion"):
            axe.add_patch(Rectangle((wall["length"] * 0.78, b.eaves), 1.2,
                                    max(6.0, (b.eaves - b.pad) * 0.5),
                                    facecolor="0.8", edgecolor=ink, lw=1.0))
        # ---- the elements the epoch DECLARES and nothing drew ------------------------------
        # fifteen names stood in ELEMENTS and only the easy half reached the paper. A table that
        # lists a Stuckband and draws none is a table that describes a different building
        H = b.eaves - b.pad
        L = wall["length"]
        street = wall is max((w for w in f.walls if w["outer"]), key=lambda w: w["length"])
        if "Erker" in el and f.levels() >= 3 and wall["bays"] >= 3:
            # an ERKER is corbelled out over the first floor and stops under the eaves, with its
            # own little roof; in plan it is already there as the projection the mass carries
            xe = (wall["bays"] // 2) * wall["bay_m"]
            z0e, z1e = b.pad + b.style.level_m, b.eaves - 0.7
            we = wall["bay_m"] * 0.88
            # the ORIEL stands BEHIND the openings, or it hides the very windows it exists to
            # carry and reads as a pier (seen in B05). Its cap is a low hip, not a spire
            axe.add_patch(MplPoly(np.array([(xe - we / 2 + 0.35, z0e - 0.9), (xe + we / 2 - 0.35, z0e - 0.9),
                                            (xe + we / 2, z0e), (xe - we / 2, z0e)]), closed=True,
                                  facecolor="0.88", edgecolor=ink, lw=0.9, zorder=6))   # Konsole
            # the oriel's SHAFT is drawn as an outline: filled it hides the windows it carries,
            # behind the wall it vanishes. An elevation shows a projecting body by its edge and
            # its shadow, which is the convention and also the only thing that reads here
            axe.add_patch(Rectangle((xe - we / 2, z0e), we, z1e - z0e, facecolor="none",
                                    edgecolor=ink, lw=1.4, zorder=6))
            axe.plot([xe + we / 2 - 0.12] * 2, [z0e, z1e], color="0.45", lw=2.2, zorder=6)
            axe.add_patch(MplPoly(np.array([(xe - we / 2 - 0.25, z1e), (xe + we / 2 + 0.25, z1e),
                                            (xe + we / 4, z1e + 0.55), (xe - we / 4, z1e + 0.55)]),
                                  closed=True, facecolor="0.84", edgecolor=ink, lw=0.9, zorder=6))
            for lv in range(1, f.levels()):
                zz = b.pad + lv * b.style.level_m
                if z0e < zz < z1e - 0.4:
                    axe.plot([xe - we / 2, xe + we / 2], [zz, zz], color=ink, lw=0.5, zorder=6)
        if "Stuckband" in el:
            zs = b.eaves - 1.15
            axe.add_patch(Rectangle((0, zs), L, 0.55, facecolor="0.90", edgecolor=ink,
                                    lw=0.7, zorder=4))
            for xs_ in np.arange(0.45, L, 0.9):
                axe.add_patch(matplotlib.patches.Circle((xs_, zs + 0.275), 0.17,
                                                        facecolor="0.82", edgecolor=ink,
                                                        lw=0.5, zorder=5))
        if "geschweifter Giebel" in el and b.ridge > b.eaves + 1.2 and street:
            # a VOLUTE GABLE over the middle bays: two S-curves meeting at a small pediment
            xm, wg = L / 2, min(L * 0.42, wall["bay_m"] * 2.4)
            hg = min(b.ridge - b.eaves, 3.0)
            # a SCHWEIFGIEBEL is two S-curves rising from the shoulders to a small pediment:
            # smoothstep is exactly that curve, concave at the foot and convex at the top
            u = np.linspace(0.0, 1.0, 48)
            left = [(xm - wg / 2 + (wg / 2) * uu, b.eaves + hg * (3 * uu ** 2 - 2 * uu ** 3))
                    for uu in u]
            volute = [(xm - wg / 2, b.eaves)] + left \
                + [(xm + wg / 2 - (x - (xm - wg / 2)), z) for x, z in reversed(left)] \
                + [(xm + wg / 2, b.eaves)]
            axe.add_patch(MplPoly(np.array(volute), closed=True, facecolor="0.88", edgecolor=ink,
                                  lw=1.0, zorder=6))
        if "Wasserspeier" in el:
            for bay in range(wall["bays"] + 1):
                x = bay * wall["bay_m"]
                axe.add_patch(MplPoly(np.array([(x - 0.14, b.eaves - 0.1), (x + 0.14, b.eaves - 0.1),
                                                (x + 0.5, b.eaves + 0.42), (x + 0.2, b.eaves + 0.42)]),
                                      closed=True, facecolor="0.80", edgecolor=ink, lw=0.7, zorder=7))
        if "Kartusche" in el and street:
            axe.add_patch(matplotlib.patches.Ellipse((L / 2, b.pad + b.style.level_m * 1.35),
                                                     1.5, 1.1, facecolor="0.86", edgecolor=ink,
                                                     lw=0.9, zorder=6))
        if "Klinkerband" in el:
            for lv in range(1, f.levels() + 1):
                zz = min(b.pad + lv * b.style.level_m, b.eaves)
                axe.add_patch(Rectangle((0, zz - 0.42), L, 0.30, facecolor="0.78",
                                        edgecolor=ink, lw=0.5, zorder=3))
        if "Loggia" in el and f.levels() >= 3 and wall["bays"] >= 3 and street:
            xl_ = (wall["bays"] // 2) * wall["bay_m"]
            wl = wall["bay_m"] * 0.9
            for lv in range(1, f.levels()):
                zz = b.pad + lv * b.style.level_m
                axe.add_patch(Rectangle((xl_ - wl / 2, zz + 0.15), wl, b.style.level_m - 0.6,
                                        facecolor="0.35", edgecolor=ink, lw=0.9, zorder=5))
                axe.add_patch(Rectangle((xl_ - wl / 2, zz + 0.15), wl, 1.0, facecolor="0.80",
                                        edgecolor=ink, lw=0.7, zorder=6))
        if "Waschbeton" in el:
            for lv in range(f.levels() + 1):
                zz = min(b.pad + lv * b.style.level_m, b.eaves)
                axe.plot([0, L], [zz, zz], color="0.55", lw=0.5, zorder=3)
            for bay in range(wall["bays"] + 1):
                axe.plot([bay * wall["bay_m"]] * 2, [b.pad, b.eaves], color="0.55", lw=0.5, zorder=3)
        if "Fensterband" in el:
            # a RIBBON WINDOW is one opening per storey and not a row of holes: the band runs
            # between the piers and the spandrel below it is the wall
            for lv in range(f.levels()):
                zz = b.pad + lv * b.style.level_m
                axe.add_patch(Rectangle((0.5, zz + b.style.level_m * 0.42), L - 1.0,
                                        b.style.level_m * 0.40, facecolor="0.55",
                                        edgecolor=ink, lw=0.8, zorder=4))
                for m in np.arange(0.5, L - 0.5, wall["bay_m"] / 2.0):
                    axe.plot([m, m], [zz + b.style.level_m * 0.42,
                                      zz + b.style.level_m * 0.82], color="white", lw=0.6, zorder=5)
        if "Pfosten-Riegel" in el:
            for m in np.arange(0.0, L + 1e-6, wall["bay_m"] / 2.0):
                axe.plot([m, m], [b.pad, b.eaves], color=ink, lw=0.7, zorder=4)
            for lv in range(f.levels() + 1):
                zz = min(b.pad + lv * b.style.level_m, b.eaves)
                axe.plot([0, L], [zz, zz], color=ink, lw=0.7, zorder=4)
        if "franzoesischer Balkon" in el:
            for (w, mid, up, ww, hh, kind) in f.openings():
                if w is not wall or kind not in ("window", "balcony-door") or up - b.pad < 2.0:
                    continue
                for r in np.linspace(mid - ww / 2, mid + ww / 2, 7):
                    axe.plot([r, r], [up, up + 1.0], color=ink, lw=0.5, zorder=6)
                axe.plot([mid - ww / 2, mid + ww / 2], [up + 1.0] * 2, color=ink, lw=0.9, zorder=6)
        if "Attika" in el and "Vorhangfassade" not in el:
            axe.add_patch(Rectangle((-0.25, b.eaves), L + 0.5, 1.0, facecolor="0.88",
                                    edgecolor=ink, lw=1.0, zorder=4))
        if "Stahlfenster" in el:
            for (w, mid, up, ww, hh, kind) in f.openings():
                if w is not wall or kind != "window":
                    continue
                for r in np.linspace(mid - ww / 2, mid + ww / 2, 4)[1:-1]:
                    axe.plot([r, r], [up, up + hh], color="white", lw=0.5, zorder=5)
                for zz in np.linspace(up, up + hh, 4)[1:-1]:
                    axe.plot([mid - ww / 2, mid + ww / 2], [zz, zz], color="white", lw=0.5, zorder=5)
        if "Rampe" in el and street:
            xr = L * 0.5
            axe.add_patch(MplPoly(np.array([(xr - 3.0, b.pad - 1.2), (xr + 3.0, b.pad - 1.2),
                                            (xr + 3.0, b.pad + 1.1), (xr - 3.0, b.pad + 1.1)]),
                                  closed=True, facecolor="0.80", edgecolor=ink, lw=0.9, zorder=5))
        if "Werbeband" in el:
            zw = b.pad + min(3.2, b.style.level_m - 0.9) + 0.85
            axe.add_patch(Rectangle((0, zw), L, 0.75, facecolor="0.30", edgecolor=ink,
                                    lw=0.9, zorder=5))
        if "Rosette" in el and street and b.ridge > b.eaves + 2.0:
            axe.add_patch(matplotlib.patches.Circle((L / 2, b.eaves - H * 0.22), min(2.2, L * 0.11),
                                                    facecolor="0.55", edgecolor=ink, lw=1.1, zorder=6))
            axe.add_patch(matplotlib.patches.Circle((L / 2, b.eaves - H * 0.22),
                                                    min(2.2, L * 0.11) * 0.45, facecolor="white",
                                                    edgecolor=ink, lw=0.7, zorder=7))
        if "Tor" in el and street:
            xt = (wall["bays"] // 2) * wall["bay_m"] + wall["bay_m"] / 2
            wt = min(wall["bay_m"] * 0.9, 4.2)
            ht = min(b.style.level_m * 1.6, H * 0.7)
            axe.add_patch(Rectangle((xt - wt / 2, b.pad), wt, ht, facecolor="0.42",
                                    edgecolor=ink, lw=1.2, zorder=5))
            axe.plot([xt, xt], [b.pad, b.pad + ht], color="white", lw=0.8, zorder=6)
        # a GAUBE stands on a roof SLOPE, and a gable end has none -- drawing one there put two
        # dormers on the Gruenderzeit block's blind gable, floating at the eaves (seen in B12).
        # So the wall must face a slope, and the dormer must fit under the ridge with a margin
        rise = b.ridge - b.eaves
        gable_ended = b.roof in ("gabled", "gambrel", "half-hipped", "skillion", "sawtooth")
        faces_slope = (not gable_ended) or abs(wall["dir"][0] * b.axis[0][0]
                                               + wall["dir"][1] * b.axis[0][1]) > 0.7
        if (b.style.epoch in ("gruenderzeit", "jugendstil", "farm", "baroque", "interwar")
                and b.roof in ("gabled", "hipped", "mansard", "half-hipped", "gambrel")
                and faces_slope and rise > 2.2 and wall["bays"] >= 2):
            hg = min(1.55, rise * 0.42)
            zg = b.eaves + rise * 0.18
            for bay in range(0, wall["bays"], 2):
                x = (bay + 0.5) * wall["bay_m"]
                axe.add_patch(Rectangle((x - 0.72, zg), 1.44, hg, facecolor="0.90",
                                        edgecolor=ink, lw=0.8))
                axe.add_patch(MplPoly(np.array([(x - 1.00, zg + hg), (x, zg + hg + 0.62),
                                                (x + 1.00, zg + hg)]), closed=True,
                                      facecolor="0.84", edgecolor=ink, lw=0.8))
                axe.add_patch(Rectangle((x - 0.42, zg + 0.28), 0.84, hg - 0.55,
                                        facecolor="0.55", edgecolor=ink, lw=0.5))
        # a SCHORNSTEIN stands at the RIDGE and not at the eaves: a flue is run up the party wall
        # a SCHORNSTEIN follows the USE and never the epoch: a baroque CHURCH got two of them on
        # its nave (seen in B02). What burns something is a dwelling, a farm or a works
        heated = any(k in b.style.kind for k in ("house", "apartment", "residential", "terrace",
                                                 "detached", "farm", "barn", "hotel", "yes"))
        if b.roof != "flat" and heated and faces_slope and b.style.epoch in (
                "gruenderzeit", "jugendstil", "farm", "interwar", "postwar", "baroque"):
            for frac in ((0.30, 0.78) if wall["length"] > 14.0 else (0.72,)):
                x = wall["length"] * frac
                axe.add_patch(Rectangle((x - 0.42, b.ridge - 0.4), 0.84,
                                        max(1.3, rise * 0.55) + 0.4,
                                        facecolor="0.86", edgecolor=ink, lw=0.9))
                axe.add_patch(Rectangle((x - 0.55, b.ridge + max(1.3, rise * 0.55) - 0.22),
                                        1.10, 0.22, facecolor="0.78", edgecolor=ink, lw=0.8))
        for (w, mid, base, width, depth) in f.balconies():
            if w is not wall:
                continue
            axe.add_patch(Rectangle((mid - width / 2, base), width, 0.18, facecolor="0.8", edgecolor=ink, lw=0.8))
            for r in np.linspace(mid - width / 2, mid + width / 2, 9):
                axe.plot([r, r], [base + 0.18, base + 1.0], color=ink, lw=0.5)
            axe.plot([mid - width / 2, mid + width / 2], [base + 1.0] * 2, color=ink, lw=0.8)
        every = 1 if f.levels() <= 8 else (5 if f.levels() <= 40 else 10)
        # the height ticks stand at the LEFT EDGE OF THE DRAWING and not at the wall's own
        # origin: a round building's walls are 1.8 m chords and the labels landed in the middle
        # of the facade (seen in B29)
        xl = 0.0
        for level in range(f.levels() + 1):
            z = b.pad + level * b.style.level_m
            axe.plot([xl - 0.6, xl], [z, z], color=ink, lw=0.6)
            if level % every == 0 or level == f.levels():
                axe.text(xl - 0.8, z, f"+{level * b.style.level_m:.2f}", ha="right", va="center",
                         fontsize=7, color=ink)
        # the BAY RHYTHM is the one horizontal dimension an elevation owes: a facade whose only
        # figures are heights cannot be set out on site
        zd = min(foot) - 1.6
        spread = extent
        # the chain counts the WALL it names, drawn where that wall stands in the elevation --
        # taken over the whole extent it read "10 x 2.86 = 138.53" for a 30 m wall (measured)
        base_x = proj(named_wall["a"])
        run = named_wall["dir"][0] * d[0] + named_wall["dir"][1] * d[1]
        for bay in range(named_wall["bays"] if abs(run) * named_wall["length"] > 0.35 * spread else 0):
            x0b = base_x + run * bay * named_wall["bay_m"]
            x1b = base_x + run * (bay + 1) * named_wall["bay_m"]
            axe.annotate("", xy=(x1b, zd), xytext=(x0b, zd),
                         arrowprops=dict(arrowstyle="<|-|>", color=ink, lw=0.55,
                                         mutation_scale=6))
            if named_wall["bays"] <= 8:
                axe.text((x0b + x1b) / 2, zd + 0.10, f"{named_wall['bay_m']:.2f}", ha="center",
                         va="bottom", fontsize=6, color=ink)
        if abs(run) * named_wall["length"] > 0.35 * spread:
            axe.text(base_x + run * named_wall["length"] / 2, zd - 0.75,
                     f"{named_wall['bays']} x {named_wall['bay_m']:.2f} = "
                     f"{named_wall['length']:.2f}", ha="center", va="top", fontsize=7, color=ink)
        axe.set_xlim(-4.0, extent + 1.0)
        tallest = max([max(sky), max(top)] + [p.ridge for p in getattr(b, "parts", [])])
        axe.set_ylim(min(foot) - 3.4, tallest + 1.5)
        axe.set_aspect("equal", adjustable="datalim"); axe.set_title(label, fontsize=9, loc="left")
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
    ax.fill_between(ss, footz, roofz, facecolor="0.96", edgecolor=ink, lw=1.2)
    # a SECTION shows what a saw would meet: two cut walls in poche, a slab per storey with its
    # thickness, a footing under each wall, and the roof's rafters. A silhouette with dashed
    # lines in it is an outline drawing and a builder cannot read a single dimension from it
    wall_t, slab_t, foot_t, foot_w = 0.36, 0.24, 0.55, 0.80        # [SET] m, ordinary masonry
    on = [s for s, ok in zip(ss, inside) if ok]
    if on:
        sL, sR = min(on), max(on)
        for s0 in (sL, sR - wall_t):
            here = (c.x + v[0] * (s0 + wall_t / 2), c.y + v[1] * (s0 + wall_t / 2))
            base = min(b.ground.at(*here), b.pad) - 0.8              # frost depth [SET] 0.8 m
            ax.add_patch(Rectangle((s0, base), wall_t, b.eaves - base,
                                   facecolor="0.55", edgecolor=ink, lw=1.0))
            ax.add_patch(Rectangle((s0 - (foot_w - wall_t) / 2, base - foot_t), foot_w,
                                   foot_t, facecolor="0.45", edgecolor=ink, lw=1.0))
        ax.add_patch(Rectangle((sL, b.pad - 0.24), sR - sL, 0.24,
                               facecolor="0.55", edgecolor=ink, lw=0.9))     # Bodenplatte
        for level in range(1, f.levels() + 1):
            z = b.pad + level * b.style.level_m
            if z > b.eaves + 0.01:
                break
            if vaulted_kind(b) and z > b.pad + b.style.level_m * 0.5:
                continue                       # a nave is one room to the vault, not a stack
            ax.add_patch(Rectangle((sL, z - slab_t), sR - sL, slab_t,
                                   facecolor="0.55", edgecolor=ink, lw=0.9))
        if b.style.epoch == "tower" and sR - sL > 8.0:
            # a TOWER IS A CORE PLUS A FLOOR PLATE: lifts, stairs and risers stand in one shaft
            # that carries the wind load, and a section without it shows 24 slabs floating
            cw = min(7.0, (sR - sL) * 0.32)
            cx = 0.5 * (sL + sR)
            for sgn in (-1, 1):
                ax.plot([cx + sgn * cw / 2] * 2, [b.pad, b.eaves], color=ink, lw=1.6, zorder=4)
            for z in np.arange(b.pad, b.eaves, b.style.level_m):
                ax.plot([cx - cw / 2, cx + cw / 2], [z, z], color="0.45", lw=0.4, zorder=4)
            ax.text(cx, b.pad + (b.eaves - b.pad) * 0.5, "KERN", rotation=90, ha="center",
                    va="center", fontsize=7, color="0.35", zorder=5)
        # a CHURCH has no ceiling slab at the eaves; it has a VAULT, and the section is the one
        # drawing that says which. A pointed barrel for gothic, a segmental one for baroque --
        # sprung from the springing line at two thirds of the wall, rising to the crown
        vaulted = any(k in b.style.kind for k in ("church", "cathedral", "chapel", "basilica")) \
            or b.style.epoch in ("gothic", "sacral")
        if vaulted and sR - sL > 4.0:
            span = sR - sL - 2 * wall_t
            x0v, x1v = sL + wall_t, sR - wall_t
            spring = b.pad + (b.eaves - b.pad) * 0.58
            pointed = b.style.epoch in ("gothic", "sacral")
            # the CROWN cannot stand above the eaves: a vault sits UNDER the roof, and one that
            # rises through it is a lens floating outside the walls (seen in B01). So the arch is
            # normalised and scaled to the room it has -- the two-centred equilateral arch for
            # gothic, a segmental one for the baroque
            crown = min(b.eaves - 0.55, spring + span * (0.62 if pointed else 0.34))
            mid_v = 0.5 * (x0v + x1v)
            R = 2.0                                # half-spans; R = 2 is the equilateral arch
            topR = math.sqrt(R * R - 1.0)

            def arch(u):
                u = min(1.0, abs(u))
                return (math.sqrt(max(0.0, R * R - (u + 1.0) ** 2)) / topR if pointed
                        else math.sqrt(max(0.0, 1.0 - u * u)))

            xs = np.linspace(x0v, x1v, 121)
            yv = [spring + (crown - spring) * arch(2.0 * (xx - mid_v) / max(span, 1e-6))
                  for xx in xs]
            ax.plot(xs, yv, color=ink, lw=1.3, zorder=3)
            ax.plot(xs, [vv - 0.30 for vv in yv], color="0.5", lw=0.6, zorder=3)
            for xk in (x0v, x1v):
                ax.plot([xk, xk], [spring - 0.6, spring], color=ink, lw=1.2, zorder=3)
                ax.plot([xk - 0.30, xk + 0.30], [spring, spring], color=ink, lw=1.0, zorder=3)
            ax.text(mid_v, spring - 1.1, "Kaempfer +%.2f" % (spring - b.pad), ha="center",
                    va="top", fontsize=6, color="0.35")
        if b.roof != "flat" and b.ridge > b.eaves + 0.05:
            for s in np.linspace(sL + 0.4, sR - 0.4, max(4, int((sR - sL) / 1.1))):
                zr = b.eaves + roof_height_at(b.poly, *(b.poly.centroid.x + b.axis[1][0] * s,
                                                        b.poly.centroid.y + b.axis[1][1] * s),
                                              b.roof, b.eaves, b.ridge, b.axis) \
                    if b.poly.contains(Point(b.poly.centroid.x + b.axis[1][0] * s,
                                             b.poly.centroid.y + b.axis[1][1] * s)) else b.eaves
                ax.plot([s, s], [zr - 0.22, zr], color=ink, lw=0.9)      # Sparren
            ax.plot(ss, [z - 0.22 if not np.isnan(z) else np.nan for z in roofz],
                    color=ink, lw=0.7)                                   # Schalung
            ax.plot([sL, sR], [b.eaves, b.eaves], color=ink, lw=0.9)     # Traufe / Fusspfette
            if (b.ridge - b.eaves > 2.6 and not vaulted_kind(b)
                    and b.roof in ("gabled", "hipped", "mansard", "gambrel", "half-hipped")):
                zk = b.eaves + (b.ridge - b.eaves) * 0.55                   # Kehlbalken
                spanr = sR - sL
                for sgn in (-1, 1):
                    xs_ = (sL + sR) / 2 + sgn * spanr * 0.26
                    ax.plot([xs_, xs_], [b.eaves, zk], color=ink, lw=1.6)   # Stuhlsaeule
                ax.plot([(sL + sR) / 2 - spanr * 0.26, (sL + sR) / 2 + spanr * 0.26],
                        [zk, zk], color=ink, lw=1.6)
                ax.plot([(sL + sR) / 2, (sL + sR) / 2], [zk, b.ridge], color=ink, lw=1.0)
    # a PART is cut by the section too: a cathedral sectioned through its nave with the towers
    # left out is a 15 m slab, which is what the outline alone gives (seen in B02)
    for part in getattr(b, "parts", []):
        hit = [part.poly.contains(Point(c.x + v[0] * s, c.y + v[1] * s)) for s in ss]
        if not any(hit):
            continue
        crown = [part.eaves + roof_height_at(part.poly, c.x + v[0] * s, c.y + v[1] * s,
                                             part.roof, part.eaves, part.ridge, part.axis)
                 if ok else np.nan for s, ok in zip(ss, hit)]
        floorz = [part.pad if ok else np.nan for ok in hit]
        ax.fill_between(ss, floorz, crown, facecolor="0.90", edgecolor=ink, lw=1.0, zorder=2)
    ax.plot(ss, gnd, color=ink, lw=1.4)
    ax.fill_between(ss, min(footz) - 1.4, gnd, facecolor="none", edgecolor="0.55", hatch="////", lw=0.0)
    every = 1 if f.levels() <= 8 else (5 if f.levels() <= 40 else 10)
    for level in range(f.levels() + 1):
        z = b.pad + level * b.style.level_m
        ax.plot([half, half + 0.3], [z, z], color=ink, lw=0.6)
        if level % every == 0 or level == f.levels():
            ax.text(half + 0.4, z, f"+{level * b.style.level_m:.2f}", ha="left", va="center",
                    fontsize=7, color=ink)
    tallest_s = max([b.ridge] + [p.ridge for p in getattr(b, "parts", [])])
    ax.annotate("", xy=(-half - 1.2, b.pad), xytext=(-half - 1.2, tallest_s),
                arrowprops=dict(arrowstyle="<|-|>", color=ink, lw=0.7))
    ax.text(-half - 1.5, (b.pad + tallest_s) / 2, f"{tallest_s - b.pad:.2f}", rotation=90,
            ha="right", va="center", fontsize=8, color=ink)
    ax.set_aspect("equal", adjustable="datalim"); ax.set_title("SECTION A-A  through the ridge  1:200", fontsize=9, loc="left")
    ax.set_xticks([]); ax.set_yticks([])
    for s in ax.spines.values():
        s.set_visible(False)

    tags = ", ".join(f"{k}={v}" for k, v in case[2].items())
    fig.suptitle(f"BLATT B{number:02d}   {b.style.kind.upper()} / {b.style.epoch.upper()}   "
                 f"auf {case[1].split('-', 1)[-1].upper()}   |   {case[0].split('-', 1)[-1]}, "
                 f"{f.levels()} Geschosse, Dach {b.roof}   |   {tags}",
                 fontsize=11, x=0.02, ha="left", fontweight="bold")
    fig.text(0.05, 0.028,
             f"levels {f.levels()}   bays {sum(w['bays'] for w in f.walls)}   openings {len(f.openings())}   "
             f"balconies {len(f.balconies())}   pad +{b.pad:.2f}   eaves +{b.eaves:.2f}   ridge +{b.ridge:.2f}   "
             f"volume {b.volume():.0f} m3   triangles L0/L1/L2/L3 "
             f"{'/'.join(str(f.counts(k)['triangles']) for k in range(4))}   |   "
             f"Elemente: {', '.join(ELEMENTS.get(b.style.epoch, ('-',)))}",
             fontsize=7.5, color="0.35")
    # A SHEET HAS A FORMAT AND EVERY DRAWING ON IT HAS A SCALE, and both were wrong: `tight`
    # let one unclipped label blow the Koelner Dom's sheet out to 22000 px, and every panel was
    # titled 1:200 whether it held a 12 m house or a 157 m cathedral. The format is fixed, and
    # the scale is MEASURED off the finished axes and rounded UP to the standard series
    fig.canvas.draw()
    series = (20, 50, 100, 200, 500, 1000, 2000, 5000)
    drawn = []
    # ONE SCALE FOR THE WHOLE SHEET. Two elevations of one building at 1:100 and 1:200 cannot be
    # compared, and comparing them is what a set of drawings is FOR (seen in B29). The coarsest
    # panel sets it, so nothing is cropped
    coarsest = {"PLAN": series[0], "VIEW": series[0]}

    def family(lbl):
        return "PLAN" if lbl.strip().upper().startswith("PLAN") else "VIEW"

    for axp in fig.axes:
        if "1:" not in axp.get_title(loc="left"):
            continue
        bx = axp.get_window_extent()
        yy0, yy1 = axp.get_ylim()
        xx0, xx1 = axp.get_xlim()
        want = max((yy1 - yy0) * 1000.0 / max(bx.height / fig.dpi * 25.4, 1e-6),
                   (xx1 - xx0) * 1000.0 / max(bx.width / fig.dpi * 25.4, 1e-6))
        fam = family(axp.get_title(loc="left").split("1:")[0])
        coarsest[fam] = max(coarsest[fam], next((s for s in series if s >= want), series[-1]))
    for axp in fig.axes:
        label = axp.get_title(loc="left")
        if "1:" not in label:
            continue
        box = axp.get_window_extent()
        mm_h = box.height / fig.dpi * 25.4
        mm_w = box.width / fig.dpi * 25.4
        y0, y1 = axp.get_ylim()
        x0, x1 = axp.get_xlim()
        need = max((y1 - y0) * 1000.0 / max(mm_h, 1e-6), (x1 - x0) * 1000.0 / max(mm_w, 1e-6))
        pick = coarsest[family(label.split("1:")[0])]
        del need
        # centre on the CONTENT and not on the limits: the elevation's limits run from -4 m to
        # the wall's end, so their centre sits a metre and a half left of the building's, and
        # every drawing on the sheet stood off to one side of its panel
        dl = axp.dataLim
        if np.isfinite(dl.x0) and dl.width > 0:
            x0, x1 = dl.x0, dl.x1
        if np.isfinite(dl.y0) and dl.height > 0:
            y0, y1 = dl.y0, dl.y1
        drawn.append((label.split("1:")[0].strip(), pick))
        # a DECLARED scale is one a ruler can check: the limits are then set to exactly what
        # that scale spans on this panel, so 1:500 IS 1:500 and not "somewhere under 500"
        axp.set_xlim(0.5 * (x0 + x1) - mm_w * pick / 2000.0, 0.5 * (x0 + x1) + mm_w * pick / 2000.0)
        axp.set_ylim(0.5 * (y0 + y1) - mm_h * pick / 2000.0, 0.5 * (y0 + y1) + mm_h * pick / 2000.0)
        axp.set_title(label.split("1:")[0] + f"1:{pick}", fontsize=9, loc="left")
    # the SCHRIFTFELD stands in the lower right corner, which is where DIN 6771 puts it and
    # where a reader of any building plan looks first
    sx, sy, sw, sh = 0.665, 0.020, 0.305, 0.078
    fig.patches.append(matplotlib.patches.Rectangle((sx, sy), sw, sh, transform=fig.transFigure,
                                                    facecolor="white", edgecolor=ink, lw=1.4,
                                                    zorder=20))
    rows = ((0.735, "Bauvorhaben", f"{case[0].split('-', 1)[-1]} auf {case[1].split('-', 1)[-1]}"),
            (0.500, "Bauteil", f"{b.style.kind} / {b.style.epoch}, Dach {b.roof}"),
            (0.265, "Massstab", (f"1:{drawn[0][1]}" if len({s for _, s in drawn}) == 1 else
                                 "  ".join(f"{n.split()[0]} 1:{s}" for n, s in drawn))),
            (0.030, "Blatt", f"B{number:02d}"))
    for (fy, key, val) in rows:
        fig.text(sx + 0.010, sy + sh * (fy + 0.10), key.upper(), fontsize=6.0, color="0.45",
                 zorder=21, va="bottom")
        fig.text(sx + 0.070, sy + sh * (fy + 0.06), val, fontsize=8.0, color=ink, zorder=21,
                 va="bottom")
        if fy > 0.05:
            fig.add_artist(matplotlib.lines.Line2D([sx, sx + sw], [sy + sh * fy] * 2,
                                                   color="0.55", lw=0.6, zorder=21,
                                                   transform=fig.transFigure))
    fig.add_artist(matplotlib.lines.Line2D([sx + 0.062] * 2, [sy, sy + sh], color="0.55",
                                           lw=0.6, zorder=21, transform=fig.transFigure))
    out = OUT / f"{number:02d}_{case[0]}_{case[1]}_{b.style.kind}_{b.style.epoch}.png"
    fig.savefig(out, dpi=110)
    plt.close(fig)
    return out


def run(case, number=0):
    fname, gname, tags = case
    poly = FOOTPRINTS[fname]()
    ground = GROUNDS[gname]()
    b = Building(poly, tags, ground)
    b.parts = []
    for (part_poly, part_tags) in PARTS.get(id(poly), []):
        merged = dict(tags)
        merged.update(part_tags)
        b.parts.append(Building(part_poly, merged, ground))
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
