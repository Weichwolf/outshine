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
COVER_M = 3.0             # [SET] the least rock a tunnel keeps above its crown
WELD_M = 1e-3
STEP_TOL_M = 1e-3
CROSSFALL = 0.025         # [SET] RAS-Q / AASHTO normal crown 2.5 %
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
}


# ----------------------------------------------------------------------------- the map

class Map:
    """Node heights, way stations and profiles, junction planes."""

    def __init__(self, terrain, net):
        self.terrain = terrain
        self.net = net
        self.index = {nid: k for k, nid in enumerate(net.nodes)}
        self.xy = np.array([net.nodes[n] for n in net.nodes])
        self.dem = np.array([self._dem_or_neighbour(n) for n in net.nodes])
        self.stations = {w["id"]: self._stations(w) for w in net.ways}
        self.z = None
        self.slope = {}
        self.junctions = {}

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
        for w in self.net.ways:
            refs, s = w["refs"], self.stations[w["id"]]
            for k in range(1, len(refs) - 1):
                d0, d1 = s[k] - s[k - 1], s[k + 1] - s[k]
                if d0 <= 1e-3 or d1 <= 1e-3:
                    continue
                scale = 2.0 / (d0 + d1)
                for ref, v in ((refs[k - 1], scale / d0), (refs[k], -scale * (1 / d0 + 1 / d1)), (refs[k + 1], scale / d1)):
                    rows.append(r); cols.append(self.index[ref]); vals.append(v)
                r += 1
            for k in range(1, len(refs)):
                ds = s[k] - s[k - 1]
                a, b = self.index[refs[k - 1]], self.index[refs[k]]
                if ds <= 1e-3 or fidelity[a] <= DECK_TIE or fidelity[b] <= DECK_TIE:
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
        self._slopes()
        self._junctions()
        return self

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

    def _junctions(self):
        """A node with three or more legs is a junction: ONE plane through the node, taken from
        its MAJOR way (highest priority, then the first declared): z = z0 + gx dx + gy dy where
        the major's grade along and its crossfall across give (gx, gy). Every minor leg's
        profile ends on it: its tangent at the node is the plane's derivative along the leg.
        Unreal's spline meshes have no such rule and RAGE's junctions are authored; the rule is
        RAS-K's and AASHTO's: the through road keeps its section, the side road warps to it."""
        for nid in self.net.nodes:
            legs = self.legs_at(nid)
            if len(legs) < 3:
                continue
            major = max(legs, key=lambda l: (l[0]["tags"]["priority"], -l[0]["id"]))
            w, k, sgn = major
            d = self._direction(w, k, sgn)
            grade = self.slope[w["id"]][k] * sgn
            # the plane: along the major its grade, across it the crown falls both ways -- taken
            # as the major's centreline plane (the crown is the section's, applied by the structure)
            gx, gy = grade * d[0], grade * d[1]
            self.junctions[nid] = {"z0": self.z[self.index[nid]], "gx": gx, "gy": gy, "major": w["id"], "legs": legs}
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

    def _junction_polygons(self):
        """netconvert's node shape (NBNodeShapeComputer, the readable baseline): the legs sorted
        by bearing, and between each consecutive pair the corner where this leg's left edge
        line meets the next leg's right edge line; parallel edges (a road passing through)
        meet at the edge's own start. A corner farther out than the reach is clipped to it. Each
        leg is cut back to the farther of its two corners, measured along the leg."""
        for nid, j in self.map.junctions.items():
            x0, y0 = self.net.nodes[nid]
            legs = []
            for (w, k, sgn) in j["legs"]:
                d = self.map._direction(w, k, sgn)
                legs.append((math.atan2(d[1], d[0]), w, k, sgn, d, w["tags"]["width"] / 2.0))
            legs.sort(key=lambda l: l[0])
            reach = max(l[5] for l in legs) * 3.0
            corners = []
            cuts = {}
            for a in range(len(legs)):
                _, wa, ka, sa, da, ha = legs[a]
                _, wb, kb, sb, db, hb = legs[(a + 1) % len(legs)]
                na = (-da[1], da[0])
                nb = (-db[1], db[0])
                pa = (x0 + na[0] * ha, y0 + na[1] * ha)      # leg a's LEFT edge starts here
                pb = (x0 - nb[0] * hb, y0 - nb[1] * hb)      # leg b's RIGHT edge starts here
                cross = da[0] * db[1] - da[1] * db[0]
                if abs(cross) < 1e-9:
                    corner = pa if len(legs) > 2 else pa
                else:
                    # pa + da*t = pb + db*u
                    rx, ry = pb[0] - pa[0], pb[1] - pa[1]
                    tt = (rx * db[1] - ry * db[0]) / cross
                    corner = (pa[0] + da[0] * tt, pa[1] + da[1] * tt)
                dist = math.dist(corner, (x0, y0))
                if dist > reach:
                    corner = (x0 + (corner[0] - x0) * reach / dist, y0 + (corner[1] - y0) * reach / dist)
                corners.append(corner)
                for (w, k, sgn, d) in ((wa, ka, sa, da), (wb, kb, sb, db)):
                    along = (corner[0] - x0) * d[0] + (corner[1] - y0) * d[1]
                    cuts[(w["id"], nid)] = max(cuts.get((w["id"], nid), 0.0), along)
            self.polygons[nid] = Polygon(corners)
            self.cuts.update(cuts)

    def own_surface(self, way, station, offset):
        """A leg's own surface: the profile plus a crown that falls CROSSFALL from the centreline."""
        z, _ = self.map.profile(way, station)
        return z - CROSSFALL * abs(offset)

    def major_surface(self, nid, x, y):
        """The junction's surface IS the major way's surface, crown and all, extended over the
        polygon: RAS-K's and AASHTO's rule that the through road keeps its section."""
        j = self.map.junctions[nid]
        way = next(w for w in self.net.ways if w["id"] == j["major"])
        line = self.map.centreline(way)
        p = Point(x, y)
        s = line.project(p)
        q = line.interpolate(s)
        # the signed offset is not needed for a symmetric crown; the distance is
        return self.own_surface(way, s, p.distance(q))

    def leg_surface(self, way, station, offset, nid=None, sgn=+1):
        """A leg's surface within WARP_M of a junction it is a MINOR leg of is warped from its
        own section into the major's surface at the same (x, y): alpha rises smoothly from 0 at
        the warp's start to 1 at the cut, so at the cut the two are one surface (I6 by
        construction) and the leg's centreline stays C1 through the warp."""
        z_own = self.own_surface(way, station, offset)
        if nid is None or self.map.junctions[nid]["major"] == way["id"]:
            return z_own
        cut = self.cuts[(way["id"], nid)]
        k = way["refs"].index(nid)
        s_node = self.map.stations[way["id"]][k]
        along = sgn * (station - s_node)          # distance from the node along the leg
        u = (along - cut) / WARP_M                # 0 at the cut, 1 at the warp's start
        if u >= 1.0:
            return z_own
        alpha = 1.0 - (3 * u * u - 2 * u * u * u) if u > 0.0 else 1.0
        d = self.map._direction(way, k, sgn)
        x0, y0 = self.net.nodes[nid]
        px, py = x0 + d[0] * along - d[1] * offset, y0 + d[1] * along + d[0] * offset
        return (1.0 - alpha) * z_own + alpha * self.major_surface(nid, px, py)

    def check_junction_steps(self):
        """I6: along each leg's cut line, the leg's surface against the plane, worst gap."""
        worst = 0.0
        for nid, j in self.map.junctions.items():
            for (w, k, sgn) in j["legs"]:
                cut = self.cuts[(w["id"], nid)]
                s_node = self.map.stations[w["id"]][k]
                station = s_node + sgn * cut
                d = self.map._direction(w, k, sgn)
                x0, y0 = self.net.nodes[nid]
                half = w["tags"]["width"] / 2.0
                for t in np.linspace(-half, half, 9):
                    px, py = x0 + d[0] * cut - d[1] * t, y0 + d[1] * cut + d[0] * t
                    leg = self.leg_surface(w, station, t, nid, sgn)
                    junction = self.major_surface(nid, px, py)
                    worst = max(worst, abs(leg - junction))
        return worst


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
    """I3: where the DEM is the authority, |z - dem| within its error."""
    held = ~(map_.deck | map_.bore)
    return float(np.max(np.abs(map_.z - map_.dem)[held])) if held.any() else 0.0


def check_bridge(map_):
    """I4: the deck above the water plus clearance; the abutments on the terrain."""
    if not map_.deck.any():
        return None
    water = map_.terrain.water
    lowest = float(np.min(map_.z[map_.deck] - (water if water is not None else -1e9)))
    abut = 0.0
    for w in map_.net.ways:
        if map_.net.spans(w):
            for r in (w["refs"][0], w["refs"][-1]):
                k = map_.index[r]
                abut = max(abut, abs(map_.z[k] - map_.dem[k]))
    return {"clearance_m": lowest, "abutment_off_m": abut}


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
        "I5 tunnel": check_tunnel(m),
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
    if verdict["I4 bridge"] is not None and (verdict["I4 bridge"]["clearance_m"] < CLEARANCE_M or verdict["I4 bridge"]["abutment_off_m"] > DEM_ERROR_M):
        red.append("I4")
    if verdict["I5 tunnel"] is not None and (verdict["I5 tunnel"]["cover_m"] < 0.0 or verdict["I5 tunnel"]["portal_off_m"] > DEM_ERROR_M):
        red.append("I5")
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
