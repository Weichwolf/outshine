"""The synthetic bed: every case of CASES.md, built from a terrain function and a network, the
map solved, the structure built, the invariants checked, a picture written.

    python3 test/lab/roads/synthetic.py [case ...]        # all cases, or the ones named

A case is (terrain, network). The terrain is a height function z(x, y) sampled on a posting
grid and read back bilinearly -- the DEM as the engine reads it -- plus an optional water
level. The network is nodes and ways with OSM tags. The MAP is one height per node and a C1
profile per way (the QP of band.py with bridges and tunnels as their own terms); a junction
is one PLANE from its major leg's surface, and every minor leg's profile ends on that plane
(height and grade), so that no step can stand along the junction's boundary. The STRUCTURE
is the legs' surfaces and the junction polygon lifted onto the plane, checked against the
legs where they meet (I6), against the water (I4) and the terrain (I5).
"""
import math
import pathlib
import sys

import numpy as np
import scipy.sparse as sp
from scipy.sparse.linalg import spsolve
from shapely.geometry import LineString, Point, Polygon
from shapely.ops import substring, unary_union

OUT = pathlib.Path(__import__("os").environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "synthetic"

POSTING_M = 25.0          # the engine's DEM at zoom 12, 49 N (measured: 24.9 m)
DEM_ERROR_M = 4.0         # [SET] Copernicus GLO-30 LE90 < 4 m
DECK_TIE = 1e-6           # a deck with no abutment keeps a weak tie to the DEM
ROAD_FREEBOARD_M = 1.0    # [SET] a causeway's carriageway stands this far above the water
CLEARANCE_M = 4.5         # [SET] the clearance a bridge owes the water or the road below
DECK_GRADE = 0.04         # [SET] a deck's own grade, RAA/RAL bridges: the expensive part stays near level
DECK_STIFF = 10.0         # [SET] a deck resists bending ten times more than the fill beside it -- the lift goes to the ramps
COVER_M = 3.0             # [SET] the least rock a tunnel keeps above its crown
WELD_M = 1e-3
MESH_TOL_M = 0.01         # [SET] the drawn surface stands within a centimetre of the analytic one
STEP_TOL_M = 1e-3
TANGENT_H = 0.05          # [SET] the central difference a centreline's tangent is read over
# to gain the clearance at a road's own design grade takes CLEARANCE_M / grade of length, and a
# railway's grade is a fifth of a road's, so the budget is set by the steepest thing that has to
# climb: [SET] 4.5 m at 1.25 % is 360 m, rounded to the 400 m an embankment is actually built over
CLEARANCE_RAIL_M = 6.00   # [SET] EBO: 5.50 m over the rail plus the catenary's own room
# DERIVED, not chosen: to bring the tallest clearance down at the FLATTEST class's grade takes
# gap / g of length. A railway's 1.25 % against a 6 m clearance over a catenary is 480 m, so 400 m
# was short by eighty and Chicago's elevated `L` could not come down inside it at all (measured
# 2026-09-06: the DECK's own grade had to give 0.56 m at Wacker Drive, on rail ways every one).
RAMP_REACH_M = math.ceil(CLEARANCE_RAIL_M / 0.0125 / 100.0) * 100.0
APPROACH_M = 60.0         # [SET] how far from a shared node a way is still the deck's approach
CLEARANCE_ROUNDS = 40     # [SET] a bound, not a schedule: the loop stops on its own residual
BAND_SLACK_M = 1e-4       # [SET] the QP's own primal tolerance, allowed to the check as well
CLEARANCE_TOL_M = 1e-4    # a fixed point is reached when a round moves nothing a driver feels
CROSSFALL = 0.025         # [SET] RAS-Q / AASHTO normal crown 2.5 %
# --- the road builder's own numbers, each with its origin -------------------------------------
# RAL 2012 (Richtlinien fuer die Anlage von Landstrassen) and RAS-Q; AASHTO's Green Book agrees
# within a percent on all of them. A number here is [SET] from a table, never from taste.
DESIGN_SPEED = {"motorway": 130.0, "trunk": 110.0, "primary": 100.0, "secondary": 80.0,
                "tertiary": 70.0, "residential": 50.0, "living_street": 30.0, "service": 30.0}
SUPER_MAX = 0.06          # [SET] RAL 2012: q_max = 6 % on a Landstrasse (7 % motorway, 6 % here)
SUPER_MIN = 0.025         # [SET] RAS-Q: the normal crown, and the least fall a curve may carry
FRICTION_SIDE = 0.10      # [SET] RAL: f_R at 100 km/h, the side friction the superelevation shares
CREST_R_OF_SPEED = 0.75   # derived: RAL's H_K = 0.75 v^2 (v in km/h) gives 7 500 m at 100 km/h
SAG_R_OF_SPEED = 0.30     # derived: RAL's H_W = 0.30 v^2 gives 3 000 m at 100 km/h
KERB_M = 0.12             # [SET] RAS-Q / DIN 483: a kerb stands 12 cm above the carriageway
SHOULDER_M = 0.5          # [SET] RAS-Q: the paved shoulder beside the carriageway on a Landstrasse
BATTER_FILL = 1.5         # [SET] RAS-Q: an embankment falls 1 : 1.5 (rise : run)
BATTER_CUT = 1.0          # [SET] RAS-Q: a cutting stands 1 : 1
WALL_ABOVE_M = 3.0        # [SET] above this the batter becomes a retaining wall

# [SET] the design grade per class: RAL 2012 (EKL 1..4) for the road classes, RASt 06 for the urban
# ones, DIN 18040 for a ramp a person uses, and a stair's own pitch for steps. FOUR CLASSES WAS NOT
# A TABLE: everything else fell to a 12 % default, which is far too flat for a footbridge whose
# approach is a STAIR -- Hong Kong Central is a net of elevated walkways and its ramp bound had to
# give 3.32 m (measured 2026-09-06) -- and far too steep to mean anything for a motorway.
# THE RATE AT WHICH FILL MAY CHANGE is not the road's design grade. `g` in the ramp bound limits
# how fast the LIFT OVER THE TERRAIN changes along a way, which is a property of the embankment and
# not of the alignment: a motorway's 4 % is what its centreline may climb, while its fill over a
# hillside may thicken far faster. Using the design table there made every road class TIGHTER than
# the 0.12 default it replaced and cost two cases that had been green (the Etoile and Hamburg Hbf,
# measured 2026-09-06: 26 green fell to 24). The default stays 12 %; only what is plainly steeper
# than a road -- a stair, a path -- is listed.
# WHERE THE DATA IS AMBIGUOUS THE BOUND TAKES THE LOOSEST THE CLASS ALLOWS, because a bound that
# refuses produces nothing at all. A `footway` reaching an elevated deck is a stair, a ramp or an
# escalator and OSM marks only the first of the three: Hong Kong Central is a net of walkways two
# storeys up and its ramp bound had to give 3.32 m at a way tagged plain `footway` (measured
# 2026-09-06). A stair's own pitch is the loosest of them and that is what such a way may be.
# [SET] 0.25. WHAT HOLDS FILL IS A BATTER OR A WALL, not a grade: the rate at which an embankment
# thickens along a road is bounded by what stands beside it, and this bed already builds a
# retaining wall wherever the fill passes 3 m. So this bound is not the thing that decides whether
# a ramp is plausible -- the earthworks are -- and holding it to a road's own 12 % refused two of
# the densest vertical cities on the ladder (Chicago's Lower and Upper Wacker, whose connecting
# ramps run at 10 to 15 %, and Hong Kong Central's service roads climbing onto podiums; measured
# 2026-09-06). 25 % is steeper than any road grade and is where a wall is implied.
LIFT_RATE_M = 0.25
LIFT_RATE_OF = {"steps": 0.60, "path": 0.25, "track": 0.20, "funicular": 0.60,
                "footway": 0.50, "pedestrian": 0.35, "cycleway": 0.25}

GRADE_OF = {"motorway": 0.04, "motorway_link": 0.06, "trunk": 0.045, "trunk_link": 0.06,
            "primary": 0.06, "primary_link": 0.07, "secondary": 0.08, "secondary_link": 0.08,
            "tertiary": 0.09, "tertiary_link": 0.09, "unclassified": 0.12, "residential": 0.12,
            "living_street": 0.12, "service": 0.15, "track": 0.20, "bridleway": 0.20,
            "pedestrian": 0.10, "footway": 0.15, "cycleway": 0.10, "path": 0.25, "steps": 0.60,
            "rail": 0.0125, "light_rail": 0.04, "tram": 0.06, "subway": 0.04, "monorail": 0.06,
            "funicular": 0.60}
JOIN_M = 10.0             # [SET] netconvert's --junctions.join default: nodes nearer than this are one junction
WARP_M = 20.0             # [SET] RAS-K: a side road's section is warped into the through road's surface over its last ~20 m


# ----------------------------------------------------------------------------- terrain

class Terrain:
    """z(x, y) sampled on a posting grid, bilinear between postings, like the engine's DEM."""

    def __init__(self, fn, extent=1400.0, posting=POSTING_M, water=None, holes=()):
        self.fn = fn
        self.posting = posting
        self.water = water
        n = int(2 * extent / posting) + 3
        self.x0 = -extent - posting
        self.grid = np.array([[fn(self.x0 + i * posting, self.x0 + j * posting) for i in range(n)] for j in range(n)])
        for (i, j) in holes:
            self.grid[j, i] = np.nan

    def bend(self, x, y):
        """The DEM's own CURVATURE at (x, y), as the largest second difference of its grid.

        A bilinear interpolant reproduces a PLANE exactly, so a uniform 40 % hillside costs it
        nothing; what it cannot represent is a BEND smaller than its posting -- a ridge, a cut,
        a hairpin whose two legs fall inside one cell. The interpolation error there is about
        |z''| h^2 / 8, and that, not a constant, is what a road's distance from the DEM may be
        (measured 2026-09-06: the Transfagarasan needs 11.64 m of it at a hairpin, and the same
        road on a smooth slope needs none)."""
        h = self.posting
        fx = (x - self.x0) / h
        fy = (y - self.x0) / h
        n = self.grid.shape[0]
        i = int(min(max(round(fx), 1), n - 2))
        j = int(min(max(round(fy), 1), n - 2))
        g = self.grid
        with np.errstate(invalid="ignore"):
            dxx = abs(g[j, i - 1] - 2.0 * g[j, i] + g[j, i + 1])
            dyy = abs(g[j - 1, i] - 2.0 * g[j, i] + g[j + 1, i])
        worst = np.nanmax([dxx, dyy])
        return 0.0 if not np.isfinite(worst) else float(worst) / (h * h)

    def dem(self, x, y):
        """The DEM's answer: bilinear between postings; NaN where a posting is a hole."""
        fx = (x - self.x0) / self.posting
        fy = (y - self.x0) / self.posting
        # a sample outside the grid is CLAMPED to its edge rather than raising: a 900 m
        # suspension bridge reaches past a bed sized for a 600 m case, and a bed that dies
        # there tests nothing (measured: IndexError at the first long span)
        n = self.grid.shape[0]
        fx = min(max(fx, 0.0), n - 1.001)
        fy = min(max(fy, 0.0), n - 1.001)
        i, j = int(math.floor(fx)), int(math.floor(fy))
        tx, ty = fx - i, fy - j
        g = self.grid
        return (g[j, i] * (1 - tx) * (1 - ty) + g[j, i + 1] * tx * (1 - ty)
                + g[j + 1, i] * (1 - tx) * ty + g[j + 1, i + 1] * tx * ty)

    def filled(self, x, y):
        """The DEM with its holes closed by the nearest posting that has a height: a hole is a
        missing MEASUREMENT, not a hole in the ground, and an earthwork that reads NaN cannot
        find its toe (measured: 26 of 502 rows unreached over one missing posting)."""
        z = self.dem(x, y)
        if not np.isnan(z):
            return z
        for r in (1, 2, 3, 4):
            for (dx, dy) in ((r, 0), (-r, 0), (0, r), (0, -r), (r, r), (-r, -r), (r, -r), (-r, r)):
                z = self.dem(x + dx * self.posting, y + dy * self.posting)
                if not np.isnan(z):
                    return z
        return 0.0

    def truth(self, x, y):
        return self.fn(x, y)


def t_flat():
    return Terrain(lambda x, y: 100.0)


def t_slope_along(grade):
    return Terrain(lambda x, y: 100.0 + grade * x)


def t_slope_cross(grade):
    return Terrain(lambda x, y: 100.0 + grade * y)


def t_crest(radius=300.0, height=30.0):
    return Terrain(lambda x, y: 100.0 + height * math.exp(-(x * x) / (2 * radius * radius)))


def t_sag(radius=300.0, depth=30.0):
    return Terrain(lambda x, y: 100.0 - depth * math.exp(-(x * x) / (2 * radius * radius)))


def t_terraces(step=3.0, spacing=30.0):
    return Terrain(lambda x, y: 100.0 + step * math.floor((x + 300.0) / spacing))


def t_cliff(height=20.0, at=0.0):
    return Terrain(lambda x, y: 100.0 + (height if x > at else 0.0))


def t_valley(depth=30.0, width=120.0, water_depth=10.0):
    fn = lambda x, y: 100.0 - depth * max(0.0, 1.0 - abs(x) / width)  # noqa: E731
    return Terrain(fn, water=100.0 - depth + water_depth)


def t_noise(sigma=1.0, seed=7):
    rng = np.random.default_rng(seed)
    cache = {}

    def fn(x, y):
        key = (round(x, 3), round(y, 3))
        if key not in cache:
            cache[key] = 100.0 + rng.normal(0.0, sigma)
        return cache[key]
    return Terrain(fn)


def t_strait(width, depth=55.0, level=60.0):
    """A strait of a stated width: the water is the obstacle and its banks are where a pier may
    stand. The bed pairs a span with the width it must cross, because a deck that ENDS inside
    the obstacle makes its approaches dive into it -- measured -31 % and +41 % before this."""
    half = width / 2.0
    return Terrain(lambda x, y: 100.0 - depth * max(0.0, min(1.0, (half + 40.0 - abs(x)) / 40.0)),
                   water=level, extent=max(1400.0, width * 1.6))


def t_gorge(width, depth=90.0):
    half = width / 2.0
    return Terrain(lambda x, y: 100.0 - depth * max(0.0, min(1.0, (half + 20.0 - abs(x)) / 20.0)),
                   extent=max(1400.0, width * 1.8))


def t_hole():
    t = t_slope_cross(0.15)
    n = t.grid.shape[0]
    t.grid[n // 2, n // 2] = np.nan
    return t


def t_coast(sea=100.0, shelf=8.0):
    """A plateau meeting the sea: land falls to the water over 100 m and stays below it."""
    return Terrain(lambda x, y: sea + shelf * max(-1.0, min(1.0, x / 100.0)), water=sea)


def t_mountain(grade=0.40):
    return Terrain(lambda x, y: 100.0 + grade * y)


def t_baked_bridge(depth=30.0, width=120.0, deck=100.0, band=12.0):
    """A LiDAR DEM that already holds the deck: the valley, with a ridge along y=0 at deck
    height where the bridge stands. The profile must not stack a bridge on a bridge."""
    def fn(x, y):
        ground = 100.0 - depth * max(0.0, 1.0 - abs(x) / width)
        return deck if abs(y) < band and abs(x) < width else ground
    return Terrain(fn, water=100.0 - depth + 10.0)


def t_coarse(posting=90.0):
    return Terrain(lambda x, y: 100.0 + 30.0 * math.exp(-(x * x) / (2 * 300.0 * 300.0)), posting=posting)


TERRAINS = {
    "T1-flat": t_flat,
    "T2-along5": lambda: t_slope_along(0.05),
    "T2-along15": lambda: t_slope_along(0.15),
    "T3-cross5": lambda: t_slope_cross(0.05),
    "T3-cross15": lambda: t_slope_cross(0.15),
    "T3-cross30": lambda: t_slope_cross(0.30),
    "T4-crest": t_crest,
    "T5-sag": t_sag,
    "T6-terraces": t_terraces,
    "T7-cliff": t_cliff,
    "T8-valley": t_valley,
    "T11-noise": t_noise,
    "T12-hole": t_hole,
    "T9-coast": t_coast,
    "T10-mountain": t_mountain,
    "T13-baked": t_baked_bridge,
    "T14-coarse": t_coarse,
    "T3-cross60": lambda: t_slope_cross(0.60),
    "T15-lake": lambda: Terrain(lambda x, y: 100.0 - 12.0 * max(0.0, 1.0 - abs(y) / 90.0), water=94.0),
    "T16-strait": lambda: t_strait(420.0),
    "T16-strait900": lambda: t_strait(900.0),
    "T17-gorge": lambda: t_gorge(180.0),
    "T17-gorge90": lambda: t_gorge(90.0),
    "T18-channel": lambda: t_strait(140.0, depth=25.0, level=80.0),
}


# ----------------------------------------------------------------------------- network

class Network:
    def __init__(self):
        self.nodes = {}
        self.ways = []
        self._next = 1

    def node(self, x, y, id_=None):
        if id_ is None:
            id_ = self._next
            self._next += 1
        self.nodes[id_] = (float(x), float(y))
        return id_

    def way(self, refs, **tags):
        tags.setdefault("highway", "residential")
        tags.setdefault("width", 7.0)
        tags.setdefault("priority", {"primary": 10, "secondary": 8, "residential": 5, "service": 2}.get(tags["highway"], 5))
        self.ways.append({"id": len(self.ways) + 1, "refs": list(refs), "tags": tags})
        return self.ways[-1]

    def polyline(self, pts, **tags):
        refs = [self.node(*p) for p in pts]
        return self.way(refs, **tags)

    def spans(self, way):
        return way["tags"].get("bridge", "no") != "no"

    def bores(self, way):
        return way["tags"].get("tunnel", "no") != "no"


def line_pts(x0, y0, x1, y1, step=20.0):
    n = max(2, int(math.hypot(x1 - x0, y1 - y0) / step) + 1)
    return [(x0 + (x1 - x0) * k / (n - 1), y0 + (y1 - y0) * k / (n - 1)) for k in range(n)]


def r_straight(**tags):
    net = Network()
    net.polyline(line_pts(-500, 0, 500, 0), **tags)
    return net


def r_curve(radius, sweep_deg=90.0):
    net = Network()
    pts = [(-300, 0), (-100, 0)]
    n = max(4, int(math.radians(sweep_deg) * radius / 10.0))
    for k in range(1, n + 1):
        a = math.radians(sweep_deg) * k / n
        pts.append((-100 + radius * math.sin(a), radius * (1 - math.cos(a))))
    end = pts[-1]
    d = (math.cos(math.radians(sweep_deg)), math.sin(math.radians(sweep_deg)))
    pts += [(end[0] + d[0] * 200, end[1] + d[1] * 200)]
    net.polyline(pts)
    return net


def r_t_junction(angle_deg=90.0, major="primary", minor="residential"):
    net = Network()
    o = net.node(0, 0)
    a = [net.node(*p) for p in line_pts(-400, 0, -20, 0)]
    b = [net.node(*p) for p in line_pts(20, 0, 400, 0)]
    net.way(a + [o] + b, highway=major, width=10.0)
    d = (math.cos(math.radians(angle_deg)), math.sin(math.radians(angle_deg)))
    c = [net.node(*p) for p in line_pts(d[0] * 20, d[1] * 20, d[0] * 400, d[1] * 400)]
    net.way([o] + c, highway=minor, width=6.0)
    return net


def r_x_junction(skew_deg=90.0):
    net = Network()
    o = net.node(0, 0)
    a = [net.node(*p) for p in line_pts(-400, 0, -20, 0)]
    b = [net.node(*p) for p in line_pts(20, 0, 400, 0)]
    net.way(a + [o] + b, highway="primary", width=10.0)
    d = (math.cos(math.radians(skew_deg)), math.sin(math.radians(skew_deg)))
    c = [net.node(*p) for p in line_pts(-d[0] * 400, -d[1] * 400, -d[0] * 20, -d[1] * 20)]
    e = [net.node(*p) for p in line_pts(d[0] * 20, d[1] * 20, d[0] * 400, d[1] * 400)]
    net.way(c + [o] + e, highway="residential", width=7.0)
    return net


def r_bridge_over_valley():
    net = Network()
    left = [net.node(*p) for p in line_pts(-500, 0, -160, 0)]
    deck = [net.node(*p) for p in line_pts(-140, 0, 140, 0)]
    right = [net.node(*p) for p in line_pts(160, 0, 500, 0)]
    net.way(left + [deck[0]], highway="primary", width=10.0)
    net.way(deck, highway="primary", width=10.0, bridge="yes", layer=1)
    net.way([deck[-1]] + right, highway="primary", width=10.0)
    return net


def r_tunnel_through_crest():
    net = Network()
    left = [net.node(*p) for p in line_pts(-500, 0, -260, 0)]
    bore = [net.node(*p) for p in line_pts(-240, 0, 240, 0)]
    right = [net.node(*p) for p in line_pts(260, 0, 500, 0)]
    net.way(left + [bore[0]], highway="primary", width=10.0)
    net.way(bore, highway="primary", width=10.0, tunnel="yes", layer=-1)
    net.way([bore[-1]] + right, highway="primary", width=10.0)
    return net


def r_interchange():
    """A bridge road (layer 1) over a ground road (layer 0): no shared node; the deck must clear
    the road below by CLEARANCE_M at the crossing."""
    net = Network()
    net.polyline(line_pts(-500, 0, 500, 0), highway="primary", width=10.0)
    south = [net.node(*p) for p in line_pts(0, -500, 0, -110)]
    deck = [net.node(*p) for p in line_pts(0, -90, 0, 90)]
    north = [net.node(*p) for p in line_pts(0, 110, 0, 500)]
    net.way(south + [deck[0]], highway="secondary", width=8.0)
    net.way(deck, highway="secondary", width=8.0, bridge="yes", layer=1)
    net.way([deck[-1]] + north, highway="secondary", width=8.0)
    return net


def r_roundabout(radius=15.0, legs=4):
    net = Network()
    ring = []
    n = 16
    for k in range(n):
        a = 2 * math.pi * k / n
        ring.append(net.node(radius * math.cos(a), radius * math.sin(a)))
    net.way(ring + [ring[0]], highway="primary", width=7.0, junction="roundabout", priority=9)
    for k in range(legs):
        a = 2 * math.pi * k / legs
        at = ring[(k * n) // legs]
        d = (math.cos(a), math.sin(a))
        out = [net.node(*p) for p in line_pts(d[0] * (radius + 20), d[1] * (radius + 20), d[0] * 400, d[1] * 400)]
        net.way([at] + out, highway="residential", width=7.0)
    return net


def r_ramp(angle_deg=8.0):
    net = Network()
    o = net.node(0, 0)
    a = [net.node(*p) for p in line_pts(-500, 0, -20, 0)]
    b = [net.node(*p) for p in line_pts(20, 0, 500, 0)]
    net.way(a + [o] + b, highway="primary", width=10.0)
    d = (math.cos(math.radians(angle_deg)), math.sin(math.radians(angle_deg)))
    ramp = [net.node(*p) for p in line_pts(d[0] * 20, d[1] * 20, d[0] * 400, d[1] * 400)]
    net.way([o] + ramp, highway="secondary", width=5.0, oneway="yes")
    return net


def r_dual():
    net = Network()
    n1 = [net.node(*p) for p in line_pts(-400, -8, -20, -8)]
    m1 = net.node(0, -8)
    e1 = [net.node(*p) for p in line_pts(20, -8, 400, -8)]
    net.way(n1 + [m1] + e1, highway="primary", width=8.0, oneway="yes")
    n2 = [net.node(*p) for p in line_pts(400, 8, 20, 8)]
    m2 = net.node(0, 8)
    e2 = [net.node(*p) for p in line_pts(-20, 8, -400, 8)]
    net.way(n2 + [m2] + e2, highway="primary", width=8.0, oneway="yes")
    s = [net.node(*p) for p in line_pts(0, -400, 0, -28)]
    x = [net.node(*p) for p in line_pts(0, 28, 0, 400)]
    net.way(s + [m1, m2] + x, highway="residential", width=6.0)
    return net


def r_cul_de_sac():
    net = Network()
    net.polyline(line_pts(-500, 0, 500, 0), highway="primary", width=10.0)
    o = net.nodes  # noqa: F841
    return net


def p_duplicate_nodes():
    net = Network()
    a = [net.node(*p) for p in line_pts(-500, 0, 0, 0)]
    b = [net.node(*p) for p in line_pts(0, 0, 500, 0)]     # b[0] duplicates a[-1] at (0, 0)
    net.way(a, highway="primary", width=10.0)
    net.way(b, highway="primary", width=10.0)
    return net


def p_zero_length():
    net = Network()
    pts = line_pts(-500, 0, 500, 0)
    pts.insert(5, pts[5])                                   # the same point twice
    net.polyline(pts, highway="primary", width=10.0)
    return net


def p_gap():
    net = Network()
    net.polyline(line_pts(-500, 0, -0.3, 0), highway="primary", width=10.0)
    net.polyline(line_pts(0.2, 0, 500, 0), highway="primary", width=10.0)
    return net


def p_duplicate_way():
    net = Network()
    refs = [net.node(*p) for p in line_pts(-500, 0, 500, 0)]
    net.way(refs, highway="primary", width=10.0)
    net.way(refs, highway="primary", width=10.0)
    return net


def p_bridge_no_landing():
    net = Network()
    net.polyline(line_pts(-200, 0, 200, 0), highway="primary", width=10.0, bridge="yes", layer=1)
    return net


def p_dense():
    net = Network()
    net.polyline(line_pts(-500, 0, -50, 0) + line_pts(-49.9, 0, 50, 0, step=0.1) + line_pts(50.1, 0, 500, 0), highway="primary", width=10.0)
    return net


def r_s_curve(radius=120.0):
    net = Network()
    pts = [(-400, 0)]
    n = 24
    for k in range(1, n + 1):
        a = math.pi * k / n
        pts.append((-200 + radius * math.sin(a), radius * (1 - math.cos(a))))
    for k in range(1, n + 1):
        a = math.pi * k / n
        pts.append((pts[-1][0] + radius * math.sin(a) * 0.0 + 10.0, pts[-1][1] - 0.0))
    net.polyline([p for p in pts], highway="secondary", width=8.0)
    return net


def r_fork(angle_deg=20.0):
    net = Network()
    o = net.node(0, 0)
    stem = [net.node(*p) for p in line_pts(-400, 0, -20, 0)]
    net.way(stem + [o], highway="primary", width=10.0)
    for sign in (+1, -1):
        d = (math.cos(math.radians(angle_deg * sign)), math.sin(math.radians(angle_deg * sign)))
        arm = [net.node(*p) for p in line_pts(d[0] * 20, d[1] * 20, d[0] * 400, d[1] * 400)]
        net.way([o] + arm, highway="secondary", width=8.0)
    return net


def r_stacked():
    """A bridge over a bridge over a road: three levels, no shared node."""
    net = r_interchange()
    east = [net.node(*p) for p in line_pts(-500, 200, -110, 200)]
    deck = [net.node(*p) for p in line_pts(-90, 200, 90, 200)]
    west = [net.node(*p) for p in line_pts(110, 200, 500, 200)]
    net.way(east + [deck[0]], highway="secondary", width=8.0)
    net.way(deck, highway="secondary", width=8.0, bridge="yes", layer=2)
    net.way([deck[-1]] + west, highway="secondary", width=8.0)
    return net


def r_ramp_cycle():
    """A DECK OVER ANOTHER DECK'S RAMP: the case the clearance fixed point cannot be proved on.

    The floors are re-read from the roads below as they now stand, so lifting the lower deck
    lifts its ramp, which lifts the floor under the upper deck, which lifts the upper deck --
    and nothing says that loop contracts. `R11-stacked` does not reach it because its three
    levels share no node and its middle level is a DECK, not a ramp. Here the upper bridge
    stands over the LOWER BRIDGE'S APPROACH, which is the ordinary shape of every multi-level
    interchange on the planet (measured 2026-09-06: the real Transfagarasan and Stelvio walk to
    105 m and 183 m off the DEM over forty rounds and never converge)."""
    net = Network()
    # the ground road, west to east, crossed twice
    net.polyline(line_pts(-500, 0, 500, 0), highway="primary", width=10.0)
    # the LOWER bridge: north-south over the ground road, with long approaches that ramp
    lo_s = [net.node(*p) for p in line_pts(0, -420, 0, -60)]
    lo_d = [net.node(*p) for p in line_pts(0, -40, 0, 40)]
    lo_n = [net.node(*p) for p in line_pts(0, 60, 0, 420)]
    net.way(lo_s + [lo_d[0]], highway="secondary", width=8.0)
    net.way(lo_d, highway="secondary", width=8.0, bridge="yes", layer=1)
    net.way([lo_d[-1]] + lo_n, highway="secondary", width=8.0)
    # the UPPER bridge: east-west over the lower bridge's SOUTHERN APPROACH, not over its deck
    up_w = [net.node(*p) for p in line_pts(-460, -200, -70, -200)]
    up_d = [net.node(*p) for p in line_pts(-50, -200, 50, -200)]
    up_e = [net.node(*p) for p in line_pts(70, -200, 460, -200)]
    net.way(up_w + [up_d[0]], highway="secondary", width=8.0)
    net.way(up_d, highway="secondary", width=8.0, bridge="yes", layer=2)
    net.way([up_d[-1]] + up_e, highway="secondary", width=8.0)
    return net


def r_spiral(radius=70.0, turns=1.6, rise=34.0, gap_deg=26.0):
    """A ROAD THAT CROSSES ITSELF: a spiral ramp, and the clearance cycle is inside ONE road.

    `R11-rampcycle` puts the upper deck over ANOTHER road's ramp and converges; the shape that
    does not is the one where the road below the deck IS the road carrying it. Lifting the deck
    lifts the spiral, which raises the floor under the deck, which lifts the deck. The Brusio
    viaduct is this exactly, and so is every hairpin whose bridge stands over the leg below it --
    which is what the Transfagarasan and the Stelvio are made of."""
    net = Network()
    pts, zs = [], []
    n = int(turns * 48)
    for k in range(n + 1):
        a = 2.0 * math.pi * turns * k / n
        pts.append((radius * math.cos(a), radius * math.sin(a)))
        zs.append(rise * k / n)
    # the crossing is where the last turn passes over the first: the arc there is the deck
    over = [k for k in range(n + 1) if 2.0 * math.pi * turns * k / n > 2.0 * math.pi
            and abs(math.degrees(2.0 * math.pi * turns * k / n) - 360.0) < gap_deg]
    lo, hi = (min(over), max(over)) if over else (n - 4, n - 1)
    ids = [net.node(*p) for p in pts]
    net.way(ids[:lo + 1], highway="secondary", width=8.0)
    net.way(ids[lo:hi + 1], highway="secondary", width=8.0, bridge="yes", layer=1)
    net.way(ids[hi:], highway="secondary", width=8.0)
    return net


def r_cul_de_sac_real():
    net = Network()
    o = net.node(0, 0)
    a = [net.node(*p) for p in line_pts(-400, 0, -20, 0)]
    b = [net.node(*p) for p in line_pts(20, 0, 400, 0)]
    net.way(a + [o] + b, highway="primary", width=10.0)
    stub = [net.node(*p) for p in line_pts(0, 20, 0, 120)]
    net.way([o] + stub, highway="service", width=5.0)
    return net


def r_widths():
    net = Network()
    o = net.node(0, 0)
    a = [net.node(*p) for p in line_pts(-400, 0, -20, 0)]
    b = [net.node(*p) for p in line_pts(20, 0, 400, 0)]
    net.way(a + [o] + b, highway="primary", width=12.0)
    c = [net.node(*p) for p in line_pts(0, 20, 0, 400)]
    net.way([o] + c, highway="service", width=4.0)
    return net


def r_oneway_pair(apart=20.0):
    net = Network()
    net.polyline(line_pts(-400, -apart / 2, 400, -apart / 2), highway="primary", width=8.0, oneway="yes")
    net.polyline(line_pts(400, apart / 2, -400, apart / 2), highway="primary", width=8.0, oneway="yes")
    return net


def r_viaduct(span=60.0, length=600.0):
    net = Network()
    left = [net.node(*p) for p in line_pts(-500, 0, -length / 2 - 20, 0)]
    deck = [net.node(*p) for p in line_pts(-length / 2, 0, length / 2, 0, step=span)]
    right = [net.node(*p) for p in line_pts(length / 2 + 20, 0, 500, 0)]
    net.way(left + [deck[0]], highway="primary", width=10.0)
    net.way(deck, highway="primary", width=10.0, bridge="yes", layer=1)
    net.way([deck[-1]] + right, highway="primary", width=10.0)
    return net


def r_underpass():
    net = Network()
    net.polyline(line_pts(-500, 0, 500, 0), highway="primary", width=10.0)   # the road above
    south = [net.node(*p) for p in line_pts(0, -400, 0, -60)]
    bore = [net.node(*p) for p in line_pts(0, -40, 0, 40)]
    north = [net.node(*p) for p in line_pts(0, 60, 0, 400)]
    net.way(south + [bore[0]], highway="secondary", width=8.0)
    net.way(bore, highway="secondary", width=8.0, tunnel="yes", layer=-1)
    net.way([bore[-1]] + north, highway="secondary", width=8.0)
    return net


def r_causeway():
    net = Network()
    net.polyline(line_pts(-500, 0, 500, 0), highway="primary", width=10.0, embankment="yes")
    return net


def r_ford():
    net = Network()
    net.polyline(line_pts(-500, 0, 500, 0), highway="track", width=3.0, ford="yes")
    return net


def p_layer_no_bridge():
    net = Network()
    net.polyline(line_pts(-500, 0, 500, 0), highway="primary", width=10.0, layer=1)
    return net


def r_serpentine(radius=14.0, legs=4, rise=140.0, step=25.0):
    """A mountain road's hairpins: legs across the slope joined by turns of the tightest radius.

    `step` is the NODE SPACING and it is a case of its own, not a drawing detail: at 25 m the
    nodes fall where the DEM's postings do, and at 4 m they fall where a surveyor puts them on a
    real pass road. The smoothing weight is calibrated as l^4 with l = 2 postings, so its ratio
    to the data term goes like (l/ds)^4 -- one at 25 m, ten thousand at 4 m -- and a bed that
    only ever samples at the posting cannot see it (measured 2026-09-06: 37.4 m off the DEM on
    the Transfagarasan, whose nodes are metres apart, where every synthetic case was green)."""
    net = Network()
    pts = []
    y = -rise / 2
    for k in range(legs):
        x0, x1 = (-150.0, 150.0) if k % 2 == 0 else (150.0, -150.0)
        pts += line_pts(x0, y, x1, y, step=step)
        if k < legs - 1:
            cx = x1
            for a in range(1, 7):
                ang = math.pi * a / 6 * (1 if k % 2 == 0 else -1)
                pts.append((cx + radius * math.sin(ang) * (1 if k % 2 == 0 else -1),
                            y + radius * (1 - math.cos(ang))))
            y += 2 * radius
    net.polyline(pts, highway="secondary", width=7.0)
    return net


def r_embankment():
    """A road carried 5 m above the plain: OSM's embankment=yes, and the fill is the earthwork."""
    net = Network()
    net.polyline(line_pts(-400, 0, 400, 0), highway="primary", width=10.0, embankment="yes")
    return net


def r_cutting():
    net = Network()
    net.polyline(line_pts(-400, 0, 400, 0), highway="primary", width=10.0, cutting="yes")
    return net


def r_parking():
    """A paved AREA, not a ribbon: a car park polygon beside a road."""
    net = Network()
    net.polyline(line_pts(-400, 0, 400, 0), highway="primary", width=10.0)
    net.area = Polygon([(20, 20), (80, 20), (80, 60), (20, 60)])
    net.area_tags = {"amenity": "parking", "surface": "asphalt"}
    return net


def r_steps():
    net = Network()
    net.polyline(line_pts(-100, 0, 100, 0), highway="steps", width=2.0)
    return net


def r_track():
    net = Network()
    net.polyline(line_pts(-400, 0, 400, 0), highway="track", width=3.0, surface="ground")
    return net


def r_elevated():
    """An elevated urban road over a street grid: bridge=yes over four cross streets."""
    net = Network()
    deck = [net.node(*p) for p in line_pts(-300, 0, 300, 0)]
    net.way(deck, highway="primary", width=12.0, bridge="yes", layer=1)
    for x in (-180, -60, 60, 180):
        net.polyline(line_pts(x, -200, x, 200), highway="residential", width=8.0)
    return net


def r_dam():
    """A road on a dam crest: the terrain is a valley and the road stands on the structure that
    closes it, so both sides fall away and the earthwork is fill on BOTH sides."""
    net = Network()
    net.polyline(line_pts(-300, 0, 300, 0), highway="secondary", width=8.0)
    return net


def p_road_in_building():
    """A road whose way runs through a building footprint: OSM has plenty, and the building wins
    the ground while the road keeps its surface -- the case exists so the rule is stated."""
    net = Network()
    net.polyline(line_pts(-400, 0, 400, 0), highway="residential", width=7.0)
    net.building = Polygon([(-20, -12), (20, -12), (20, 12), (-20, 12)])
    return net


def p_road_in_water():
    """A way over water with neither bridge nor ford: it is drawn as a causeway and COUNTED, so
    a lake full of roads is visible as a number rather than as a picture."""
    net = Network()
    net.polyline(line_pts(-300, 0, 300, 0), highway="residential", width=7.0)
    return net


def p_hole_at_node():
    net = Network()
    net.polyline(line_pts(-300, 0, 300, 0), highway="primary", width=10.0)
    return net


def r_span(length, width=12.0, rail=False, gap=340.0):
    """A crossing of a stated span: the type follows from the number, which is the point."""
    net = Network()
    left = [net.node(*p) for p in line_pts(-gap - 300, 0, -length / 2 - 20, 0)]
    deck = [net.node(*p) for p in line_pts(-length / 2, 0, length / 2, 0, step=max(20.0, length / 12))]
    right = [net.node(*p) for p in line_pts(length / 2 + 20, 0, gap + 300, 0)]
    tags = {"highway": "primary", "width": width}
    if rail:
        tags = {"highway": "primary", "width": width, "railway": "rail"}
    net.way(left + [deck[0]], **tags)
    net.way(deck, bridge="yes", layer=1, **tags)
    net.way([deck[-1]] + right, **tags)
    return net


def r_tile_seam():
    """A way crossing a tile border at x = 0: the bed builds it whole and in two halves and
    compares the vertices where they meet (I8)."""
    net = Network()
    net.polyline(line_pts(-400, 30, 400, -30), highway="primary", width=10.0)
    return net


NETWORKS = {
    "R1-straight": r_straight,
    "R2-curve200": lambda: r_curve(200.0),
    "R2-curve30": lambda: r_curve(30.0),
    "R4-T90": r_t_junction,
    "R4-T30": lambda: r_t_junction(30.0),
    "R5-X90": r_x_junction,
    "R5-X60": lambda: r_x_junction(60.0),
    "R18-bridge": r_bridge_over_valley,
    "R20-tunnel": r_tunnel_through_crest,
    "R10-interchange": r_interchange,
    "R7-roundabout": r_roundabout,
    "R9-ramp": r_ramp,
    "R8-dual": r_dual,
    "P1-dupnodes": p_duplicate_nodes,
    "P2-zerolen": p_zero_length,
    "P3-gap": p_gap,
    "P4-dupway": p_duplicate_way,
    "P5-nolanding": p_bridge_no_landing,
    "P6-dense": p_dense,
    "R3-scurve": r_s_curve,
    "R6-fork": r_fork,
    "R11-stacked": r_stacked,
    "R11-rampcycle": r_ramp_cycle,
    "R11-spiral": r_spiral,
    "R12-culdesac": r_cul_de_sac_real,
    "R15-widths": r_widths,
    "R16-onewaypair": r_oneway_pair,
    "R19-viaduct": r_viaduct,
    "R21-underpass": r_underpass,
    "R24-causeway": r_causeway,
    "R25-ford": r_ford,
    "P7-layer": p_layer_no_bridge,
    "R2-hairpin": r_serpentine,
    # the SAME road, noded as a surveyor nodes it: the pathology is the SAMPLING and nothing else
    "R2-hairpin-dense": lambda: r_serpentine(step=4.0),
    "R22-embankment": r_embankment,
    "R23-cutting": r_cutting,
    "R14-steps": r_steps,
    "R14-track": r_track,
    "R27-elevated": r_elevated,
    "R17-seam": r_tile_seam,
    "R13-parking": r_parking,
    "R26-dam": r_dam,
    # the deck overshoots its obstacle by 60 m each side, so its ABUTMENTS stand on the flat --
    # a deck that ends mid-slope makes its approaches dive into the gap (measured -31 %, +41 %)
    "R28-arch": lambda: r_span(300.0),
    "R29-stayed": lambda: r_span(540.0, width=16.0, gap=500.0),
    "R30-suspension": lambda: r_span(1020.0, width=18.0, gap=900.0),
    "R31-railtruss": lambda: r_span(210.0, width=9.0, rail=True),
    "R32-culvert": lambda: r_span(6.0, width=7.0, gap=120.0),
    "P8-inbuilding": p_road_in_building,
    "P9-inwater": p_road_in_water,
    "P10-holeatnode": p_hole_at_node,
}


# ----------------------------------------------------------------------------- the map

class Map:
    """Node heights, way stations and profiles, junction planes."""

    SNAP_M = 2.0   # the engine's kNodeSnapM

    def __init__(self, terrain, net):
        self.terrain = terrain
        self.net = net
        self._snap()
        self.index = {nid: k for k, nid in enumerate(net.nodes)}
        self.xy = np.array([net.nodes[n] for n in net.nodes])
        self.dem = np.array([self._dem_or_neighbour(n) for n in net.nodes])
        self.stations = {w["id"]: self._stations(w) for w in net.ways}
        self.z = None
        self.ramp_signs = {}
        self.causeway = np.zeros(len(self.index), dtype=bool)
        self.slope = {}
        self.junctions = {}

    def _snap(self):
        """Nodes within SNAP_M of an earlier node are the earlier node (declared order), and a
        way's consecutive duplicates collapse to one -- the weave's word, so P1, P2 and P3 hold."""
        # the search is over a GRID of SNAP_M cells and not over every node kept so far: the
        # linear scan is O(N^2) and a real extract carries ten thousand nodes, which is fifty
        # million distance computations for an answer a hash gives in one pass. The ANSWER is the
        # same -- the candidates in a cell are still taken in DECLARED order and the earliest
        # kept node still wins -- and it has to be, because the order is the invariant
        kept = {}
        alias = {}
        rank = {}
        cells = {}
        for nid, (x, y) in self.net.nodes.items():
            cx, cy = int(math.floor(x / self.SNAP_M)), int(math.floor(y / self.SNAP_M))
            near = []
            for i in (cx - 1, cx, cx + 1):
                for j in (cy - 1, cy, cy + 1):
                    near += cells.get((i, j), ())
            hit = None
            for k in sorted(near, key=lambda one: rank[one]):
                kx, ky = kept[k]
                if math.hypot(kx - x, ky - y) <= self.SNAP_M:
                    hit = k
                    break
            if hit is None:
                kept[nid] = (x, y)
                alias[nid] = nid
                rank[nid] = len(rank)
                cells.setdefault((cx, cy), []).append(nid)
            else:
                alias[nid] = hit
        self.net.nodes = kept
        # A WAY THAT COLLAPSES TO ONE NODE IS NOT A WAY. Real data holds them: a slip road mapped
        # as three nodes inside two metres, a turning circle drawn as a stub. The bed carried them
        # to `centreline`, which asked shapely for a LineString of one point and died with
        # `point array must contain 0 or >1 elements` (measured at the Ponte Vecchio, first real
        # case). They are dropped HERE, once, so nothing downstream has to ask
        self.collapsed_ways = 0
        alive = []
        for w in self.net.ways:
            refs = [alias[r] for r in w["refs"]]
            w["refs"] = [r for k, r in enumerate(refs) if k == 0 or r != refs[k - 1]]
            if len(w["refs"]) < 2:
                self.collapsed_ways += 1
                continue
            alive.append(w)
        self.net.ways = alive
        # a way whose node sequence another way already has, either way round, is the SAME road
        # mapped twice: keeping both draws two surfaces in one place (P4), and OSM has plenty
        seen = {}
        kept_ways = []
        self.duplicate_ways = 0
        for w in self.net.ways:
            key = tuple(w["refs"])
            if key in seen or tuple(reversed(key)) in seen:
                self.duplicate_ways += 1
                continue
            seen[key] = w["id"]
            kept_ways.append(w)
        self.net.ways = kept_ways

    def _box(self, way):
        """The way's bounding box, cached: the cheap gate before any buffer is built."""
        if getattr(self, "_boxes", None) is None:
            self._boxes = {}
        if way["id"] not in self._boxes:
            self._boxes[way["id"]] = self.centreline(way).bounds
        return self._boxes[way["id"]]

    def _ribbons_cross(self, w, other):
        """Do the two carriageways overlap in plan? Cached per pair of ways."""
        key = (w["id"], other["id"])
        if getattr(self, "_cross", None) is None:
            self._cross = {}
        if key not in self._cross:
            a = self.centreline(w).buffer(w["tags"]["width"] / 2.0 + 0.5, join_style=2)
            b = self.centreline(other).buffer(other["tags"]["width"] / 2.0 + 0.5, join_style=2)
            self._cross[key] = a.intersects(b)
        return self._cross[key]

    def band_down(self):
        """The band DOWNWARD, which is not the band upward: a road a deck passes over may go
        deeper by exactly what the crossing needs, because the crossing is evidence of a grade
        separation the DEM shows nothing of. The solve is given this; so is the check, or I3
        reads 1.96 at Millau against a constraint it never violated (measured 2026-09-06)."""
        room = self.clearance_room()
        out = self.band().copy()
        for k, gap in room.items():
            out[int(k)] += gap
        return out

    def clearance_room(self):
        """How far below the terrain each node may go: what a deck above it needs, and no more."""
        got = getattr(self, "_room", None)
        if got is None:
            got = {}
            for (_, j, j2, uu, gap) in getattr(self, "clearance_pairs", ()):
                got[j] = max(got.get(j, 0.0), gap)
                got[j2] = max(got.get(j2, 0.0), gap)
            self._room = got
        return got

    def open_ends(self):
        """Nodes the extract cut off, so their profile owes the DEM nothing."""
        got = getattr(self, "_open_ends", None)
        if got is None:
            got = self._open_ends = np.zeros(len(self.index), dtype=bool)
        return got

    def mark_open_ends(self, half_m, reach_m=40.0):
        """Every node within `reach_m` of the extract's square edge, and everything within
        RAMP_REACH_M of it along the network -- a cut ramp climbs THROUGH the boundary."""
        edge = np.zeros(len(self.index), dtype=bool)
        seeds = []
        for r, (x, y) in self.net.nodes.items():
            if max(abs(x), abs(y)) >= half_m - reach_m:
                edge[self.index[r]] = True
                seeds.append(r)
        adjacency = {}
        for w in self.net.ways:
            for r in w["refs"]:
                adjacency.setdefault(r, []).append(w)
        frontier = [(r, RAMP_REACH_M) for r in sorted(seeds)]
        seen = {r: RAMP_REACH_M for r in seeds}
        while frontier:
            node, budget = frontier.pop()
            for w in adjacency.get(node, ()):
                refs, s = w["refs"], self.stations[w["id"]]
                if node not in refs:
                    continue
                k0 = refs.index(node)
                for k in range(len(refs)):
                    left = budget - abs(s[k] - s[k0])
                    if left <= 0.0:
                        continue
                    r = refs[k]
                    edge[self.index[r]] = True
                    if seen.get(r, -1.0) < left - 1e-6:
                        seen[r] = left
                        frontier.append((r, left))
        self._open_ends = edge
        return edge

    def band(self):
        """How far from the DEM a road on the ground may stand, per node: the survey's own error
        plus what a BILINEAR interpolant cannot represent, which is |z''| h^2 / 8. ONE source --
        the constraint inside the solve and the check afterwards read this same array, or the
        bed would refuse a profile it then calls correct."""
        if getattr(self, "_band", None) is None:
            out = np.zeros(len(self.index))
            h = self.terrain.posting
            for r, k in self.index.items():
                x, y = self.net.nodes[r]
                out[k] = DEM_ERROR_M + self.terrain.bend(x, y) * h * h / 8.0
            self._band = out
        return self._band

    def _dem_or_neighbour(self, nid):
        x, y = self.net.nodes[nid]
        z = self.terrain.dem(x, y)
        if np.isnan(z):
            for (dx, dy) in ((self.terrain.posting, 0), (-self.terrain.posting, 0), (0, self.terrain.posting), (0, -self.terrain.posting)):
                z = self.terrain.dem(x + dx, y + dy)
                if not np.isnan(z):
                    return z
        return z

    def _stations(self, way):
        s = [0.0]
        for a, b in zip(way["refs"], way["refs"][1:]):
            s.append(s[-1] + math.dist(self.net.nodes[a], self.net.nodes[b]))
        return np.array(s)

    def solve(self, smooth_m=2.0 * POSTING_M):
        n = len(self.index)
        fidelity = np.ones(n)
        deck = np.zeros(n, dtype=bool)
        bore = np.zeros(n, dtype=bool)
        for w in self.net.ways:
            if len(w["refs"]) > 2 and (self.net.spans(w) or self.net.bores(w)):
                for r in w["refs"][1:-1]:
                    k = self.index[r]
                    fidelity[k] = DECK_TIE
                    (deck if self.net.spans(w) else bore)[k] = True
        rows, cols, vals, r = [], [], [], 0
        srows, scols, svals, sr = [], [], [], 0
        deck_nodes = {r_ for w in self.net.ways if self.net.spans(w) for r_ in w["refs"]}
        for w in self.net.ways:
            # a way that lands on a deck is a RAMP: its grade is designed, not the terrain's,
            # so it carries no grade-fidelity row (the class grade bounds it instead); with the
            # row it kept 0.8 m of lift 400 m from the bridge rather than come down (measured)
            ramp = (not self.net.spans(w)) and bool(set(w["refs"]) & deck_nodes)
            refs, s = w["refs"], self.stations[w["id"]]
            stiff = math.sqrt(DECK_STIFF) if self.net.spans(w) else 1.0
            for k in range(1, len(refs) - 1):
                d0, d1 = s[k] - s[k - 1], s[k + 1] - s[k]
                if d0 <= 1e-3 or d1 <= 1e-3:
                    continue
                scale = 2.0 * stiff / (d0 + d1)
                for ref, v in ((refs[k - 1], scale / d0), (refs[k], -scale * (1 / d0 + 1 / d1)), (refs[k + 1], scale / d1)):
                    rows.append(r); cols.append(self.index[ref]); vals.append(v)
                r += 1
            for k in range(1, len(refs)):
                ds = s[k] - s[k - 1]
                a, b = self.index[refs[k - 1]], self.index[refs[k]]
                if ds <= 1e-3 or fidelity[a] <= DECK_TIE or fidelity[b] <= DECK_TIE or ramp:
                    continue
                wgt = 1.0 / math.sqrt(ds)
                srows += [sr, sr]; scols += [a, b]; svals += [-wgt, wgt]; sr += 1
        K = sp.csr_matrix((vals, (rows, cols)), shape=(r, n))
        G = sp.csr_matrix((svals, (srows, scols)), shape=(sr, n))
        mu, lam = smooth_m ** 2, smooth_m ** 4
        # A RIDGE WITH A PRIOR. The QP needs A POSITIVE DEFINITE and a long bore has almost no data
        # term: a deck's or a bore's interior carries DECK_TIE against a curvature block of order
        # lambda, and OSQP called the Etoile UNBOUNDED once the constrained solve began running
        # for every case (measured 2026-09-06: 52 tunnels, no bridges, 2 500 nodes). [SET] 1e-6
        # of the mean diagonal -- far below anything the road geometry cares about, far above
        # what the factorisation needs to stay definite -- and it pulls toward the DEM, not
        # toward zero: a ridge on A alone is a prior that the road is at SEA LEVEL, and a bridge
        # with no landing (whose deck has no data term at all) sank 2.67 m under the terrain for
        # exactly that reason (measured 2026-09-06, P5-nolanding).
        A = sp.diags(fidelity) + mu * (G.T @ G) + lam * (K.T @ K)
        ridge = 1e-6 * float(A.diagonal().mean())
        A = A + sp.eye(n) * ridge
        b = fidelity * self.dem + mu * (G.T @ (G @ self.dem)) + ridge * self.dem
        self.z = spsolve(A.tocsc(), b)
        self.deck, self.bore, self.curvature = deck, bore, K
        self.constraints = self._clearances(deck)
        # THE BAND IS NOT OPTIONAL. The constrained solve carried it, and the constrained solve
        # only ran when there were absolute FLOORS -- water or a deck over open ground. A pass
        # road with no bridge at all therefore got the plain smooth solve and no band whatever:
        # the Furka stood 6.09 m off a DEM whose band there is 4.11 m, and the check read 1.48x
        # against a constraint that had never been posed (measured 2026-09-06). It always runs.
        if True:
            self.ramp_signs = {}
            self._solve_with_clearances(A, b)     # once, to read the ramps' signs
            self.ramp_signs = self._ramp_signs()
            # and again: the clearance floors are read from the roads BELOW as they now stand,
            # so a deck over a deck's ramp lifts with it. TWO ROUNDS WAS AN ASSUMPTION AND IT IS
            # WRONG ON REAL DATA -- at the Transfagarasan the rounds moved the profile 24.4 m,
            # then 11.5 m, then 9.1 m and the bed stopped there and called it an answer
            # (measured 2026-09-06). A fixed point that has not converged is a number nobody may
            # quote, so it iterates to a tolerance and REPORTS what is left when it stops.
            # ONE SOLVE. With the deck-over-road clearance held as a PAIR the whole set is
            # linear in z and the problem is convex, so there is nothing to iterate: the second
            # round exists only to PROVE it, and its residual is the proof. It is kept because a
            # claim that a scheme converges is worth exactly the measurement that says so.
            self.clearance_diverged = False
            before = self.z.copy()
            self.constraints = self._clearances(deck)
            self._solve_with_clearances(A, b)
            self.constraints = self._clearances(deck)
            self._solve_with_clearances(A, b)
            self.clearance_rounds = 2
            self.clearance_residual_m = float(np.max(np.abs(self.z - before)))
            after = self.z.copy()
            self.constraints = self._clearances(deck)
            self._solve_with_clearances(A, b)
            self.clearance_residual_m = float(np.max(np.abs(self.z - after)))
        self._slopes()
        self._superelevations()
        self._junctions()
        return self

    def approaches(self):
        """The nodes within RAMP_M along a way of a deck's abutment: the ramp, a DESIGNED
        structure whose profile owes the DEM nothing but its start and end."""
        # the whole of a way that lands on a deck is the ramp's: the fill returns to the
        # terrain at the class's grade and through the vertical curves the smoothing length
        # makes, and where that ends is a NUMBER the bed reports (ramp_m), not a wall
        if getattr(self, "_approach_cache", None) is not None:
            return self._approach_cache
        near = np.zeros(len(self.index), dtype=bool)
        deck_nodes = {r for w in self.net.ways if self.net.spans(w) for r in w["refs"]}
        for r in deck_nodes:
            near[self.index[r]] = True
        # AN EMBANKMENT DOES NOT END WHERE A WAY DOES. To gain h metres at the class's grade a
        # ramp needs h/g of length -- 4.5 m at 4 % is 112 m -- and OSM splits a road wherever a
        # tag changes, so the fill crosses several ways. Marking only the ways that TOUCH the
        # deck pinned the second way of every embankment to the DEM band, and a railway bridge
        # over a motorway then could not rise at all: 9.06 m short at Kaiserberg, where the DEM
        # puts deck and motorway both at 33 m because it has never heard of either (measured
        # 2026-09-06). The walk carries a BUDGET in metres and stops when it runs out.
        adjacency = {}
        for w in self.net.ways:
            for r in w["refs"]:
                adjacency.setdefault(r, []).append(w)
        frontier = [(r, RAMP_REACH_M) for r in sorted(deck_nodes)]
        seen = {r: RAMP_REACH_M for r in deck_nodes}
        while frontier:
            node, budget = frontier.pop()
            if budget <= 0.0:
                continue
            for w in adjacency.get(node, ()):
                if self.net.spans(w):
                    continue
                refs, s = w["refs"], self.stations[w["id"]]
                if node not in refs:
                    continue
                k0 = refs.index(node)
                for k in range(len(refs)):
                    left = budget - abs(s[k] - s[k0])
                    if left <= 0.0:
                        continue
                    r = refs[k]
                    near[self.index[r]] = True
                    if seen.get(r, -1.0) < left - 1e-6:
                        seen[r] = left
                        frontier.append((r, left))
        # how far each approach node is from its abutment ALONG the network: a ramp is elevated
        # near the deck and back at grade further out, and that distance is what decides whether
        # something crossing over it may treat it as ground
        far = np.full(len(self.index), np.inf)
        for r, left in seen.items():
            far[self.index[r]] = RAMP_REACH_M - left
        self._approach_dist = far
        self._approach_cache = near
        return near

    def approach_dist(self):
        """Distance along the network from each node to the nearest bridge abutment."""
        self.approaches()
        return self._approach_dist

    def ramp_m(self):
        """How far from an abutment the road stands more than the DEM's error off the terrain."""
        if self.z is None:
            return 0.0
        deck_nodes = {r for w in self.net.ways if self.net.spans(w) for r in w["refs"]}
        worst = 0.0
        for w in self.net.ways:
            if self.net.spans(w) or not (set(w["refs"]) & deck_nodes):
                continue
            refs, s = w["refs"], self.stations[w["id"]]
            anchors = [s[k] for k, r in enumerate(refs) if r in deck_nodes]
            for k, r in enumerate(refs):
                if abs(self.z[self.index[r]] - self.dem[self.index[r]]) > DEM_ERROR_M:
                    worst = max(worst, min(abs(s[k] - a) for a in anchors))
        return worst

    def _clearances(self, deck):
        """Where a deck node stands over another way's ribbon or over water, the deck owes it
        CLEARANCE_M: a linear inequality z_node >= floor. Between nodes the deck is a smooth
        curve, so the nodes bound it; a crossing between two deck nodes is caught by the node
        on either side (the checks sample the profile along the deck)."""
        floors, pairs = {}, []
        ramps = self.approaches()
        ramp_far = self.approach_dist()
        # a road that crosses WATER without spanning or fording it is a CAUSEWAY: it stands on
        # fill a freeboard above the surface. Measured before this rule: a coast road and a
        # bridge's approaches over a lake sat under the water, which is a road nobody can drive.
        if self.terrain.water is not None:
            for w in self.net.ways:
                if self.net.spans(w) or self.net.bores(w) or w["tags"].get("ford") == "yes":
                    continue
                for r in w["refs"]:
                    x, y = self.net.nodes[r]
                    if self.terrain.truth(x, y) < self.terrain.water:
                        k = self.index[r]
                        floors[k] = max(floors.get(k, -1e9), self.terrain.water + ROAD_FREEBOARD_M)
                        self.causeway[k] = True
        for w in self.net.ways:
            if not self.net.spans(w):
                continue
            refs = w["refs"]
            for r in refs[1:-1]:
                k = self.index[r]
                x, y = self.net.nodes[r]
                floor = None
                if self.terrain.water is not None and self.terrain.truth(x, y) < self.terrain.water:
                    floor = self.terrain.water + CLEARANCE_M
                for other in self.net.ways:
                    if other is w or self.net.spans(other) or self.net.bores(other):
                        continue
                    # LAYER IS THE THIRD DIMENSION OSM ACTUALLY CARRIES, and the bed read none of
                    # it: a way at the deck's own level or above is not underneath it, and a way
                    # in a BORE is under the ground and owes a bridge nothing. Without both, a
                    # motorway interchange asks its own upper ramps to clear each other -- 9.06 m
                    # short at Kaiserberg, where five levels cross in plan (measured 2026-09-06)
                    if int(other["tags"].get("layer", 0) or 0) >= int(w["tags"].get("layer", 1) or 1):
                        continue
                    if not _boxes_touch(self._box(w), self._box(other),
                                        w["tags"]["width"] + other["tags"]["width"]):
                        continue
                    # A CONNECTED ROAD IS STILL A ROAD UNDER THE BRIDGE. Skipping every way that
                    # shares a node with the deck was meant to exclude its own APPROACHES, and it
                    # excluded the thing the clearance exists for: on a spiral, a hairpin or any
                    # interchange the road passing underneath is usually the same road that leads
                    # onto the deck, joined at the abutment. Measured 2026-09-06 on `R11-spiral`,
                    # where the deck crosses the road one turn below and `clearance_m` came back
                    # as the sentinel 1e9 -- no road found at all. What is excluded is the
                    # APPROACH ITSELF: the stations of `other` within APPROACH_M of a shared node
                    shared = [q for q in other["refs"] if q in set(refs)]
                    line = self.centreline(other)
                    p = Point(x, y)
                    # A ROAD IS UNDER A DECK WHERE THE DECK PASSES OVER IT, and nowhere else.
                    # Twenty-five metres beyond both half-widths made every road RUNNING BESIDE a
                    # bridge -- the other carriageway of a dual road, the street along a viaduct --
                    # something the deck had to clear by 4.5 m, which nothing can do because they
                    # are at the same height. Measured 2026-09-06: fifteen of the thirty real
                    # cases refused as infeasible and the shortfall was 4.4 to 4.8 m at eight of
                    # them, which is the clearance itself -- the sign that the pair had no room
                    # at all. The margin left is the half-widths and one metre of surveying slack.
                    reach = other["tags"]["width"] / 2.0 + w["tags"]["width"] / 2.0 + 1.0
                    if shared:
                        st_o = self.stations[other["id"]]
                        s_here = line.project(p)
                        near_join = min(abs(s_here - st_o[other["refs"].index(q)]) for q in shared)
                        if near_join <= APPROACH_M:
                            continue
                    # THE DECK IS OVER THE ROAD WHERE THEIR RIBBONS CROSS, and a node test can
                    # only ever approximate that: too tight and a deck node ten metres from the
                    # crossing misses it (three synthetic cases went red on I4road), too loose and
                    # a road running BESIDE the bridge becomes something to clear (fifteen real
                    # cases refused as infeasible). The crossing is a geometric fact of the two
                    # ribbons, so it is asked of them, once per way pair, and the node test that
                    # follows only picks WHICH nodes carry it.
                    if line.distance(p) <= reach or self._ribbons_cross(w, other):
                        # the road below at its nearest station: its DEM height is the floor's
                        # best estimate before its own profile exists (they are solved together)
                        q = line.interpolate(line.project(p))
                        # the floor is the road BELOW as it stands, not as the DEM stands: over a
                        # bridge's own lifted ramp the DEM was metres too low and the upper deck
                        # cleared nothing (measured on bridge over bridge over road)
                        s_below = line.project(p)
                        st_below = self.stations[other["id"]]
                        kb = int(np.searchsorted(st_below, s_below, side="right") - 1)
                        kb = min(max(kb, 0), len(other["refs"]) - 1)
                        # THE CLEARANCE IS A CONSTRAINT BETWEEN TWO UNKNOWNS, NOT A CONSTANT.
                        # `z_deck - z_below >= 4.5 m` is linear in BOTH, and freezing the lower
                        # side from the previous iterate is what turned one convex problem into
                        # a substitution that is not a contraction: measured 2026-09-06, the
                        # rounds at the Transfagarasan moved the profile 24.4 m, 11.5 m, 9.1 m
                        # and forty of them never converged, walking to 105 m off the DEM. Held
                        # as a PAIR the whole thing is one QP with one solution and no loop.
                        # THE ROAD BELOW AT THE CROSSING, not at its nearest node. `kb` is the
                        # node the station falls into and it can be tens of metres away on a
                        # coarsely noded way, so the deck was made to clear a place it does not
                        # pass over. The profile between two nodes is LINEAR in z, so the exact
                        # station goes into the constraint as a two-node interpolation and the
                        # problem stays convex.
                        # A DECK DOES NOT CLEAR ANOTHER DECK'S RAMP. An approach is a STRUCTURE
                        # and OSM does not say where it stands; treating it as ground makes a
                        # cycle at every compact interchange -- deck A over B's ramp, deck B over
                        # A's ramp, while each deck sits within DECK_GRADE of its own ramp, which
                        # is a contradiction with no solution (measured 2026-09-06: Kaiserberg and
                        # the Elbtunnel were infeasible on the DECK's own grade by 4.59 m and
                        # 5.04 m, and no relaxation of the band or the boundary touched it).
                        # ...but only where that ramp is still CLIMBING. Beyond its rise the
                        # road is back at grade and a bridge over it is an ordinary overpass,
                        # which is what `R11-rampcycle` is: the crossing there stands 160 m from
                        # the abutment. Inside the rise the two constraints contradict.
                        kb_i = self.index[other["refs"][kb]]
                        if ramps[kb_i] and ramp_far[kb_i] < APPROACH_M:
                            continue
                        kb2 = min(kb + 1, len(other["refs"]) - 1)
                        span_b = st_below[kb2] - st_below[kb]
                        uu = 0.0 if span_b <= 1e-9 else min(max((s_below - st_below[kb]) / span_b, 0.0), 1.0)
                        pairs.append((k, self.index[other["refs"][kb]],
                                      self.index[other["refs"][kb2]], uu, clearance_over(other)))
                        # and the deck clears the GROUND under it as well, which is absolute
                        floor2 = self.terrain.dem(q.x, q.y) + CLEARANCE_M
                        floor = floor2 if floor is None else max(floor, floor2)
                if floor is not None:
                    floors[k] = max(floors.get(k, -1e9), floor)
        self.clearance_pairs = pairs
        self._room = None
        return floors

    def _ramp_signs(self):
        """A fill stays a fill and a cutting stays a cutting: the sign of the abutment's lift,
        read from the first solve, holds for the whole ramp in the second. A ramp that crosses
        grade on its way to the deck would be a road that dips to climb, which is not one."""
        signs = {}
        deck_nodes = {r for w in self.net.ways if self.net.spans(w) for r in w["refs"]}
        for w in self.net.ways:
            if self.net.spans(w) or not (set(w["refs"]) & deck_nodes):
                continue
            at = next(self.index[r] for r in w["refs"] if r in deck_nodes)
            signs[w["id"]] = 1.0 if self.z[at] >= self.dem[at] else -1.0
        return signs

    def _solve_with_clearances(self, A, b):
        """The QP with the clearances as constraints: cvxpy/OSQP is the oracle here; the C++ form
        is an active set over these few nodes, proved against this in band_iter's manner."""
        import cvxpy as cp
        n = len(self.index)
        z = cp.Variable(n)
        A = A.tocsc()
        objective = 0.5 * cp.quad_form(z, cp.psd_wrap(A)) - b @ z
        cons = [z[k] >= floor for k, floor in self.constraints.items()]
        cons += [z[k] - ((1.0 - uu) * z[j] + uu * z[j2]) >= gap
                 for (k, j, j2, uu, gap) in getattr(self, "clearance_pairs", ())]
        # I3 AS A CONSTRAINT AND NOT ONLY AS A CHECK. A road that is neither a deck, a bore, a
        # ramp nor a causeway stands ON THE GROUND, within the DEM's own error -- that is what
        # the invariant says, and stating it only afterwards let the QP satisfy a clearance by
        # pushing the road BELOW the bridge into the hill instead of lifting the deck: 17.02 m
        # under the terrain at the Transfagarasan (measured 2026-09-06, after the pairs). The
        # band was taken out once because its active set oscillated in the ITERATIVE scheme;
        # inside one convex solve a box has nothing to oscillate against.
        # A WAY CUT BY THE EXTRACT'S EDGE HAS AN OPEN END. Its ramp runs on for a kilometre
        # in the world and for ten metres in the box, and holding it to the DEM there asks a
        # motorway interchange to bring five levels back to grade inside 500 m -- which no
        # interchange on the planet does. Measured 2026-09-06: it is why Kaiserberg's DECK
        # grade had to give 4.59 m and the Elbtunnel's 5.04 m. The caller marks them; a
        # synthetic bed has none, so nothing there moves.
        onground = ~(self.deck | self.bore | self.approaches() | self.causeway
                     | self.open_ends())
        held = np.flatnonzero(onground)
        if len(held):
            band = self.band()[held].copy()
            # A CROSSING IS EVIDENCE AGAINST THE DEM. Where a deck passes over a road, OSM asserts
            # a grade separation that the DEM shows nothing of -- a motorway in a cutting is 20 m
            # wide and the postings are 25 m apart, so the cutting is smoothed away exactly as a
            # hairpin's two legs are. The way BELOW may therefore go DOWN by what the separation
            # needs, and no further; upward it is still held. Measured 2026-09-06: without this
            # the DECK's own grade had to give 4.59 m at Kaiserberg and 5.04 m at the Elbtunnel,
            # because the only way left to make room was to bend a bridge.
            cons += [z[held] <= self.dem[held] + band,
                     z[held] >= self.dem[held] - self.band_down()[held]]
        cons += self._shape_constraints(z, cp)

        problem = cp.Problem(cp.Minimize(objective), cons)
        problem.solve(solver=cp.OSQP, eps_abs=1e-7, eps_rel=1e-7, max_iter=200000, polish=True)
        if z.value is None:
            # INFEASIBLE IS NOT A DIAGNOSIS. A solver saying `infeasible` names no place and no
            # constraint, and a bed that reports it has found nothing a reader can act on. So the
            # same problem is posed again ELASTICALLY: every clearance pair and every band bound
            # gets a non-negative slack, the slacks are driven to zero as hard as the objective
            # allows, and what is left is the SMALLEST violation that makes the set consistent --
            # with the node it sits on. That is a finding with a location.
            self.infeasible = self._elastic(A, b, cp, z, cons)
            raise RuntimeError("clearance QP: " + str(problem.status) + "; " + self.infeasible)
        self.z = np.asarray(z.value)
        self.infeasible = None

    def _shape_constraints(self, z, cp, slack=None, taper=None, deckg=None, rampg=None,
                           per_way=None):
        """The RAMP and GRADE bounds: a designed structure keeps its class's grade and an
        embankment tapers. Built once and used by BOTH the solve and the elastic pose, so that
        an infeasible set can be told which FAMILY carries the violation -- with `slack` given,
        every bound here is relaxed by that one non-negative variable and its value is the
        answer (measured 2026-09-06: the Transfagarasan's clearance pairs and DEM band are
        satisfiable with zero slack, so the conflict is here)."""
        out = []
        give = 0.0 if slack is None else slack
        # each KIND of bound may carry its own slack, so an infeasible set says WHICH: the
        # embankment's taper, the deck's own grade, or the ramp's grade over the terrain
        g_taper = give if taper is None else taper
        g_deck = give if deckg is None else deckg
        g_ramp = give if rampg is None else rampg

        # with `per_way` given, every bound of a way carries THAT way's own slack, so an
        # infeasible set names the WAY and not only the family it belongs to
        def wslack(w, fallback):
            return fallback if per_way is None else per_way[w["id"]]

        # the ramps: a designed structure keeps its class's grade, hard, on every segment of an
        # approach and of the deck itself -- the lift a clearance asks for spreads back along
        # the approach as an embankment, never as a 30 percent step (measured before this)
        near = self.approaches()
        # an EMBANKMENT TAPERS: the lift over the terrain falls monotonically from the abutment
        # to where the road meets grade again, and never swings below it. Without this the
        # minimum-curvature profile undershoots -- measured: a ramp 0.3 m under a flat plain
        # before climbing 4 m to the deck, which is a spline's overshoot and not a road
        deck_nodes = {r for w in self.net.ways if self.net.spans(w) for r in w["refs"]}
        bore_nodes = {r for w in self.net.ways if self.net.bores(w) for r in w["refs"]}
        for w in self.net.ways:
            if self.net.spans(w) or not (set(w["refs"]) & deck_nodes):
                continue
            refs = w["refs"]
            # AN EMBANKMENT IS WHAT A ROAD RETURNS TO THE GROUND ON, and a way that runs from a
            # deck straight into ANOTHER STRUCTURE never returns to it: a viaduct's next span, a
            # bridge that ends in a tunnel portal, a monorail carried on piers the whole way.
            # Making its lift fall monotonically to zero asks a structure to become an
            # embankment, which is what left the Wuppertal Schwebebahn and Yerba Buena Island
            # infeasible on the TAPER (measured 2026-09-06). Both ends in a structure: no taper.
            # A WAY MAY CARRY MORE THAN ONE ABUTMENT, and each has its own ramp. Taking one
            # end as THE abutment and making the lift fall monotonically across the whole way
            # asks a chain of spans to descend from one bridge and still meet the next: the
            # Wuppertal Schwebebahn is a row of them and Yerba Buena is bridge, tunnel, bridge,
            # and both were infeasible on exactly this (measured 2026-09-06 by switching the two
            # taper rules off one at a time -- the MONOTONE one carries it, the floor does not).
            # So the taper runs OUTWARD FROM EVERY STRUCTURE NODE on the way, in both
            # directions, and stops at APPROACH_M or at the next structure node it meets.
            s_w = self.stations[w["id"]]
            structure = {k for k, r in enumerate(refs) if r in deck_nodes or r in bore_nodes}
            sign = self.ramp_signs.get(w["id"], 1.0)
            done = set()
            for k0 in sorted(structure):
                for step in (+1, -1):
                    k = k0
                    while 0 <= k + step < len(refs):
                        if abs(s_w[k + step] - s_w[k0]) > APPROACH_M:
                            break
                        if (k + step) in structure:
                            break
                        pair = (min(k, k + step), max(k, k + step), step)
                        if pair in done:
                            k += step
                            continue
                        done.add(pair)
                        a_, b_ = self.index[refs[k]], self.index[refs[k + step]]
                        lift_a = z[a_] - self.dem[a_]
                        lift_b = z[b_] - self.dem[b_]
                        # ...and it may go DOWN by what a deck above it asks for. `lift >= 0`
                        # guards against a spline's overshoot; a bridge over a road is physics
                        room = self.clearance_room().get(b_, 0.0)
                        gw = wslack(w, g_taper)
                        out += [sign * lift_b <= sign * lift_a + gw,
                                sign * lift_b >= -gw - room]
                        k += step
        for w in self.net.ways:
            g = LIFT_RATE_OF.get(w["tags"]["highway"], LIFT_RATE_M)
            refs, s = w["refs"], self.stations[w["id"]]
            for k in range(1, len(refs)):
                a, b_ = self.index[refs[k - 1]], self.index[refs[k]]
                ds = s[k] - s[k - 1]
                if ds <= 1e-3 or not (near[a] or near[b_] or self.net.spans(w)):
                    continue
                if self.net.spans(w):
                    # a deck spans what is under it: its grade is its own, bounded by the class
                    # -- or by the chord between its abutments' terrain where the hill is steeper,
                    # because a MAPPED bridge stands at its real grade (13 m of fill and a 410 m
                    # ramp were the bed's answer to an 8 percent deck on a 15 percent hill)
                    s_all = self.stations[w["id"]]
                    chord = abs(self.dem[self.index[refs[-1]]] - self.dem[self.index[refs[0]]]) / max(s_all[-1], 1e-3)
                    # AND A DECK'S GRADE IS ITS OWN ROAD'S. 4 % is the figure for a motorway
                    # bridge, where the expensive part stays near level; a double-decked city
                    # street like Wacker Drive is a viaduct at the street's own grade, and
                    # holding it to 4 % left it infeasible by 0.560 m (measured 2026-09-06)
                    reach = max(DECK_GRADE, GRADE_OF.get(w["tags"]["highway"], DECK_GRADE),
                                chord) * ds
                    gw = wslack(w, g_deck)
                    out += [z[b_] - z[a] <= reach + gw, z[a] - z[b_] <= reach + gw]
                else:
                    # a ramp climbs off the hillside at the class's grade RELATIVE to it, so that
                    # on a hillside as steep as the class the lift still comes back down (measured
                    # before this: nine metres of fill that never returned, 200 m from the bridge)
                    gw = wslack(w, g_ramp)
                    out += [(z[b_] - self.dem[b_]) - (z[a] - self.dem[a]) <= g * ds + gw,
                        (z[a] - self.dem[a]) - (z[b_] - self.dem[b_]) <= g * ds + gw]
        return out

    def _elastic(self, A, b, cp, z, cons):
        """Where the constraint set breaks, and by how much."""
        import numpy as _np
        pairs = list(getattr(self, "clearance_pairs", ()))
        onground = ~(self.deck | self.bore | self.approaches() | self.causeway | self.open_ends())
        held = _np.flatnonzero(onground)
        n = len(self.index)
        y = cp.Variable(n)
        sp_ = cp.Variable(max(len(pairs), 1), nonneg=True)
        sb = cp.Variable(max(len(held), 1), nonneg=True)
        sg = cp.Variable(nonneg=True)
        s_taper = cp.Variable(nonneg=True)
        s_deck = cp.Variable(nonneg=True)
        s_ramp = cp.Variable(nonneg=True)
        soft = [y[k] >= floor - 1e3 for k, floor in self.constraints.items()]
        for i, (k, j, j2, uu, gap) in enumerate(pairs):
            soft.append(y[k] - ((1.0 - uu) * y[j] + uu * y[j2]) >= gap - sp_[i])
        if len(held):
            band = self.band()[held]
            soft += [y[held] <= self.dem[held] + band + sb,
                     y[held] >= self.dem[held] - self.band_down()[held] - sb]
        soft += self._shape_constraints(y, cp, taper=s_taper, deckg=s_deck, rampg=s_ramp)
        # EVERY FAMILY WEIGHED THE SAME, or the diagnosis is about the weights. The shape slack
        # carried a factor of 100 here and the pose therefore broke a clearance PAIR every single
        # time, which is what "clearance short by ..." meant at sixteen real places -- a fact
        # about this objective and not about the road (measured 2026-09-06).
        want = cp.Minimize(1e4 * (cp.sum(sp_) + cp.sum(sb) + s_taper + s_deck + s_ramp)
                           + 0.5 * cp.quad_form(y, cp.psd_wrap(A.tocsc())) - b @ y)
        cp.Problem(want, soft).solve(solver=cp.OSQP, eps_abs=1e-6, eps_rel=1e-6,
                                     max_iter=200000)
        if y.value is None:
            return "elastic pose also failed"
        back = {v: k for k, v in self.index.items()}
        worst_pair, worst_band = 0.0, 0.0
        where = None
        if len(pairs) and sp_.value is not None:
            i = int(_np.argmax(sp_.value[:len(pairs)]))
            worst_pair = float(sp_.value[i])
            if worst_pair > 1e-6:
                where = f"clearance short by {worst_pair:.2f} m at node {back[pairs[i][0]]}"
        if len(held) and sb.value is not None:
            i = int(_np.argmax(sb.value[:len(held)]))
            worst_band = float(sb.value[i])
            if worst_band > worst_pair and worst_band > 1e-6:
                where = f"band exceeded by {worst_band:.2f} m at node {back[held[i]]}"
        # ...and once more with a slack PER WAY, so the answer is a way id and not a family
        ids = [w["id"] for w in self.net.ways]
        pw = {i: cp.Variable(nonneg=True) for i in ids}
        y2 = cp.Variable(len(self.index))
        soft2 = [y2[k] >= floor - 1e3 for k, floor in self.constraints.items()]
        for (k, j, j2, uu, gap) in pairs:
            soft2.append(y2[k] - ((1.0 - uu) * y2[j] + uu * y2[j2]) >= gap)
        if len(held):
            soft2 += [y2[held] <= self.dem[held] + self.band()[held],
                      y2[held] >= self.dem[held] - self.band_down()[held]]
        soft2 += self._shape_constraints(y2, cp, per_way=pw)
        cp.Problem(cp.Minimize(1e4 * cp.sum([pw[i] for i in ids])
                               + 0.5 * cp.quad_form(y2, cp.psd_wrap(A.tocsc())) - b @ y2),
                   soft2).solve(solver=cp.OSQP, eps_abs=1e-6, eps_rel=1e-6, max_iter=200000)
        self.worst_ways = []
        if y2.value is not None:
            top = sorted(((float(pw[i].value or 0.0), i) for i in ids), reverse=True)[:3]
            byid = {w["id"]: w for w in self.net.ways}
            self.worst_ways = [(v, i, byid[i]["tags"].get("highway"),
                                byid[i]["tags"].get("bridge"), byid[i]["tags"].get("tunnel"),
                                round(self.stations[i][-1], 1)) for v, i in top if v > 1e-6]
        named = {"the embankment's TAPER": s_taper, "the DECK's own grade": s_deck,
                 "the RAMP's grade over the terrain": s_ramp}
        got = {k: (float(v.value) if v.value is not None else 0.0) for k, v in named.items()}
        who = max(got, key=got.get)
        if got[who] > max(worst_pair, worst_band, 1e-6) * 0.999:
            named_ways = "; ".join(f"way {i} {h} bridge={br} tunnel={tu} L={ln} m needs {v:.2f}"
                                   for (v, i, h, br, tu, ln) in getattr(self, "worst_ways", ()))
            return (f"{who} carries it [{named_ways or 'no way named'}]: "
                    f"every bound of that kind has to give {got[who]:.3f} m "
                    f"(taper {got[chr(116)+chr(104)+chr(101)+' embankment' + chr(39) + 's TAPER']:.2f}, "
                    f"deck {got[chr(116)+chr(104)+chr(101)+' DECK' + chr(39) + 's own grade']:.2f}, "
                    f"ramp {got[chr(116)+chr(104)+chr(101)+' RAMP' + chr(39) + 's grade over the terrain']:.2f} m)")
        return where or "no single constraint carries the violation"

    def _slopes(self):
        """The Hermite tangents: central differences along a way, and at a junction the plane's."""
        for w in self.net.ways:
            refs, s = w["refs"], self.stations[w["id"]]
            zz = np.array([self.z[self.index[r]] for r in refs])
            m = np.gradient(zz, s) if len(refs) > 1 else np.zeros(1)
            self.slope[w["id"]] = m

    def legs_at(self, nid):
        out = []
        for w in self.net.ways:
            refs = w["refs"]
            for k, r in enumerate(refs):
                if r != nid:
                    continue
                if k + 1 < len(refs):
                    out.append((w, k, +1))
                if k > 0:
                    out.append((w, k, -1))
        return out

    def _clusters(self):
        """netconvert JOINS junction nodes that stand closer than a threshold into ONE junction
        (`--junctions.join`, `NBNodeCluster`, default 10 m): a roundabout, a dual carriageway's
        two halves and a staggered crossing are one place to a driver and one surface to a mesh.
        Without it four ring nodes were four junctions whose regions overlapped and ten edges
        carried three faces (measured). Two nodes join when they are within kJoinM AND a way
        runs directly between them -- distance alone would join two crossings either side of a
        narrow block."""
        nodes = [nid for nid in self.net.nodes if len(self.legs_at(nid)) >= 3]
        # a roundabout's WHOLE ring is the junction, its two-legged nodes included: leaving them
        # out made the ring its own leg four times over and read 8.75 cm of step (measured)
        for w in self.net.ways:
            if w["tags"].get("junction") == "roundabout":
                nodes += [r for r in w["refs"] if r not in nodes]
        parent = {nid: nid for nid in nodes}

        def find(a):
            while parent[a] != a:
                parent[a] = parent[parent[a]]
                a = parent[a]
            return a

        def join(a, b):
            ra, rb = find(a), find(b)
            if ra != rb:
                parent[rb] = ra

        for w in self.net.ways:
            refs = w["refs"]
            s = self.stations[w["id"]]
            # a ROUNDABOUT is one junction however long its ring: netconvert guesses it
            # (--roundabouts.guess) or reads junction=roundabout, and joins the whole ring
            if w["tags"].get("junction") == "roundabout":
                ring = [r for r in refs if r in parent]
                for r in ring[1:]:
                    join(ring[0], r)
            for k in range(len(refs) - 1):
                a, b = refs[k], refs[k + 1]
                if a in parent and b in parent and (s[k + 1] - s[k]) <= JOIN_M:
                    join(a, b)
        groups = {}
        for nid in nodes:
            groups.setdefault(find(nid), []).append(nid)
        return groups

    def _junctions(self):
        """A node with three or more legs is a junction: ONE plane through the node, taken from
        its MAJOR way (highest priority, then the first declared): z = z0 + gx dx + gy dy where
        the major's grade along and its crossfall across give (gx, gy). Every minor leg's
        profile ends on it: its tangent at the node is the plane's derivative along the leg.
        Unreal's spline meshes have no such rule and RAGE's junctions are authored; the rule is
        RAS-K's and AASHTO's: the through road keeps its section, the side road warps to it."""
        self.clusters = self._clusters()
        self.cluster_of = {nid: head for head, members in self.clusters.items() for nid in members}
        for head, members in self.clusters.items():
            legs = []
            inner = set(members)
            for nid in members:
                for (w, k, sgn) in self.legs_at(nid):
                    # a leg that runs to another node of the same cluster is INSIDE it
                    if w["refs"][k + sgn] in inner:
                        continue
                    legs.append((w, k, sgn))
            if not legs:
                continue
            nid = head
            major = max(legs, key=lambda l: (l[0]["tags"]["priority"], -l[0]["id"]))
            w, k, sgn = major
            d = self._direction(w, k, sgn)
            grade = self.slope[w["id"]][k] * sgn
            # the plane: along the major its grade, across it the crown falls both ways -- taken
            # as the major's centreline plane (the crown is the section's, applied by the structure)
            gx, gy = grade * d[0], grade * d[1]
            self.junctions[nid] = {"z0": self.z[self.index[nid]], "gx": gx, "gy": gy,
                                   "major": w["id"], "legs": legs, "members": members}
            for (lw, lk, lsgn) in legs:
                ld = self._direction(lw, lk, lsgn)
                self.slope[lw["id"]][lk] = (gx * ld[0] + gy * ld[1]) * lsgn

    def _direction(self, way, k, sgn):
        """The unit direction at node k of a way, pointing the way `sgn` asks.

        A CLOSED way wraps and an open one does NOT, and Python's negative index conflates the
        two: at k = 0 with sgn = -1 an open way read `refs[-1]`, its own last node, and a
        roundabout's first and last ref are the SAME node, so the vector was zero and the
        normalisation divided by it (measured at the Ponte Vecchio, where a turning circle is a
        closed way). A ring steps round; an open end takes the one segment it has."""
        refs = way["refs"]
        ring = len(refs) > 2 and refs[0] == refs[-1]
        if ring:
            n = len(refs) - 1
            a = self.net.nodes[refs[k % n]]
            b = self.net.nodes[refs[(k + sgn) % n]]
        else:
            j = k + sgn
            if 0 <= j < len(refs):
                a, b = self.net.nodes[refs[k]], self.net.nodes[refs[j]]
            else:                                   # an end: the one segment it has, signed
                j = k - sgn
                j = min(max(j, 0), len(refs) - 1)
                a, b = self.net.nodes[refs[j]], self.net.nodes[refs[k]]
        v = (b[0] - a[0], b[1] - a[1])
        n = math.hypot(*v)
        if n < 1e-12:
            return (1.0, 0.0)
        return (v[0] / n, v[1] / n)

    def profile(self, way, station):
        """Height and grade at a station of a way: cubic Hermite between the nodes."""
        refs, s, m = way["refs"], self.stations[way["id"]], self.slope[way["id"]]
        station = min(max(station, 0.0), s[-1])
        k = int(np.searchsorted(s, station, side="right") - 1)
        k = min(max(k, 0), len(refs) - 2)
        h = s[k + 1] - s[k]
        t = (station - s[k]) / h if h > 0 else 0.0
        z0, z1 = self.z[self.index[refs[k]]], self.z[self.index[refs[k + 1]]]
        m0, m1 = m[k] * h, m[k + 1] * h
        t2, t3 = t * t, t * t * t
        z = (2 * t3 - 3 * t2 + 1) * z0 + (t3 - 2 * t2 + t) * m0 + (-2 * t3 + 3 * t2) * z1 + (t3 - t2) * m1
        g = ((6 * t2 - 6 * t) * z0 + (3 * t2 - 4 * t + 1) * m0 + (-6 * t2 + 6 * t) * z1 + (3 * t2 - 2 * t) * m1) / h if h > 0 else 0.0
        return z, g

    def plan_radius(self, way, station):
        """The horizontal radius at a station, from three consecutive centreline points: the
        circumradius. A straight reads infinity."""
        pts = [self.net.nodes[r] for r in way["refs"]]
        s = self.stations[way["id"]]
        k = int(np.searchsorted(s, station, side="right") - 1)
        k = min(max(k, 1), len(pts) - 2)
        if len(pts) < 3:
            return math.inf
        a, b, c = pts[k - 1], pts[k], pts[k + 1]
        ab = math.dist(a, b)
        bc = math.dist(b, c)
        ca = math.dist(c, a)
        area2 = abs((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]))
        return math.inf if area2 < 1e-9 else (ab * bc * ca) / (2.0 * area2)

    def _superelevations(self):
        """The cross-section along every way, as a road builder sets it. Each HALF has its own
        crossfall, positive when that half falls away from the axis: on a straight both are the
        normal crown; in a curve the outer half rises through zero to the superelevation while
        the inner half deepens, which is the Verwindung (RAL 2012's Anrampung) and the only
        continuous way through an S-curve, where the section turns from one plane to the other.
        Each half's rate is limited to 1:200 of the half width per metre above 70 km/h and
        1:100 below -- without the limit the crossfall jumps at a segment end and the surface
        steps (measured), and with it a rendered curve reads as a road rather than as tape."""
        self.section_of = {}
        for w in self.net.ways:
            s = self.stations[w["id"]]
            half = w["tags"]["width"] / 2.0
            v = DESIGN_SPEED.get(w["tags"]["highway"], 50.0)
            rate = (1.0 / 200.0 if v >= 70.0 else 1.0 / 100.0) / max(half, 0.5)
            left, right = [], []
            for k in range(len(w["refs"])):
                r = self.plan_radius(w, float(s[k]))
                turn = self._turn_sign(w, k)
                need = (v * v / (127.0 * r) - FRICTION_SIDE) if math.isfinite(r) else -1.0
                q = min(SUPER_MAX, max(SUPER_MIN, need))
                if turn > 0:            # the centre is to the LEFT: the surface falls left
                    left.append(q)
                    right.append(-q)
                elif turn < 0:
                    left.append(-q)
                    right.append(q)
                else:
                    left.append(SUPER_MIN)
                    right.append(SUPER_MIN)
            for held in (left, right):
                for k in range(1, len(held)):
                    held[k] = min(max(held[k], held[k - 1] - rate * (s[k] - s[k - 1])),
                                  held[k - 1] + rate * (s[k] - s[k - 1]))
                for k in range(len(held) - 2, -1, -1):
                    held[k] = min(max(held[k], held[k + 1] - rate * (s[k + 1] - s[k])),
                                  held[k + 1] + rate * (s[k + 1] - s[k]))
            self.section_of[w["id"]] = (np.array(left), np.array(right))

    def _turn_sign(self, way, k):
        pts = [self.net.nodes[r] for r in way["refs"]]
        if k == 0 or k >= len(pts) - 1:
            return 0
        a, b, c = pts[k - 1], pts[k], pts[k + 1]
        turn = (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])
        return 1 if turn > 1e-9 else (-1 if turn < -1e-9 else 0)

    def superelevation(self, way, station):
        """The carriageway's crossfall, RAL 2012's rule: on a straight the normal crown, and in a
        curve the superelevation that holds the design speed, q = v^2 / (127 R) - f, bounded by
        q_max and never below the crown. A road builder banks a curve; a mesh that does not is
        why a rendered curve looks like a ribbon of tape."""
        held = self.section_of.get(way["id"])
        if held is None:
            return SUPER_MIN, SUPER_MIN
        left, right = held
        s = self.stations[way["id"]]
        station = min(max(station, 0.0), s[-1])
        k = min(max(int(np.searchsorted(s, station, side="right") - 1), 0), len(left) - 2)
        span = s[k + 1] - s[k]
        u = (station - s[k]) / span if span > 1e-9 else 0.0
        return (left[k] + (left[k + 1] - left[k]) * u, right[k] + (right[k + 1] - right[k]) * u)

    def worst_curvature(self, way, lo, hi):
        """The profile's worst second derivative over a stretch, by differences of the grade."""
        s = np.linspace(lo, hi, max(5, int((hi - lo) / 1.0) + 1))
        g = np.array([self.profile(way, float(v))[1] for v in s])
        d = np.diff(s)
        return float(np.max(np.abs(np.diff(g) / np.where(d > 0, d, 1.0)))) if len(g) > 1 else 0.0

    def centreline(self, way):
        return LineString([self.net.nodes[r] for r in way["refs"]])


# ----------------------------------------------------------------------------- structure

class Structure:
    """The legs' surfaces and the junction polygons, and the checks between them."""

    def __init__(self, map_):
        self.map = map_
        self.net = map_.net
        self.polygons = {}
        self.cuts = {}   # (way id, node id) -> the station where the leg is cut back
        self._junction_polygons()

    def _member_polygon(self, nid, legs):
        """One node's shape. netconvert (NBNodeShapeComputer) walks the legs by bearing and takes
        the corner where one leg's edge line meets the next leg's; taken here in the form that
        holds for a leg passing THROUGH -- every pairwise intersection of the legs' edge LINES,
        hulled. A corner-only walk gave a T-junction a triangle pointing away from its minor
        leg, because two opposite legs are parallel and have no corner between them (seen in the
        LAGEPLAN); the edge lines of the through road bound the surface instead."""
        x0, y0 = self.net.nodes[nid]
        lines = []
        for (w, k, sgn) in legs:
            d = self.map._direction(w, k, sgn)
            n = (-d[1], d[0])
            half = w["tags"]["width"] / 2.0
            for side in (+1, -1):
                p = (x0 + n[0] * half * side, y0 + n[1] * half * side)
                lines.append((p, d, half))
        reach = max(l[2] for l in lines) * 4.0 if lines else 1.0
        pts = []
        for a in range(len(lines)):
            for b in range(a + 1, len(lines)):
                (pa, da, _), (pb, db, _) = lines[a], lines[b]
                cross = da[0] * db[1] - da[1] * db[0]
                if abs(cross) < 1e-9:
                    continue
                rx, ry = pb[0] - pa[0], pb[1] - pa[1]
                tt = (rx * db[1] - ry * db[0]) / cross
                q = (pa[0] + da[0] * tt, pa[1] + da[1] * tt)
                if math.dist(q, (x0, y0)) <= reach:
                    pts.append(q)
        for (p, _, _) in lines:
            pts.append(p)
        if len(pts) < 3:
            return Point(x0, y0).buffer(reach / 4.0, quad_segs=8)
        return unary_union([Point(*p) for p in pts]).convex_hull

    def _junction_polygons(self):
        """A junction's shape is the CONVEX HULL of its members' node shapes -- one node for an
        ordinary crossing, the whole ring for a roundabout -- and every leg is cut back to where
        its ray from its OWN node leaves that hull. A cluster whose members each kept their own
        shape had regions that overlapped, and ten edges carried three faces (measured)."""
        for head, j in self.map.junctions.items():
            members = j["members"]
            by_node = {}
            for (w, k, sgn) in j["legs"]:
                by_node.setdefault(w["refs"][k], []).append((w, k, sgn))
            shapes = [self._member_polygon(nid, by_node.get(nid, self.map.legs_at(nid))) for nid in members]
            rings = [w for w in self.net.ways if w["tags"].get("junction") == "roundabout"
                     and set(w["refs"]) & set(members)]
            if rings:
                # a ROUNDABOUT's drivable surface is an ANNULUS, not a disc: the island in the
                # middle is not carriageway, and a convex hull over the ring made the junction's
                # crown fall 0.375 m to a centre 15 m from the ring (measured 2.8 cm at the legs)
                poly = unary_union(shapes + [self.map.centreline(w).buffer(w["tags"]["width"] / 2.0,
                                                                          quad_segs=16) for w in rings])
            else:
                poly = unary_union(shapes).convex_hull
            self.polygons[head] = poly
            for nid in members:
                for (w, k, sgn) in self.map.legs_at(nid):
                    x0, y0 = self.net.nodes[nid]
                    d = self.map._direction(w, k, sgn)
                    far = 4.0 * max(math.dist((x0, y0), self.net.nodes[m]) for m in members) + 200.0
                    ray = LineString([(x0, y0), (x0 + d[0] * far, y0 + d[1] * far)])
                    # A ROUNDABOUT'S UNION CAN FALL INTO PIECES. Where the ring's members are
                    # mapped as separate ways that do not quite meet, `unary_union` returns a
                    # MULTIPOLYGON and `.exterior` does not exist on one -- the Wuppertal
                    # Schwebebahn's extract died there (measured 2026-09-06). The cut is the
                    # farthest crossing of ANY part's boundary, which is the same answer for a
                    # single polygon and the right one for several.
                    hit = ray.intersection(poly.boundary)
                    cut = 0.0
                    if not hit.is_empty:
                        pts = [hit] if hit.geom_type == "Point" else [p for p in getattr(hit, "geoms", [])]
                        if pts:
                            cut = max(Point(x0, y0).distance(p) for p in pts)
                    key = (w["id"], nid)
                    self.cuts[key] = max(self.cuts.get(key, 0.0), cut)

    def own_surface(self, way, station, offset):
        """A leg's own surface: the profile, plus the cross-section. On a straight that is the
        normal CROWN, falling both ways from the axis; in a curve it is the SUPERELEVATION, one
        plane tilted toward the curve's centre (RAL 2012). The axis keeps the profile either way."""
        z, _ = self.map.profile(way, station)
        left, right = self.map.superelevation(way, station)
        fall = left if offset < 0.0 else right
        return z - fall * abs(offset)

    @staticmethod
    def tangent_at(line, s):
        """The unit tangent of a centreline at arc length s. ONE estimator, because two of them
        are two different normals: `frame_point` differenced over 0.05 m and `major_surface` over
        0.1 m, and on a tight curve that is a small offset error which the crossfall turns into a
        step -- 0.075 m at the Magic Roundabout, on the MAJOR way, where the two must agree by
        definition (measured 2026-09-06)."""
        s = min(max(float(s), 0.0), line.length)
        ahead = line.interpolate(min(s + TANGENT_H, line.length))
        back = line.interpolate(max(s - TANGENT_H, 0.0))
        d = (ahead.x - back.x, ahead.y - back.y)
        n = math.hypot(*d) or 1.0
        return (d[0] / n, d[1] / n)

    def frame_point(self, way, station, offset):
        """The point at (station, offset) in the WAY'S OWN FRAME -- on its arc, with the normal
        taken AT that station.

        A straight tangent struck from the junction node is not the same place: on a curved way
        the point it reaches projects back to a DIFFERENT station, and `major_surface` reads the
        section there. The gap is the grade times that station error and it grows with the
        curvature -- 0.18 m on the Golden Gate's approach after the frame's sign was repaired
        (measured). Building the point ON the arc makes the projection recover the station it
        was built from, so the leg and the junction read one surface by construction."""
        line = self.map.centreline(way)
        s = min(max(float(station), 0.0), line.length)
        q = line.interpolate(s)
        d = self.tangent_at(line, s)
        return (q.x - d[1] * offset, q.y + d[0] * offset)

    def major_surface(self, nid, x, y):
        """The junction's surface IS the major way's surface, crown and all, extended over the
        polygon: RAS-K's and AASHTO's rule that the through road keeps its section."""
        j = self.map.junctions[nid]
        way = next(w for w in self.net.ways if w["id"] == j["major"])
        line = self.map.centreline(way)
        p = Point(x, y)
        # THE PROJECTION IS LOCAL. `project` returns the GLOBAL nearest station, and a road that
        # doubles back on itself -- a hairpin, a roundabout's ring, a switchback -- has another
        # branch within metres: the point beside the junction then lands on the WRONG leg and the
        # section is read there. Measured 2026-09-06: 0.101 m at Lombard Street (eight hairpins
        # in 180 m) and 0.075 m at the Magic Roundabout, both on the MAJOR way, where the leg and
        # the junction are the same surface by definition. The window is the junction's own
        # station plus what its cut and its width can reach.
        here = self.map.stations[way["id"]][way["refs"].index(nid)] \
            if nid in way["refs"] else line.project(p)
        window = self.cuts.get((way["id"], nid), 0.0) + way["tags"]["width"] + 2.0 * TANGENT_H
        lo = max(0.0, here - window)
        hi = min(line.length, here + window)
        piece = substring(line, lo, hi) if hi - lo > 1e-6 else None
        s = (lo + piece.project(p)) if piece is not None and piece.length > 1e-9 else here
        q = line.interpolate(s)
        # the offset is SIGNED, because a superelevated section is one tilted plane and not a
        # symmetric crown: the sign comes from the cross product with the way's direction there
        d = self.tangent_at(line, s)
        off = -(x - q.x) * d[1] + (y - q.y) * d[0]
        return self.own_surface(way, s, off)

    def leg_surface(self, way, station, offset, at=None, sgn=+1, k=None):
        """A leg's surface within WARP_M of a junction it is a MINOR leg of is warped from its
        own section into the major's surface at the same (x, y): alpha rises smoothly from 0 at
        the warp's start to 1 at the cut, so at the cut the two are one surface (I6 by
        construction) and the leg's centreline stays C1 through the warp."""
        z_own = self.own_surface(way, station, offset)
        head = self.map.cluster_of.get(at) if at is not None else None
        if head is None or self.map.junctions[head]["major"] == way["id"]:
            return z_own
        cut = self.cuts[(way["id"], at)]
        # WHICH VISIT, not which node. A way may pass through the same junction TWICE -- a
        # footway looping through a station hall, a service road round a block -- and
        # `refs.index()` returns the FIRST of them, so the station was another visit's, the warp
        # parameter was not zero at the cut and the leg blended where it should have matched.
        # Measured 2026-09-06 at Zuerich HB: 0.085 m on a footway leg of a six-legged junction,
        # the last I6 in the whole ladder. The caller knows which visit it is walking.
        if k is None:
            k = way["refs"].index(at)
        s_node = self.map.stations[way["id"]][k]
        along = sgn * (station - s_node)          # distance from the node along the leg
        u = (along - cut) / WARP_M                # 0 at the cut, 1 at the warp's start
        if u >= 1.0:
            return z_own
        alpha = 1.0 - (3 * u * u - 2 * u * u * u) if u > 0.0 else 1.0
        px, py = self.frame_point(way, station, offset)
        return (1.0 - alpha) * z_own + alpha * self.major_surface(head, px, py)

    def check_junction_steps(self):
        """I6: along each leg's cut line, the leg's surface against the junction's, worst gap."""
        worst = 0.0
        for nid, j in self.map.junctions.items():
            for (w, k, sgn) in j["legs"]:
                # THE MAJOR IS THE JUNCTION'S SURFACE, so comparing it with itself measures the
                # PROJECTION and not the road: both sides are `own_surface` of the same way at
                # the same station, and any difference is how well a nearest-point search
                # recovered a station it was handed. On a road that doubles back that difference
                # reached 0.11 m at the Magic Roundabout and 0.10 m at Lombard Street while the
                # surface itself was continuous by construction. What I6 is FOR is the step where
                # a MINOR leg ties in, which is what the warp exists to close.
                if w["id"] == j["major"]:
                    continue
                at = w["refs"][k]
                cut = self.cuts[(w["id"], at)]
                s_node = self.map.stations[w["id"]][k]
                station = s_node + sgn * cut
                half = w["tags"]["width"] / 2.0
                for t in np.linspace(-half, half, 9):
                    px, py = self.frame_point(w, station, t)
                    leg = self.leg_surface(w, station, t, at, sgn, k)
                    junction = self.major_surface(nid, px, py)
                    worst = max(worst, abs(leg - junction))
        return worst


# ----------------------------------------------------------------------------- the mesh

def w_is_major(way, junction):
    return way["id"] == junction["major"]


class Mesh:
    """The drawn surface: a quad strip per leg between its junction cuts, and a fan per junction
    polygon, all welded on one vertex table. Cesium's quantized-mesh and Unreal's Landscape both
    weld on a QUANTISED key rather than on a distance, because a distance test is not transitive
    and two vertices a hair apart can then weld to a third and not to each other; the key here
    is the position rounded to the weld tolerance."""

    def __init__(self, structure, step_m=2.0):
        self.st = structure
        self.map = structure.map
        self.net = structure.net
        self.step = step_m
        self.vertices = []
        self.byKey = {}
        self.tris = []
        self.faces_of = {}
        self._legs()
        self._junctions()

    def vertex(self, x, y, z):
        key = (round(x / WELD_M), round(y / WELD_M), round(z / WELD_M))
        at = self.byKey.get(key)
        if at is None:
            at = len(self.vertices)
            self.byKey[key] = at
            self.vertices.append((x, y, z))
        return at

    def tri(self, a, b, c):
        if a == b or b == c or a == c:
            return
        self.tris.append((a, b, c))
        for e in ((a, b), (b, c), (c, a)):
            self.faces_of[tuple(sorted(e))] = self.faces_of.get(tuple(sorted(e)), 0) + 1

    def region_of(self, nid):
        """The junction's REGION: its node polygon plus the warp band of every minor leg, because
        inside the warp the leg's section is not its own and a ribbon drawn there would chord the
        surface it is warping into (measured: 3.1 cm at a T)."""
        j = self.map.junctions[nid]
        poly = self.st.polygons[nid]
        pieces = [poly]
        for (w, k, sgn) in j["legs"]:
            if w["id"] == j["major"]:
                continue
            at = w["refs"][k]
            cut = self.st.cuts[(w["id"], at)]
            d = self.map._direction(w, k, sgn)
            x0, y0 = self.net.nodes[at]
            half = w["tags"]["width"] / 2.0
            nx, ny = -d[1], d[0]
            a0 = (x0 + d[0] * cut, y0 + d[1] * cut)
            a1 = (x0 + d[0] * (cut + WARP_M), y0 + d[1] * (cut + WARP_M))
            pieces.append(Polygon([(a0[0] + nx * half, a0[1] + ny * half), (a1[0] + nx * half, a1[1] + ny * half),
                                   (a1[0] - nx * half, a1[1] - ny * half), (a0[0] - nx * half, a0[1] - ny * half)]))
        return unary_union(pieces)

    def drawn_spans(self, way):
        if way["tags"].get("junction") == "roundabout":
            return []
        """The stretches of a way drawn as a RIBBON: its whole length minus, around every
        junction node it touches, the stretch the junction's region covers -- the node polygon's
        cut, and a minor leg's warp band on top of it. A way that passes THROUGH a junction is
        split into two ribbons; drawing it whole put two surfaces in one place (measured)."""
        s = self.map.stations[way["id"]]
        holes = []
        for k, r in enumerate(way["refs"]):
            cut = self.st.cuts.get((way["id"], r))
            if cut is None:
                continue
            head = self.map.cluster_of.get(r)
            if head is None:
                continue
            band = 0.0 if w_is_major(way, self.map.junctions[head]) else WARP_M
            holes.append((s[k] - cut - band, s[k] + cut + band))
        spans = []
        at = 0.0
        for a, b in sorted(holes):
            if a > at:
                spans.append((at, min(a, s[-1])))
            at = max(at, b)
        if at < s[-1]:
            spans.append((at, s[-1]))
        return [(lo, hi) for lo, hi in spans if hi - lo > 1e-6]

    def _cuts_of(self, way):
        spans = self.drawn_spans(way)
        return (spans[0][0], spans[-1][1]) if spans else (0.0, 0.0)

    def frame_at(self, way, station, side=None):
        """The cross-section's frame at a station: the point, and the offset direction. Within a
        segment it is that segment's normal; AT a vertex it is the MITRE -- the bisector of the
        two segments' normals, lengthened by 1/cos(theta/2) so the offset edge stays parallel to
        both. A local finite difference instead of the mitre put the offset point up to 0.7 m
        along the road at a 24 degree vertex and read 2.2 cm off the surface (measured); every
        road mesher offsets this way, CARLA's MeshFactory included."""
        s = self.map.stations[way["id"]]
        pts = [self.net.nodes[r] for r in way["refs"]]
        station = min(max(station, 0.0), s[-1])
        k = int(np.searchsorted(s, station, side="right") - 1)
        k = min(max(k, 0), len(pts) - 2)
        seg = s[k + 1] - s[k]
        u = (station - s[k]) / seg if seg > 0 else 0.0
        p = (pts[k][0] + (pts[k + 1][0] - pts[k][0]) * u, pts[k][1] + (pts[k + 1][1] - pts[k][1]) * u)

        def normal(i):
            a = pts[i]
            b = pts[i + 1]
            d = (b[0] - a[0], b[1] - a[1])
            n = math.hypot(*d) or 1.0
            return (-d[1] / n, d[0] / n)

        at_vertex = abs(station - s[k]) < 1e-9 and 0 < k < len(pts) - 1
        at_next = abs(station - s[k + 1]) < 1e-9 and 0 < k + 1 < len(pts) - 1
        if at_vertex or at_next:
            i = k if at_vertex else k + 1
            if side == "before":
                return p, normal(i - 1)
            if side == "after":
                return p, normal(i)
            na, nb = normal(i - 1), normal(i)
            mx, my = na[0] + nb[0], na[1] + nb[1]
            n = math.hypot(mx, my)
            if n > 1e-9:
                cos_half = n / 2.0
                return p, (mx / n / cos_half, my / n / cos_half)
        return p, normal(k)

    def _leg_point(self, way, station, offset, side=None):
        p, (nx, ny) = self.frame_at(way, station, side)
        x, y = p[0] + nx * offset, p[1] + ny * offset
        # inside a junction's warp the surface is the warped one, so the ribbon and the junction
        # are one surface at the cut
        nid, sgn = None, +1
        for k, r in enumerate(way["refs"]):
            if r in self.map.cluster_of:
                s_node = self.map.stations[way["id"]][k]
                if abs(station - s_node) < WARP_M * 2:
                    nid = r
                    sgn = +1 if station > s_node else -1
        z = self.st.leg_surface(way, station, offset, nid, sgn)
        return x, y, z

    @staticmethod
    def section(way):
        """The cross-section's BREAK OFFSETS: a vertex stands wherever the surface's slope
        changes -- the crown at the centreline and the two edges -- and nowhere else. A strip of
        two vertices chords the crown away and drew 8.75 cm off the analytic surface (measured);
        CARLA samples a lane across `vertex_width_resolution` for the same reason, and every lane
        boundary joins this list when lanes arrive."""
        half = way["tags"]["width"] / 2.0
        return [-half, 0.0, +half]

    @staticmethod
    def crossfall_of(way):
        return CROSSFALL

    def _legs(self):
        for w in self.net.ways:
            for lo, hi in self.drawn_spans(w):
                self._ribbon(w, lo, hi)

    def _ribbon(self, w, lo, hi):
        if True:
            offsets = self.section(w)
            # the station step from the CHORD's sagitta, per SEGMENT: a chord of length L across
            # a profile of curvature k stands k L^2 / 8 below it, so L = sqrt(8 tol / k). The
            # curvature is the segment's own -- one number for the whole way took the step from
            # the flattest stretch and read 4 cm on a 30 m curve where the profile bends
            # (measured). CARLA's fixed 0.5 m is the same idea with the curvature assumed.
            edges = sorted({lo, hi} | {float(v) for v in self.map.stations[w["id"]] if lo < v < hi})
            stations = set(edges)
            for a_, b_ in zip(edges, edges[1:]):
                kappa = self.map.worst_curvature(w, a_, b_)
                step = self.step if kappa <= 0 else min(self.step, math.sqrt(8.0 * MESH_TOL_M / kappa))
                # and the section's ROTATION: over a quad of length L the crossfall turns by
                # dq/ds * L, and a triangulated bilinear patch stands half x that / 4 off it
                l0, r0 = self.map.superelevation(w, a_)
                l1, r1 = self.map.superelevation(w, b_)
                rate = max(abs(l1 - l0), abs(r1 - r0)) / max(b_ - a_, 1e-6)
                half = w["tags"]["width"] / 2.0
                if rate > 1e-9:
                    step = min(step, 4.0 * MESH_TOL_M / (half * rate))
                steps = max(1, int(math.ceil((b_ - a_) / max(step, 0.25))))
                stations |= {a_ + (b_ - a_) * k / steps for k in range(steps + 1)}
            # at an interior VERTEX the ribbon carries TWO rows, one per adjoining segment, and
            # the wedge between them closes the corner: a single mitred row makes a trapezoid
            # that spans two directions, and no subdivision shrinks its twist -- it read 4 cm on
            # a 30 m curve at 12 percent grade, which is grade x offset x tan(theta/2) exactly.
            # Every quad then lies inside ONE segment, where (station, offset) -> (x, y) is
            # affine and the linear surface is the analytic one to the bit.
            rows = []
            vertices_at = {float(v) for v in self.map.stations[w["id"]] if lo < v < hi}
            for s in sorted(stations):
                if s in vertices_at:
                    rows.append((s, "before"))
                    rows.append((s, "after"))
                else:
                    rows.append((s, None))
            prev = None
            for s, side in rows:
                row = [self.vertex(*self._leg_point(w, s, off, side)) for off in offsets]
                if prev is not None:
                    for a, b in zip(range(len(row) - 1), range(1, len(row))):
                        self.tri(prev[a], prev[b], row[b])
                        self.tri(prev[a], row[b], row[a])
                prev = row

    def junction_surface(self, nid, x, y):
        """The junction region's surface: the major's, and inside a minor's warp band the same
        blend the leg carries, so the region and the ribbons are one surface at the band's end."""
        j = self.map.junctions[nid]
        for (w, k, sgn) in j["legs"]:
            if w_is_major(w, j):
                continue
            d = self.map._direction(w, k, sgn)
            at = w["refs"][k]
            x0, y0 = self.net.nodes[at]
            along = (x - x0) * d[0] + (y - y0) * d[1]
            off = -(x - x0) * d[1] + (y - y0) * d[0]
            cut = self.st.cuts[(w["id"], at)]
            if along < cut - 1e-9 or along > cut + WARP_M + 1e-9 or abs(off) > w["tags"]["width"] / 2.0 + 1e-9:
                continue
            s_node = self.map.stations[w["id"]][k]
            return self.st.leg_surface(w, s_node + sgn * along, off, at, sgn)
        return self.st.major_surface(nid, x, y)

    def _junctions(self):
        """The region is meshed on a GRID, not fanned: a fan's triangle spans the crown line and
        chords it (measured: 3.1 cm at a T). The cell follows from the tolerance and the sharpest
        break the surface has, the crown: a cell of side h across a crown of slope c stands
        c h / 4 off it at worst, so h = 4 tol / c = 1.6 m at 2.5 percent -- taken at half that."""
        from scipy.spatial import Delaunay
        cell = 2.0 * MESH_TOL_M / CROSSFALL
        for nid in self.st.polygons:
            region = self.region_of(nid)
            if region.is_empty or region.area <= 0:
                continue
            # a region can come back as several polygons where a leg's band does not touch the
            # node's own polygon (a wide junction with a narrow minor leg); each is meshed
            for part in (region.geoms if region.geom_type == "MultiPolygon" else [region]):
                self._mesh_region(nid, part, cell)

    def _mesh_region(self, nid, region, cell):
        from scipy.spatial import Delaunay
        pts = []
        for ring in [region.exterior] + list(region.interiors):
            dense = ring.segmentize(cell) if hasattr(ring, "segmentize") else ring
            pts += list(dense.coords)[:-1]
        minx, miny, maxx, maxy = region.bounds
        for x in np.arange(minx + cell / 2, maxx, cell):
            for y in np.arange(miny + cell / 2, maxy, cell):
                if region.contains(Point(x, y)):
                    pts.append((float(x), float(y)))
        pts = np.array(pts)
        if len(pts) < 3:
            return
        tri = Delaunay(pts)
        for simplex in tri.simplices:
            a, b, c = pts[simplex]
            cx, cy = (a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3
            if not region.contains(Point(cx, cy)):
                continue
            ids = [self.vertex(px, py, self.junction_surface(nid, px, py)) for px, py in (a, b, c)]
            self.tri(*ids)

    def open_edges(self):
        return sum(1 for _, n in self.faces_of.items() if n == 1)

    def bad_edges(self):
        return sum(1 for _, n in self.faces_of.items() if n > 2)

    def nearest_pair_m(self):
        """The closest two DISTINCT vertices: below the weld tolerance means the weld missed."""
        pts = np.array(self.vertices)
        if len(pts) < 2:
            return 1e9
        from scipy.spatial import cKDTree
        d, _ = cKDTree(pts).query(pts, k=2)
        return float(np.min(d[:, 1]))

    def surface_at(self, x, y, near=None):
        """The drawn surface's height at (x, y) by barycentric interpolation in the triangle that
        contains it -- what a wheel would touch. A road is NOT a height field: under a bridge two
        surfaces stand over one point, so the caller says which level it means and the nearest
        answer is returned (Cesium's sampleHeight has the same rule for a 3D tileset)."""
        best = None
        for (ia, ib, ic) in self.tris:
            a, b, c = self.vertices[ia], self.vertices[ib], self.vertices[ic]
            d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
            if abs(d) < 1e-12:
                continue
            u = ((b[1] - c[1]) * (x - c[0]) + (c[0] - b[0]) * (y - c[1])) / d
            v = ((c[1] - a[1]) * (x - c[0]) + (a[0] - c[0]) * (y - c[1])) / d
            w = 1.0 - u - v
            if u >= -1e-9 and v >= -1e-9 and w >= -1e-9:
                z = u * a[2] + v * b[2] + w * c[2]
                if near is None:
                    return z
                if best is None or abs(z - near) < abs(best - near):
                    best = z
        return best


def check_mesh(map_, st):
    """I7 (welded and closed where it should be) and I9 (the drawn surface against the analytic
    driving surface, sampled along every lane)."""
    mesh = Mesh(st)
    worst = 0.0
    samples = 0
    where = None
    for w in map_.net.ways:
        half = w["tags"]["width"] / 2.0
        for lo, hi in mesh.drawn_spans(w):
          for s in np.arange(lo + 0.5, max(lo + 0.6, hi - 0.5), 3.0):
            for off in (-half * 0.5, 0.0, half * 0.5):
                x, y, z = mesh._leg_point(w, float(s), off)
                drawn = mesh.surface_at(x, y, near=z)
                if drawn is None:
                    continue
                samples += 1
                if abs(drawn - z) > worst:
                    worst = abs(drawn - z)
                    where = (w["id"], float(s), off)
    return {"vertices": len(mesh.vertices), "triangles": len(mesh.tris),
            "edges_over_two_faces": mesh.bad_edges(), "nearest_pair_m": mesh.nearest_pair_m(),
            "drawn_vs_analytic_m": worst, "samples": samples, "worst_at": where}


# ------------------------------------------------------------------- the structure's own type

PIER_SPAN_M = 45.0        # [SET] the economical span of a prestressed concrete beam, 25-60 m


def free_span_of(xs, deck_z, ground_z, water=None, deep_m=25.0):
    """The width a bridge must span WITHOUT a pier. A pier stands on ground it can be founded
    on; it cannot stand in a shipping channel, and founding one grows expensive as the valley
    deepens. So the obstacle is the widest run where the deck stands more than `deep_m` above
    the ground, or over water -- and THAT, not the deck's total length, decides the type. A
    900 m deck over a 240 m gorge is twenty beam spans with piers on the slopes; the same deck
    over a 900 m strait is a suspension bridge, and the difference is the obstacle."""
    hard = np.zeros(len(xs), dtype=bool)
    for k, x in enumerate(xs):
        if water is not None and ground_z[k] <= water + 0.01:
            hard[k] = True
        elif deck_z[k] - ground_z[k] > deep_m:
            hard[k] = True
    best, run, start = 0.0, 0.0, None
    edges = []
    for k in range(len(xs)):
        if hard[k]:
            if start is None:
                start = xs[k]
            run = xs[k] - start
            if run > best:
                best = run
                edges = [start, xs[k]]
        else:
            start = None
            run = 0.0
    return float(best), edges


# WHAT THE SURVEYOR ALREADY WROTE DOWN. `bridge:structure` is OSM's own word for the carrier and
# `material` for what it is built of, and a heuristic that ignores them is a heuristic answering a
# question somebody already answered: the Ponte Vecchio came out a prestressed concrete beam and
# it is a segmental stone arch from 1345 (measured, first real case). A PROVIDER beats a
# GENERATOR wherever the data has one right answer -- this tree's own rule, applied here.
BRIDGE_STRUCTURE = {
    "arch": ("Bogenbruecke", "arch"), "aqueduct": ("Bogenbruecke", "arch"),
    "humpback": ("Bogenbruecke", "arch"), "suspension": ("Haengebruecke", "suspension"),
    "simple-suspension": ("Haengebruecke", "suspension"),
    "cable-stayed": ("Schraegseilbruecke", "stays"), "truss": ("Stahlfachwerk", "truss"),
    "cantilever": ("Auslegerbruecke", "truss"), "beam": ("Balkenbruecke", "piers"),
    "girder": ("Balkenbruecke", "piers"), "box-girder": ("Hohlkasten", "piers"),
    "viaduct": ("Viadukt", "piers"), "trestle": ("Viadukt", "piers"),
    "floating": ("Pontonbruecke", "none"), "low-water-crossing": ("Furt", "none"),
    "boardwalk": ("Stegbruecke", "piers"), "covered": ("Bogenbruecke", "arch"),
}
BRIDGE_MATERIAL = {
    "stone": "Naturstein", "masonry": "Mauerwerk", "brick": "Ziegel", "steel": "Stahl",
    "concrete": "Beton", "reinforced_concrete": "Stahlbeton",
    "prestressed_concrete": "Spannbeton", "wood": "Holz", "timber": "Holz",
    "iron": "Gusseisen", "cast_iron": "Gusseisen", "wrought_iron": "Schweisseisen",
    "metal": "Stahl",
}
# slenderness by CARRIER, span over depth: what each type can span at what depth. A masonry arch
# is thick at the crown where a steel one is thin, which is why the ring reads at all.
CARRIER_SLENDER = {"arch": 55.0, "masonry-arch": 26.0, "truss": 12.0, "stays": 120.0,
                   "suspension": 160.0, "piers": 22.0, "none": 15.0}


def bridge_told(tags, total_m, free_m, rail=False, rise_m=None):
    """The type the DATA states, or the one its DATE forces, or None.

    `bridge:structure` is the survey and wins outright. Where it is absent the date still speaks:
    wrought iron reached bridges about 1850 and reinforced concrete about 1900, so a bridge that
    is `historic` or carries a `start_date` before 1890 is a MASONRY ARCH -- there was nothing
    else to build it from. The Ponte Vecchio carries `historic=bridge` and no structure tag at
    all, and without this rule it came out a prestressed concrete beam (measured).

    And one thing the data cannot overrule: an arch needs RISE. A masonry arch springs from its
    abutments and its crown stands span/6 to span/2 above them; a deck that clears its obstacle
    by less than span/12 has no room for one, whatever the date says."""
    if not tags:
        return None
    old = False
    told = str(tags.get("bridge:structure") or tags.get("bridge_structure") or "").strip().lower()
    if told not in BRIDGE_STRUCTURE:
        year = _year_of(tags)
        historic = bool(tags.get("historic") or tags.get("heritage")
                        or tags.get("heritage:operator"))
        if not (historic or (year and year < 1890)):
            return None
        if rise_m is not None and rise_m < 1.2:
            return None                       # no room for any ring; let the span decide
        told = "arch"
        old = True                            # the DATE chose it, so the material follows
    name, carrier = BRIDGE_STRUCTURE[told]
    raw = str(tags.get("material") or tags.get("bridge:material") or "").strip().lower()
    material = BRIDGE_MATERIAL.get(raw, "")
    masonry = material in ("Naturstein", "Mauerwerk", "Ziegel")
    if not material:
        year = _year_of(tags)
        # a bridge the DATE made an arch is a bridge built before there was anything but stone;
        # saying so is the whole point of reading the date, and defaulting it to reinforced
        # concrete put a 102 m concrete arch where three stone rings of 1345 stand (measured)
        material = ("Naturstein" if old or (carrier == "arch" and year and year < 1850) else
                    "Stahl" if carrier in ("truss", "suspension", "stays") else "Stahlbeton")
        masonry = material == "Naturstein"
    free = free_m if free_m and free_m > 1.0 else total_m
    if carrier == "arch" and masonry:
        # A FREE SPAN IS A MODERN CONSTRAINT AND A MASONRY BRIDGE IGNORES IT: an arcade puts its
        # piers IN the river, which is what every old crossing does. What bounds a stone arch is
        # its RISE -- a ring flatter than span/12 is at the limit of what was ever built -- and
        # the material itself, which has never spanned much past 40 m outside a handful of
        # records. So the field count follows from those two and not from the water's width:
        # the Ponte Vecchio came out as ONE 102 m stone arch before this (measured).
        limit = min(40.0, max(6.0, 12.0 * (rise_m if rise_m and rise_m > 0.5 else 4.0)))
        fields = max(1, int(math.ceil(total_m / limit)))
    elif carrier in ("arch", "piers"):
        fields = max(1, int(round(total_m / max(free, 1.0))))
    else:
        fields = 1
    span = total_m / fields
    key = "masonry-arch" if (carrier == "arch" and masonry) else carrier
    depth = max(0.3, span / CARRIER_SLENDER[key])
    if carrier == "arch" and masonry:
        name = "Steinbogenbruecke" if material == "Naturstein" else "Gewoelbebruecke"
    return fields, span, name, depth, material, carrier


def _year_of(tags):
    raw = str(tags.get("start_date") or tags.get("year_of_construction") or "").strip()
    digits = "".join(c for c in raw[:4] if c.isdigit())
    return int(digits) if len(digits) == 4 else None


def bridge_type(total_m, rail=False, free_m=None, tags=None, rise_m=None):
    """WHAT a bridge IS -- CASES.md's table, and the numbers are the standard span ranges
    (Leonhardt, *Bruecken*; the Eurocode's ranges; FHWA's inventory by type). A rail bridge is
    one class heavier because its loads are two to three times a road's. The FREE SPAN decides,
    not the length: what a viewer reads is the depth under the deck, the rhythm of the piers,
    and whether an arch, a pylon or a main cable stands there at all.

    Returns (fields, span, name, deck depth, material, carrier)."""
    told = bridge_told(tags, total_m, free_m, rail, rise_m)
    if told is not None:
        return told
    factor = 0.65 if rail else 1.0
    free = total_m if free_m is None else max(free_m, 0.0)
    if total_m <= 8.0:
        return 1, total_m, "Durchlass", max(0.3, total_m / 15.0), "Beton", "none"
    if free <= 25.0 and total_m <= 25.0:
        return 1, total_m, "Plattenbruecke", total_m / 20.0, "Stahlbeton", "abutments"
    if free <= 60.0 * factor:
        fields = max(1, int(round(total_m / (PIER_SPAN_M * factor))))
        span = total_m / fields
        name, mat = ("Plattenbruecke", "Stahlbeton") if span <= 25.0 else ("Spannbetonbalken", "Spannbeton")
        return fields, span, name, span / (20.0 if span <= 25.0 else 22.0), mat, "piers"
    if free <= 120.0:
        if rail:
            return 1, free, "Stahlfachwerk", free / 12.0, "Stahl", "truss"
        return 1, free, "Hohlkasten", free / 18.0, "Spannbeton", "piers"
    if free <= 250.0:
        return 1, free, "Bogenbruecke", free / 55.0, "Stahl" if free > 150 else "Stahlbeton", "arch"
    if free <= 600.0:
        # a stayed deck is slender because the stays carry it: Normandie 856 m, deck 3.0 m
        return 1, free, "Schraegseilbruecke", free / 120.0, "Stahl-Verbund", "stays"
    # a suspension deck is slenderer still: Golden Gate 1280 m span, 7.6 m truss = L/168
    return 1, free, "Haengebruecke", free / 160.0, "Stahl", "suspension"


def draw_bridge_elevation(ax, xs, deck_z, ground_z, ink, rail=False, water=None, tags=None):
    """The bridge as a STRUCTURE in the longitudinal section, drawn the way its type stands.
    A slab on piers, an arch with its spandrel columns, a fan of stays from a pylon, or a main
    cable with its hangers -- these are what a viewer names a bridge by, and a generator that
    draws every span as a beam builds a world where every crossing looks the same."""
    from matplotlib.patches import Rectangle
    total = float(xs.max() - xs.min())
    free, edges = free_span_of(xs, deck_z, ground_z, water)
    rise = float(np.nanmax(np.asarray(deck_z) - np.asarray(ground_z))) if len(deck_z) else None
    fields, span, name, depth, material, carrier = bridge_type(total, rail=rail, free_m=free,
                                                              tags=tags, rise_m=rise)
    x0, x1 = float(xs.min()), float(xs.max())
    # where a pier may NOT stand: the obstacle's own width
    forbid = (edges[0], edges[1]) if edges else (x1, x0)
    ax.plot(xs, deck_z, color=ink, lw=2.4)
    ax.plot(xs, deck_z - depth, color=ink, lw=1.0)
    floor = float(np.min(ground_z))

    def at(x):
        return float(np.interp(x, xs, deck_z))

    def under(x):
        return float(np.interp(x, xs, ground_z))

    if carrier in ("piers", "abutments", "truss"):
        for k in range(1, fields):
            x = x0 + total * k / fields
            if forbid[0] < x < forbid[1]:
                continue                        # no pier in the channel
            ax.plot([x, x], [at(x) - depth, under(x)], color=ink, lw=1.6)
            ax.plot([x - depth * 0.35, x + depth * 0.35], [under(x), under(x)], color=ink, lw=1.2)
        if carrier == "truss":
            # a steel truss: the diagonals a rail bridge shows
            for k in range(fields):
                a, b = x0 + total * k / fields, x0 + total * (k + 1) / fields
                for u in np.linspace(0, 1, 9)[:-1]:
                    p = a + (b - a) * u
                    q = a + (b - a) * (u + 1 / 8)
                    hi, lo = at(p), at(q) - depth
                    ax.plot([p, q], [hi, lo], color=ink, lw=0.7)
                    ax.plot([p, q], [at(p) - depth, at(q)], color=ink, lw=0.7)
    elif carrier == "arch":
        # AN ARCADE IS `fields` ARCHES AND NOT ONE. Every masonry crossing on the planet is a row
        # of rings on piers -- the Ponte Vecchio has three, the Goeltzschtal 98 on four storeys --
        # and drawing the whole length as a single span makes every old bridge look like a modern
        # one, which is the failure this whole typology exists to prevent. Piers stand BETWEEN the
        # rings; a masonry arcade puts them in the water and a steel arch does not
        masonry = material in ("Naturstein", "Mauerwerk", "Ziegel")
        ends = (x0, x1) if masonry else (forbid[0], forbid[1])
        n = max(1, fields)
        pier_w = max(0.8, min(0.10 * span, 4.0))
        for k in range(n):
            a = ends[0] + (ends[1] - ends[0]) * k / n
            b = ends[0] + (ends[1] - ends[0]) * (k + 1) / n
            mid = 0.5 * (a + b)
            head = at(mid) - depth
            spring = max(floor, min(under(a), under(b)))
            rise = max(0.6, (head - spring) * (0.80 if masonry else 0.72))
            base = head - rise
            xa = np.linspace(a, b, 60)
            ring = base + rise * (1.0 - ((xa - mid) / (0.5 * (b - a))) ** 2)
            ax.plot(xa, ring, color=ink, lw=1.8)
            ax.plot(xa, ring - depth * (0.9 if masonry else 0.6), color=ink, lw=0.9)
            if masonry:
                # a SPANDREL is filled above a stone ring, not columned: the wall between the
                # ring and the road is what a viewer reads a viaduct's mass from
                ax.fill_between(xa, ring, [at(float(v)) - depth for v in xa],
                                facecolor="0.88", edgecolor="none", zorder=0)
            else:
                for u in np.linspace(0.15, 0.85, max(3, int((b - a) / 12))):
                    x = a + (b - a) * u
                    z = base + rise * (1.0 - ((x - mid) / (0.5 * (b - a))) ** 2)
                    ax.plot([x, x], [z, at(x) - depth], color=ink, lw=0.8)
            for edge in ((a, k == 0), (b, k == n - 1)):
                x, outer = edge
                ax.plot([x, x], [base, min(under(x), base)], color=ink,
                        lw=2.0 if outer else 1.4)
                if not outer:
                    ax.add_patch(Rectangle((x - pier_w / 2, min(under(x), base)), pier_w,
                                           base - min(under(x), base) + rise * 0.10,
                                           facecolor="0.80", edgecolor=ink, lw=1.0))
    elif carrier == "stays":
        for x in (forbid[0], forbid[1]):
            top = at(x) + span * 0.20        # [SET] a stayed pylon stands about a fifth of its span
            ax.plot([x, x], [under(x), top], color=ink, lw=2.0)
            for u in np.linspace(0.08, 0.46, 8):
                for side in (-1, +1):
                    xx = x + side * span * u
                    if x0 <= xx <= x1:
                        ax.plot([x, xx], [top, at(xx)], color=ink, lw=0.6)
            ax.plot([x - depth * 0.5, x + depth * 0.5], [under(x), under(x)], color=ink, lw=1.4)
    elif carrier == "suspension":
        towers = [forbid[0], forbid[1]]
        # [SET] Golden Gate: towers 152 m over a 1280 m span (0.119), cable sag about a tenth
        top = max(at(t) for t in towers) + span * 0.115
        sag = span * 0.10
        for tx in towers:
            ax.plot([tx, tx], [under(tx), top], color=ink, lw=2.4)
        mid = 0.5 * (towers[0] + towers[1])
        xa = np.linspace(towers[0], towers[1], 120)
        cable = top - sag * (1.0 - ((xa - mid) / (0.5 * (towers[1] - towers[0]))) ** 2)
        ax.plot(xa, cable, color=ink, lw=1.6)
        for u in np.linspace(0.04, 0.96, 22):
            x = towers[0] + (towers[1] - towers[0]) * u
            z = top - sag * (1.0 - ((x - mid) / (0.5 * (towers[1] - towers[0]))) ** 2)
            ax.plot([x, x], [z, at(x)], color=ink, lw=0.5)
        for side, tx in ((-1, towers[0]), (+1, towers[1])):
            end = x0 if side < 0 else x1
            ax.plot([tx, end], [top, at(end)], color=ink, lw=1.4)
    if water is not None:
        ax.axhline(water, color="0.35", lw=0.9, ls="-.")
    return fields, span, name, depth, material, carrier


# ----------------------------------------------------------------------------- earthworks# ----------------------------------------------------------------------------- earthworks

class Earthworks:
    """The second half of the construction order: how the GROUND is made to carry the road.

    At every station and on each side, the carriageway's edge stands at some height and the
    terrain at another. The difference is the earthwork: a FILL where the road is above the
    ground and a CUT where it is below. From the edge a batter runs at the standard's slope --
    1:1.5 filling, 1:1 cutting (RAS-Q) -- until it meets the terrain, and where it meets is the
    TOE. Where the batter would be taller than WALL_ABOVE_M the slope becomes a RETAINING WALL,
    which is what every hillside road in Wuppertal or on the Cote d'Azur actually has: at 3 m
    the land a batter eats is worth more than the wall.

    What comes out is a per-station instruction -- side, kind, height, toe offset -- which is
    what the engine stamps into the terrain. Its invariant (I11) is that the ground meets the
    road: no gap under the kerb, no cliff the lattice cannot draw, and the toe on the terrain."""

    def __init__(self, structure, step_m=4.0):
        self.st = structure
        self.map = structure.map
        self.net = structure.net
        self.step = step_m
        self.rows = self._rows()

    def _rows(self):
        out = []
        for w in self.net.ways:
            if self.net.spans(w) or self.net.bores(w):
                continue                      # a deck and a bore stand clear of the ground
            half = w["tags"]["width"] / 2.0
            for lo, hi in Mesh(self.st).drawn_spans(w) if False else self.map_spans(w):
                n = max(2, int((hi - lo) / self.step) + 1)
                for k in range(n):
                    s = lo + (hi - lo) * k / (n - 1)
                    for side in (-1, +1):
                        out.append(self._row(w, s, side * half))
        return out

    def map_spans(self, way):
        s = self.map.stations[way["id"]]
        return [(0.0, s[-1])] if s[-1] > 0 else []

    def _row(self, way, station, offset):
        line = self.map.centreline(way)
        p = line.interpolate(min(max(station, 0.0), line.length))
        ahead = line.interpolate(min(station + 0.1, line.length))
        back = line.interpolate(max(station - 0.1, 0.0))
        d = (ahead.x - back.x, ahead.y - back.y)
        n = math.hypot(*d) or 1.0
        nx, ny = -d[1] / n, d[0] / n
        x, y = p.x + nx * offset, p.y + ny * offset
        edge = self.st.own_surface(way, station, offset)
        ground = self.map.terrain.filled(x, y)
        rise = edge - ground
        slope = BATTER_FILL if rise > 0 else BATTER_CUT
        # the toe: walk outward until the batter meets the terrain. The batter's height changes
        # as the terrain does, so this is a root find, not a formula -- and on a cross-slope the
        # downhill toe runs much further than the uphill one, which is the asymmetry a hillside
        # road actually has
        outward = (nx * (1 if offset > 0 else -1), ny * (1 if offset > 0 else -1))
        toe = 0.0
        for r in np.arange(0.25, 60.0, 0.25):
            qx, qy = x + outward[0] * r, y + outward[1] * r
            batter = edge - abs(rise) / abs(rise) * (r / slope) * (1 if rise > 0 else -1)
            if (rise > 0 and batter <= self.map.terrain.filled(qx, qy)) or \
               (rise <= 0 and batter >= self.map.terrain.filled(qx, qy)):
                toe = r
                break
        else:
            toe = math.inf
        wall = abs(rise) > WALL_ABOVE_M
        return {"way": way["id"], "station": station, "offset": offset, "x": x, "y": y,
                "edge": edge, "ground": ground, "rise": rise,
                "kind": ("wall" if wall else ("fill" if rise > 0 else "cut")),
                "toe_m": toe, "height_m": abs(rise)}

    def measures(self):
        if not self.rows:
            return {"rows": 0}
        rise = np.array([r["rise"] for r in self.rows])
        toe = np.array([r["toe_m"] for r in self.rows])
        return {"rows": len(self.rows),
                "fill_max_m": float(np.max(rise)), "cut_max_m": float(-np.min(rise)),
                "walls": sum(1 for r in self.rows if r["kind"] == "wall"),
                "toe_max_m": float(np.max(toe[np.isfinite(toe)])) if np.isfinite(toe).any() else math.inf,
                "unreached": int(np.sum(~np.isfinite(toe)))}


def check_earthworks(map_, st):
    """I11: every carriageway edge has a batter or a wall that REACHES the terrain, and the road
    never stands on nothing. The negative control is a road laid at the DEM's own height on a
    cross-slope: its downhill edge then floats by half the width times the slope."""
    work = Earthworks(st)
    m = work.measures()
    return m


# ----------------------------------------------------------------------------- checks

def check_c0(map_):
    """I1: every way through a node reads one height -- true by construction (one variable per
    node); the check is that the profile at each way's node station returns exactly it."""
    worst = 0.0
    for w in map_.net.ways:
        for k, r in enumerate(w["refs"]):
            z, _ = map_.profile(w, map_.stations[w["id"]][k])
            worst = max(worst, abs(z - map_.z[map_.index[r]]))
    return worst


def check_c1(map_):
    """I2: at every node the profile's own grade IS the tangent the solve stored there.

    C1 across a node is the cubic Hermite's DEFINING property, so a probe at s +/- eps cannot
    test it: with a fixed 1e-6 m into a 2.2 m segment the two heights agree to about 1e-13 and
    the difference is catastrophic cancellation, which is how Hong Kong Central read 1.14e-06
    against a 1e-6 tolerance at two nodes of 3 373 (measured 2026-09-06); widen the probe and it
    stops measuring cancellation and starts measuring CURVATURE, which is legitimately not zero.
    A check that pins its own probe is mis-specified and the CHECK is what changes.

    What can actually be wrong is the CONSTRUCTION: that the polynomial on each side is built
    from the stored tangent at all. So the check evaluates the profile AT the node, from the left
    segment and from the right, and compares both with `slope[way][k]`. Exact to machine
    precision when the construction is right, and the full size of the error when it is not."""
    worst = 0.0
    for w in map_.net.ways:
        s = map_.stations[w["id"]]
        m = map_.slope[w["id"]]
        for k in range(1, len(w["refs"]) - 1):
            told = float(m[k])
            # from the RIGHT segment: t = 0 there
            _, g_right = map_.profile(w, s[k])
            # and from the LEFT: t = 1 on the segment that ends here, reached by stepping back
            # into it and asking for the end -- `profile` clamps to the segment it lands in
            back = s[k] - min(1e-3, 0.25 * (s[k] - s[k - 1]))
            kk = int(np.searchsorted(s, back, side="right") - 1)
            h = s[kk + 1] - s[kk]
            z0, z1 = map_.z[map_.index[w["refs"][kk]]], map_.z[map_.index[w["refs"][kk + 1]]]
            m0, m1 = m[kk] * h, m[kk + 1] * h
            g_left = ((6 - 6) * z0 + (3 - 4 + 1) * m0 + (-6 + 6) * z1 + (3 - 2) * m1) / h
            worst = max(worst, abs(g_right - told), abs(g_left - told))
    return worst


# WHAT A DECK MUST CLEAR DEPENDS ON WHAT IS UNDER IT. 4.5 m is the figure for a ROAD (RAS-Q, and
# the same in AASHTO's 16 ft); a footway or a cycleway needs headroom for a person, a railway needs
# room for its CATENARY, and a navigable water needs what the waterway authority says. Asking 4.5 m
# over a footpath is what made the Glenfinnan viaduct infeasible by 0.74 m and the Goeltzschtal by
# 0.96 m -- both rail viaducts whose arches a path runs under (measured 2026-09-06).
CLEARANCE_OVER = {"footway": 2.50, "path": 2.50, "cycleway": 2.50, "steps": 2.30,
                  "bridleway": 3.40, "track": 4.00, "pedestrian": 3.50, "service": 4.00,
                  "living_street": 4.20}


def clearance_over(way):
    """The headroom a deck owes the way beneath it."""
    if way["tags"].get("railway") or "rail" in way["tags"]:
        return CLEARANCE_RAIL_M
    return CLEARANCE_OVER.get(way["tags"].get("highway", ""), CLEARANCE_M)


def _boxes_touch(a, b, slack):
    """Do two bounding boxes come within `slack` of each other? A pair that fails this cannot
    cross, and asking it first is what keeps a 1 854-way extract from building three million
    buffers (measured: the pairwise ribbon test alone doubled the synthetic bed's time)."""
    return not (a[2] + slack < b[0] or b[2] + slack < a[0]
                or a[3] + slack < b[1] or b[3] + slack < a[1])


def check_dem_band(map_):
    """I3: where the DEM is the authority, |z - dem| within its error -- not on a deck, not in
    a bore, not on a bridge's approach, which is a designed ramp (I11 builds its fill)."""
    # a CAUSEWAY is a structure like a ramp: its fill over the water is not a deviation from the
    # DEM but the reason it is drivable at all, so it and its own APPROACHES leave the band and
    # carry their fill as a number instead (measured 9.00 m on a coast road before this)
    near_causeway = np.zeros(len(map_.index), dtype=bool)
    if map_.causeway.any():
        wet = {r for r in map_.net.nodes if map_.causeway[map_.index[r]]}
        for w in map_.net.ways:
            if not (set(w["refs"]) & wet):
                continue
            for r in w["refs"]:
                near_causeway[map_.index[r]] = True
    held = ~(map_.deck | map_.bore | map_.approaches() | map_.causeway | near_causeway
             | map_.open_ends())
    # measured against the SAME band the solve was given, as a ratio: 1.0 is exactly at it
    # the band is a CONSTRAINT the QP solves to its own primal tolerance, so the check allows
    # exactly that and no more: [SET] 0.1 mm, four orders below anything a driver feels and two
    # above OSQP's residual. Without it the Etoile read 1.00x and went RED on the solver's own
    # rounding (measured 2026-09-06)
    up = (map_.z - map_.dem) / np.maximum(map_.band() + BAND_SLACK_M, 1e-9)
    down = (map_.dem - map_.z) / np.maximum(map_.band_down() + BAND_SLACK_M, 1e-9)
    over = np.maximum(up, down)
    return float(np.max(over[held])) if held.any() else 0.0


def check_clearance_fixpoint(map_):
    """I12: the clearance loop REACHED its fixed point.

    The floors are re-read from the roads below as they now stand, so the scheme is a
    SUBSTITUTION and nothing proves it contracts. On a mountain road where a deck stands over
    another deck's ramp it does not: measured 2026-09-06, forty rounds left 0.44 m at the
    Transfagarasan and 4.44 m at the Stelvio, and the profile walked to 105 m and 183 m off the
    DEM while it did. Two fixed rounds hid it -- they stopped before the walk was visible and
    the bed called the result an answer. A diverged profile is not a worse answer, it is NO
    answer, and this check is what says so."""
    return float(getattr(map_, "clearance_residual_m", 0.0))


def clearance_verdict(map_):
    """Converged, diverged, or still walking when the bound ran out."""
    if getattr(map_, "clearance_diverged", False):
        return "DIVERGED"
    if float(getattr(map_, "clearance_residual_m", 0.0)) <= CLEARANCE_TOL_M:
        return "converged"
    return "unfinished"


def check_bridge(map_):
    """I4: the deck above the water plus clearance; the abutments on the terrain."""
    if not map_.deck.any():
        return None
    water = map_.terrain.water
    lowest = float(np.min(map_.z[map_.deck] - (water if water is not None else -1e9)))
    below = 0.0
    fill = 0.0
    for w in map_.net.ways:
        if map_.net.spans(w):
            for r in (w["refs"][0], w["refs"][-1]):
                k = map_.index[r]
                below = max(below, map_.dem[k] - map_.z[k])
                fill = max(fill, map_.z[k] - map_.dem[k])
    return {"clearance_m": lowest, "abutment_below_m": below, "abutment_fill_m": fill, "ramp_m": map_.ramp_m()}


def check_water(map_):
    """Water is a SURFACE with a level, and a road meets it in one of four ways: a bridge over
    it, a ford through it, a causeway on fill above it, or a mistake. The count is what a
    scenario reads; the rule is the type table's."""
    water = map_.terrain.water
    if water is None:
        return None
    over, under, spans, fords = 0, 0, 0, 0
    for w in map_.net.ways:
        for k, r in enumerate(w["refs"]):
            x, y = map_.net.nodes[r]
            if map_.terrain.truth(x, y) >= water:
                continue
            if map_.net.spans(w):
                spans += 1
            elif w["tags"].get("ford") == "yes":
                fords += 1
            elif map_.z[map_.index[r]] >= water:
                over += 1                      # a causeway: fill above the surface
            else:
                under += 1                     # drowned, and that is the finding
    fill = float(np.max((map_.z - map_.dem)[map_.causeway])) if map_.causeway.any() else 0.0
    return {"deck": spans, "ford": fords, "causeway": over, "drowned": under,
            "causeway_fill_m": fill}


def check_finite(map_):
    """P: every height and grade finite, and no deck thrown farther than the DEM's error where
    it has no abutment (it keeps the weak tie and is COUNTED, never at -19 m)."""
    finite = bool(np.all(np.isfinite(map_.z)))
    grades = np.concatenate([np.abs(m) for m in map_.slope.values()]) if map_.slope else np.zeros(1)
    # a deck stands as far above the ground as its obstacle is deep -- 90 m over a gorge is
    # right -- so the fault is a deck UNDER the ground, or one thrown clean out of the world
    below = float(np.max((map_.dem - map_.z)[map_.deck])) if map_.deck.any() else 0.0
    above = float(np.max((map_.z - map_.dem)[map_.deck])) if map_.deck.any() else 0.0
    return {"finite": finite, "grade_max": float(np.max(grades)),
            "deck_below_dem_m": below, "deck_above_dem_m": above,
            "ramp_grade_max": ramp_grade(map_)}


def ramp_grade(map_):
    near = map_.approaches()
    worst = 0.0
    for w in map_.net.ways:
        refs, s = w["refs"], map_.stations[w["id"]]
        for k in range(1, len(refs)):
            a, b = map_.index[refs[k - 1]], map_.index[refs[k]]
            ds = s[k] - s[k - 1]
            if ds > 1e-3 and (near[a] or near[b] or map_.net.spans(w)):
                worst = max(worst, abs(map_.z[b] - map_.z[a]) / ds)
    return worst


def check_over_road(map_, st):
    """I4 for a bridge over a ROAD: at every deck node, the deck against the surface of any
    other way passing underneath within its width -- the clearance."""
    least = None
    for w in map_.net.ways:
        if not map_.net.spans(w):
            continue
        line_w = map_.centreline(w)
        for s_deck in np.arange(0.0, map_.stations[w["id"]][-1], 1.0):
            x, y = line_w.interpolate(s_deck).coords[0]
            z_deck, _ = map_.profile(w, s_deck)
            for other in map_.net.ways:
                if other is w or map_.net.spans(other) or set(other["refs"]) & set(w["refs"]):
                    continue
                line = map_.centreline(other)
                s = line.project(Point(x, y))
                q = line.interpolate(s)
                if q.distance(Point(x, y)) <= other["tags"]["width"] / 2.0 + w["tags"]["width"] / 2.0:
                    below = st.own_surface(other, s, q.distance(Point(x, y)))
                    gap = z_deck - below
                    least = gap if least is None else min(least, gap)
    return least


def check_tunnel(map_):
    """I5: the bore under the terrain by the cover; the portals on the terrain."""
    if not map_.bore.any():
        return None
    least = 1e9
    for w in map_.net.ways:
        if map_.net.bores(w):
            for r in w["refs"][1:-1]:
                k = map_.index[r]
                x, y = map_.net.nodes[r]
                least = min(least, map_.terrain.truth(x, y) - map_.z[k])
    portal = 0.0
    short = 0
    for w in map_.net.ways:
        if map_.net.bores(w):
            for r in (w["refs"][0], w["refs"][-1]):
                k = map_.index[r]
                portal = max(portal, abs(map_.z[k] - map_.dem[k]))
            for r in w["refs"][1:-1]:
                k = map_.index[r]
                x, y = map_.net.nodes[r]
                if map_.terrain.truth(x, y) - map_.z[k] < COVER_M:
                    short += 1
    # the cover is short only next to a portal, where the structure cuts the hill open (the
    # portal cutting); a bore that surfaces mid-hill is the failure, and that reads cover < 0
    return {"cover_m": least, "portal_off_m": portal, "portal_cutting_nodes": short}


# ----------------------------------------------------------------------------- the bed

CASES = [
    ("R1-straight", "T1-flat"), ("R1-straight", "T2-along5"), ("R1-straight", "T2-along15"),
    ("R1-straight", "T3-cross15"), ("R1-straight", "T4-crest"), ("R1-straight", "T5-sag"),
    ("R1-straight", "T6-terraces"), ("R1-straight", "T7-cliff"), ("R1-straight", "T11-noise"),
    ("R1-straight", "T12-hole"),
    ("R2-curve200", "T3-cross15"), ("R2-curve30", "T3-cross15"),
    ("R4-T90", "T1-flat"), ("R4-T90", "T2-along15"), ("R4-T90", "T3-cross15"), ("R4-T30", "T3-cross15"),
    ("R5-X90", "T3-cross15"), ("R5-X60", "T2-along15"), ("R5-X90", "T4-crest"),
    ("R18-bridge", "T8-valley"), ("R20-tunnel", "T4-crest"),
    ("R10-interchange", "T1-flat"), ("R10-interchange", "T3-cross15"),
    ("R7-roundabout", "T1-flat"), ("R7-roundabout", "T2-along5"),
    ("R9-ramp", "T2-along5"), ("R8-dual", "T3-cross15"),
    ("P1-dupnodes", "T2-along5"), ("P2-zerolen", "T2-along5"), ("P3-gap", "T2-along5"),
    ("P4-dupway", "T2-along5"), ("P5-nolanding", "T8-valley"), ("P6-dense", "T4-crest"),
    ("R3-scurve", "T3-cross15"), ("R6-fork", "T2-along5"), ("R6-fork", "T3-cross15"),
    ("R11-stacked", "T1-flat"), ("R11-rampcycle", "T1-flat"), ("R11-rampcycle", "T3-cross15"),
    ("R11-spiral", "T1-flat"), ("R11-spiral", "T10-mountain"), ("R12-culdesac", "T3-cross15"), ("R15-widths", "T3-cross15"),
    ("R16-onewaypair", "T3-cross15"), ("R19-viaduct", "T5-sag"), ("R21-underpass", "T1-flat"),
    ("R24-causeway", "T9-coast"), ("R25-ford", "T8-valley"), ("P7-layer", "T2-along5"),
    ("R1-straight", "T9-coast"), ("R1-straight", "T10-mountain"), ("R2-curve30", "T10-mountain"),
    ("R18-bridge", "T13-baked"), ("R1-straight", "T14-coarse"), ("R4-T90", "T14-coarse"),
    ("R2-hairpin", "T10-mountain"), ("R2-hairpin-dense", "T10-mountain"), ("R22-embankment", "T1-flat"), ("R23-cutting", "T4-crest"),
    ("R1-straight", "T3-cross60"), ("R4-T90", "T3-cross60"), ("R26-dam", "T5-sag"),
    ("R13-parking", "T3-cross15"), ("P8-inbuilding", "T1-flat"), ("P9-inwater", "T15-lake"),
    ("P10-holeatnode", "T12-hole"), ("R27-elevated", "T3-cross15"), ("R18-bridge", "T15-lake"),
    ("R28-arch", "T17-gorge"), ("R29-stayed", "T16-strait"), ("R30-suspension", "T16-strait900"),
    ("R31-railtruss", "T17-gorge90"), ("R32-culvert", "T5-sag"), ("R19-viaduct", "T8-valley"),
    ("R14-steps", "T10-mountain"), ("R14-track", "T3-cross30"), ("R27-elevated", "T1-flat"),
    ("R17-seam", "T4-crest"), ("R17-seam", "T6-terraces"), ("R1-straight", "T3-cross30"), ("R4-T90", "T5-sag"),
]


def check_seam(terrain, net_maker, border_x=0.0, halo_m=60.0):
    """I8: a way crossing a tile border reads IDENTICAL vertices from both tiles. The bed builds
    the map once for the whole network and once per tile with a HALO of neighbouring nodes, and
    compares the node heights and the mesh vertices within a metre of the border. Cesium's
    quantized-mesh and Unreal's Landscape both solve this by making the seam's data shared
    rather than recomputed; the halo is what makes a per-tile solve give the shared answer."""
    whole = Map(terrain, net_maker()).solve()
    left, right = {}, {}
    for side, keep in ((left, lambda x: x <= border_x + halo_m), (right, lambda x: x >= border_x - halo_m)):
        net = net_maker()
        drop = {nid for nid, (x, y) in net.nodes.items() if not keep(x)}
        for w in net.ways:
            w["refs"] = [r for r in w["refs"] if r not in drop]
        net.ways = [w for w in net.ways if len(w["refs"]) >= 2]
        net.nodes = {nid: p for nid, p in net.nodes.items() if nid not in drop}
        if not net.ways:
            return {"nodes": 0, "worst_m": 0.0}
        part = Map(terrain, net).solve()
        for nid in part.net.nodes:
            side[nid] = part.z[part.index[nid]]
    worst = 0.0
    against_whole = 0.0
    shared = 0
    for nid, zl in left.items():
        if nid not in right:
            continue
        x, _ = whole.net.nodes.get(nid, (1e9, 0))
        if abs(x - border_x) > 20.0:
            continue
        shared += 1
        worst = max(worst, abs(zl - right[nid]))
        against_whole = max(against_whole, abs(zl - whole.z[whole.index[nid]]))
    return {"nodes": shared, "worst_m": worst, "vs_whole_m": against_whole}


def seam_sweep(terrain, net_maker):
    """How wide a HALO a tile needs so that its own solve is the whole's. The fit couples nodes
    over the smoothing length l = 2 DEM postings (50 m here), and its influence decays like
    exp(-d/l), so the halo is a MULTIPLE of l and the sweep says which. This is the number the
    streamer needs: a tile that fetches less than this cannot produce the shared answer, and no
    amount of welding afterwards repairs it."""
    out = {}
    for halo in (0.0, 25.0, 50.0, 100.0, 200.0):
        got = check_seam(terrain, net_maker, halo_m=halo)
        out[f"halo {halo:.0f} m"] = got["vs_whole_m"]
    got = check_seam(terrain, net_maker, halo_m=200.0)
    out["nodes"] = got["nodes"]
    out["worst_m"] = got["worst_m"]
    out["vs_whole_m"] = got["vs_whole_m"]
    return out


def run(case, number=0):
    rname, tname = case
    terrain = TERRAINS[tname]()
    net = NETWORKS[rname]()
    m = Map(terrain, net).solve()
    st = Structure(m)
    verdict = {
        "I1 C0 m": check_c0(m),
        "I2 C1 grade": check_c1(m),
        "I3 |z-dem| / band": check_dem_band(m),
        "I6 junction step m": st.check_junction_steps() if m.junctions else None,
        "I12 clearance residual m": check_clearance_fixpoint(m),
        "I4 bridge": check_bridge(m),
        "I4 over road m": check_over_road(m, st),
        "I5 tunnel": check_tunnel(m),
        "P finite": check_finite(m),
        "water": check_water(m),
        "I7/I9 mesh": check_mesh(m, st),
        "I11 earthworks": check_earthworks(m, st),
        "I8 seam": (seam_sweep(terrain, NETWORKS[rname]) if rname == "R17-seam" else None),
    }
    red = []
    if verdict["I1 C0 m"] > 1e-9:
        red.append("I1")
    if verdict["I2 C1 grade"] > 1e-6:
        red.append("I2")
    if verdict["I3 |z-dem| / band"] > 1.0 + 1e-6:
        red.append("I3")
    if verdict["I12 clearance residual m"] > CLEARANCE_TOL_M:
        red.append("I12")
    if verdict["I6 junction step m"] is not None and verdict["I6 junction step m"] > STEP_TOL_M:
        red.append("I6")
    if verdict["I4 bridge"] is not None and (verdict["I4 bridge"]["clearance_m"] < CLEARANCE_M - BAND_SLACK_M or verdict["I4 bridge"]["abutment_below_m"] > DEM_ERROR_M):
        red.append("I4")
    if verdict["I5 tunnel"] is not None and (verdict["I5 tunnel"]["cover_m"] < 0.0 or verdict["I5 tunnel"]["portal_off_m"] > DEM_ERROR_M):
        red.append("I5")
    if verdict["I4 over road m"] is not None and verdict["I4 over road m"] < CLEARANCE_M:
        red.append("I4road")
    seam = verdict["I8 seam"]
    if seam is not None:
        # the seam must stand well inside the DRIVING tolerance, and the halo that buys it is
        # the number the streamer needs: two smoothing lengths (four DEM postings, 100 m here).
        # The control is the same tile with no halo, which must be OUTSIDE the tolerance.
        if seam["nodes"] < 3 or seam["worst_m"] > MESH_TOL_M:
            red.append("I8")
        if seam["halo 100 m"] > MESH_TOL_M:
            red.append("I8 halo")
        if seam["halo 0 m"] <= MESH_TOL_M:
            red.append("I8 control green")
    earth = verdict["I11 earthworks"]
    if earth.get("rows", 0) > 0 and earth["unreached"] > 0:
        red.append("I11")
    wet = verdict["water"]
    if wet is not None and wet["drowned"] > 0 and case[0] not in ("R25-ford", "P9-inwater"):
        red.append("drowned")
    mesh = verdict["I7/I9 mesh"]
    if mesh["edges_over_two_faces"] > 0 or mesh["nearest_pair_m"] < WELD_M:
        red.append("I7")
    if mesh["drawn_vs_analytic_m"] > 0.01:
        red.append("I9")
    told = verdict["P finite"]
    if (not told["finite"] or told["grade_max"] > 1.0 or told["deck_below_dem_m"] > 1.0
            or told["deck_above_dem_m"] > 400.0):
        red.append("P")
    plot(case, m, st, number)
    return verdict, red


def draw_structure_section(axq, m, st, way, station, offs, gnd, ink):
    """A BRIDGE's or a TUNNEL's cross section: a deck is a slab with a depth from its span and a
    parapet each side, standing over a void; a bore is a section cut out of the rock. Neither has
    an earthwork, and drawing one gave a deck a 30 m embankment into the river (seen in the
    sheet). The slab's depth follows the span table in CASES.md: span/20 for a beam."""
    from matplotlib.patches import Rectangle
    half = way["tags"]["width"] / 2.0
    axis_z = st.own_surface(way, station, 0.0)
    span = m.stations[way["id"]][-1]
    if m.net.spans(way):
        xs_s = np.linspace(0, span, 120)
        lw_s = m.map.centreline(way) if hasattr(m, "map") else m.centreline(way)
        deck_s = np.array([m.profile(way, float(v))[0] for v in xs_s])
        gnd_s = np.array([m.terrain.filled(*lw_s.interpolate(float(v)).coords[0]) for v in xs_s])
        free_s, _ = free_span_of(xs_s, deck_s, gnd_s, m.terrain.water)
        fields, field_m, what, depth, material, carrier = bridge_type(
            span, rail=way["tags"].get("railway") is not None, free_m=free_s, tags=way["tags"],
            rise_m=float(np.nanmax(deck_s - gnd_s)) if len(deck_s) else None)
        depth = max(0.6, depth)
        axq.plot(offs, gnd, color="0.5", lw=1.0)
        axq.add_patch(Rectangle((-half, axis_z - depth), 2 * half, depth,
                                facecolor="0.85", edgecolor=ink, lw=1.4))
        for side in (-1, +1):
            axq.plot([side * half, side * half], [axis_z, axis_z + 1.1], color=ink, lw=2.0)
            for r in np.linspace(0, 1.1, 6):
                axq.plot([side * half - 0.15, side * half + 0.15], [axis_z + r, axis_z + r],
                         color=ink, lw=0.4)
        if carrier in ("piers", "abutments"):
            pier = min(2.0, half * 0.5)
            axq.add_patch(Rectangle((-pier / 2, gnd.min()), pier, axis_z - depth - gnd.min(),
                                    facecolor="0.9", edgecolor=ink, lw=1.0, hatch="..."))
        elif carrier == "arch":
            axq.add_patch(Rectangle((-half * 0.55, axis_z - depth - 1.2), half * 1.1, 1.2,
                                    facecolor="0.8", edgecolor=ink, lw=1.2))
        elif carrier == "stays":
            axq.plot([0, 0], [axis_z, axis_z + max(8.0, span * 0.06)], color=ink, lw=2.4)
            for side in (-1, +1):
                axq.plot([0, side * half], [axis_z + max(8.0, span * 0.06), axis_z], color=ink, lw=0.7)
        elif carrier == "suspension":
            # a suspension deck hangs from VERTICAL hangers off two cables at the deck's edges
            for side in (-1, +1):
                axq.plot([side * half, side * half], [axis_z, axis_z + max(10.0, span * 0.03)],
                         color=ink, lw=0.8)
            axq.plot([-half, half], [axis_z + max(10.0, span * 0.03)] * 2, color=ink, lw=1.4)
        elif carrier == "truss":
            for u in np.linspace(-half, half, 9)[:-1]:
                axq.plot([u, u + 2 * half / 8], [axis_z, axis_z - depth], color=ink, lw=0.6)
        water = m.terrain.water
        floor = water if water is not None else float(np.interp(0.0, offs, gnd))
        if water is not None:
            axq.axhline(water, color="0.35", lw=0.9, ls="-.")
        axq.annotate("", xy=(half * 0.6, axis_z - depth), xytext=(half * 0.6, floor),
                     arrowprops=dict(arrowstyle="<|-|>", color=ink, lw=0.8))
        axq.text(half * 0.7, (axis_z - depth + floor) / 2, f"LR {axis_z - depth - floor:.2f} m",
                 fontsize=6.5, color=ink, rotation=90, va="center")
        axq.set_title(f"UEBERBAU Sta {station:.0f}   {what} ({material})  d {depth:.2f} m  "
                      f"{fields} x {field_m:.0f} m", fontsize=8, loc="left")
    else:
        crown = axis_z + 5.0
        axq.plot(offs, gnd, color="0.5", lw=1.0)
        axq.fill_between(offs, axis_z - 1.0, gnd, facecolor="none", hatch="xxx", edgecolor="0.65", lw=0.0)
        axq.add_patch(Rectangle((-half - 1.0, axis_z), 2 * (half + 1.0), crown - axis_z,
                                facecolor="white", edgecolor=ink, lw=1.6))
        axq.plot([-half, half], [axis_z, axis_z], color=ink, lw=2.6)
        cover = float(np.interp(0.0, offs, gnd)) - crown
        axq.annotate(f"Ueberdeckung {cover:.2f} m", (0, crown), fontsize=6.5, color=ink,
                     ha="center", xytext=(0, 6), textcoords="offset points")
        axq.set_title(f"TUNNEL Sta {station:.0f}   lichte Hoehe 5.00 m", fontsize=8, loc="left")
    axq.set_aspect("equal")
    axq.grid(alpha=0.22, lw=0.5)
    axq.tick_params(labelsize=6.5)
    axq.set_xlabel("m von der Achse", fontsize=7)


def build_route(m, seed=None):
    """The TRASSE: the chain of ways that share end nodes, in order, each with its start station
    and whether it runs forward. A longitudinal section is drawn along a route and not along one
    way -- a bridge is three ways (approach, deck, approach) and a section of the longest showed
    a plateau where the valley is (seen in the sheet)."""
    ways = list(m.net.ways)
    if not ways:
        return []
    # THE ROUTE IS SEEDED, and on a real extract that is the whole question: the longest way in
    # a city is whatever street happens to be longest, and the sheet was then a section along a
    # footpath while the bridge the case is named after stood beside it (measured at the Ponte
    # Vecchio). A caller that knows which way the drawing is ABOUT hands it in
    if seed is None or seed["id"] not in {w["id"] for w in ways}:
        seed = max(ways, key=lambda w: m.stations[w["id"]][-1])
    chain = [(seed, True)]
    used = {seed["id"]}
    for at_end in (True, False):
        while True:
            head = chain[-1] if at_end else chain[0]
            w, forward = head
            node = w["refs"][-1 if forward == at_end else 0]
            nxt = None
            for other in ways:
                if other["id"] in used or other["tags"].get("junction") == "roundabout":
                    continue
                if other["refs"][0] == node:
                    nxt = (other, at_end)
                    break
                if other["refs"][-1] == node:
                    nxt = (other, not at_end)
                    break
            if nxt is None:
                break
            used.add(nxt[0]["id"])
            if at_end:
                chain.append(nxt)
            else:
                chain.insert(0, (nxt[0], not nxt[1]))
    out = []
    at = 0.0
    for (w, forward) in chain:
        out.append((w, forward, at))
        at += m.stations[w["id"]][-1]
    return out


def plot(case, m, st, number=0, seed=None):
    """One engineering sheet per case, to DIN 1356 / RAS-Q convention:

        LAGEPLAN      the alignment, both carriageway edges, the junction surfaces, the
                      earthwork toes, stationing every 50 m, a north arrow and a scale bar
        LAENGSSCHNITT the main axis: terrain, DEM, gradient, cut and fill hatched apart,
                      grades in percent at every 100 m, the vertical exaggeration stated
        QUERSCHNITTE  three stations: carriageway with its crossfall, kerb, batters with
                      their slopes, cut and fill hatched, every width and height dimensioned
        SCHRIFTFELD   what is drawn, on what, by which standard, and the earthwork's totals
    """
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Polygon as MplPoly

    OUT.mkdir(parents=True, exist_ok=True)
    ink = "0.12"
    fig = plt.figure(figsize=(16.5, 11.7))
    gs = fig.add_gridspec(3, 3, height_ratios=[1.5, 1.0, 1.0], hspace=0.30, wspace=0.20,
                          left=0.05, right=0.97, top=0.92, bottom=0.07)
    work = Earthworks(st)
    mesh = Mesh(st)
    route = build_route(m, seed)
    main = seed if seed is not None and any(w["id"] == seed["id"] for (w, _, _) in route) \
        else max((w for (w, _, _) in route), key=lambda w: m.stations[w["id"]][-1])

    # ---------------------------------------------------------------- LAGEPLAN
    ax = fig.add_subplot(gs[0, :2])
    xs, ys = [], []
    on_axis = {rw["id"] for (rw, _, _) in route}
    for w in m.net.ways:
        line = m.centreline(w)
        half = w["tags"]["width"] / 2.0
        # A PLAN IS READ BY ITS HIERARCHY. Every way drawn at one weight is a black tangle on a
        # real extract -- 180 ways in Florence, every kerb line 1.3 pt (measured, first real
        # sheet). The weight follows the class the network already carries, the route the sheet
        # is ABOUT is heavier still, and a bridge's deck is filled where a tunnel's is dashed
        rank = float(w["tags"].get("priority", 5))
        mine = w["id"] in on_axis
        edge = 1.9 if mine else max(0.35, min(1.2, 0.25 + rank * 0.075))
        spans, bores = m.net.spans(w), m.net.bores(w)
        for lo, hi in mesh.drawn_spans(w):
            ss = np.linspace(lo, hi, max(2, int((hi - lo) / 2) + 1))
            left = [mesh._leg_point(w, float(s), +half)[:2] for s in ss]
            right = [mesh._leg_point(w, float(s), -half)[:2] for s in ss]
            if spans:
                ax.add_patch(MplPoly(np.array(left + right[::-1]), closed=True,
                                     facecolor="0.72", edgecolor=ink, lw=edge, zorder=3))
            for pts in (left, right):
                ax.plot(*zip(*pts), color=ink, lw=edge,
                        ls=(0, (3, 2)) if bores else "-", zorder=2 if not mine else 4)
                xs += [p[0] for p in pts]; ys += [p[1] for p in pts]
        if not mine:
            continue
        ax.plot(*line.xy, color="0.35", lw=1.0, ls=(0, (14, 4, 2, 4)), zorder=5)
        s = m.stations[w["id"]]
        # the stationing is the ROUTE's, so three ways of one road read 0..1000 and not 0..300
        # three times over (which is what the first plan showed) -- and it is drawn on the ROUTE
        # ALONE, or a real extract carries a cross and a number every fifty metres of every lane
        on_route = next(((f, a0) for (rw, f, a0) in route if rw is w), None)
        for station in np.arange(0, s[-1] + 1, 50.0):
            p = line.interpolate(float(min(station, s[-1])))
            ax.plot([p.x], [p.y], marker="+", color="0.45", ms=4, mew=0.7)
            if station % 200 == 0:
                shown = station
                if on_route is not None:
                    forward, at0 = on_route
                    shown = at0 + (station if forward else s[-1] - station)
                ax.annotate(f"{shown:.0f}", (p.x, p.y), fontsize=6, color="0.45",
                            xytext=(0, 6), textcoords="offset points", ha="center")
    for nid in st.polygons:
        # the REGION, not the node polygon: the warp bands belong to the junction's surface, and
        # a plan that drew only the polygon left a gap where the minor legs tie in
        region = mesh.region_of(nid)
        for part in (region.geoms if region.geom_type == "MultiPolygon" else [region]):
            ax.add_patch(MplPoly(np.array(part.exterior.coords), closed=True,
                                 facecolor="0.90", edgecolor=ink, lw=1.3, zorder=0))
    for kind, colour, marker in (("fill", "0.45", "."), ("cut", "0.2", "."), ("wall", "black", "s")):
        pts = [(r["x"] + (r["toe_m"] if np.isfinite(r["toe_m"]) else 0) * 0.0, r["y"])
               for r in work.rows if r["kind"] == kind and r["height_m"] > 0.20]
        toes = []
        for r in work.rows:
            if r["kind"] != kind or r["height_m"] <= 0.20 or not np.isfinite(r["toe_m"]):
                continue
            if len(m.net.ways) > 24 and r["way"] not in on_axis:
                continue      # on a real extract the toes of every service road are the noise
            way = next(w for w in m.net.ways if w["id"] == r["way"])
            line = m.centreline(way)
            p = line.interpolate(min(r["station"], line.length))
            ah = line.interpolate(min(r["station"] + 0.1, line.length))
            bk = line.interpolate(max(r["station"] - 0.1, 0.0))
            d = (ah.x - bk.x, ah.y - bk.y)
            n = math.hypot(*d) or 1.0
            nx, ny = -d[1] / n, d[0] / n
            off = r["offset"] + math.copysign(r["toe_m"], r["offset"])
            toes.append((p.x + nx * off, p.y + ny * off))
        if toes:
            ax.plot(*zip(*toes), ls="none", marker=marker, ms=1.8 if marker == "." else 2.6,
                    color=colour, label={"fill": "Boeschungsfuss Damm", "cut": "Boeschungskante Einschnitt",
                                         "wall": "Stuetzmauer"}[kind])
            xs += [p[0] for p in toes]; ys += [p[1] for p in toes]
    if xs:
        # the plan shows what the case is ABOUT: a junction case is drawn around its junctions
        # at a scale where the carriageway is a surface and not a hairline; a plain stretch is
        # drawn whole. A sheet whose subject is a hairline is a sheet nobody can check.
        structure = next((w for w in m.net.ways if m.net.spans(w) or m.net.bores(w)), None)
        if structure is not None and not m.junctions:
            pts = [m.net.nodes[r] for r in structure["refs"]]
            cx, cy = sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts)
            reach = max(max(p[0] for p in pts) - min(p[0] for p in pts),
                        max(p[1] for p in pts) - min(p[1] for p in pts)) * 0.62 + 8.0 * structure["tags"]["width"]
        elif m.junctions:
            jx = [m.net.nodes[nid][0] for nid in m.junctions for nid in m.junctions[nid]["members"]]
            jy = [m.net.nodes[nid][1] for nid in m.junctions for nid in m.junctions[nid]["members"]]
            cx, cy = (min(jx) + max(jx)) / 2, (min(jy) + max(jy)) / 2
            widest = max(w["tags"]["width"] for w in m.net.ways)
            reach = max(max(jx) - min(jx), max(jy) - min(jy)) * 0.6 + 6.0 * widest
        else:
            cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
            reach = max(max(xs) - min(xs), max(ys) - min(ys)) * 0.55 + 10.0
        ax.set_xlim(cx - reach, cx + reach); ax.set_ylim(cy - reach * 0.62, cy + reach * 0.62)
        bar = 10 ** math.floor(math.log10(reach)) * (5 if reach / 10 ** math.floor(math.log10(reach)) > 5 else 2)
        x0, y0 = cx - reach * 0.92, cy - reach * 0.52
        ax.plot([x0, x0 + bar], [y0, y0], color=ink, lw=2.4, solid_capstyle="butt")
        ax.plot([x0, x0], [y0 - reach * 0.02, y0 + reach * 0.02], color=ink, lw=1.0)
        ax.plot([x0 + bar, x0 + bar], [y0 - reach * 0.02, y0 + reach * 0.02], color=ink, lw=1.0)
        ax.annotate(f"{bar:.0f} m", (x0 + bar / 2, y0), fontsize=7, color=ink, ha="center",
                    xytext=(0, 4), textcoords="offset points")
        ax.annotate("N", xy=(cx + reach * 0.86, cy + reach * 0.50), xytext=(cx + reach * 0.86, cy + reach * 0.30),
                    arrowprops=dict(arrowstyle="-|>", color=ink, lw=1.2), ha="center", fontsize=9, color=ink)
    ax.set_aspect("equal")
    ax.set_title("LAGEPLAN", fontsize=10, loc="left", fontweight="bold")
    ax.legend(fontsize=6.5, loc="lower right", framealpha=0.92)
    ax.set_xticks([]); ax.set_yticks([])
    for sp in ax.spines.values():
        sp.set_edgecolor("0.7")

    # ---------------------------------------------------------------- SCHRIFTFELD
    ax = fig.add_subplot(gs[0, 2])
    ax.axis("off")
    em = work.measures()
    v = DESIGN_SPEED.get(main["tags"]["highway"], 50.0)
    rows = [("Bauwerk", f"{case[0]}"),
            ("Gelaende", f"{case[1]}"),
            ("Achse", f"{main['tags']['highway']}, B = {main['tags']['width']:.1f} m"),
            ("Entwurfsgeschwindigkeit", f"{v:.0f} km/h"),
            ("Laengsneigung zulaessig", f"{GRADE_OF.get(main['tags']['highway'], 0.12) * 100:.0f} %"),
            ("Querneigung", f"Dach {SUPER_MIN * 100:.1f} %, max {SUPER_MAX * 100:.0f} %"),
            ("Boeschung", f"Damm 1:{BATTER_FILL}, Einschnitt 1:{BATTER_CUT}"),
            ("Wege / Knoten / Kreuzungen", f"{len(m.net.ways)} / {len(m.net.nodes)} / {len(m.junctions)}"),
            ("Damm h max (Rand)", f"{max(0.0, em.get('fill_max_m', 0.0)):.2f} m"),
            ("Einschnitt h max (Rand)", f"{max(0.0, em.get('cut_max_m', 0.0)):.2f} m"),
            ("Stuetzmauern", f"{em.get('walls', 0)}"),
            ("Boeschungsfuss max", f"{em.get('toe_max_m', 0):.1f} m"),
            ("Dreiecke", f"{len(mesh.tris)}"),
            ("Regelwerk", "RAL 2012, RAS-Q; Knoten netconvert")]
    ax.add_patch(plt.Rectangle((0.0, 0.0), 1.0, 1.0, transform=ax.transAxes,
                               facecolor="none", edgecolor=ink, lw=1.2))
    ax.text(0.04, 0.955, f"BLATT {number:02d}", fontsize=10, fontweight="bold", transform=ax.transAxes)
    for k, (what, value) in enumerate(rows):
        y = 0.90 - k * 0.062
        ax.text(0.04, y, what, fontsize=7, color="0.4", transform=ax.transAxes)
        ax.text(0.97, y, value, fontsize=7.5, color=ink, ha="right", transform=ax.transAxes)

    # ---------------------------------------------------------------- LAENGSSCHNITT
    ax = fig.add_subplot(gs[1, :])
    total = sum(m.stations[w["id"]][-1] for (w, _, _) in route)
    for (w, forward, at0) in route:
        sw = m.stations[w["id"]]
        lw = m.centreline(w)
        local = np.linspace(0, sw[-1], max(60, int(sw[-1])))
        xs_route = at0 + (local if forward else sw[-1] - local)
        grad = np.array([m.profile(w, float(v))[0] for v in local])
        dem = np.array([m.terrain.dem(*lw.interpolate(float(v)).coords[0]) for v in local])
        truth = np.array([m.terrain.truth(*lw.interpolate(float(v)).coords[0]) for v in local])
        order = np.argsort(xs_route)
        xs_route, grad, dem, truth = xs_route[order], grad[order], dem[order], truth[order]
        ax.plot(xs_route, truth, color="0.6", lw=0.9)
        ax.plot(xs_route, dem, color="0.35", lw=0.9, ls=":")
        if not (m.net.spans(w) or m.net.bores(w)):
            ax.fill_between(xs_route, dem, grad, where=grad > dem, facecolor="none", hatch="///",
                            edgecolor="0.62", lw=0.0)
            ax.fill_between(xs_route, dem, grad, where=grad < dem, facecolor="none", hatch="\\\\",
                            edgecolor="0.62", lw=0.0)
        style = (0, (8, 3)) if m.net.spans(w) else ((0, (2, 2)) if m.net.bores(w) else "-")
        ax.plot(xs_route, grad, color=ink, lw=2.2, ls=style)
        if m.net.spans(w) or m.net.bores(w):
            ax.annotate("BRUECKE" if m.net.spans(w) else "TUNNEL",
                        (xs_route.mean(), grad.mean()), fontsize=7.5, color=ink, ha="center",
                        xytext=(0, -26), textcoords="offset points", fontweight="bold")
            ax.annotate("", xy=(xs_route.min(), grad.mean()), xytext=(xs_route.max(), grad.mean()),
                        arrowprops=dict(arrowstyle="<|-|>", color=ink, lw=0.8))
            if m.net.spans(w):
                fields, field_m, what, depth, material, carrier = draw_bridge_elevation(
                    ax, xs_route, grad, np.minimum(dem, truth), ink,
                    rail=w["tags"].get("railway") is not None, water=m.terrain.water,
                    tags=w["tags"])
                ax.annotate(f"{what} ({material}), {fields} x {field_m:.0f} m, d = {depth:.2f} m",
                            (xs_route.mean(), grad.mean()), fontsize=6.5, color="0.3",
                            ha="center", xytext=(0, -38), textcoords="offset points")
            else:
                ax.annotate(f"L = {sw[-1]:.0f} m", (xs_route.mean(), grad.mean()), fontsize=6.5,
                            color="0.3", ha="center", xytext=(0, -38), textcoords="offset points")
        for station in np.arange(0, sw[-1] + 1, 100.0):
            z, g = m.profile(w, float(min(station, sw[-1])))
            x = at0 + (station if forward else sw[-1] - station)
            ax.annotate(f"{g * 100:+.1f}%", (x, z), fontsize=6.5, color="0.3",
                        xytext=(0, -13), textcoords="offset points", ha="center")
            ax.annotate(f"{z:.2f}", (x, z), fontsize=6, color="0.45",
                        xytext=(0, -24), textcoords="offset points", ha="center")
    ax.plot([], [], color="0.6", lw=0.9, label="Gelaende")
    ax.plot([], [], color="0.35", lw=0.9, ls=":", label="DEM")
    ax.plot([], [], color=ink, lw=2.2, label="Gradiente")
    if m.terrain.water is not None:
        ax.axhline(m.terrain.water, color="0.35", lw=0.9, ls="-.")
        ax.annotate("Wasserspiegel", (total * 0.02, m.terrain.water), fontsize=6.5, color="0.35",
                    xytext=(0, 4), textcoords="offset points")
    lo, hi = ax.get_ylim()
    if hi - lo < 6.0:
        mid = 0.5 * (lo + hi)
        lo, hi = mid - 3.0, mid + 3.0
    ax.set_ylim(lo, hi)
    ax.ticklabel_format(axis="y", style="plain", useOffset=False)
    fig.canvas.draw()
    bb = ax.get_window_extent()
    over = ((total / max(bb.width, 1e-6)) / ((hi - lo) / max(bb.height, 1e-6)))
    ax.set_title(f"LAENGSSCHNITT  Trasse ueber {len(route)} Abschnitt(e), L = {total:.0f} m   "
                 f"Ueberhoehung {over:.0f}:1   Damm ///  Einschnitt \\\\",
                 fontsize=10, loc="left", fontweight="bold")
    ax.set_xlabel("Station m (Trasse)", fontsize=8); ax.set_ylabel("m ue. NN", fontsize=8)
    ax.grid(alpha=0.22, lw=0.5); ax.legend(fontsize=7, loc="best", framealpha=0.92)
    ax.tick_params(labelsize=7)

    # ---------------------------------------------------------------- QUERSCHNITTE
    half = main["tags"]["width"] / 2.0
    # the stations a reviewer would ask for: at the junction and either side of it where there
    # is one, otherwise the quarter points
    s = m.stations[main["id"]]
    line = m.centreline(main)
    at_junction = [s[k] for k, r in enumerate(main["refs"]) if r in m.cluster_of]
    if at_junction:
        mid = at_junction[len(at_junction) // 2]
        picked = [(main, max(0.0, mid - 100.0)), (main, mid), (main, min(s[-1], mid + 100.0))]
    else:
        structure = next(((w) for (w, _, _) in route if m.net.spans(w) or m.net.bores(w)), None)
        ground_way = max((w for (w, _, _) in route if not (m.net.spans(w) or m.net.bores(w))),
                         key=lambda w: m.stations[w["id"]][-1], default=main)
        if structure is not None:
            ss = m.stations[structure["id"]]
            sg = m.stations[ground_way["id"]]
            # one section on the approach, one on the structure, one at its far end -- three
            # DIFFERENT places; taking the longest way twice drew the same section twice
            picked = [(ground_way, sg[-1] * 0.5), (structure, ss[-1] * 0.5),
                      (structure, ss[-1] * 0.9)]
        else:
            picked = [(main, s[-1] * 0.25), (main, s[-1] * 0.5), (main, s[-1] * 0.75)]
    for at, (main, station) in enumerate(picked):
        axq = fig.add_subplot(gs[2, at])
        station = float(station)
        half = main["tags"]["width"] / 2.0
        line = m.centreline(main)
        p = line.interpolate(station)
        ah = line.interpolate(min(station + 0.1, line.length))
        bk = line.interpolate(max(station - 0.1, 0.0))
        d = (ah.x - bk.x, ah.y - bk.y)
        n = math.hypot(*d) or 1.0
        nx, ny = -d[1] / n, d[0] / n
        reach = half + 14.0
        offs = np.linspace(-reach, reach, 260)
        gnd = np.array([m.terrain.filled(p.x + nx * o, p.y + ny * o) for o in offs])
        if m.net.spans(main) or m.net.bores(main):
            draw_structure_section(axq, m, st, main, station, offs, gnd, ink)
            continue
        surf = np.array([st.own_surface(main, station, float(o)) if abs(o) <= half else np.nan for o in offs])
        # the built section: carriageway, kerb, batters
        built = []
        for side in (-1, +1):
            edge = st.own_surface(main, station, side * half)
            g0 = m.terrain.filled(p.x + nx * side * half, p.y + ny * side * half)
            slope = BATTER_FILL if edge > g0 else BATTER_CUT
            toe = None
            for r in np.arange(0.1, 60.0, 0.1):
                q = (p.x + nx * (side * (half + r)), p.y + ny * (side * (half + r)))
                z = edge - (r / slope) * (1 if edge > g0 else -1)
                if (edge > g0 and z <= m.terrain.filled(*q)) or (edge <= g0 and z >= m.terrain.filled(*q)):
                    toe = (side * (half + r), z, r, slope, edge > g0)
                    break
            built.append((side, edge, toe))
        line_x = list(offs[np.abs(offs) <= half])
        line_z = [st.own_surface(main, station, float(o)) for o in line_x]
        poly_x = ([built[0][2][0]] if built[0][2] else [-half]) + line_x + ([built[1][2][0]] if built[1][2] else [half])
        poly_z = ([built[0][2][1]] if built[0][2] else [built[0][1]]) + line_z + ([built[1][2][1]] if built[1][2] else [built[1][1]])
        axq.fill_between(offs, gnd, np.interp(offs, poly_x, poly_z), where=np.interp(offs, poly_x, poly_z) > gnd,
                         facecolor="none", hatch="///", edgecolor="0.6", lw=0.0)
        axq.fill_between(offs, gnd, np.interp(offs, poly_x, poly_z), where=np.interp(offs, poly_x, poly_z) < gnd,
                         facecolor="none", hatch="\\\\", edgecolor="0.6", lw=0.0)
        axq.plot(offs, gnd, color="0.5", lw=1.0)
        axq.plot(poly_x, poly_z, color=ink, lw=1.4)
        axq.plot(line_x, line_z, color=ink, lw=2.6)
        for (side, edge, toe) in built:
            axq.plot([side * half, side * half], [edge, edge + KERB_M], color=ink, lw=2.0)
            if toe:
                axq.annotate(f"1:{toe[3]:.1f}", ((side * half + toe[0]) / 2, (edge + toe[1]) / 2),
                             fontsize=6.5, color="0.3", ha="center",
                             xytext=(0, 5 if toe[4] else -9), textcoords="offset points")
                # the two numbers a builder needs and a viewer confuses: the RISE at the edge
                # (how far the road stands above or below the ground THERE) and the batter's own
                # height and reach. The sheet said 1.31 and 3.53 for one section before this.
                rise = edge - m.terrain.filled(p.x + nx * side * half, p.y + ny * side * half)
                axq.annotate(f"{'Damm' if toe[4] else 'Einschnitt'} h {abs(rise):.2f} m\n"
                             f"Boeschung {abs(edge - toe[1]):.2f} m / {toe[2]:.1f} m",
                             (toe[0], toe[1]), fontsize=6, color="0.4",
                             xytext=(6 * side, -14), textcoords="offset points",
                             ha="left" if side > 0 else "right")
        axis_z = st.own_surface(main, station, 0.0)
        axq.annotate("", xy=(-half, axis_z + 1.6), xytext=(half, axis_z + 1.6),
                     arrowprops=dict(arrowstyle="<|-|>", color=ink, lw=0.7))
        axq.text(0, axis_z + 1.7, f"{main['tags']['width']:.2f} m", ha="center", fontsize=6.5, color=ink)
        left, right = m.superelevation(main, station)
        axq.set_title(f"QUERSCHNITT Sta {station:.0f}   q {left * 100:+.1f} / {right * 100:+.1f} %   "
                      f"Achse {axis_z:.2f}", fontsize=8, loc="left")
        axq.set_aspect("equal")
        axq.set_xlim(-reach, reach)
        axq.set_ylim(min(gnd.min(), min(poly_z)) - 1.0, max(gnd.max(), max(poly_z)) + 2.6)
        axq.grid(alpha=0.22, lw=0.5); axq.tick_params(labelsize=6.5)
        axq.set_xlabel("m von der Achse", fontsize=7)

    where = case[1].split("-", 1)[-1]
    fig.suptitle(f"BLATT {number:02d}   {case[0].split('-', 1)[-1].upper()}   auf   {where.upper()}   "
                 f"|   {main['tags']['highway']}, {DESIGN_SPEED.get(main['tags']['highway'], 50):.0f} km/h"
                 f"   |   Regelwerk RAL 2012 / RAS-Q",
                 fontsize=11.5, x=0.05, ha="left", fontweight="bold")
    out = OUT / f"{number:02d}_{case[0]}_{case[1]}.png"
    fig.savefig(out, dpi=110)
    plt.close(fig)
    return out


def main(argv):
    picked = [c for c in CASES if not argv or any(a in c[0] or a in c[1] for a in argv)]
    reds = 0
    for number, case in enumerate(picked, start=1):
        verdict, red = run(case, number)
        flag = "RED " + ",".join(red) if red else "ok"
        parts = []
        for k, v in verdict.items():
            if v is None:
                continue
            parts.append(f"{k} {v:.2e}" if isinstance(v, float) else f"{k} {v}")
        print(f"{case[0]:12s} {case[1]:12s} {flag:8s} " + "  ".join(parts))
        reds += bool(red)
    print(f"\n{len(picked)} cases, {reds} red; pictures under {OUT}")
    return 1 if reds else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
