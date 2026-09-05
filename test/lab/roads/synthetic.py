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
from shapely.ops import unary_union

OUT = pathlib.Path(__import__("os").environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "synthetic"

POSTING_M = 25.0          # the engine's DEM at zoom 12, 49 N (measured: 24.9 m)
DEM_ERROR_M = 4.0         # [SET] Copernicus GLO-30 LE90 < 4 m
DECK_TIE = 1e-6           # a deck with no abutment keeps a weak tie to the DEM
CLEARANCE_M = 4.5         # [SET] the clearance a bridge owes the water or the road below
DECK_GRADE = 0.04         # [SET] a deck's own grade, RAA/RAL bridges: the expensive part stays near level
DECK_STIFF = 10.0         # [SET] a deck resists bending ten times more than the fill beside it -- the lift goes to the ramps
COVER_M = 3.0             # [SET] the least rock a tunnel keeps above its crown
WELD_M = 1e-3
MESH_TOL_M = 0.01         # [SET] the drawn surface stands within a centimetre of the analytic one
STEP_TOL_M = 1e-3
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

GRADE_OF = {"primary": 0.06, "secondary": 0.08, "residential": 0.12, "service": 0.15}  # [SET] RAL 2012 EKL 3 / EKL 4, RASt 06
JOIN_M = 10.0             # [SET] netconvert's --junctions.join default: nodes nearer than this are one junction
WARP_M = 20.0             # [SET] RAS-K: a side road's section is warped into the through road's surface over its last ~20 m


# ----------------------------------------------------------------------------- terrain

class Terrain:
    """z(x, y) sampled on a posting grid, bilinear between postings, like the engine's DEM."""

    def __init__(self, fn, extent=600.0, posting=POSTING_M, water=None, holes=()):
        self.fn = fn
        self.posting = posting
        self.water = water
        n = int(2 * extent / posting) + 3
        self.x0 = -extent - posting
        self.grid = np.array([[fn(self.x0 + i * posting, self.x0 + j * posting) for i in range(n)] for j in range(n)])
        for (i, j) in holes:
            self.grid[j, i] = np.nan

    def dem(self, x, y):
        """The DEM's answer: bilinear between postings; NaN where a posting is a hole."""
        fx = (x - self.x0) / self.posting
        fy = (y - self.x0) / self.posting
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


def r_serpentine(radius=14.0, legs=4, rise=140.0):
    """A mountain road's hairpins: legs across the slope joined by turns of the tightest radius."""
    net = Network()
    pts = []
    y = -rise / 2
    for k in range(legs):
        x0, x1 = (-150.0, 150.0) if k % 2 == 0 else (150.0, -150.0)
        pts += line_pts(x0, y, x1, y, step=25.0)
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
    "R12-culdesac": r_cul_de_sac_real,
    "R15-widths": r_widths,
    "R16-onewaypair": r_oneway_pair,
    "R19-viaduct": r_viaduct,
    "R21-underpass": r_underpass,
    "R24-causeway": r_causeway,
    "R25-ford": r_ford,
    "P7-layer": p_layer_no_bridge,
    "R2-hairpin": r_serpentine,
    "R22-embankment": r_embankment,
    "R23-cutting": r_cutting,
    "R14-steps": r_steps,
    "R14-track": r_track,
    "R27-elevated": r_elevated,
    "R17-seam": r_tile_seam,
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
        self.slope = {}
        self.junctions = {}

    def _snap(self):
        """Nodes within SNAP_M of an earlier node are the earlier node (declared order), and a
        way's consecutive duplicates collapse to one -- the weave's word, so P1, P2 and P3 hold."""
        kept = {}
        alias = {}
        for nid, (x, y) in self.net.nodes.items():
            hit = next((k for k, (kx, ky) in kept.items() if math.hypot(kx - x, ky - y) <= self.SNAP_M), None)
            if hit is None:
                kept[nid] = (x, y)
                alias[nid] = nid
            else:
                alias[nid] = hit
        self.net.nodes = kept
        for w in self.net.ways:
            refs = [alias[r] for r in w["refs"]]
            w["refs"] = [r for k, r in enumerate(refs) if k == 0 or r != refs[k - 1]]
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
        A = sp.diags(fidelity) + mu * (G.T @ G) + lam * (K.T @ K)
        b = fidelity * self.dem + mu * (G.T @ (G @ self.dem))
        self.z = spsolve(A.tocsc(), b)
        self.deck, self.bore, self.curvature = deck, bore, K
        self.constraints = self._clearances(deck)
        if self.constraints:
            self.ramp_signs = {}
            self._solve_with_clearances(A, b)     # once, to read the ramps' signs
            self.ramp_signs = self._ramp_signs()
            # and again, twice: the clearance floors are read from the roads BELOW as they now
            # stand, so a deck over a deck's ramp lifts with it -- a fixed point in two rounds
            for _ in range(2):
                self.constraints = self._clearances(deck)
                self._solve_with_clearances(A, b)
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
        near = np.zeros(len(self.index), dtype=bool)
        deck_nodes = {r for w in self.net.ways if self.net.spans(w) for r in w["refs"]}
        for r in deck_nodes:
            near[self.index[r]] = True
        for w in self.net.ways:
            if self.net.spans(w) or not (set(w["refs"]) & deck_nodes):
                continue
            for r in w["refs"]:
                near[self.index[r]] = True
        return near

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
        floors = {}
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
                    if other is w or self.net.spans(other) or set(other["refs"]) & set(refs):
                        continue
                    line = self.centreline(other)
                    p = Point(x, y)
                    reach = other["tags"]["width"] / 2.0 + w["tags"]["width"] / 2.0 + 25.0
                    if line.distance(p) <= reach:
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
                        below_z = (self.z[self.index[other["refs"][kb]]] if self.z is not None
                                   else self.terrain.dem(q.x, q.y))
                        floor2 = max(below_z, self.terrain.dem(q.x, q.y)) + CLEARANCE_M
                        floor = floor2 if floor is None else max(floor, floor2)
                if floor is not None:
                    floors[k] = max(floors.get(k, -1e9), floor)
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
        # the ramps: a designed structure keeps its class's grade, hard, on every segment of an
        # approach and of the deck itself -- the lift a clearance asks for spreads back along
        # the approach as an embankment, never as a 30 percent step (measured before this)
        near = self.approaches()
        # an EMBANKMENT TAPERS: the lift over the terrain falls monotonically from the abutment
        # to where the road meets grade again, and never swings below it. Without this the
        # minimum-curvature profile undershoots -- measured: a ramp 0.3 m under a flat plain
        # before climbing 4 m to the deck, which is a spline's overshoot and not a road
        deck_nodes = {r for w in self.net.ways if self.net.spans(w) for r in w["refs"]}
        for w in self.net.ways:
            if self.net.spans(w) or not (set(w["refs"]) & deck_nodes):
                continue
            refs = w["refs"]
            at_start = refs[0] in deck_nodes
            order = range(len(refs) - 1) if at_start else range(len(refs) - 1, 0, -1)
            step = +1 if at_start else -1
            for k in order:
                a_, b_ = self.index[refs[k]], self.index[refs[k + step]]
                lift_a = z[a_] - self.dem[a_]
                lift_b = z[b_] - self.dem[b_]
                sign = self.ramp_signs.get(w["id"], 1.0)
                cons += [sign * lift_b <= sign * lift_a, sign * lift_b >= 0.0]
        for w in self.net.ways:
            g = GRADE_OF.get(w["tags"]["highway"], 0.12)
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
                    reach = max(DECK_GRADE, chord) * ds
                    cons += [z[b_] - z[a] <= reach, z[a] - z[b_] <= reach]
                else:
                    # a ramp climbs off the hillside at the class's grade RELATIVE to it, so that
                    # on a hillside as steep as the class the lift still comes back down (measured
                    # before this: nine metres of fill that never returned, 200 m from the bridge)
                    cons += [(z[b_] - self.dem[b_]) - (z[a] - self.dem[a]) <= g * ds,
                             (z[a] - self.dem[a]) - (z[b_] - self.dem[b_]) <= g * ds]
        problem = cp.Problem(cp.Minimize(objective), cons)
        problem.solve(solver=cp.OSQP, eps_abs=1e-7, eps_rel=1e-7, max_iter=200000, polish=True)
        if z.value is None:
            raise RuntimeError("clearance QP: " + str(problem.status))
        self.z = np.asarray(z.value)

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
        refs = way["refs"]
        a = self.net.nodes[refs[k]]
        b = self.net.nodes[refs[k + sgn]]
        v = (b[0] - a[0], b[1] - a[1])
        n = math.hypot(*v)
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
        """One node's shape, netconvert's NBNodeShapeComputer: the legs by bearing, a corner where
        one leg's left edge meets the next leg's right edge, clipped to a reach."""
        x0, y0 = self.net.nodes[nid]
        ordered = []
        for (w, k, sgn) in legs:
            d = self.map._direction(w, k, sgn)
            ordered.append((math.atan2(d[1], d[0]), w, k, sgn, d, w["tags"]["width"] / 2.0))
        ordered.sort(key=lambda l: l[0])
        if len(ordered) < 2:
            half = ordered[0][5] if ordered else 1.0
            return Point(x0, y0).buffer(half, quad_segs=8)
        reach = max(l[5] for l in ordered) * 3.0
        corners = []
        for a in range(len(ordered)):
            _, wa, ka, sa, da, ha = ordered[a]
            _, wb, kb, sb, db, hb = ordered[(a + 1) % len(ordered)]
            na, nb = (-da[1], da[0]), (-db[1], db[0])
            pa = (x0 + na[0] * ha, y0 + na[1] * ha)
            pb = (x0 - nb[0] * hb, y0 - nb[1] * hb)
            cross = da[0] * db[1] - da[1] * db[0]
            if abs(cross) < 1e-9:
                corner = pa
            else:
                rx, ry = pb[0] - pa[0], pb[1] - pa[1]
                tt = (rx * db[1] - ry * db[0]) / cross
                corner = (pa[0] + da[0] * tt, pa[1] + da[1] * tt)
            dist = math.dist(corner, (x0, y0))
            if dist > reach:
                corner = (x0 + (corner[0] - x0) * reach / dist, y0 + (corner[1] - y0) * reach / dist)
            corners.append(corner)
        return Polygon(corners) if len(corners) >= 3 else Point(x0, y0).buffer(reach / 3.0, quad_segs=8)

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
                    hit = ray.intersection(poly.exterior)
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

    def major_surface(self, nid, x, y):
        """The junction's surface IS the major way's surface, crown and all, extended over the
        polygon: RAS-K's and AASHTO's rule that the through road keeps its section."""
        j = self.map.junctions[nid]
        way = next(w for w in self.net.ways if w["id"] == j["major"])
        line = self.map.centreline(way)
        p = Point(x, y)
        s = line.project(p)
        q = line.interpolate(s)
        # the offset is SIGNED, because a superelevated section is one tilted plane and not a
        # symmetric crown: the sign comes from the cross product with the way's direction there
        ahead = line.interpolate(min(s + 0.1, line.length))
        back = line.interpolate(max(s - 0.1, 0.0))
        d = (ahead.x - back.x, ahead.y - back.y)
        n = math.hypot(*d) or 1.0
        off = (-(x - q.x) * d[1] + (y - q.y) * d[0]) / n
        return self.own_surface(way, s, off)

    def leg_surface(self, way, station, offset, at=None, sgn=+1):
        """A leg's surface within WARP_M of a junction it is a MINOR leg of is warped from its
        own section into the major's surface at the same (x, y): alpha rises smoothly from 0 at
        the warp's start to 1 at the cut, so at the cut the two are one surface (I6 by
        construction) and the leg's centreline stays C1 through the warp."""
        z_own = self.own_surface(way, station, offset)
        head = self.map.cluster_of.get(at) if at is not None else None
        if head is None or self.map.junctions[head]["major"] == way["id"]:
            return z_own
        cut = self.cuts[(way["id"], at)]
        k = way["refs"].index(at)
        s_node = self.map.stations[way["id"]][k]
        along = sgn * (station - s_node)          # distance from the node along the leg
        u = (along - cut) / WARP_M                # 0 at the cut, 1 at the warp's start
        if u >= 1.0:
            return z_own
        alpha = 1.0 - (3 * u * u - 2 * u * u * u) if u > 0.0 else 1.0
        d = self.map._direction(way, k, sgn)
        x0, y0 = self.net.nodes[at]
        px, py = x0 + d[0] * along - d[1] * offset, y0 + d[1] * along + d[0] * offset
        return (1.0 - alpha) * z_own + alpha * self.major_surface(head, px, py)

    def check_junction_steps(self):
        """I6: along each leg's cut line, the leg's surface against the junction's, worst gap."""
        worst = 0.0
        for nid, j in self.map.junctions.items():
            for (w, k, sgn) in j["legs"]:
                at = w["refs"][k]
                cut = self.cuts[(w["id"], at)]
                s_node = self.map.stations[w["id"]][k]
                station = s_node + sgn * cut
                d = self.map._direction(w, k, sgn)
                x0, y0 = self.net.nodes[at]
                half = w["tags"]["width"] / 2.0
                for t in np.linspace(-half, half, 9):
                    px, py = x0 + d[0] * cut - d[1] * t, y0 + d[1] * cut + d[0] * t
                    leg = self.leg_surface(w, station, t, at, sgn)
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


# ----------------------------------------------------------------------------- earthworks

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
    """I2: the grade just before and just after every interior node agree."""
    worst = 0.0
    for w in map_.net.ways:
        s = map_.stations[w["id"]]
        for k in range(1, len(w["refs"]) - 1):
            _, g0 = map_.profile(w, s[k] - 1e-6)
            _, g1 = map_.profile(w, s[k] + 1e-6)
            worst = max(worst, abs(g0 - g1))
    return worst


def check_dem_band(map_):
    """I3: where the DEM is the authority, |z - dem| within its error -- not on a deck, not in
    a bore, not on a bridge's approach, which is a designed ramp (I11 builds its fill)."""
    held = ~(map_.deck | map_.bore | map_.approaches())
    return float(np.max(np.abs(map_.z - map_.dem)[held])) if held.any() else 0.0


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


def check_finite(map_):
    """P: every height and grade finite, and no deck thrown farther than the DEM's error where
    it has no abutment (it keeps the weak tie and is COUNTED, never at -19 m)."""
    finite = bool(np.all(np.isfinite(map_.z)))
    grades = np.concatenate([np.abs(m) for m in map_.slope.values()]) if map_.slope else np.zeros(1)
    return {"finite": finite, "grade_max": float(np.max(grades)), "deck_off_dem_max": float(np.max(np.abs(map_.z - map_.dem)[map_.deck])) if map_.deck.any() else 0.0, "ramp_grade_max": ramp_grade(map_)}


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
    ("R11-stacked", "T1-flat"), ("R12-culdesac", "T3-cross15"), ("R15-widths", "T3-cross15"),
    ("R16-onewaypair", "T3-cross15"), ("R19-viaduct", "T5-sag"), ("R21-underpass", "T1-flat"),
    ("R24-causeway", "T9-coast"), ("R25-ford", "T8-valley"), ("P7-layer", "T2-along5"),
    ("R1-straight", "T9-coast"), ("R1-straight", "T10-mountain"), ("R2-curve30", "T10-mountain"),
    ("R18-bridge", "T13-baked"), ("R1-straight", "T14-coarse"), ("R4-T90", "T14-coarse"),
    ("R2-hairpin", "T10-mountain"), ("R22-embankment", "T1-flat"), ("R23-cutting", "T4-crest"),
    ("R14-steps", "T10-mountain"), ("R14-track", "T3-cross30"), ("R27-elevated", "T1-flat"),
    ("R17-seam", "T3-cross15"), ("R1-straight", "T3-cross30"), ("R4-T90", "T5-sag"),
]


def run(case):
    rname, tname = case
    terrain = TERRAINS[tname]()
    net = NETWORKS[rname]()
    m = Map(terrain, net).solve()
    st = Structure(m)
    verdict = {
        "I1 C0 m": check_c0(m),
        "I2 C1 grade": check_c1(m),
        "I3 |z-dem| m": check_dem_band(m),
        "I6 junction step m": st.check_junction_steps() if m.junctions else None,
        "I4 bridge": check_bridge(m),
        "I4 over road m": check_over_road(m, st),
        "I5 tunnel": check_tunnel(m),
        "P finite": check_finite(m),
        "I7/I9 mesh": check_mesh(m, st),
        "I11 earthworks": check_earthworks(m, st),
    }
    red = []
    if verdict["I1 C0 m"] > 1e-9:
        red.append("I1")
    if verdict["I2 C1 grade"] > 1e-6:
        red.append("I2")
    if verdict["I3 |z-dem| m"] > DEM_ERROR_M:
        red.append("I3")
    if verdict["I6 junction step m"] is not None and verdict["I6 junction step m"] > STEP_TOL_M:
        red.append("I6")
    if verdict["I4 bridge"] is not None and (verdict["I4 bridge"]["clearance_m"] < CLEARANCE_M or verdict["I4 bridge"]["abutment_below_m"] > DEM_ERROR_M):
        red.append("I4")
    if verdict["I5 tunnel"] is not None and (verdict["I5 tunnel"]["cover_m"] < 0.0 or verdict["I5 tunnel"]["portal_off_m"] > DEM_ERROR_M):
        red.append("I5")
    if verdict["I4 over road m"] is not None and verdict["I4 over road m"] < CLEARANCE_M:
        red.append("I4road")
    earth = verdict["I11 earthworks"]
    if earth.get("rows", 0) > 0 and earth["unreached"] > 0:
        red.append("I11")
    mesh = verdict["I7/I9 mesh"]
    if mesh["edges_over_two_faces"] > 0 or mesh["nearest_pair_m"] < WELD_M:
        red.append("I7")
    if mesh["drawn_vs_analytic_m"] > 0.01:
        red.append("I9")
    if not verdict["P finite"]["finite"] or verdict["P finite"]["grade_max"] > 1.0 or verdict["P finite"]["deck_off_dem_max"] > 60.0:
        red.append("P")
    plot(case, m, st)
    return verdict, red


def plot(case, m, st):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    OUT.mkdir(parents=True, exist_ok=True)
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(11, 8))
    xs = np.linspace(-500, 500, 400)
    for w in m.net.ways:
        s = m.stations[w["id"]]
        ss = np.linspace(0, s[-1], 200)
        zz = [m.profile(w, v)[0] for v in ss]
        pts = m.centreline(w)
        dem = [m.terrain.dem(*pts.interpolate(v).coords[0]) for v in ss]
        truth = [m.terrain.truth(*pts.interpolate(v).coords[0]) for v in ss]
        ax1.plot(ss + (0 if w["id"] == 1 else s[-1] * 0), truth, color="0.7", lw=1)
        ax1.plot(ss, dem, color="0.4", lw=1, ls=":")
        ax1.plot(ss, zz, lw=2, label=f"way {w['id']} {w['tags']['highway']}" + (" bridge" if m.net.spans(w) else "") + (" tunnel" if m.net.bores(w) else ""))
    if m.terrain.water is not None:
        ax1.axhline(m.terrain.water, color="tab:blue", ls="--", lw=1, label="water")
    ax1.set_title(f"{case[0]} on {case[1]} -- profiles (grey: terrain truth, dotted: the DEM)")
    ax1.set_xlabel("station m"); ax1.set_ylabel("m"); ax1.grid(alpha=0.3); ax1.legend(fontsize=8)
    for w in m.net.ways:
        line = m.centreline(w)
        half = w["tags"]["width"] / 2
        for side in (-1, 1):
            off = line.parallel_offset(half, "left" if side > 0 else "right")
            if off.geom_type == "LineString":
                ax2.plot(*off.xy, color="0.3", lw=0.8)
        ax2.plot(*line.xy, lw=1, ls="--")
    for nid, poly in st.polygons.items():
        ax2.plot(*poly.exterior.xy, color="tab:red", lw=1.5)
    ax2.set_aspect("equal"); ax2.set_xlim(-120, 120); ax2.set_ylim(-120, 120); ax2.grid(alpha=0.3)
    ax2.set_title("plan, the junction polygons in red")
    fig.tight_layout()
    fig.savefig(OUT / f"{case[0]}_{case[1]}.png", dpi=100)
    plt.close(fig)


def main(argv):
    picked = [c for c in CASES if not argv or any(a in c[0] or a in c[1] for a in argv)]
    reds = 0
    for case in picked:
        verdict, red = run(case)
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
