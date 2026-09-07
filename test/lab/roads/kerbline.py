"""THE KERB AS ONE RING AROUND THE NETWORK, never as two strips per way.

netconvert computes a junction's shape from the extended lane boundaries of its edges and CARLA
walls and pavements the result. Taken per WAY instead, the same construction lays every way's
footway across every carriageway it meets: measured here at a T-junction, the minor leg's 2.50 m
footway lay over the whole 10 m major carriageway, and the major's kerb ran unbroken across the
minor's mouth -- a wall where the turn is. The area is the thing, and the area is built once:

    drivable   every carriageway ribbon and every junction polygon, unioned
    channel    drivable, widened by the gutter
    face       channel CLOSED with the corner radius -- buffer(+R) then buffer(-R), round joins,
               which fills exactly the concave corners a junction has and touches nothing else
    top        face widened by the kerbstone, less the face
    walk       top widened by the clear width, less the top

RASt 06 sets the corner radius: 6 m where two residential streets meet, 8 m where a bus or a
lorry has to turn in, and the wider leg decides. The closing is done per junction on its own
window, so a radius is a junction's property and not the network's.

The rings are triangulated as polygons WITH HOLES -- the holes are the carriageway -- so a
footway has no seam and a kerb no overlap, however many ways meet.
"""
import math

import numpy as np
from shapely.geometry import LineString, Point, Polygon
from shapely.ops import unary_union
from shapely.strtree import STRtree

KERB_UP_M = 0.12          # [SET] RASt 06: the upstand at a carriageway edge
KERB_DROP_M = 0.03        # [SET] RASt 06: and what it drops to at a crossing
DROP_RAMP_M = 1.5         # [SET] the run the kerb and the footway take to get back up
KERB_WIDE_M = 0.30        # [SET] the kerbstone's own width
GUTTER_M = 0.30           # [SET] the channel laid flat against it
WALK_M = 2.50             # [SET] RASt 06's minimum clear width for a footway
WALK_FALL = 0.025         # [SET] RASt 06: a footway falls 2.5 % toward the kerb
R_SMALL_M = 6.0           # [SET] RASt 06: the corner radius where two residential streets meet
R_LARGE_M = 8.0           # [SET] RASt 06: where a bus or a lorry turns in
FINE = "pq30a3.0"         # [SET] Shewchuk: 30 deg minimum angle, 3 m2 maximum area
WALK_FINE = "pq30a3.0"    # [SET] and the footway is graded by SEEDS where a dropped kerb ramps
ARC = 16                  # [SET] chords per quarter circle: at 8 the 8 m corner read as a
                          # polygon, at 16 an 8 m radius is 9.5 mm off its own arc
OUTLINE_M = 0.05          # [SET] how far the terrain's cut may leave the footway's own edge
LARGE = ("primary", "primary_link", "secondary", "secondary_link", "tertiary", "tertiary_link")

# what has a kerb at all: a motorway has a hard shoulder, a footway IS the walk, a rail has ballast
DRIVABLE = ("primary", "secondary", "tertiary", "residential", "unclassified", "living_street",
            "primary_link", "secondary_link", "tertiary_link", "service", "road", "pedestrian")


def _rings(poly):
    """Every (exterior, interiors) of a polygon or a multipolygon."""
    if poly.is_empty:
        return
    for part in ([poly] if poly.geom_type == "Polygon" else poly.geoms):
        if part.geom_type == "Polygon" and part.area > 1e-9:
            yield part


def triangulate(poly, z_of, opts="p", seeds=()):
    """A polygon with holes, as triangles at the height the surface gives each vertex.

    `opts` reaches Shewchuk's switches: a footway is not flat -- it falls 2.5 % to the kerb --
    so a bare CDT gives it triangles long enough to fold the fall into visible creases along
    every corner (rendered in plan and looked at). A quality mesh with a size bound has none."""
    import triangle as tri
    # ONE INDEX, AS IN `ground.py`, AND FOR THE SAME REASON: a point snapped to a millimetre
    # appears once and a segment is an unordered pair of indices. Built ring by ring without it,
    # the footway of a real extract handed Shewchuk the same point twice wherever two buffers
    # touched, and `triangle` answered with SIGNAL 11 (measured 2026-09-07 on OldTown).
    index, pts, bars, holes = {}, [], set(), []

    def point(x, y):
        key = (round(float(x), 3), round(float(y), 3))
        at = index.get(key)
        if at is None:
            at = len(pts)
            index[key] = at
            pts.append(key)
        return at

    for part in _rings(poly):
        for ring, is_hole in [(part.exterior, False)] + [(r, True) for r in part.interiors]:
            coords = list(ring.coords)[:-1]
            if len(coords) < 3:
                continue
            got = [point(x, y) for (x, y) in coords]
            n = len(got)
            for i in range(n):
                u, v = got[i], got[(i + 1) % n]
                if u != v:
                    bars.add((min(u, v), max(u, v)))
            if is_hole:
                q = Polygon(ring).representative_point()
                holes.append((q.x, q.y))
    segs = sorted(bars)
    if len(pts) < 3:
        return [], []
    # A GRADED MESH, and the seeds are where the grading is needed: a uniform quality mesh fine
    # enough for a dropped kerb's 1.5 m ramp put 201 268 triangles in one roundabout's footway.
    pts = pts + [(float(x), float(y)) for (x, y) in seeds]
    spec = {"vertices": np.array(pts, dtype=float), "segments": np.array(segs, dtype=np.int32)}
    if holes:
        spec["holes"] = np.array(holes, dtype=float)
    got = tri.triangulate(spec, opts)
    if "triangles" not in got:
        return [], []
    verts = [(float(x), float(y), z_of(float(x), float(y))) for (x, y) in got["vertices"]]
    tris = []
    for (a, b, c) in got["triangles"]:
        p, q, r = (np.asarray(verts[i]) for i in (a, b, c))
        up = float(np.cross(q - p, r - p)[2])
        tris.append((int(a), int(b), int(c)) if up > 0.0 else (int(a), int(c), int(b)))
    return verts, tris


def extrude(poly, z_of, rise):
    # `rise` may be a number or a field of (x, y) -- a dropped kerb is the second
    """The boundary of a polygon as a vertical band -- the kerb's FACE, the one vertical surface
    a street has and the only thing that casts a shadow along its whole length."""
    verts, tris = [], []
    for part in _rings(poly):
        for ring, outward in [(part.exterior, True)] + [(r, False) for r in part.interiors]:
            coords = list(ring.coords)
            if len(coords) < 3:
                continue
            # shapely orients an exterior CCW and an interior CW when normalised; the face has to
            # look AT the carriageway, which is inside the exterior and outside an interior
            base = len(verts)
            up = rise if callable(rise) else (lambda x, y: rise)
            for (x, y) in coords:
                z = z_of(float(x), float(y))
                verts += [(float(x), float(y), z),
                          (float(x), float(y), z + up(float(x), float(y)))]
            n = len(coords)
            for k in range(n - 1):
                i, j = base + 2 * k, base + 2 * (k + 1)
                if outward:
                    tris += [(i, j + 1, j), (i, i + 1, j + 1)]
                else:
                    tris += [(i, j, j + 1), (i, j + 1, i + 1)]
    return verts, tris


class Surface:
    """THE HEIGHT OF THE DRAWN CARRIAGEWAY AT A POINT -- read off the MESH and never recomputed.

    A kerb, a marking and a gully all sit on the road, so all three have to read the same surface
    the road is DRAWN with. Recomputed from the bed's own section instead, they missed the warp a
    minor leg carries into a junction's plane and a zebra stood 0.12 m proud of the asphalt it was
    painted on -- which Cycles drew as four rows of kerbstones (looked at, 2026-09-06). One source
    per rule: the mesh is the source, and everything laid on the road interpolates it.

    NO GEOS ON THIS PATH. It is called once per vertex of every ring in the network, and a
    shapely `Point` plus an STRtree query per call put GEOS at the top of a place twin's profile.
    The candidates come from one k-d tree of triangle CENTROIDS and the containment test is three
    cross products in numpy -- the same answer, without an allocation.

    A point OFF the carriageway (a footway, the far side of a kerb) takes the height of the
    nearest points ON it, blended: at a corner fillet the nearest leg SWITCHES and a single
    nearest jumps by the difference of the two crowns, which read as a crease and a bright wedge
    along every corner."""

    REACH_M = 60.0            # [SET] beyond this the road is not what the ground does

    def __init__(self, mesh, z_at, blend=8, edge=None):
        from scipy.spatial import cKDTree
        self.z_at, self.blend = z_at, blend
        # OFF THE CARRIAGEWAY THE HEIGHT COMES FROM THE CARRIAGEWAY'S OWN EDGE, and this is the
        # whole of board:2160. Blending the eight nearest TRIANGLES was put there to stop a corner
        # fillet jumping between two legs' crowns, and on a hill those eight belong to ways metres
        # apart: measured 2026-09-07 on Rothenburg, the footway's 99th-percentile slope was
        # 229.8 % and its worst face 6 304 % against RASt 06's 6 %. A kerb sits on the EDGE of a
        # road, so it reads the edge -- one curve, smooth along its own length, and no way that
        # happens to pass nearby can reach it.
        self.rail = None
        if edge is not None and not edge.is_empty:
            seeds = []
            for part in (edge.geoms if hasattr(edge, "geoms") else [edge]):
                n = max(2, int(part.length / 1.0))
                seeds += [(q.x, q.y) for q in (part.interpolate(part.length * i / n)
                                               for i in range(n + 1))]
            if seeds:
                self.rail = cKDTree(np.asarray(seeds))
                self.rail_at = np.asarray(seeds)
        tri = np.asarray([[mesh.vertices[i] for i in t] for t in mesh.tris], dtype=float) \
            if mesh.tris else np.zeros((0, 3, 3))
        self.a, self.b, self.c = tri[:, 0, :], tri[:, 1, :], tri[:, 2, :]
        mid = tri.mean(axis=1)[:, :2] if len(tri) else np.zeros((0, 2))
        self.near = cKDTree(mid) if len(mid) else None
        self.cache = {}

    def _lift(self, i, x, y):
        (ax, ay, az), (bx, by, bz), (cx, cy, cz) = self.a[i], self.b[i], self.c[i]
        det = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(det) < 1e-12:
            return (az + bz + cz) / 3.0
        u = ((by - cy) * (x - cx) + (cx - bx) * (y - cy)) / det
        v = ((cy - ay) * (x - cx) + (ax - cx) * (y - cy)) / det
        u, v = max(0.0, min(1.0, u)), max(0.0, min(1.0, v))
        w = max(0.0, 1.0 - u - v)
        t = u + v + w
        return (u * az + v * bz + w * cz) / t

    def _inside(self, i, x, y):
        (ax, ay, _), (bx, by, _), (cx, cy, _) = self.a[i], self.b[i], self.c[i]
        d1 = (x - bx) * (ay - by) - (ax - bx) * (y - by)
        d2 = (x - cx) * (by - cy) - (bx - cx) * (y - cy)
        d3 = (x - ax) * (cy - ay) - (cx - ax) * (y - ay)
        return not ((d1 < 0 or d2 < 0 or d3 < 0) and (d1 > 0 or d2 > 0 or d3 > 0))

    def __call__(self, x, y):
        key = (round(x, 3), round(y, 3))
        got = self.cache.get(key)
        if got is not None:
            return got
        z = self._solve(float(x), float(y))
        self.cache[key] = z
        return z

    def _on_road(self, x, y):
        """The height ON the carriageway at a point that lies on it -- the containing triangle,
        or the nearest one where floating point puts the point a hair outside."""
        d, idx = self.near.query([x, y], k=min(12, self.near.n))
        for i in np.atleast_1d(idx):
            if self._inside(int(i), x, y):
                return self._lift(int(i), x, y)
        return self._lift(int(np.atleast_1d(idx)[0]), x, y)

    def _solve(self, x, y):
        if self.near is None:
            return self.z_at(x, y)
        k = min(max(self.blend, 12), self.near.n)
        d, idx = self.near.query([x, y], k=k)
        # AND THE ROAD IS NOT WHAT THE GROUND DOES A KILOMETRE AWAY. Without this the nearest
        # carriageway answered for every point on Earth, and a ground patch far from any road
        # was lifted onto it.
        if float(np.atleast_1d(d)[0]) > self.REACH_M:
            return self.z_at(x, y)
        if self.rail is not None:
            # THE NEAREST POINTS ON THE CARRIAGEWAY'S EDGE, blended among THEMSELVES. They lie on
            # one curve, so the blend is smooth along it and cannot mix two roads' crowns.
            dd, ii = self.rail.query([x, y], k=min(4, self.rail.n))
            dd, ii = np.atleast_1d(dd), np.atleast_1d(ii)
            w = 1.0 / np.maximum(dd, 1e-6) ** 2
            here = np.array([self._on_road(*self.rail_at[int(j)]) for j in ii])
            return float((w * here).sum() / w.sum())
        d, idx = np.atleast_1d(d), np.atleast_1d(idx)
        for i in idx:
            if self._inside(int(i), x, y):
                return self._lift(int(i), x, y)
        w = 1.0 / np.maximum(d[:self.blend], 1e-6) ** 2
        zs = np.array([self._lift(int(i), x, y) for i in idx[:self.blend]])
        return float((w * zs).sum() / w.sum())


def _radius(m, nid):
    """RASt 06's corner radius, from the classes that meet: the wider leg decides."""
    for (w, _, _) in m.legs_at(nid):
        if w["tags"].get("highway") in LARGE or w["tags"]["width"] >= 9.0:
            return R_LARGE_M
    return R_SMALL_M


def seen_of(st):
    """WHAT THE EYE CAN REACH, or None for the whole network. `Structure.seen` is set once by the
    caller that owns the camera and read by every SURFACE operator here -- the kerb, the channel,
    the footway. It is not an optimisation bolted on: a kerb ring is a morphological closing per
    junction, and run over a whole quarter it took 14 minutes at OldTown and never finished a
    picture (measured 2026-09-07). These operators have a HARD support radius -- a kerb at a point
    is decided by ways within 3.75 + 8.0 + 2.5 m by RASt 06 -- so restricting them to the visible
    set plus that halo changes nothing anybody can see, and the halo is the caller's to apply."""
    return getattr(st, "fine", None) or getattr(st, "seen", None)


def drivable_area(m, st):
    """Every carriageway ribbon and every junction surface, as ONE area."""
    # THE MEMO HANGS ON THE STRUCTURE ITSELF. Keyed on `id(st)` in a module dict it would
    # answer for a DIFFERENT structure the moment CPython reused a freed address, which over
    # an 81-case ladder it certainly does.
    got = getattr(st, "_drivable", None)
    if got is not None:
        return got
    seen = seen_of(st)
    parts = []
    for w in m.net.ways:
        if w["tags"].get("highway") not in DRIVABLE:
            continue
        if seen is not None and w["id"] not in seen[0]:
            continue
        line = m.centreline(w)
        if line.length < 1e-6:
            continue
        parts.append(line.buffer(w["tags"]["width"] / 2.0, cap_style=2, quad_segs=ARC))
    parts += [p for nid, p in st.polygons.items()
              if not p.is_empty and (seen is None or nid in seen[1])]
    out = unary_union(parts) if parts else Polygon()
    st._drivable = out
    return out


def kerb_face_area(m, st, drivable=None):
    """The line the kerbstone runs on: the channel, with every junction's corner rounded to its
    own radius. A closing with radius R fills exactly the concave corners narrower than R and
    leaves a straight edge untouched, which is what a corner radius IS."""
    got = getattr(st, "_kerb_face", None)
    if got is not None:
        return got
    drivable = drivable_area(m, st) if drivable is None else drivable
    if drivable.is_empty:
        return drivable
    channel = drivable.buffer(GUTTER_M, join_style=1, quad_segs=ARC)
    seen = seen_of(st)
    out = [channel]
    for nid in (m.junctions if seen is None else (n for n in m.junctions if n in seen[1])):
        r = _radius(m, nid)
        x0, y0 = m.net.nodes[nid]
        window = Point(x0, y0).buffer(r * 3.0 + GUTTER_M + 20.0, quad_segs=ARC)
        near = channel.intersection(window)
        if near.is_empty:
            continue
        closed = near.buffer(r, join_style=1, quad_segs=ARC).buffer(-r, join_style=1, quad_segs=ARC)
        out.append(closed.intersection(window))
    face = unary_union(out)
    st._kerb_face = face
    return face


def walk_area(m, st, drivable=None):
    """The footway's own AREA -- what a lamp, a bench, a tree pit or a bollard has to stand in."""
    face = kerb_face_area(m, st, drivable)
    if face.is_empty:
        return face
    top_out = face.buffer(KERB_WIDE_M, join_style=1, quad_segs=ARC)
    return top_out.buffer(WALK_M, join_style=1, quad_segs=ARC).difference(top_out)


def street_footprint(m, st, drivable=None):
    """EVERYTHING THE STREET COVERS: carriageway, gutter, kerb and footway, as one outline. What
    the terrain has to be cut by, and what a building's plot has to stop at."""
    face = kerb_face_area(m, st, drivable)
    if face.is_empty:
        return face
    # TWO BUFFERS IN SEQUENCE ARE NOT ONE BUFFER OF THE SUM. Taken as `buffer(KERB + WALK)` the
    # outline differed from the footway's own outer ring at every corner fillet, and the terrain
    # and the footway then overlapped in slivers that Cycles drew as bright streaks along every
    # corner (looked at, 2026-09-06). The footprint IS the footway's outer ring, by construction.
    #
    # AND IT IS SIMPLIFIED, because this outline is a CONSTRAINT on the terrain's triangulation
    # and not geometry anybody sees: at full resolution OldTown handed Shewchuk 260 000 segments
    # and the noding alone took longer than the whole twin. 50 mm is a tenth of the kerbstone's
    # own width and the terrain meets the footway at the same HEIGHT either way.
    return face.buffer(KERB_WIDE_M, join_style=1, quad_segs=ARC) \
               .buffer(WALK_M, join_style=1, quad_segs=ARC) \
               .simplify(OUTLINE_M)


def edge_height(surface, sites=()):
    """The height of the street at its OUTER edge -- the footway's far side, where the terrain
    meets it. A terrain ring that read its own height there leaves a crack as deep as the kerb."""
    up = upstand(sites)

    def z(x, y):
        return surface(x, y) + up(x, y) + WALK_FALL * WALK_M
    return z


def upstand(sites, half_extra=GUTTER_M):
    """THE KERB'S UPSTAND AS A FIELD, not as a number. RASt 06 drops it from 0.12 m to 0.03 m
    across a crossing so that a wheel can take it, and the footway behind ramps down with it --
    which is why this is one function the kerb face, the kerb top and the footway all read."""
    if not sites:
        return lambda x, y: KERB_UP_M
    from scipy.spatial import cKDTree
    ends = []
    for ((x, y), d, half) in sites:
        nx, ny = -d[1], d[0]
        for side in (-1.0, +1.0):
            r = (half + half_extra) * side
            ends.append((x + nx * r, y + ny * r))
    tree = cKDTree(np.asarray(ends))
    reach = 0.0

    def up(x, y):
        d = float(tree.query([x, y], k=1)[0]) - reach
        t = min(1.0, max(0.0, d / DROP_RAMP_M))
        return KERB_DROP_M + (KERB_UP_M - KERB_DROP_M) * t
    return up


def street_edge(m, st, surface, sites=(), fine_reach_m=None):
    """GUTTER, KERB FACE, KERB TOP, FOOTWAY -- one ring each, around the whole network.

    `fine_reach_m` is where the QUALITY mesh stops. The footway ramps at a dropped kerb and its
    2.5 % fall needs metre triangles to carry -- near the camera. A whole city's footway meshed
    that way is hundreds of thousands of triangles for a fall of six centimetres nobody can see
    at two hundred metres, so beyond the disc it is a plain constrained triangulation. The disc
    is a CONSTRAINT in both, so the two share their vertices and there is no crack.

    Returns [(role, vertices, triangles)]."""
    drivable = drivable_area(m, st)
    if drivable.is_empty:
        return ()
    face = kerb_face_area(m, st, drivable)
    top_out = face.buffer(KERB_WIDE_M, join_style=1, quad_segs=ARC)
    walk_out = top_out.buffer(WALK_M, join_style=1, quad_segs=ARC)


    up = upstand(sites)

    def at_road(x, y):
        return surface(x, y)

    def at_kerb(x, y):
        return surface(x, y) + up(x, y)

    disc = None
    if fine_reach_m:
        disc = Point(0.0, 0.0).buffer(float(fine_reach_m), quad_segs=64)

    def by_reach(role, poly, z_of, opts, seeds=()):
        """The quality mesh inside the disc, a plain CDT outside it."""
        if poly.is_empty:
            return
        if disc is None:
            out.append((role, *triangulate(poly, z_of, opts, seeds)))
            return
        near = poly.intersection(disc)
        far = poly.difference(disc)
        if not near.is_empty:
            out.append((role, *triangulate(near, z_of, opts,
                                           [q for q in seeds if disc.covers(Point(*q))])))
        if not far.is_empty:
            # AND SIMPLIFIED OUT THERE. The outline is what the triangulation costs -- a city's
            # footway is a multipolygon of thousands of rings -- and 30 mm is under a pixel at
            # the distance this half of the ring is seen from.
            far = far.simplify(0.03)
            if not far.is_empty:
                out.append((role, *triangulate(far, z_of, "p")))

    out = []
    apron = face.difference(drivable)
    if not apron.is_empty:
        by_reach("gutter", apron, at_road, FINE)
    out.append(("kerb", *extrude(face, at_road, up)))
    by_reach("kerb", top_out.difference(face), at_kerb, FINE)
    walk = walk_out.difference(top_out)
    if not walk.is_empty:
        # THE DISTANCE TO THE KERB, WITHOUT GEOS. `boundary.distance(Point)` per vertex put
        # GEOSProject at the top of a place twin's profile; the boundary densified to 0.5 m in a
        # k-d tree gives the same 2.5 % fall to within a centimetre.
        from scipy.spatial import cKDTree
        edge = top_out.boundary
        seeds = []
        for part in (edge.geoms if hasattr(edge, "geoms") else [edge]):
            n = max(2, int(part.length / 0.5))
            seeds += [(p.x, p.y) for p in (part.interpolate(part.length * i / n)
                                           for i in range(n + 1))]
        rail = cKDTree(np.asarray(seeds)) if seeds else None

        def at_walk(x, y):
            gap = float(rail.query([x, y], k=1)[0]) if rail is not None else 0.0
            return surface(x, y) + up(x, y) + WALK_FALL * gap
        seeds = []
        for ((sx, sy), d, half) in sites:
            nx, ny = -d[1], d[0]
            for side in (-1.0, +1.0):
                r = (half + GUTTER_M + KERB_WIDE_M) * side
                cx, cy = sx + nx * r, sy + ny * r
                for at in (0.5, 1.0, 1.6, 2.2):
                    for k in range(10):
                        a = 2 * math.pi * k / 10
                        seeds.append((cx + at * math.cos(a), cy + at * math.sin(a)))
        seeds = [q for q in seeds if walk.covers(Point(*q))]
        by_reach("walk", walk, at_walk, WALK_FINE, seeds)
    return tuple((r, v, t) for (r, v, t) in out if t)


def check_walk_off_carriageway(m, st):
    """I17: A FOOTWAY NEVER LIES ON A CARRIAGEWAY. The area of the overlap, in square metres.

    The defect this catches was visible and green for a whole round: a per-way construction laid
    the minor leg's footway across the major's carriageway at every junction in the network."""
    drivable = drivable_area(m, st)
    if drivable.is_empty:
        return 0.0
    face = kerb_face_area(m, st, drivable)
    top_out = face.buffer(KERB_WIDE_M, join_style=1, quad_segs=ARC)
    walk = top_out.buffer(WALK_M, join_style=1, quad_segs=ARC).difference(top_out)
    return float(walk.intersection(drivable).area)


def check_carriageway_connected(m, st):
    """I18: THE KERB BOUNDS THE CARRIAGEWAY AND NEVER CROSSES IT. The number of drivable pieces
    the street layer leaves behind, minus the number the network itself has.

    A kerb drawn straight across a junction mouth walls the minor leg shut, and no count of
    triangles can see it -- the area can."""
    drivable = drivable_area(m, st)
    if drivable.is_empty:
        return 0
    face = kerb_face_area(m, st, drivable)
    kerbstone = face.buffer(KERB_WIDE_M, join_style=1, quad_segs=ARC).difference(face)
    left = drivable.difference(kerbstone)
    def pieces(g):
        return 0 if g.is_empty else (1 if g.geom_type == "Polygon" else len(g.geoms))
    return pieces(left) - pieces(drivable)
