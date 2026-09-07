"""THE NINE PLACES, BUILT BY THE LAB AND SEEN THROUGH THE CLIENT'S OWN CAMERA.

`make shots` stands the outshine client at nine real coordinates and writes a picture. This does
the same thing with the LAB's geometry: the same OSM extract, the same terrarium DEM, the same
generators the beds prove -- and then the SAME CAMERA, so the two pictures answer one question.
Where they differ, the difference is the tree's, and it is legible rather than argued about.

THE CAMERA IS READ, NEVER COPIED. `PLACES` below is parsed out of `src/client/PlaceCamera.cpp`
and `PlaceCamera.h` at run time: the coordinate, the bearing, the instant, whether the view is an
eye or a plan and its span, plus `kEyeAglM`, `kPitchDeg`, `kFovDeg`, `kPlanAboveM`,
`kOverheadPitchDeg`, `kWidePx` and `kHighPx`. A table retyped here would agree with the client on
the day it was written and quietly stop agreeing afterwards, and a twin that is one bearing out is
worse than no twin at all -- it invites a comparison that is already wrong.

WHAT THE PICTURE IS AND IS NOT. It is flat shading over the lab's own meshes with a two-colour
sky: ONE normal per triangle, a Lambert term against the sun where the client's clock puts it, and
a z-buffer. It is NOT the engine's atmosphere, its lights, its shadows or its materials -- those
are the renderer's and belong there. What this twin can settle is GEOMETRY: whether a building
stands on the ground rather than in it, whether a roof faces the right way, whether a street runs
where a street runs, whether the massing of a real place reads as that place.

    python3 test/lab/places.py [name ...]
"""
import math
import os
import pathlib
import re
import sys
import time

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE / "roads"))
import importlib.util as _util  # noqa: E402

import data as roaddata  # noqa: E402
import publish  # noqa: E402
import blend  # noqa: E402
import camera as lab_camera  # noqa: E402
import geometry  # noqa: E402
import materials as stock  # noqa: E402
import street  # noqa: E402
import kerbline  # noqa: E402
import junction  # noqa: E402
import amenities  # noqa: E402
import surfaces  # noqa: E402
import wear  # noqa: E402
import furniture  # noqa: E402
import ground as lab_ground  # noqa: E402

_spec = _util.spec_from_file_location("outshine_road_bed", HERE / "roads" / "synthetic.py")
roadbed = _util.module_from_spec(_spec)
_spec.loader.exec_module(roadbed)
_spec = _util.spec_from_file_location("outshine_building_bed", HERE / "buildings" / "synthetic.py")
bldbed = _util.module_from_spec(_spec)
_spec.loader.exec_module(bldbed)
sys.path.insert(0, str(HERE / "buildings"))
import region as region_of  # noqa: E402

_road_real = _util.spec_from_file_location("outshine_road_real", HERE / "roads" / "real.py")
roadreal = _util.module_from_spec(_road_real)
_road_real.loader.exec_module(roadreal)

OUT = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "places"
CACHE = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-lab" / "places-cache"
DEM_ZOOM = 14                    # the engine samples FinestZoomOf(Elevation) - 1, which is 14

# WHAT THE TWIN TAKES IN. Three radii rather than one, because they are bounded by three
# different things and folding them into one number makes the largest of them pay for the others.
# A TWIN THAT DRAWS HALF THE TOWN IS NOT A TWIN.
# 900 m and a cap of 1400 bodies left 1900 of OldTown's 3300 buildings -- 58 percent -- never
# built at all, while the outshine client draws the town to the horizon (measured 2026-09-06 by
# counting the extract against the row the twin printed). The cap was there because the generator
# cost 210 ms a body; it costs 54 now, so 3300 bodies are three minutes and the cap has no reason
# to exist. What bounds the reach is what the LENS reads, and at 60 m with a 55 degree lens that
# is kilometres.
# THE REACH IS OVERRIDABLE, because a twin is a thing you have to be able to LOOK at often. The
# road bed's profile is one convex solve over the whole extract and its cost grows faster than
# the extract does -- OldTown at 700 m spends minutes inside cvxpy before a triangle exists -- so
# `OUTSHINE_REACH` cuts both reaches for a look and the default is what a published twin uses.
_REACH = float(os.environ.get("OUTSHINE_REACH", "0") or 0)
BUILT_REACH_M = _REACH * 3.6 if _REACH else 2500.0   # buildings: as far as the lens resolves one
ROAD_REACH_M = _REACH or 700.0   # roads: one convex solve over the extract, and it grows with it
FINE_ROAD_M = 120.0              # and how far the carriageway carries its weathering rows
GROUND_REACH_M = 12000.0         # terrain: far enough that the world does not END inside the frame
GROUND_RINGS = 72                # a POLAR grid: rings times spokes, so no T-junction and no seam
GROUND_SPOKES = 96
GROUND_NEAR_M = 8.0
BUILT_MOST = 40000               # [SET] a guard against a runaway extract, not a quality knob

WALL_COLOUR = {"brick": (0.62, 0.46, 0.40), "stone": (0.72, 0.70, 0.65),
               "timber": (0.58, 0.47, 0.36), "frame": (0.70, 0.72, 0.74)}
ROAD_COLOUR = (0.34, 0.34, 0.35)
GROUND_COLOUR = (0.44, 0.48, 0.36)
WATER_COLOUR = (0.30, 0.42, 0.52)


# ------------------------------------------------------------------ the client's own camera

def _constants(text):
    found = {}
    for m in re.finditer(r"constexpr\s+(?:double|int)\s+(k\w+)\s*=\s*([-\d.]+)", text):
        found[m.group(1)] = float(m.group(2))
    for m in re.finditer(r"constexpr\s+int\s+(k\w+)\s*=\s*(\d+)", text):
        found[m.group(1)] = float(m.group(2))
    return found


def client_places():
    """The table the client draws, read from the client. Returns (places, camera constants)."""
    cpp = (ROOT / "src/client/PlaceCamera.cpp").read_text()
    hdr = (ROOT / "src/client/PlaceCamera.h").read_text()
    k = _constants(hdr)
    k.update(_constants(cpp))
    for want in ("kEyeAglM", "kPlanAboveM", "kPitchDeg", "kFovDeg", "kOverheadPitchDeg",
                 "kWidePx", "kHighPx"):
        if want not in k:
            raise RuntimeError(f"{want} is no longer a constexpr in the client -- the twin reads "
                               f"the camera from there and cannot invent it")
    block = re.search(r"kPlaces\{\{(.*?)\}\};", cpp, re.S)
    if block is None:
        raise RuntimeError("`kPlaces` is no longer an aggregate in PlaceCamera.cpp")
    places = []
    for one in re.finditer(r"\{\.Name\s*=\s*\"(\w+)\"(.*?)\}", block.group(1), re.S):
        body = one.group(2)

        def field(key, cast=float):
            m = re.search(rf"\.{key}\s*=\s*\"?([-\w.:+]+)\"?", body)
            return cast(m.group(1)) if m else None

        places.append(dict(name=one.group(1), lat=field("LatitudeDeg"), lon=field("LongitudeDeg"),
                           bearing=field("BearingDeg"), span=field("SpanM") or 0.0,
                           plan="Plan" in (field("From", str) or ""), when=field("WhenUtc", str)))
    if len(places) != 9:
        print(f"note: the client's table now holds {len(places)} places, not nine")
    return places, k


PLACES, CAM = client_places()


def camera_for(place):
    """The lab camera that stands exactly where the client's does.

    `OUTSHINE_EYE=agl,bearing,pitch[,fov[,dx,dy]]` moves it, which is the one instrument a twin needs and
    did not have: the client's own camera stands above the roofs, so a whole street pass -- the
    kerb, the markings, the crossings, the footway's paving -- was built and never once LOOKED at
    in a real place. The default is the client's and nothing published moves it."""
    eye = os.environ.get("OUTSHINE_EYE")
    if eye:
        got = [float(v) for v in eye.split(",")]
        agl, bearing, pitch = got[0], got[1], got[2]
        fov = got[3] if len(got) > 3 else CAM["kFovDeg"]
        return lab_camera.Camera(place["lat"], place["lon"], agl_m=agl, bearing_deg=bearing,
                                 pitch_deg=pitch, fov_deg=fov,
                                 width=int(CAM["kWidePx"]), height=int(CAM["kHighPx"]))
    if place["plan"]:
        return lab_camera.Camera(place["lat"], place["lon"], bearing_deg=place["bearing"],
                                 pitch_deg=CAM["kOverheadPitchDeg"], fov_deg=CAM["kFovDeg"],
                                 width=int(CAM["kWidePx"]), height=int(CAM["kHighPx"]),
                                 plan_above_m=CAM["kPlanAboveM"], span_m=place["span"])
    return lab_camera.Camera(place["lat"], place["lon"], agl_m=CAM["kEyeAglM"],
                             bearing_deg=place["bearing"], pitch_deg=CAM["kPitchDeg"],
                             fov_deg=CAM["kFovDeg"], width=int(CAM["kWidePx"]),
                             height=int(CAM["kHighPx"]))


# ------------------------------------------------------------------ the extract

def overpass(place, reach_m):
    """Buildings and highways around the place, cached the way the road bed caches its own."""
    lat, lon = place["lat"], place["lon"]
    dlat = reach_m / 111132.0
    dlon = reach_m / (111320.0 * math.cos(math.radians(lat)))
    bbox = f"{lat - dlat:.6f},{lon - dlon:.6f},{lat + dlat:.6f},{lon + dlon:.6f}"
    # AND THE GROUND ITSELF. A twin whose every gap between the buildings is a green field has
    # thrown away half the extract: OSM states what the ground IS -- a square is paved, a car
    # park is asphalt, a park is grass, a river is water -- and `surfaces.py` reads it.
    query = ("[out:json][timeout:300];("
             f'way["building"]({bbox});way["building:part"]({bbox});'
             f'way["highway"]({bbox});way["railway"]({bbox});'
             f'way["barrier"]({bbox});node["natural"="tree"]({bbox});'
             f'node["barrier"]({bbox});node["highway"="street_lamp"]({bbox});'
             f'node["amenity"]({bbox});'
             f'way["landuse"]({bbox});way["natural"]({bbox});way["leisure"]({bbox});'
             f'way["waterway"]({bbox});way["amenity"]({bbox});way["place"]({bbox});'
             f'way["man_made"]({bbox});'
             ");(._;>;);out body;")
    # THE CACHE KEY CARRIES THE QUERY. Named by the place alone, an extract fetched before a
    # tag was added to the query is served for one that asks for it, and the new tags are simply
    # absent -- which looks exactly like a place that has none of them.
    import hashlib
    stamp = hashlib.sha1(query.encode()).hexdigest()[:8]
    name = f"place-{place['name']}-{int(reach_m)}-{stamp}.json"
    import json
    import urllib.parse
    held = roaddata.fetch(roaddata.OVERPASS, roaddata.CACHE / name,
                          data=urllib.parse.urlencode({"data": query}).encode())
    return json.loads(held)


class Frame:
    """The local ENU frame of one place: x east, y north, z above the DEM AT THE ORIGIN.

    The z datum is the ground under the CAMERA rather than sea level, because that is what makes
    `agl_m` mean what the client means by it -- the client samples the height and stands 60 m over
    it, and a twin measuring from the geoid would put the eye underground in Bern."""

    def __init__(self, place):
        self.dem = roaddata.Dem(zoom=DEM_ZOOM)
        self.lat0, self.lon0 = place["lat"], place["lon"]
        self.per_lat = 111132.0
        self.per_lon = 111320.0 * math.cos(math.radians(self.lat0))
        self.datum = float(self.dem.at(self.lat0, self.lon0))

    def xy(self, lat, lon):
        return ((lon - self.lon0) * self.per_lon, (lat - self.lat0) * self.per_lat)

    def z(self, x, y):
        return float(self.dem.at(self.lat0 + y / self.per_lat,
                                 self.lon0 + x / self.per_lon)) - self.datum


# ------------------------------------------------------------------ the three bodies

def ground_fan(frame, put=None):
    """THE TERRAIN AS A POLAR FAN. A square grid fine enough for the near field and wide enough
    for the horizon is a quarter of a million triangles; two grids of different pitch meet at a
    T-junction and crack. Rings on a geometric progression with a fixed spoke count are neither:
    one closed fan, no seam, and the triangle size grows with the distance the way the pixel it
    covers does."""
    q = (GROUND_REACH_M / GROUND_NEAR_M) ** (1.0 / GROUND_RINGS)
    radii = [0.0] + [GROUND_NEAR_M * q ** k for k in range(GROUND_RINGS + 1)]
    angles = np.linspace(0.0, 2 * math.pi, GROUND_SPOKES, endpoint=False)
    rows = []
    for r in radii:
        if r == 0.0:
            rows.append([(0.0, 0.0, frame.z(0.0, 0.0))])
            continue
        rows.append([(r * math.sin(a), r * math.cos(a), frame.z(r * math.sin(a), r * math.cos(a)))
                     for a in angles])
    verts, tris, base = [], [], []
    for row in rows:
        base.append(len(verts))
        verts.extend(row)
    # THE FAN'S WINDING IS THE TREE'S: counter-clockwise seen from OUTSIDE, which for ground is
    # from above. The spoke angle runs x = r sin a, y = r cos a -- north toward east, which is
    # CLOCKWISE in the xy-plane -- so the naive order gives every face a normal pointing DOWN.
    # Measured 2026-09-06: the OldTown twin rendered a black ground under a lit city, because
    # `skyward` and the Lambert term both read zero on a face turned away from the sky.
    for k in range(len(angles)):
        tris.append((base[0], base[1] + (k + 1) % len(angles), base[1] + k))
    for j in range(1, len(rows) - 1):
        for k in range(len(angles)):
            k2 = (k + 1) % len(angles)
            a, b = base[j] + k, base[j] + k2
            c, d = base[j + 1] + k, base[j + 1] + k2
            tris.append((a, d, c))
            tris.append((a, b, d))
    up = 0
    for (ia, ib, ic) in tris:
        pa, pb, pc = (np.asarray(verts[i], dtype=float) for i in (ia, ib, ic))
        up += float(np.cross(pb - pa, pc - pa)[2]) > 0.0
    if up != len(tris):
        raise RuntimeError(f"the ground fan has {len(tris) - up} of {len(tris)} faces turned "
                           f"away from the sky -- a ground the camera sees the underside of")
    return verts, tris


def _rings(doc):
    nodes = {e["id"]: (e["lat"], e["lon"]) for e in doc["elements"] if e["type"] == "node"}
    ways = [e for e in doc["elements"] if e["type"] == "way" and "nodes" in e]
    return nodes, ways


def buildings_of(place, frame, doc, red):
    """Every closed `building` way in the extract, massed by the building bed at LOD 0.

    NO FACADE. The twin asks whether a place's MASSING reads as that place; an opening is a metre
    of geometry a 1280 px frame at 300 m cannot resolve, and paying for it here would spend the
    whole picture on the nearest three houses."""
    from shapely.geometry import Polygon
    nodes, ways = _rings(doc)
    where = region_of.of(place["lat"], place["lon"], frame.datum)
    made, dropped = [], 0
    for w in ways:
        tags = w.get("tags", {})
        if "building" not in tags and "building:part" not in tags:
            continue
        refs = w["nodes"]
        if len(refs) < 4 or refs[0] != refs[-1] or any(r not in nodes for r in refs):
            dropped += 1
            continue
        pts = [frame.xy(*nodes[r]) for r in refs[:-1]]
        if math.hypot(*np.mean(pts, axis=0)) > BUILT_REACH_M:
            continue
        poly = Polygon(pts)
        if not poly.is_valid or poly.area < 4.0:
            dropped += 1
            continue
        made.append((poly, tags))
    if len(made) > BUILT_MOST:
        made.sort(key=lambda pt: pt[0].centroid.x ** 2 + pt[0].centroid.y ** 2)
        made = made[:BUILT_MOST]
    ground = bldbed.Ground(lambda x, y: frame.z(x, y))
    # A PARTY WALL NEEDS A NEIGHBOUR, and finding it by scanning every other footprint is O(n^2)
    # over thousands of bodies. The index is shapely's own, built once.
    import shapely
    shapes = [p for (p, _) in made]
    tree = shapely.STRtree(shapes) if shapes else None
    bodies = []
    for at, (poly, tags) in enumerate(made):
        b = bldbed.Building(poly, tags, ground, where=where)
        if tree is not None:
            near = tree.query(poly.buffer(bldbed.Building.PARTY_GAP_M))
            b.neighbours = [shapes[int(i)] for i in near if shapes[int(i)] is not poly]
        if not b.watertight():
            red.append(f"B-closed({tags.get('name', poly.centroid.wkt)})")
        wrong, degenerate, _ = b.winding()
        if wrong or degenerate:
            red.append(f"B-wound({wrong}e,{degenerate}deg)")
        if b.volume() <= 0.0:
            red.append("B-volume")
        bodies.append(b)
    return bodies, dropped


def roads_of(place, frame, red):
    """The carriageway surface, from the road bed's own solve. The bed already owns the invariants
    -- C0 at a node, C1 through it, the DEM band, the continuous Trasse -- so the twin RUNS them
    rather than restating them, and a red one keeps the picture out of `build/shots/lab`."""
    doc = roadreal.fetch(place["name"], place["lat"], place["lon"], ROAD_REACH_M)
    nodes, ways = _rings(doc)
    net, kept, _ = roadreal.net_of(nodes, ways, place["lat"], place["lon"])
    if not net.ways:
        return None, 0
    terrain = roadbed.Terrain(roadreal.RealTerrain(place["lat"], place["lon"]),
                              extent=ROAD_REACH_M + 200.0, posting=roadbed.POSTING_M)
    m = roadbed.Map(terrain, net)
    m.mark_open_ends(ROAD_REACH_M)
    m = m.solve()
    for label, value, limit in (("I1", roadbed.check_c0(m), 1e-9),
                                ("I2", roadbed.check_c1(m), 1e-6),
                                ("I13", roadbed.check_route_c0(m), 1e-6),
                                ("I3", roadbed.check_dem_band(m), 1.0 + 1e-6)):
        if value > limit:
            red.append(f"{label}({value:.2e})")
    if not roadbed.check_finite(m)["finite"]:
        red.append("P finite")
    # THE WEAR ROWS ARE DRAWN WHERE THEY CAN BE SEEN and nowhere else: their bands are 0.35 m
    # wide, the camera stands at the origin, and past a hundred metres they cost seven times the
    # vertices for nothing (measured: 1 660 149 triangles and 165 s at OldTown's 240 m reach).
    roadbed.Mesh.FINE_REACH_M = FINE_ROAD_M
    return roadbed.Mesh(roadbed.Structure(m)), kept


# ------------------------------------------------------------------ the picture

class Parts:
    """THE DOOR'S `Geometry`, WEARING THE NAME THE TWIN ALREADY CALLS IT BY.

    A part is a surface's worth of mesh -- `Geometry::addPart(named, material)` -- and its arrays
    are flat `float32` positions, flat `uint32` triangles and RGBA colours, because that is what
    `include/scene/Geometry.h` takes and a generator's conversion to C++ should be a copy rather
    than a translation.

    It was a dict of lists of Python tuples. Measured 2026-09-07: a million vertices costs 144 MB
    that way against 12 MB as `float32`, a million triangles 156 MB against 12 MB -- twelve times
    -- so a town twin of 6.9 million triangles stood at 2.1 GB where the arithmetic says 163 MB,
    and the system killed the renderer for want of memory. The C++ never had the problem because
    the door had already said what a mesh IS."""

    def __init__(self):
        self.geom = geometry.Geometry()
        self.at = {}
        self.held = {}

    def add(self, role, verts, tris, material=None, colours=None):
        """A PART IS A SURFACE'S WORTH OF MESH, and the role only names it. Keyed by the role
        alone a town twin was 6 941 parts -- one per building per surface -- which is 6 941 draw
        calls for thirty materials. Two pieces that share a surface share a part, which is what
        `Geometry::addPart(named, material)` means and what a draw call is."""
        v = np.asarray(verts, dtype=np.float32).reshape(-1, 3)
        t = np.asarray(tris, dtype=np.uint32).reshape(-1, 3)
        if not len(t):
            return
        mark = material.key() if hasattr(material, "key") else (
            tuple(material) if isinstance(material, (list, tuple)) else material)
        key = (role.split(".")[0], mark)
        held = self.held.setdefault(key, {"v": [], "t": [], "c": [], "at": 0, "m": material,
                                          "name": role.split(".")[0]})
        held["v"].append(v)
        held["t"].append(t + held["at"])
        held["at"] += len(v)
        held["c"].append(np.zeros((len(v), 4), dtype=np.float32) if colours is None
                         else np.asarray(colours, dtype=np.float32).reshape(-1, 4))
        if material is not None:
            held["m"] = material

    def close(self):
        """Every surface's pieces become ONE part -- which is what a draw call is."""
        for key, held in self.held.items():
            if key in self.at:
                continue
            part = self.geom.addPart(held["name"], held["m"])
            self.at[key] = part
            self.geom.setPositions(part, np.concatenate(held["v"]))
            self.geom.setTriangles(part, np.concatenate(held["t"]))
            if any(c.any() for c in held["c"]):
                self.geom.setColours(part, np.concatenate(held["c"]))
        self.held = {}
        return self.geom

    @property
    def of(self):
        """The twin's own view: {role: (positions as (N, 3), triangles as (M, 3))}."""
        self.close()
        out = {}
        for part in range(self.geom.parts()):
            out[f"{self.geom.nameOf(part)}_{part}"] = (
                self.geom.positionsOf(part).reshape(-1, 3),
                self.geom.trianglesOf(part).reshape(-1, 3).astype(np.int64))
        return out

    def counts(self):
        return {k: len(t) for k, (v, t) in self.of.items()}


def split_body(b):
    """A body's triangles as ROOF and WALL. A face standing wholly at or above the eaves, with at
    least one vertex above them, is the roof; everything else is wall, floor or the eaves band."""
    V = np.asarray(b.vertices, dtype=float)
    T = np.asarray(b.tris, dtype=np.int64)
    if not len(T):
        return (V, []), (V, [])
    z = V[T][:, :, 2]
    roof = (z >= b.eaves - 1e-9).all(axis=1) & (z > b.eaves + 1e-9).any(axis=1)
    return (V, T[~roof].tolist()), (V, T[roof].tolist())


def _fingerprint():
    """WHAT THE GEOMETRY DEPENDS ON: this lab's own sources. A cache keyed on anything less is a
    cache that hands back yesterday's answer after a repair."""
    import hashlib
    h = hashlib.sha256()
    for f in sorted(HERE.rglob("*.py")):
        h.update(f.name.encode())
        h.update(str(int(f.stat().st_mtime)).encode())
    for v in (BUILT_REACH_M, ROAD_REACH_M, GROUND_REACH_M, GROUND_RINGS, GROUND_SPOKES,
              DEM_ZOOM, BUILT_MOST):
        h.update(repr(v).encode())
    return h.hexdigest()[:16]


def cached_parts(place, frame, doc, red, lod=3):
    """THE PLACE'S GEOMETRY, BUILT ONCE. The road bed's solve is 75 percent of the time -- 260 s
    of 346 on a 400 m extract of OldTown, profiled 2026-09-06 -- and it is DETERMINISTIC, so
    rebuilding it to try another exposure or another palette is time spent proving something
    already proved. The key is the place, the reaches and the mtime of every source in the lab,
    so a repair invalidates it and nothing else does."""
    import pickle
    key = CACHE / f"{place['name']}-{lod}-{_fingerprint()}.pickle"
    if key.exists():
        try:
            parts, looks, counts = pickle.loads(key.read_bytes())
            got = Parts()
            for role, (v, t) in parts.items():
                got.add(role, v, t)
            got.close()
            return got, looks, counts
        except Exception:
            key.unlink(missing_ok=True)
    parts, looks, counts = parts_of(place, frame, doc, red, lod)
    if not red:
        key.parent.mkdir(parents=True, exist_ok=True)
        key.write_bytes(pickle.dumps((parts.of, looks, counts)))
    return parts, looks, counts


def parts_of(place, frame, doc, red, lod=3):
    """THE PLACE AS GEOMETRY BY ROLE, which is what a look can be judged from.

    `scene_of` below builds the same world for the FLAT rasteriser, one colour per role, because
    that instrument's job is to show a crack. This one keeps every body's own palette and its
    roof's own covering, and hands the street its kerb, its gutter, its footway, its markings and
    its lamps -- which is where a large share of what a player sees at eye level actually is."""
    parts, looks, fields = Parts(), {}, {}

    def put(role, verts, tris, rgb):
        parts.add(role, verts, tris, material=rgb)
        looks[role] = rgb

    # WHERE THE TIME WENT, said on every run. A twin is a thing you have to be able to LOOK at
    # often, and a stage that costs minutes has to name itself rather than be sampled for.
    clock = [time.time()]

    def took(what):
        # AND WHAT IT COST TO HOLD. A stage that overshoots its arithmetic says so in a number
        # rather than in a kill signal: the geometry of a town twin is 43 MB by `geometry.budget`
        # and the process stood at 760 MB, which is the generators' own working set and not the
        # mesh. Peak RSS is monotonic, so each line is the high-water mark up to that stage.
        import resource
        now = time.time()
        peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / (1024 * 1024)
        print(f"    {what:16s} {now - clock[0]:7.2f}s   peak {peak:6.0f} MB", flush=True)
        clock[0] = now

    stuffs = {"timber": stock.STOCK["timber"], "iron": stock.STOCK["iron"],
              "steel": stock.STOCK["steel"], "glass": stock.STOCK["glass"],
              "paint": stock.STOCK["paint"], "limestone": stock.STOCK["limestone"]}
    mesh, ways = roads_of(place, frame, red)
    took("roads")
    street_face = None
    if mesh is not None:
        # ONE SURFACE, READ OFF THE DRAWN ROAD. The kerb, every marking and every gully sit on the
        # carriageway, so all of them interpolate the mesh rather than recompute the section --
        # and the terrain is CUT by what the street covers, because a road has a crown and a sheet
        # drawn straight under it swallows everything but the crown.
        def z_at(x, y):
            return frame.z(x, y)

        surface = kerbline.Surface(mesh, z_at,
                                   edge=kerbline.drivable_area(mesh.map, mesh.st).boundary)
        sites = junction.crossing_sites(mesh.map, mesh.st)
        street_face = kerbline.street_footprint(mesh.map, mesh.st)
        took("street area")

    def drop(vv):
        return [(v[0], v[1], v[2] - frame.datum) for v in vv]

    # WHAT THE GROUND IS, from OSM, with the street already spoken for
    patches = surfaces.regions(doc, frame, GROUND_REACH_M, street_face)
    took("surfaces")
    over = surfaces.check_no_overlap(patches, street_face)
    if over > 1.0:
        red.append(f"I22 surfaces overlap {over:.1f} m2")
    if street_face is None and not patches:
        put("ground", *ground_fan(frame), stock.STOCK["grass"])
    else:
        sheet = lab_ground.surface(lambda x, y: frame.z(x, y), street_face,
                                   kerbline.edge_height(surface, sites) if street_face is not None
                                   else None,
                                   reach_m=GROUND_REACH_M, rings=GROUND_RINGS,
                                   spokes=GROUND_SPOKES, patches=patches)
        for role, (gv, gt) in sheet.items():
            put(f"g.{role}", drop(gv), gt,
                stock.STOCK["grass" if role == "ground" else role])
        took("ground")

    if mesh is not None:
        road = drop(mesh.vertices)
        faces = []
        for (ia, ib, ic) in mesh.tris:
            pa, pb, pc = (np.asarray(road[i], dtype=float) for i in (ia, ib, ic))
            faces.append((ia, ib, ic) if float(np.cross(pb - pa, pc - pa)[2]) > 0.0
                         else (ia, ic, ib))
        put("road", road, faces, stock.STOCK["asphalt"])
        # THE WEATHERING FIELD, and every channel of it is a consequence: a tyre polished the
        # wheel paths, water left its silt in the last half metre before the kerb.
        fields["road"] = wear.carriageway(mesh, mesh.map)
        took("wear")
        colour = {"kerb": stock.STOCK["kerbstone"], "gutter": stock.STOCK["asphalt"],
                  "walk": stock.STOCK["paving"], "paint": stock.STOCK["paint"],
                  "metal": stock.STOCK["iron"], "lamp": stock.STOCK["steel"],
                  "iron": stock.STOCK["iron"]}
        for (role, vv, tt) in kerbline.street_edge(mesh.map, mesh.st, surface, sites, FINE_ROAD_M):
            put(role, drop(vv), tt, colour.get(role) or stock.STOCK["concrete"])
        took("kerb ring")
        walk = kerbline.walk_area(mesh.map, mesh.st)
        for w in mesh.net.ways:
            for (role, vv, tt) in (street.markings(mesh.map, w, surface, mesh.st)
                                   + street.lamps(mesh.map, w, surface, walk)):
                put(role, drop(vv), tt, colour.get(role) or stock.STOCK["concrete"])
        for (role, vv, tt) in (junction.stop_lines(mesh.map, mesh.st, surface, street.PAINT_M)
                               + junction.crossings(mesh.map, mesh.st, surface, street.PAINT_M)
                               + junction.gullies(mesh.map, mesh.st, surface, kerbline.KERB_UP_M,
                                                  kerbline.GUTTER_M * 0.5)):
            put(f"j.{role}", drop(vv), tt, colour.get(role) or stock.STOCK["concrete"])
        for (role, vv, tt) in junction.signals_and_signs(doc, frame, z_at):
            put(f"j.{role}", vv, tt, colour.get(role) or stock.STOCK["concrete"])

        # WHAT THE SURVEYOR PUT ON THE PAVEMENT: a bench, a bin, a bus shelter, a post box, a
        # bicycle stand. OSM carries the position and the standard carries the dimensions; the
        # one thing neither carries is which way the thing faces, so it takes that from the
        # street it stands on -- a bench with its back to the pavement is the tell nobody looked.
        # ONE TREE, BUILT ONCE. Asked per piece of furniture over every way in the extract this
        # is a thousand shapely distances per bench, which is the product of two numbers a city
        # makes large.
        from shapely.geometry import Point
        from shapely.strtree import STRtree
        axes = [mesh.map.centreline(w) for w in mesh.net.ways]
        near = STRtree(axes) if axes else None

        def facing(x, y):
            if near is None:
                return ((1.0, 0.0), (0.0, 1.0))
            p = Point(x, y)
            line = axes[int(near.nearest(p))]
            at = line.project(p)
            a = line.interpolate(max(0.0, at - 0.5))
            b = line.interpolate(min(line.length, at + 0.5))
            dx, dy = b.x - a.x, b.y - a.y
            n = math.hypot(dx, dy) or 1.0
            q = line.interpolate(at)
            side = 1.0 if ((x - q.x) * (-dy / n) + (y - q.y) * (dx / n)) > 0 else -1.0
            return ((dx / n, dy / n), (-dy / n * side, dx / n * side))

        for (role, vv, tt) in amenities.from_osm(doc, frame, z_at, facing, ROAD_REACH_M):
            put(f"a.{role}", vv, tt, stuffs.get(role) or stock.STOCK["concrete"])
        took("amenities")

    # WHAT THE SURVEYOR ALREADY PUT THERE: trees, walls, fences, hedges, bollards. A carriageway
    # with a kerb is a road; a road with these is a place, and in the references a large share of
    # what a player sees at eye level is exactly this.
    stuff = {"leaf": stock.STOCK["leaf"], "bark": stock.STOCK["bark"],
             "masonry": stock.STOCK["masonry"], "limestone": stock.STOCK["limestone"],
             "timber": stock.STOCK["timber"], "iron": stock.STOCK["iron"]}
    took("markings")
    for (role, vv, tt) in furniture.from_osm(doc, frame, lambda x, y: frame.z(x, y), BUILT_REACH_M):
        put(f"f.{role}", vv, tt, stuff.get(role) or stock.STOCK["concrete"])

    bodies, dropped = buildings_of(place, frame, doc, red)
    for at, b in enumerate(bodies):
        mats = b.materials()
        made = b.body(lod)
        # THE HEIGHT OVER THIS BODY'S OWN GROUND, per vertex, in the blue channel. It is LINEAR
        # in z, so it interpolates exactly across a wall quad that runs from the pavement to the
        # eaves in one step -- and the material does the clamping into a 0.5 m band, which is the
        # non-linear half no vertex can hold (I23). Carried as a per-material constant instead,
        # it is what stopped seven thousand parts from ever being batched into one mesh.
        foot = float(min(v[2] for (vv, _) in made.values() for v in vv)) if made else 0.0
        for role, (vv, tt) in made.items():
            put(f"{role}.{at}", vv, tt, mats.get(role) or (0.35, 0.33, 0.30))
            fields[f"{role}.{at}"] = np.array(
                [(0.0, 0.0, min(1.0, max(0.0, (v[2] - foot) / blend.HEIGHT_SCALE_M)))
                 for v in vv], dtype=float)
    took("buildings")
    # WHAT THE TWIN IS MADE OF, by role. A generator that was never reached because of a missing
    # import produced 5 085 776 triangles the day it was -- 74 percent of the scene -- and the
    # system killed the renderer for want of memory before any of it could be looked at.
    import collections
    tally = collections.Counter()
    for role, (vv, tt) in parts.of.items():
        tally[role.split(".")[0]] += len(tt)
    whole = sum(tally.values()) or 1
    print("    " + "  ".join(f"{k} {n // 1000}k" for k, n in tally.most_common(8))
          + f"   TOTAL {whole // 1000}k tris", flush=True)
    return parts, looks, dict(ways=ways, buildings=len(bodies), dropped=dropped, fields=fields)


def ink_share(img):
    """HOW MUCH OF THE FRAME IS NOT SKY. A twin whose extract failed renders a clean gradient and
    every geometric check stays green, because there is no geometry to be wrong -- the trap
    CLAUDE.md names as `a gate blind to a path`. Read off the RENDERED picture now: a pixel whose
    rows above and below it differ hardly at all is sky, and geometry is what breaks that."""
    a = img.astype(np.int16)
    step = np.abs(np.diff(a, axis=0)).max(axis=2)
    return float(np.mean(step > 4))


LOOK_ENGINE = os.environ.get("OUTSHINE_LAB_ENGINE", "CYCLES")
LOOK_SAMPLES = int(os.environ.get("OUTSHINE_LAB_SAMPLES", "64"))
INK_LEAST = 0.02
DARK_MOST = 0.06          # [SET] a face turned from the sky renders near black; a twin has few


def dark_share(img):
    """How much of the frame is near BLACK. `ink_share` counts everything that is not the sky and
    a black ground is not the sky, so it read 59.6 % on a twin whose whole terrain was unlit
    (measured 2026-09-06). Flat shading gives an up-facing face at least the ambient term, so a
    large black area means faces turned away from the sky -- a mesh handed over inside out."""
    return float(np.mean(img.reshape(-1, 3).max(axis=1) < 24))


def one(place):
    """ONE PLACE, RENDERED BY CYCLES ON THE GPU. There is one renderer in this lab and it is
    Blender's -- a look that is judged has to be judged on what a player would see, and a second
    renderer is a second answer to the same question."""
    import blend
    red = []
    frame = Frame(place)
    doc = overpass(place, BUILT_REACH_M)
    camera = camera_for(place)
    if place["plan"]:
        # the plan camera is stated ABOVE SEA LEVEL (`SamplesHeight` is false for it), so the
        # frame's own datum is what turns that into a height over this ground
        camera.agl_m = CAM["kPlanAboveM"] - frame.datum
    parts, looks, counts = cached_parts(place, frame, doc, red, lod=3)
    if os.environ.get("OUTSHINE_CLAY"):
        # A CLAY RENDER: every material the same matte grey, which is what a modeller looks at
        # when the question is the FORM. Twice in one session a defect was blamed on the
        # material and twice the material was innocent; a picture with no material in it cannot
        # be argued with (2026-09-07).
        clay = stock.Material("clay", (0.42, 0.41, 0.40), roughness=0.92)
        looks = {k: clay for k in looks}
    if any(not np.isfinite(v).all() for (v, _) in parts.of.values()):
        red.append("P finite")
    OUT.mkdir(parents=True, exist_ok=True)
    shot = OUT / f"{place['name']}.png"
    # AND THE EYE MAY STAND SOMEWHERE ELSE. A place's origin is a coordinate a surveyor chose,
    # and in a dense old town it is usually INSIDE a block: a camera at 1.7 m there renders 100 %
    # black, which is what a street-level look at OldTown gave until the eye could be moved.
    eye = os.environ.get("OUTSHINE_EYE", "")
    got = [float(v) for v in eye.split(",")] if eye else []
    if len(got) >= 6:
        dx, dy = got[4], got[5]
        for role, (vv, tt) in list(parts.of.items()):
            moved = vv.copy()
            moved[:, 0] -= dx
            moved[:, 1] -= dy
            parts.of[role] = (moved, tt)
        # AND `agl_m` MEANS ABOVE THE GROUND UNDER THE EYE, not above the origin's. A town on a
        # hill puts those metres apart, and 1.7 m over the wrong one is either underground or a
        # first-floor window.
        # `Frame.z` ALREADY ANSWERS RELATIVE TO THE DATUM -- z(0, 0) is 0.00 by construction --
        # so subtracting the datum again put the eye 439 m underground (measured 2026-09-07).
        camera.agl_m += frame.z(dx, dy)
    blend.render({k: (v, t) for k, (v, t) in parts.of.items() if len(t)}, camera,
                 lab_camera.sun_direction(place["lat"], place["lon"], place["when"]),
                 str(shot), samples=LOOK_SAMPLES, looks=looks, engine=LOOK_ENGINE,
                 fields=counts.get("fields"))
    from PIL import Image
    img = np.asarray(Image.open(shot).convert("RGB"))
    share, dark = ink_share(img), dark_share(img)
    if share < INK_LEAST:
        red.append(f"empty({share * 100:.1f}% ink)")
    if dark > DARK_MOST:
        red.append(f"unlit({dark * 100:.1f}% black)")
    publish.take("places", place["name"], shot, red)
    tris = sum(len(t) for (_, t) in parts.of.values())
    print(f"{place['name']:14s} {'RED ' + ','.join(red) if red else 'ok':30s} "
          f"{'PLAN ' + str(int(place['span'])) + ' m' if place['plan'] else 'EYE  '} "
          f"bearing {place['bearing']:6.2f}  buildings {counts['buildings']:5d} "
          f"(-{counts['dropped']:3d})  ways {counts['ways']:4d}  "
          f"tris {tris:8d}  ink {share * 100:5.1f}%  black {dark * 100:4.1f}%  "
          f"datum {frame.datum:7.1f} m")
    return red


def main(argv):
    picked = [p for p in PLACES if not argv or any(a.lower() in p["name"].lower() for a in argv)]
    if not argv:
        publish.sweep("places")
    reds = 0
    for place in picked:
        try:
            reds += bool(one(place))
        except Exception as why:
            if os.environ.get("OUTSHINE_TRACE"):
                import traceback
                traceback.print_exc()
            print(f"{place['name']:14s} REFUSED {type(why).__name__}: {why}")
            reds += 1
    print(f"\n{len(picked)} place(s), {reds} red; pictures under {OUT}")
    return 1 if reds else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
