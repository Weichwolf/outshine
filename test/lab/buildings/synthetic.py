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

import sys as _sys, pathlib as _pl
_sys.path.insert(0, str(_pl.Path(__file__).resolve().parent))
_sys.path.insert(0, str(_pl.Path(__file__).resolve().parent.parent))
import features
import publish
import roofs
import region as region_of
from shapely.geometry import Point, Polygon
from shapely.ops import unary_union

OUT = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "buildings"

LEVEL_M = 3.0             # [SET] OSM wiki's building:levels convention, and GHS-BUILT's storey
ROOF_PITCH = math.radians(35.0)   # [SET] the median pitch of a European gabled roof
EAVES_M = 0.4             # [SET] the eaves' overhang past the wall
WELD_M = 1e-3
FOOT_M = 0.5              # [SET] the wall is buried this far, which is what a foundation is
RING_RISE_M = 0.25        # [SET] how much a roof may rise between two sampled level sets
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
    "F12-bungalow": lambda: Polygon([(-7, -4.5), (7, -4.5), (7, 4.5), (1, 4.5), (1, 8.5),
                                     (-4, 8.5), (-4, 4.5), (-7, 4.5)]),
    "F13-detached": lambda: Polygon([(-5, -4.5), (5, -4.5), (5, 4.5), (1.8, 4.5), (1.8, 5.9),
                                     (-1.8, 5.9), (-1.8, 4.5), (-5, 4.5)]),
    "F14-semi": lambda: Polygon([(-7.5, -4.5), (7.5, -4.5), (7.5, 4.5), (0.2, 4.5), (0.2, 3.0),
                                 (-0.2, 3.0), (-0.2, 4.5), (-7.5, 4.5)]),
    "F2-L": f_l,
    "F3-U": f_u,
    "F4-courtyard": f_courtyard,
    "F5-round": f_round,
    "F6-thin": f_thin,
    "F7-tower": f_tower,
}


# ----------------------------------------------------------------------------- the roof

class RoofField:
    """THE ROOF'S HEIGHT FIELD OVER ONE FOOTPRINT, with everything constant computed ONCE.

    `roof_height_at` used to rebuild the whole context PER SAMPLED POINT. Per building that was
    26 960 calls to `axis_half`, each walking the ring through shapely's accessors, and 490 510
    shapely calls in all: 210 ms for a 12 x 8 house, 4.8 buildings a second, 294 seconds of
    geometry for one place's 1400 buildings (measured 2026-09-06 by cProfile). Nothing in that
    context depends on the POINT except the distance and the two projections.

    The polygon is also PREPARED once: `covers` on a raw polygon rebuilds the index per call."""

    __slots__ = ("poly", "shape", "eaves", "ridge", "u", "v", "cx", "cy", "inradius",
                 "half_u", "half_v", "rings", "ready", "known")

    def __init__(self, poly, shape, eaves_h, ridge_h, axis=None):
        import shapely.prepared
        self.poly, self.shape = poly, shape
        self.eaves, self.ridge = eaves_h, ridge_h
        self.u, self.v = axis if axis is not None else principal_axis(poly)
        c = poly.centroid
        self.cx, self.cy = c.x, c.y
        self.known = shape != "flat" and roofs.known(shape)
        self.inradius = roof_inradius(poly) if self.known else 1.0
        self.half_u = axis_half(poly, self.u) if self.known else 1.0
        self.half_v = axis_half(poly, self.v) if self.known else 1.0
        self.rings = [poly.exterior] + list(poly.interiors)
        self.ready = shapely.prepared.prep(poly)

    def at(self, x, y, d=None):
        if not self.known:
            return 0.0
        here = Point(x, y)
        if not self.ready.covers(here):
            return 0.0
        if d is None:
            d = min(ring.distance(here) for ring in self.rings)
        ctx = roofs.Ctx(poly=self.poly, x=x, y=y, d=d, eaves=self.eaves, ridge=self.ridge,
                        axis=(self.u, self.v), inradius=self.inradius, half_v=self.half_v,
                        half_u=self.half_u, pitch=ROOF_PITCH,
                        across=abs((x - self.cx) * self.v[0] + (y - self.cy) * self.v[1]),
                        along=abs((x - self.cx) * self.u[0] + (y - self.cy) * self.u[1]))
        return roofs.height_at(self.shape, ctx)


_FIELDS = {}


def roof_field(poly, shape, eaves_h, ridge_h, axis=None):
    """The field for this footprint and shape, built once and kept -- the same memoisation
    `roof_inradius` already uses, on the same key."""
    key = (id(poly), shape, round(float(eaves_h), 6), round(float(ridge_h), 6))
    held = _FIELDS.get(key)
    if held is not None and held[0] is poly:
        return held[1]
    got = RoofField(poly, shape, eaves_h, ridge_h, axis)
    _FIELDS[key] = (poly, got)
    return got


def roof_height_at(poly, x, y, shape, eaves_h, ridge_h, axis=None):
    """The roof's height above the eaves at (x, y), from the SHAPE REGISTRY in `roofs.py`.

    THE DISTANCE IS SIGNED AND THE FUNCTION IS ZERO OUTSIDE. `distance` to a ring is positive on
    both sides of it, so a point OUTSIDE the footprint reads as one deep inside and the roof was
    evaluated there: the elevation of a round building grew two horns rising past the eaves at
    the stations beyond its own tangent (seen in B29). A roof exists over its footprint only.

    This function is now only the CONTEXT: it computes what every shape may ask for -- the
    distance to the boundary, the inradius, the half-widths across and along the principal axis
    -- and hands it to the one registered shape. Adding a roof is adding a function to `roofs.py`
    and nothing here changes."""
    return roof_field(poly, shape, eaves_h, ridge_h, axis).at(x, y)


_INRADIUS = {}


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


_AXIS_HALF = {}


def axis_half(poly, v):
    """MEMOISED, like the inradius beside it. `_AXIS_HALF` stood here as an empty dict -- the
    memo was intended and never written -- while this walked the ring 26 960 times per building
    (measured 2026-09-06 by cProfile: 1.90 s of a 3.01 s build)."""
    key = (id(poly), round(float(v[0]), 9), round(float(v[1]), 9))
    held = _AXIS_HALF.get(key)
    if held is not None and held[0] is poly:
        return held[1]
    c = poly.centroid
    got = max(abs((p[0] - c.x) * v[0] + (p[1] - c.y) * v[1]) for p in poly.exterior.coords)
    _AXIS_HALF[key] = (poly, got)
    return got


# ----------------------------------------------------------------------------- the body

def metres(tags, key, otherwise=0.0):
    """A LENGTH OUT OF AN OSM TAG, read the way a boundary reads: defensively.

    `height` is free text written by a person. It arrives as `2,5` (a German decimal comma), as
    `12 m`, as `40'` and as things that are not numbers at all, and `float()` on any of them
    raises. Measured 2026-09-06: `height=2,5` on one building in Rothenburg's old town threw
    `ValueError` out of the constructor and took the whole OldTown twin with it -- the picture
    never rendered and the run reported one red place with no picture beside it.

    Metres, or `otherwise`. Feet and inches are converted; anything unreadable is refused
    quietly, because a surveyor's typo is not this bed's to repair."""
    raw = tags.get(key)
    if raw is None or raw == "":
        return otherwise
    if isinstance(raw, (int, float)):
        return float(raw)
    text = str(raw).strip().lower().replace(",", ".")
    scale = 1.0
    for unit, factor in ((" m", 1.0), ("m", 1.0), (" ft", 0.3048), ("ft", 0.3048),
                         ("'", 0.3048), ("\"", 0.0254)):
        if text.endswith(unit):
            text, scale = text[: -len(unit)].strip(), factor
            break
    try:
        return float(text) * scale
    except ValueError:
        return otherwise


class _Detail:
    """What a geometry element is given. One object rather than twenty arguments."""

    __slots__ = ("place", "bays", "epoch", "levels", "level_m", "sill_m", "win_w", "win_h",
                 "cornice", "roof", "ridge_z", "foot_z", "rise_m", "heated", "faces_slope",
                 "street", "eaves_over_m", "floor_over_ground_m", "ridge_run",
                 "ridge_here")

    def __init__(self, **kw):
        for k in self.__slots__:
            setattr(self, k, kw.get(k))


class Building:
    """Footprint + tags + ground -> a closed body: pad, walls, eaves, roof."""

    def __init__(self, poly, tags, ground, cell=1.0, where=None):
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
        self.levels = metres(tags, "building:levels")
        self.min_m = metres(tags, "min_height")
        told_height = metres(tags, "height")
        self.where = where
        self.style = Style(tags, poly, self.levels, told_height or self.levels * LEVEL_M,
                           where=where)
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
        self.roof_h = metres(tags, "roof:height") or self._roof_height()
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
        # A REBUILD IS A BUILD, NOT AN APPEND. Called twice -- which is what forcing a roof shape
        # onto a body the style refused does -- this used to leave TWO copies of every face on one
        # vertex table: 5408 triangles instead of 2704, 4056 edges with more than two faces, 8112
        # directed edges without a partner, and a volume of exactly twice the body (measured
        # 2026-09-06). The checks catch it; nothing stopped it happening.
        self.vertices, self.tris, self.faces_of, self.byKey = [], [], {}, {}
        # walls, every ring, densified so a wall meets the ground's own steps
        # THE WINDING IS ALREADY IN THE RING. `orient(poly, 1.0)` gives the exterior
        # counter-clockwise and every hole clockwise, and that convention exists precisely so
        # that the SOLID is on the same side of both -- walk either ring and the material is on
        # your left. Flipping the wall for a hole therefore turns it inside out: the courtyard
        # case carried 80 mismatched directed edges and twice its own volume until this went
        # (measured 2026-09-06, by the per-face winding check on its first run).
        for at, ring in enumerate(self._rings()):
            outward = 1
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
        self._floor_mesh()
        self._roof_mesh()

    def seed(self):
        """THE BODY'S OWN SEED, from its own place. A street where every wall carries one colour
        reads as a diagram and one where they are random reads as a toy; the truth between them is
        a PALETTE drawn from deterministically, and the draw has to be the same every time the
        same building is built -- determinism is compulsory here as everywhere."""
        c = self.poly.centroid
        return (int(round(c.x * 37.0)) * 73856093 ^ int(round(c.y * 37.0)) * 19349663) & 0xFFFF

    def covering(self):
        """What this roof is LAID WITH, and therefore what colour it is and what pitch it needs."""
        from elements import covering as cov
        where = self.where.name if self.where is not None else "anywhere"
        pitch = math.degrees(math.atan2(self.ridge - self.eaves,
                                        max(roof_inradius(self.poly), 1e-6)))
        return cov.for_pitch(pitch, where, self.style.epoch, self.seed())

    def palette(self):
        """Every role's colour for this body: the epoch's field, its trim, its plinth, and the
        roof's own covering."""
        from elements import palette as pal
        got = pal.of(self.style.epoch, self.style.wall, self.seed())
        got["roof"] = self.covering()["rgb"]
        return got

    def materials(self):
        """Every role's MATERIAL for this body, in glTF 2.0's metallic-roughness terms -- which
        is the vocabulary the engine's own importer speaks, so the handover is a copy."""
        from elements import palette as pal, covering as cov
        got = pal.materials_of(self.style.epoch, self.style.wall, self.seed())
        got["roof"] = cov.material(self.covering()["name"])
        return got

    def body(self, lod=3, street_dir=None):
        """THE WHOLE BODY AT ONE RUNG, by role: {role: (vertices, triangles)}.

        L0 and L1 keep the mass's own solid wall -- at that distance a wall IS a quad and a game
        would put a texture on it. From L2 the wall is REPLACED by a facade meshed with its
        openings, because a hole is not something you add: the first attempt put a reveal, a sill
        and a lintel around every window and the windows stayed invisible, since the glass sat
        inside solid geometry (rendered and looked at, 2026-09-06)."""
        import elements
        V = np.asarray(self.vertices, dtype=float)
        T = np.asarray(self.tris, dtype=np.int64)
        got = {}

        def put(role, verts, tris):
            v, s = got.setdefault(role, ([], []))
            base = len(v)
            v.extend([tuple(map(float, q)) for q in verts])
            s.extend([(a + base, b + base, c + base) for (a, b, c) in tris])

        if len(T):
            z = V[T][:, :, 2]
            roof = (z >= self.eaves - 1e-9).all(axis=1) & (z > self.eaves + 1e-9).any(axis=1)
            put("roof", V, T[roof].tolist())
            if lod < 2:
                put("wall", V, T[~roof].tolist())
            else:
                # the mass's FLOOR and its eaves band stay; only the standing wall is replaced
                keep = ~roof & (z <= self.eaves + 1e-9).all(axis=1) & \
                       (z <= self.pad + 1e-6).all(axis=1)
                put("wall", V, T[keep].tolist())
                for (place, ctx) in self._faces(street_dir):
                    for (role, vv, tt) in elements.holed_wall(place, elements.openings_of(ctx),
                                                              elements.facade.FRAME_M + 0.10):
                        put(role, vv, tt)
                # A PARTY WALL KEEPS THE MASS'S OWN SOLID FACE: it is shared, unseen and unglazed
                for at, wall in enumerate(Facade(self)._walls()):
                    if at not in self.party_walls() or not wall["outer"]:
                        continue
                    a, b = wall["a"], wall["b"]
                    lo0, lo1 = self.wall_foot(*a), self.wall_foot(*b)
                    hi0, hi1 = self.wall_top(*a), self.wall_top(*b)
                    v = [(a[0], a[1], lo0), (b[0], b[1], lo1), (b[0], b[1], hi1), (a[0], a[1], hi0)]
                    n = np.cross(np.subtract(v[1], v[0]), np.subtract(v[3], v[0]))
                    facing = n[0] * wall["outward"][0] + n[1] * wall["outward"][1]
                    put("wall", v, [(0, 1, 2), (0, 2, 3)] if facing > 0
                        else [(0, 2, 1), (0, 3, 2)])
        for role, (vv, tt) in self.detail(lod, street_dir).items():
            put(role, vv, tt)
        return got

    PARTY_GAP_M = 0.80        # [SET] two footprints this close are one block, and OSM draws the
                              # party wall as two lines a surveyor's width apart

    def party_walls(self):
        """WHICH OF THIS BODY'S WALLS ARE PARTY WALLS -- shared with the building next door.

        A Gruenderzeit row, a terrace, a market square, a courtyard block: in all of them the
        buildings TOUCH, and a party wall has no windows, no cornice, no gutter and no gable. Four
        blocks standing 1 m apart with four exposed flanks and four separate ridges is the single
        loudest tell that a street is generated (rendered a row of four and looked at,
        2026-09-06), and it is also what the C++ side already carries as `BuildingShape::Party`.

        Returns the set of wall indices, from the neighbours the caller handed over."""
        held = getattr(self, "_party", None)
        if held is not None:
            return held
        out = set()
        near = [n for n in getattr(self, "neighbours", ())
                if n is not self.poly and n.distance(self.poly) < self.PARTY_GAP_M]
        if near:
            from shapely.geometry import LineString
            for at, wall in enumerate(Facade(self)._walls()):
                if not wall["outer"]:
                    continue
                seg = LineString([wall["a"], wall["b"]])
                mid = seg.interpolate(0.5, normalized=True)
                probe = (mid.x + wall["outward"][0] * self.PARTY_GAP_M,
                         mid.y + wall["outward"][1] * self.PARTY_GAP_M)
                for n in near:
                    # THE PROBE ALONE DECIDES. Asking whether the neighbour is near the SEGMENT
                    # marks every wall of a body that touches anywhere, corners included: the row
                    # of four lost every facade it had and came back with 1818 triangles instead
                    # of 18 570 (measured 2026-09-06). A party wall is one you cannot stand in
                    # front of, so the test is a point pushed out of its middle.
                    if n.covers(Point(*probe)):
                        out.add(at)
                        break
        self._party = out
        return out

    def _faces(self, street_dir=None):
        """Every outer wall as (Place, detail context), which is the one place both the relief
        and the facade mesher read their geometry from."""
        import elements
        walls = [w for w in Facade(self)._walls() if w["outer"] and w["length"] >= 1.5]
        if not walls:
            return []
        if street_dir is not None:
            front = max(walls, key=lambda w: abs(w["dir"][0] * street_dir[0]
                                                 + w["dir"][1] * street_dir[1]) * w["length"])
        else:
            front = max(walls, key=lambda w: w["length"])
        u, _ = self.axis
        party = self.party_walls()
        allwalls = Facade(self)._walls()
        out = []
        for wall in walls:
            if allwalls.index(wall) in party:
                continue                       # a party wall carries nothing: it is not seen
            a = wall["a"]
            place = elements.Place(
                origin=(a[0], a[1], self.wall_foot(*a)),
                along=(wall["dir"][0], wall["dir"][1], 0.0),
                out=(wall["outward"][0], wall["outward"][1], 0.0),
                length=wall["length"], height=self.eaves - self.wall_foot(*a))
            out.append((place, _Detail(
                place=place, bays=wall["bays"], epoch=self.style.epoch,
                levels=int(self.levels) or 1, level_m=self.style.level_m,
                sill_m=self.style.sill_m, win_w=self.style.win_w, win_h=self.style.win_h,
                cornice=self.style.cornice, roof=self.roof, ridge_z=self.ridge,
                foot_z=self.wall_foot(*a), rise_m=self.ridge - self.eaves,
                heated=self.style.epoch not in ("industrial", "hall", "tower"),
                faces_slope=abs(wall["dir"][0] * u[0] + wall["dir"][1] * u[1]) > 0.7,
                street=wall is front, eaves_over_m=self.style.eaves_m,
                floor_over_ground_m=max(0.0, self.pad - self.ground.at(*a)),
                ridge_run=self._ridge_run() if wall is front else None,
                ridge_here=self._ridge_run())))
        return out

    def detail(self, lod=3, street_dir=None):
        """THE DETAIL RUNGS, AS GEOMETRY. Returns {role: (vertices, triangles)}.

        L0 is the mass this class already builds. Every rung above it ADDS -- a cornice, a
        chimney, a reveal, a shopfront -- in the wall's own frame, and none of it moves a vertex
        of the mass. `street_dir` is the direction the street runs, where one is known: a
        building's front is the wall most nearly facing it, and the ground floor's whole budget
        goes there (board:2138). Without one the longest outer wall is taken as the front."""
        import elements
        got = {}
        for (place, ctx) in self._faces(street_dir):
            for role, (vv, tt) in elements.build_all(ctx, upto=lod).items():
                v, s = got.setdefault(role, ([], []))
                base = len(v)
                v.extend(vv)
                s.extend([(x + base, y + base, z + base) for (x, y, z) in tt])
        return got

    def _ridge_run(self):
        """The ridge as a segment, for the elements that run along it."""
        if self.roof not in ("gabled", "half-hipped", "gambrel", "hipped"):
            return None
        u, _ = self.axis
        c = self.poly.centroid
        half = axis_half(self.poly, u) * (0.55 if self.roof in ("hipped", "half-hipped") else 0.98)
        return ((c.x - u[0] * half, c.y - u[1] * half, self.ridge),
                (c.x + u[0] * half, c.y + u[1] * half, self.ridge))

    def _floor_mesh(self):
        """ONE FLOOR, and a courtyard is a HOLE in it.

        The floor used to be fanned from the polygon's representative point ONCE PER RING --
        including the hole's -- so a courtyard got a second floor laid over the solid, wound the
        other way. The body was still closed and its volume still positive, which is exactly why
        a global volume proves nothing: the per-face winding check found 40 mismatched directed
        edges on the first run it ever made (measured 2026-09-06). A polygon with a hole has one
        floor and it is the polygon's own triangulation, the same constrained one the roof uses."""
        rings = [self._dense_ring(r) for r in self._rings()]
        verts, segs, seen = [], [], {}

        def put(q):
            key = (round(q[0], 6), round(q[1], 6))
            if key not in seen:
                seen[key] = len(verts)
                verts.append([float(q[0]), float(q[1])])
            return seen[key]

        for ring in rings:
            ids = [put(q) for q in ring]
            segs += [[ids[i], ids[(i + 1) % len(ids)]] for i in range(len(ids))
                     if ids[i] != ids[(i + 1) % len(ids)]]
        if len(verts) < 3 or not segs:
            return
        job = {"vertices": np.array(verts), "segments": np.array(segs)}
        holes = [list(Polygon(r).representative_point().coords)[0] for r in self.poly.interiors]
        if holes:
            job["holes"] = np.array(holes)
        out = triangle.triangulate(job, "p")
        pts = out["vertices"]
        for simplex in out["triangles"]:
            a, b, c = pts[simplex]
            ids = [self.vertex(px, py, self.wall_foot(float(px), float(py))) for px, py in (a, b, c)]
            # a floor's normal points DOWN, so it is wound clockwise seen from above
            if (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]) > 0:
                ids = [ids[0], ids[2], ids[1]]
            self.tri(*ids)

    def _roof_z(self, x, y):
        """The roof above the eaves at (x, y), taking a ring point's OWN offset distance where
        the mesh built it at one -- see `_ring_d`."""
        d = getattr(self, "_ring_d", {}).get((round(x, 6), round(y, 6)))
        if d is None:
            return roof_height_at(self.poly, x, y, self.roof, self.eaves, self.ridge, self.axis)
        here = Point(x, y)
        if not self.poly.covers(here):
            return 0.0
        u, v = self.axis
        c = self.poly.centroid
        ctx = roofs.Ctx(poly=self.poly, x=x, y=y, d=d, eaves=self.eaves, ridge=self.ridge,
                        axis=(u, v), inradius=roof_inradius(self.poly),
                        half_v=axis_half(self.poly, v), half_u=axis_half(self.poly, u),
                        across=abs((x - c.x) * v[0] + (y - c.y) * v[1]),
                        along=abs((x - c.x) * u[0] + (y - c.y) * u[1]), pitch=ROOF_PITCH)
        return roofs.height_at(self.roof, ctx)

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

        # THE HEIGHT OF A RING POINT COMES FROM THE DISTANCE IT WAS BUILT AT, not from one
        # recomputed afterwards. `buffer(-d)` is an approximation, so a point meant to lie d
        # from the boundary reads back as d +/- epsilon, and where dz/dd is steep -- a barrel's
        # crown, an onion's lantern, a mansard's break -- that epsilon becomes a COMB of spikes
        # along the crest. Seen in the roof gallery on six of the fourteen shapes (measured
        # 2026-09-06). Remembering d costs a dict and removes the artefact at its source.
        self._ring_d = {}

        # EVERY RING CARRIES THE SAME PARAMETERISATION, and this is what the COMB was.
        # Each offset used to be re-densified on its own, so two neighbouring rings came out with
        # different point counts at unrelated positions; the triangulation between them then
        # alternates its apex high and low, which on a surface that is steep at the springing
        # reads as a row of SPIKES. Measured 2026-09-06 on a 12 x 8 rectangle: the eaves ring had
        # 80 points and the ring 0.125 m inside it had 78, at unrelated arc positions, across a
        # band that rose 0.695 m. Sampling every ring at the SAME normalised arc fractions makes
        # that band a clean strip. A ring that has split into several parts takes its share.
        def ring_points(d):
            inner = self.poly.buffer(-d, join_style=2)
            if inner.is_empty:
                return None
            out = []
            for part in (inner.geoms if inner.geom_type == "MultiPolygon" else [inner]):
                for ring in [part.exterior] + list(part.interiors):
                    # THE SAME DENSIFIER AS THE EAVES RING, and this is the point. Sampling an
                    # offset at normalised ARC FRACTIONS puts its points at positions unrelated
                    # to the eaves ring's, so the band between them zigzags. `_dense_ring` walks
                    # a ring's own vertices and subdivides each EDGE at `cell`; a mitred offset
                    # keeps the corner count and the edge directions, so the two rings' points
                    # then stand above one another and the band is a strip.
                    got = self._dense_ring(list(ring.coords)[:-1])
                    for q in got:
                        self._ring_d[(round(q[0], 6), round(q[1], 6))] = d
                    out += got
            return out

        # THE RING LADDER STEPS IN HEIGHT, NOT IN DISTANCE. A dome, a barrel or an onion stands
        # almost VERTICALLY off its eaves -- dz/dd is unbounded at the springing -- so a ladder
        # of equal distances puts one band of near-vertical triangles there, and because the two
        # rings carry different point counts the band zigzags and reads as a COMB of spikes
        # along the eaves (seen in the roof gallery on six of the fourteen shapes, 2026-09-06).
        # The next ring is the one whose crown is at most RING_RISE_M above this one, floored at
        # a quarter cell so a flat roof does not build a thousand of them.
        def crown(dd):
            """The roof's height AT distance dd -- which means a point ON that offset ring.

            This used to probe `representative_point()`, a point somewhere in the MIDDLE of the
            offset, and for a dome that point is already at the apex: `crown` returned 2.8008 for
            every ring, the ladder read a rise of exactly 0.0000 between all of them, and the
            whole height-stepping mechanism never once stepped. Measured 2026-09-06 -- the rings
            came out at a flat 0.25 m of distance apart and the band above the eaves rose 0.81 m
            across 0.25 m, which is the COMB. A point on the ring has geometric distance dd, so
            the shape is asked the question the ladder is actually about."""
            inner = self.poly.buffer(-dd, join_style=2)
            if inner.is_empty:
                return None
            part = inner if inner.geom_type == "Polygon" else max(inner.geoms, key=lambda g: g.area)
            q = part.exterior.interpolate(0.5, normalized=True).coords[0]
            return roof_height_at(self.poly, q[0], q[1], self.roof, self.eaves, self.ridge,
                                  self.axis)

        # THE FIRST BAND IS A BAND. The ladder used to place ring one at a fixed distance and
        # only THEN start stepping in height, so a dome -- vertical at its springing -- rose
        # 0.695 m across the 0.125 m between the eaves and ring one, three times RING_RISE_M
        # (measured 2026-09-06). The step is chosen from the eaves upward like every other.
        # A RING LADDER MUST BE ISOTROPIC, and that bound beats the height step.
        # The points ALONG a ring stand `cell` apart. Put two rings 8 mm apart and the point set
        # is 60:1 anisotropic: a Delaunay triangulation cannot make a strip out of that, so it
        # makes slivers that SKIP rings -- measured 2026-09-06 on the dome, the worst faces span
        # d = 0.000, 0.008 and 0.164 while rings at 0.039 and 0.102 sit unused between them, and
        # that is the COMB along the eaves. So the radial step is floored at the along-ring
        # spacing. A dome IS near-vertical at its springing and one honest band there beats six
        # bands the mesher cannot connect; resolving that band properly needs a STRUCTURED
        # ring-and-spoke stitch rather than a Delaunay, which is board:2156.
        floor_d = cell
        d = 0.0
        last = 0.0
        guard = 0
        while guard < 4000:
            guard += 1
            here = self.eaves if d <= 0.0 else crown(d)
            trial = step
            for _ in range(14):
                ahead = crown(d + trial)
                if (ahead is None or here is None or abs(ahead - here) <= RING_RISE_M
                        or trial <= floor_d):
                    break
                trial *= 0.5
            d = d + max(trial, floor_d)
            got = ring_points(d)
            if got is None:
                break
            pts += got
            last = d
        # the RIDGE is the last non-empty offset, and a fixed step steps over it: bisect for it,
        # or the apex reads short by half a step times the pitch (measured: 0.28 m on a 12 x 8
        # house). The limit of these offsets IS the straight skeleton's ridge set.
        # AND THE BRACKET HAS TO CONTAIN IT. `last + step` was the window whatever step the loop
        # actually took, and once the ladder's step could exceed `step` the bracket no longer
        # held the ridge: seven cases went red on B-roof with the apex 0.35 m short (measured
        # 2026-09-06, by making the ladder isotropic). The upper bound is GROWN until it is
        # empty, so the bracket always straddles the last non-empty offset.
        lo, hi = last, last + max(step, floor_d)
        for _ in range(40):
            if ring_points(hi) is None:
                break
            lo, hi = hi, hi * 2.0 if hi > 0 else max(step, floor_d)
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
                z = self.eaves + self._roof_z(float(px), float(py))
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

    def winding(self):
        """B-wound, PER FACE: is the surface consistently oriented, and outward?

        A positive VOLUME is a global test and a global test can be fooled -- a handful of
        inverted triangles cancel inside a sum and the body still reports a volume. What proves
        it locally is the DIRECTED edge: on a closed, consistently oriented surface every edge
        (a, b) appears exactly once, and its partner (b, a) exactly once, in the neighbouring
        face. A flipped triangle breaks that at all three of its edges and cannot hide.

        Returns (edges wrong, degenerate faces, worst normal length error). Consistency plus a
        positive volume is what makes the orientation OUTWARD rather than merely agreed."""
        seen = {}
        for (ia, ib, ic) in self.tris:
            for e in ((ia, ib), (ib, ic), (ic, ia)):
                seen[e] = seen.get(e, 0) + 1
        wrong = 0
        for (a_, b_), n in seen.items():
            if n != 1 or seen.get((b_, a_), 0) != 1:
                wrong += 1
        degenerate, worst = 0, 0.0
        for (ia, ib, ic) in self.tris:
            p, q, r = (np.array(self.vertices[i], dtype=float) for i in (ia, ib, ic))
            nvec = np.cross(q - p, r - p)
            ln = float(np.linalg.norm(nvec))
            if ln <= 1e-12:
                degenerate += 1
                continue
            worst = max(worst, abs(ln / ln - 1.0))
        return wrong, degenerate, worst

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
    "siedlungshaus": ("Klappladen", "Vordach", "Schornstein"),
    "bungalow":     ("Fensterband", "Vordach", "Carport"),
    "einfamilienhaus": ("Vordach", "Gaube", "Carport"),
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
    # A HOUSE IS NOT A SMALL BLOCK. The three epochs above it -- postwar, late20, contemporary --
    # carry an APARTMENT BUILDING's numbers: a 2.8 m storey, a 3.4 m bay, a 1.8 m window. A
    # detached house has a 2.5 m storey, a bay of one room and a window a person stands at, and
    # those three numbers are most of what a viewer reads the type from. Until this the bed held
    # exactly ONE detached-house case and no bungalow at all, which for the commonest building
    # on the planet is a hole in the middle of the default world.
    "siedlungshaus": (2.5, 2.4, (1.0, 1.2), 0.95, False, False,
                      ("gabled", "half-hipped", "hipped")),
    "bungalow":     (2.5, 3.0, (2.4, 1.4), 0.90, False, False, ("flat", "hipped", "skillion")),
    "einfamilienhaus": (2.6, 2.8, (1.4, 1.4), 0.95, False, True,
                        ("gabled", "hipped", "half-hipped")),
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


def house_epoch(kind, year, levels, area):
    """WHICH HOUSE. Three types, and each is a period with its own proportions:

        siedlungshaus     1920-1955, the small settlement house: steep gable, tiny windows,
                          two storeys on a 60 to 90 m2 plan, no balcony and no cornice
        bungalow          1955-1980, ONE storey spread wide: a shallow hip or a flat roof, a
                          picture window, a carport. The type is defined by the storey COUNT
        einfamilienhaus   1980 onward, the developer's detached house: a 2.6 m storey, a gabled
                          or hipped roof, a dormer and a balcony

    A dated villa before 1919 is still a Gruenderzeit house and keeps that table."""
    if kind == "bungalow":
        return "bungalow"
    if year is None:
        return "bungalow" if levels <= 1 and area > 90.0 else "einfamilienhaus"
    if year < 1919:
        return "gruenderzeit"
    if year < 1955:
        return "siedlungshaus"
    if year < 1980:
        return "bungalow" if levels <= 1 else "siedlungshaus"
    return "einfamilienhaus"


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
    # A HOUSE HAS ITS OWN LADDER and it is read first, or the block's year table catches it: a
    # 1962 detached house came out `postwar`, which is an apartment building's proportions
    if kind in HOUSE and levels <= 2 and area < 400.0:
        return kind, house_epoch(kind, year, levels, area)
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

    def __init__(self, tags, poly, levels, height_m, where=None):
        self.kind, self.epoch = classify(tags, poly, levels, height_m)
        (self.level_m, self.bay_m, (self.win_w, self.win_h), self.sill_m,
         self.cornice, self.balconies, self.roofs) = EPOCHS[self.epoch]
        # THE PLACE IS AN INPUT, not a label. The generator takes an EPOCH and a COORDINATE: the
        # epoch sets the proportions, and the coordinate sets what the epoch cannot know -- the
        # snow a roof must shed and therefore the pitch below which nobody builds, how far the
        # eaves oversail, what a wall is made of, and how many storeys of masonry the ground
        # allows before a frame is needed. Where both speak, the EPOCH wins, because it is the
        # more specific of the two; where the epoch is silent, the region answers.
        self.where = where
        self.wall = "brick"
        self.eaves_m = EAVES_M
        if where is not None:
            self.wall = where.wall
            self.eaves_m = where.eaves_m
            allowed = [r for r in self.roofs if r in where.roofs] or list(self.roofs)
            self.roofs = tuple(allowed)
            if levels > where.storeys_masonry and self.wall in ("brick", "stone"):
                # a wall this tall is not laid, it is FRAMED -- the fact a viewer reads from a
                # window that runs the whole bay instead of a hole punched in masonry
                self.wall = "frame"
        # A STOREY HEIGHT FOLLOWS THE USE where the use says it plainly. A parliament, a palace,
        # a museum or a station has rooms twice a flat's height, and dividing the Reichstag's
        # 48 m by a dwelling's 3.6 m gave it thirteen floors (measured). [SET] from the German
        # building types: representative 5.5 m, a concourse 7 m
        tall = {"government": 5.5, "palace": 5.5, "castle": 5.5, "museum": 5.5, "civic": 5.5,
                "townhall": 5.5, "public": 5.0, "train_station": 7.0, "transportation": 7.0}
        if self.kind in tall:
            self.level_m = max(self.level_m, tall[self.kind])

    def roof_for(self, told):
        """The roof OSM says, if this epoch can carry it; otherwise the epoch's first -- and
        never one flatter than the snow here allows.

        A roof sheds what falls on it or it carries it, and that is a rule of the PLACE and not
        of the period: the alpine 45 degrees is a snow load before it is a style. A flat roof
        under two kilonewtons of snow is not a style choice, it is a collapse."""
        got = told if (told and told in self.roofs) else (told or self.roofs[0])
        if self.where is None:
            return got
        # only where the TRADITION has no flat roof at all: 40 degrees is the alpine rule and
        # nothing below it forbids a flat roof -- Cologne's minimum is 22 and its flat roofs are
        # everywhere. The region's own roof list does the rest of the work by intersection.
        if self.where.min_pitch_deg >= 40.0 and got in ("flat", "butterfly", "sawtooth"):
            steep = [r for r in self.roofs if r not in ("flat", "butterfly", "sawtooth")]
            return steep[0] if steep else got
        return got


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
    # THE COMMONEST BUILDING ON THE PLANET, which this bed held one case of until 2026-09-06
    ("F13-detached", "G1-flat", {"building": "house", "building:levels": 2,
                                 "start_date": "1936", "roof:shape": "gabled"}),
    ("F13-detached", "G2-cross15", {"building": "detached", "building:levels": 2,
                                    "start_date": "1998"}),
    ("F12-bungalow", "G1-flat", {"building": "bungalow", "building:levels": 1,
                                 "start_date": "1966", "roof:shape": "hipped"}),
    ("F12-bungalow", "G3-cross40", {"building": "house", "building:levels": 1,
                                    "start_date": "1971", "roof:shape": "flat"}),
    ("F14-semi", "G1-flat", {"building": "semidetached_house", "building:levels": 2,
                             "start_date": "1952"}),
    ("F13-detached", "G1-flat", {"building": "house", "building:levels": 2,
                                 "start_date": "2019", "roof:shape": "hipped"}),
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
        H = b.eaves - b.pad
        L = wall["length"]
        street = wall is max((w for w in f.walls if w["outer"]), key=lambda w: w["length"])
        rise = b.ridge - b.eaves
        gable_ended = b.roof in ("gabled", "gambrel", "half-hipped", "skillion", "sawtooth")
        faces_slope = (not gable_ended) or abs(named_wall["dir"][0] * b.axis[0][0]
                                               + named_wall["dir"][1] * b.axis[0][1]) > 0.7
        heated = any(k in b.style.kind for k in ("house", "apartment", "residential", "terrace",
                                                 "detached", "farm", "barn", "hotel", "yes"))
        # EVERY ELEMENT COMES FROM THE REGISTRY. The sheet no longer knows what a Kranzgesims is;
        # it hands the context to `features.draw_all` and every registered element that applies
        # here draws itself, at or below the LOD rung asked for. Adding one is adding a function
        # to `features.py` and one line -- there is no dispatcher here to extend.
        features.draw_all(features.Draw(
            axe=axe, b=b, f=f, wall=wall, el=el, ink=ink, extent=extent, joints=joints,
            street=street, rise=rise, gable_ended=gable_ended, faces_slope=faces_slope,
            heated=heated, named_wall=named_wall, proj=proj, H=H, L=L))
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
    wrong, degenerate, _ = b.winding()
    if wrong or degenerate:
        red.append(f"B-wound({wrong}e,{degenerate}deg)")
    if b.volume() <= 0:
        red.append("B-wound")
    if b.skirt_gap_m() > 1e-6:
        red.append("B1-gap")
    if b.ridge_error_m() > 0.05:
        red.append("B-roof")
    if ground.water is not None and b.pad < ground.water:
        red.append("B3-water")
    publish.take("buildings", f"{case[0]}_{case[1]}_{b.style.kind}_{b.style.epoch}",
                 draw(case, b, f, number), red)
    return b, red, f, ladder


def main(argv):
    picked = [c for c in CASES if not argv or any(a in c[0] or a in c[1] or a in str(c[2]) for a in argv)]
    if not argv:
        publish.sweep("buildings")
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
