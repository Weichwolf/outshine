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
from shapely.ops import unary_union

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
import occlusion  # noqa: E402
import detail  # noqa: E402
import vector  # noqa: E402
import visible  # noqa: E402
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
GROUND_RINGS = 72                # a POLAR grid: rings times spokes, so no T-junction and no seam
GROUND_SPOKES = 96
GROUND_NEAR_M = 8.0
FAR_LAND_M = 24000.0             # how far the store's own vector tiles are asked for landcover
FAR_LAND_ZOOM = 11               # what `versatiles.osm` holds above 14; see `store.reach`
GROUND_TILT_MOST_DEG = 80.0      # [SET] steeper than any 25 m-posting DEM can carry; see `P ground`
GROUND_NEEDLE = 1e-3             # [SET] plan area over longest edge squared; under it the normal is noise
# A CEILING THAT MAY ONLY FALL -- AND IT IS A RATE, because the thing it bounds is a PROPERTY of
# the triangulation and not of one mesh's size. Declared as an absolute 32 it went red the moment
# the ground reached 240 km instead of 12 and grew from 17 531 faces to 28 949, while the needles
# per face FELL from 0.183 % to 0.128 %. A check that pins a number where the property is a rate
# is mis-specified, and CLAUDE.md says the CHECK changes.
GROUND_NEEDLE_RATE = 0.00183     # measured 2026-09-07: 32 of 17 531. It reads 0.128 % today
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

# THE GROUND REACHES AS FAR AS THE CLIENT SAYS IT SEES, and the number is the client's own:
# `kSightM`, 240 km (`Laying.cpp:400`). At 12 km the world ENDED inside the frame -- a dark band
# across the horizon that reads as a sea, looked at 2026-09-07 over Rothenburg. The fan's radii
# are geometric, so reaching twenty times further costs nothing at all: the same 72 rings and 96
# spokes, with `q` growing from 1.107 to 1.153. What it does cost is DEM tiles, and that is what
# `Frame.rung` answers.
GROUND_REACH_M = CAM["kSightM"]


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
        cam = lab_camera.Camera(place["lat"], place["lon"], agl_m=agl, bearing_deg=bearing,
                                pitch_deg=pitch, fov_deg=fov,
                                width=int(CAM["kWidePx"]), height=int(CAM["kHighPx"]))
        # WHERE THE EYE STANDS, ON THE CAMERA. The offset was parsed at the very end and applied
        # by translating the finished geometry, so everything BUILT -- the cull above all -- ran
        # from the origin while the picture was taken from somewhere else. A culler asked about
        # the wrong eye answers the wrong question and is worse than none.
        cam.at_xy = (got[4], got[5]) if len(got) >= 6 else (0.0, 0.0)
        return cam
    if place["plan"]:
        return lab_camera.Camera(place["lat"], place["lon"], bearing_deg=place["bearing"],
                                 pitch_deg=CAM["kOverheadPitchDeg"], fov_deg=CAM["kFovDeg"],
                                 width=int(CAM["kWidePx"]), height=int(CAM["kHighPx"]),
                                 plan_above_m=CAM["kPlanAboveM"], span_m=place["span"])
    return lab_camera.Camera(place["lat"], place["lon"], agl_m=CAM["kEyeAglM"],
                             bearing_deg=place["bearing"], pitch_deg=CAM["kPitchDeg"],
                             fov_deg=CAM["kFovDeg"], width=int(CAM["kWidePx"]),
                             height=int(CAM["kHighPx"]))


def eye_of(camera, frame):
    """Where the eye stands in the frame's own metres, and how high. ONE answer, so the culler,
    the road and the render cannot disagree about it."""
    x, y = getattr(camera, "at_xy", (0.0, 0.0))
    return (x, y), frame.z(x, y) + camera.agl_m


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

    # THE GROUND IS A CASCADE, WHICH IS WHY IT CAN REACH THE HORIZON AT ALL. `LayPatchwork` lays
    # 4 tiles at the finest zoom and doubles the span with every level one zoom coarser, so the
    # engine holds `kSightM` -- 240 km, `Laying.cpp:400` -- for a cost per level rather than per
    # square metre. The lab's polar fan already grades that way by construction; what it did NOT
    # do was READ the DEM that way, and sampling 240 km at zoom 14 is a hundred and fifty tiles
    # across. One rung coarser per doubling of the distance, which is the rule `Generate.h`
    # states for everything standing on the ground as well.
    FINEST_ZOOM = DEM_ZOOM
    COARSEST_ZOOM = 8          # what the engine's store actually holds at the top; see store.py

    def __init__(self, place):
        self.lat0, self.lon0 = place["lat"], place["lon"]
        self.dems = {}
        self.dem = self._dem(self.FINEST_ZOOM)
        self.per_lat = 111132.0
        self.per_lon = 111320.0 * math.cos(math.radians(self.lat0))
        self.datum = float(self.dem.at(self.lat0, self.lon0))

    def _dem(self, zoom):
        got = self.dems.get(zoom)
        if got is None:
            got = self.dems[zoom] = roaddata.Dem(zoom=int(zoom))
        return got

    def rung(self, away_m):
        """The zoom to read the ground at, `away_m` from the eye. The finest posting is 6.2 m at
        zoom 14 and this latitude; a rung coarser doubles it, and the fan's own ring spacing
        grows the same way, so the two stay matched all the way out."""
        if away_m <= self.NEAR_FINE_M:
            return self.FINEST_ZOOM
        steps = int(math.log2(away_m / self.NEAR_FINE_M))
        return max(self.COARSEST_ZOOM, self.FINEST_ZOOM - steps)

    NEAR_FINE_M = 400.0        # [SET] out to here the finest rung, which is the built reach

    def xy(self, lat, lon):
        return ((lon - self.lon0) * self.per_lon, (lat - self.lat0) * self.per_lat)

    # WHAT A HEIGHT ON EARTH CAN BE. The Dead Sea shore is -430 m and Everest is 8 849 m; a
    # sample outside this is not a low place, it is a broken tile. Measured 2026-09-07: three
    # 270-byte black PNGs west of Rothenburg decoded to terrarium's no-data, -32 768 m, and the
    # ground funnelled 33 200 m down. The picture rendered 0.7 % ink and every count in the run
    # was healthy -- 2 285 000 triangles built, none of them visible.
    LOWEST_M, HIGHEST_M = -500.0, 9000.0

    def z(self, x, y):
        lat, lon = self.lat0 + y / self.per_lat, self.lon0 + x / self.per_lon
        got = float(self._dem(self.rung(math.hypot(x, y))).at(lat, lon))
        if not (self.LOWEST_M <= got <= self.HIGHEST_M):
            raise RuntimeError(f"the ground at ({x:.0f}, {y:.0f}) reads {got:.1f} m, which is not "
                               f"a place on Earth -- a no-data tile is not a height")
        return got - self.datum


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
    """Every closed `building` way in the extract, READ -- footprint, tags, epoch, ground -- and
    not one of them meshed.

    THE CHECK USED TO LIVE HERE AND IT BUILT EVERY BODY. `watertight`, `winding` and `volume`
    each ask for the mesh, so a reader that checks is a builder: 69 s and 686 MB to answer a
    question about 1 054 bodies of which the frame shows fourteen, and the culler downstream had
    nothing left to save. A claim about every body in an extract belongs in a SWEEP with its own
    oracle -- `buildings/sweep.py` -- and the twin checks what it actually builds.

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
        bodies.append(b)
    return bodies, dropped


# THE SURFACE'S HALO, AND IT IS DERIVED RATHER THAN MEASURED. A kerb, a marking and the wear are
# LOCAL operators: what the road looks like at a point is decided by the ways within
# `max half width + corner radius + footway` -- 3.75 + 8.0 + 2.5 by RASt 06 -- so a way whose
# nearest visible point is further than that cannot change anything the eye is looking at. 25 m
# is that 14.25 m rounded up, and it is a different quantity from the ALIGNMENT's halo, which is
# set by how far a profile's stiffness reaches and shares no number with it.
SURFACE_HALO_M = 25.0
SAMPLE_M = 10.0                  # how finely a centre line is asked "can any of you be seen"


def kerb_reach_m(camera):
    """HOW FAR A KERB IS STILL A KERB, by the door's own rule and not by a taste.

    `Generate.h: Unseen(errorM, focalPx, awayM)` says a feature that MOVES geometry by `errorM` is
    invisible once `errorM * focalPx <= kErrorPx * awayM`. A kerb's upstand is RASt 06's 0.12 m and
    the focal length at 55 degrees over 1280 px is 1229 px, so a kerb is under one pixel beyond

        0.12 * 1229 / 1.0 = 147 m                                              [derived]

    Beyond that the carriageway is still drawn -- asphalt is metres wide -- but its EDGE furniture
    is not, because none of it can be told from a painted line at that range. The same arithmetic
    is what board:2163 asks the whole ladder to be built on. Measured 2026-09-07: the kerb ring
    over everything the eye could reach at OldTown cost 83.2 s of a 3-minute picture."""
    focal = detail.focal_px(camera.fov_deg, camera.width)
    return kerbline.KERB_UP_M * focal / detail.ERROR_PX


def seen_of(mesh_map, structure, horizon, reach_m=None):
    """WHICH WAYS AND WHICH JUNCTIONS THE EYE CAN REACH. The alignment is already solved over the
    whole extract and baked, so this cuts the SURFACE only.

    CONSERVATIVE AT EVERY STEP, because a culler may only err towards drawing: a way is sampled
    every `SAMPLE_M` and asked at its HIGHEST solved node -- the height that is hardest to hide --
    and everything within `SURFACE_HALO_M` of a station that IS seen is kept as well, so no local
    operator loses an input it needed."""
    from scipy.spatial import cKDTree
    ex, ey = horizon.eye
    ways, nodes, lit = set(), set(), []
    for w in structure.net.ways:
        refs = w["refs"]
        if len(refs) < 2:
            continue
        pts = np.array([structure.net.nodes[r] for r in refs], dtype=float)
        step = np.hypot(*(pts[1:] - pts[:-1]).T)
        run = np.concatenate(([0.0], np.cumsum(step)))
        if run[-1] <= 0.0:
            continue
        want = np.arange(0.0, run[-1] + SAMPLE_M, SAMPLE_M)
        sx = np.interp(want, run, pts[:, 0])
        sy = np.interp(want, run, pts[:, 1])
        top = float(max(mesh_map.z[mesh_map.index[r]] for r in refs))
        got = horizon.sees(sx, sy, np.full(len(sx), top))
        if reach_m is not None:
            got &= np.hypot(sx - ex, sy - ey) <= reach_m
        if got.any():
            ways.add(w["id"])
            lit.append(np.column_stack((sx[got], sy[got])))
    if not lit:
        return ways, nodes
    seen_at = cKDTree(np.vstack(lit))
    for nid in structure.polygons:
        if seen_at.query(structure.net.nodes[nid])[0] <= SURFACE_HALO_M:
            nodes.add(nid)
    for w in structure.net.ways:
        if w["id"] in ways or len(w["refs"]) < 2:
            continue
        pts = np.array([structure.net.nodes[r] for r in w["refs"]], dtype=float)
        if seen_at.query(pts)[0].min() <= SURFACE_HALO_M:
            ways.add(w["id"])
    return ways, nodes


def roads_of(place, frame, red, horizon=None, camera=None):
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
    st = roadbed.Structure(m)
    # ONE VISIBILITY SET, READ BY EVERY SURFACE OPERATOR. It hangs on the Structure because the
    # kerb, the channel and the footway all reach for it and threading it through six signatures
    # would let two of them disagree about what is seen.
    st.seen = None if horizon is None else seen_of(m, st, horizon)
    # AND THE EDGE FURNITURE IS A FINER SET THAN THE CARRIAGEWAY. A road stays a road to the
    # horizon; its kerb, its channel and its footway stop where they fall under a pixel.
    st.fine = None if horizon is None else seen_of(m, st, horizon, kerb_reach_m(camera))
    return roadbed.Mesh(st, seen=st.seen), kept


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
        self.look = {}
        self.field = {}

    def add(self, role, verts, tris, material=None, colours=None, field=None):
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
        held = self.held.setdefault(key, {"v": [], "t": [], "c": [], "f": [], "at": 0,
                                          "m": material, "worn": False,
                                          "name": role.split(".")[0]})
        held["v"].append(v)
        held["t"].append(t + held["at"])
        held["at"] += len(v)
        held["c"].append(np.zeros((len(v), 4), dtype=np.float32) if colours is None
                         else np.asarray(colours, dtype=np.float32).reshape(-1, 4))
        # THE WEATHERING FIELD RIDES WITH THE VERTICES IT BELONGS TO. Kept in a dict beside the
        # parts it was keyed by the CALLER's role while the parts are keyed by the part, and the
        # renderer looked it up under a name that no longer existed.
        held["f"].append(np.zeros((len(v), 3), dtype=np.float32) if field is None
                         else np.asarray(field, dtype=np.float32).reshape(-1, 3))
        if field is not None:
            held["worn"] = True
        if material is not None:
            held["m"] = material

    def close(self):
        """Every surface's pieces become ONE part -- which is what a draw call is."""
        for key, held in self.held.items():
            if key in self.at:
                continue
            part = self.geom.addPart(held["name"], held["m"])
            self.at[key] = part
            self.look[part] = held["m"]
            if held["worn"]:
                self.field[part] = np.concatenate(held["f"])
            self.geom.setPositions(part, np.concatenate(held["v"]))
            self.geom.setTriangles(part, np.concatenate(held["t"]))
            if any(c.any() for c in held["c"]):
                self.geom.setColours(part, np.concatenate(held["c"]))
        self.held = {}
        return self.geom

    def _named(self, part):
        return f"{self.geom.nameOf(part)}_{part}"

    @property
    def looks(self):
        """THE MATERIAL PER PART, UNDER THE PART'S OWN NAME. `of` names a part
        `<role>_<index>` and the look was collected under the CALLER's role, so the renderer
        looked up `roof` where the part was called `roof_44`, found nothing, and SKIPPED it --
        silently, for every part, in every place. Measured 2026-09-07: 760 031 triangles handed
        over, no mesh file written, Blender rendering an empty sky in 3.3 s, and every geometric
        check green because the geometry was perfect and never drawn.

        One source for the name, and it is the part."""
        self.close()
        return {self._named(p): m for p, m in self.look.items() if m is not None}

    @property
    def fields(self):
        """The per-vertex weathering field per part, under the same name as `of`."""
        self.close()
        return {self._named(p): f for p, f in self.field.items()}

    @property
    def of(self):
        """The twin's own view: {role: (positions as (N, 3), triangles as (M, 3))}.

        IT IS A VIEW AND IT IS READ-ONLY. Built fresh on every read, `parts.of[role] = ...`
        writes into a dict that is thrown away on the next line -- and `place()` moved the whole
        scene under the camera exactly that way. The move was silently lost whenever the parts
        came from a BUILD and silently applied whenever they came from the CACHE, so the same
        command rendered the alley or the place's origin depending on whether a pickle existed.
        Two street-level pictures were judged before the difference between them was noticed
        (2026-09-07). Use `move` to translate; there is no other way in."""
        self.close()
        out = {}
        for part in range(self.geom.parts()):
            out[self._named(part)] = (
                self.geom.positionsOf(part).reshape(-1, 3),
                self.geom.trianglesOf(part).reshape(-1, 3).astype(np.int64))
        return out

    def move(self, dx, dy, dz=0.0):
        """Translate every part in place -- the one way the scene is moved under the camera."""
        self.close()
        for part in range(self.geom.parts()):
            v = self.geom.positionsOf(part).reshape(-1, 3)
            v[:, 0] -= dx
            v[:, 1] -= dy
            v[:, 2] -= dz

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


def cached_parts(place, frame, doc, red, lod=3, camera=None):
    """THE PLACE'S GEOMETRY, BUILT ONCE. The road bed's solve is 75 percent of the time -- 260 s
    of 346 on a 400 m extract of OldTown, profiled 2026-09-06 -- and it is DETERMINISTIC, so
    rebuilding it to try another exposure or another palette is time spent proving something
    already proved. The key is the place, the reaches and the mtime of every source in the lab,
    so a repair invalidates it and nothing else does."""
    import pickle
    # THE KEY CARRIES THE CAMERA, because what is BUILT now depends on what is SEEN. A cache
    # named without it would serve a street's fourteen bodies to an aerial view.
    eye = "" if camera is None else \
        f"-{camera.agl_m:.1f},{camera.bearing_deg:.1f},{camera.pitch_deg:.1f},{camera.fov_deg:.1f}" \
        + os.environ.get("OUTSHINE_EYE", "")
    key = CACHE / f"{place['name']}-{lod}{eye}-{_fingerprint()}.pickle"
    if key.exists():
        try:
            held, looks, counts = pickle.loads(key.read_bytes())
            return Baked(held), looks, counts
        except Exception:
            key.unlink(missing_ok=True)
    parts, looks, counts = parts_of(place, frame, doc, red, lod, camera)
    if not red:
        key.parent.mkdir(parents=True, exist_ok=True)
        key.write_bytes(pickle.dumps((parts.of, looks, counts)))
    return parts, looks, counts


class Baked:
    """A BAKE READ BACK, and it does NOT go through `Parts` again. Rebuilding one by calling
    `add` for every entry of `of` re-keys the parts -- the material is gone by then, so every
    part sharing a name collapses into one and the looks no longer line up with it. A cache that
    changes what it returns is not a cache."""

    def __init__(self, held):
        self.of = dict(held)

    def move(self, dx, dy, dz=0.0):
        for role, (v, t) in self.of.items():
            moved = v.copy()
            moved[:, 0] -= dx
            moved[:, 1] -= dy
            moved[:, 2] -= dz
            self.of[role] = (moved, t)


PARALLEL_CORES = int(os.environ.get("OUTSHINE_CORES", "0")) or os.cpu_count() or 1
PARALLEL_LEAST = 24              # below this the pool costs more to start than the work is worth
_MESH_JOB = None                 # (bodies, lod, fov) -- inherited by fork, never pickled


def _mesh_one(at):
    """ONE BODY, MESHED AND CHECKED, in whichever process picks it up. Returns everything the
    parent needs and nothing it does not: the arrays, the materials, and the findings.

    WHAT IS BUILT IS CHECKED, and the check runs HERE. The sweep owns the claim about every body
    in the extract; this owns the claim about every body in the PICTURE, and a body meshed in a
    child is checked in that child or it is not checked at all."""
    bodies, lod, fov = _MESH_JOB
    b = bodies[at]
    found = []
    if not b.watertight():
        found.append(f"B-closed({b.tags.get('name', b.poly.centroid.wkt)})")
    wrong, degenerate, _ = b.winding()
    if wrong or degenerate:
        found.append(f"B-wound({wrong}e,{degenerate}deg)")
    rung = min(lod, visible.rung_for(math.hypot(b.poly.centroid.x, b.poly.centroid.y), fov))
    return at, b.body(rung), b.materials(), found


def parts_of(place, frame, doc, red, lod=3, camera=None):
    """THE PLACE AS GEOMETRY BY ROLE, which is what a look can be judged from.

    `scene_of` below builds the same world for the FLAT rasteriser, one colour per role, because
    that instrument's job is to show a crack. This one keeps every body's own palette and its
    roof's own covering, and hands the street its kerb, its gutter, its footway, its markings and
    its lamps -- which is where a large share of what a player sees at eye level actually is."""
    parts = Parts()

    def put(role, verts, tris, rgb, field=None):
        parts.add(role, verts, tris, material=rgb, field=field)

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
    # THE OCCLUDERS FIRST, BECAUSE THE ROAD IS CULLED BY THEM TOO. The bodies are READ from the
    # extract -- footprint, tags, epoch, ground -- and nothing is meshed by reading them, which is
    # what lets the quadtree stand in front of every generator rather than only in front of the
    # buildings. `visible` walks front to back and leaves the horizon COMPLETE, and a completed
    # horizon answers about any point on the ground: see `Horizon.sees`.
    bodies, dropped = buildings_of(place, frame, doc, red)
    boxes = np.array([b.poly.bounds for b in bodies]) if bodies else np.zeros((0, 4))
    tops = np.array([b.ridge for b in bodies]) if bodies else np.zeros(0)
    tree = occlusion.Quadtree(boxes, tops)
    at_xy, at_z = eye_of(camera, frame)
    horizon = occlusion.Horizon(at_xy, at_z, camera.bearing_deg, camera.fov_deg)
    keep, node_tests, leaf_tests = occlusion.visible(tree, horizon)
    # THE CULLER HAS AN OFF SWITCH BECAUSE ITS PROOF NEEDS ONE. board:2162's P1 says the culled
    # picture equals the uncut one pixel for pixel, and a claim like that is a COMPARISON or it
    # is nothing. `OUTSHINE_NOCULL=1` builds everything and renders it, which is the other half.
    if os.environ.get("OUTSHINE_NOCULL"):
        keep, horizon = set(range(len(bodies))), None
    took(f"cull {len(keep)}/{len(bodies)} {node_tests}+{leaf_tests}")

    mesh, ways = roads_of(place, frame, red, horizon, camera)
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
    patches = list(surfaces.regions(doc, frame, GROUND_REACH_M, street_face))
    # AND THE FAR FIELD, FROM THE ENGINE'S OWN TILES. The extract is an Overpass query on a
    # RADIUS and the ground now reaches 240 km, so beyond the extract the world was a uniform
    # green plain where Franconia is forest, farmland, meadow and vineyard -- looked at,
    # 2026-09-07. A radius cannot fetch that (a hundred and fifty tiles across) and does not need
    # to: `versatiles.osm` is already on this disk, gridded, at the zoom the distance asks for.
    near = unary_union([g for (_, g) in patches] + ([street_face] if street_face is not None else []))
    have, want = vector.held(frame, FAR_LAND_M, FAR_LAND_ZOOM)
    for role, got in vector.land(frame, FAR_LAND_M, FAR_LAND_ZOOM):
        cut = got if near.is_empty else got.difference(near)
        if not cut.is_empty and cut.area > 0:
            patches.append((role, cut))
    took(f"surfaces {have}/{want} far tiles")
    over = surfaces.check_no_overlap(patches, street_face)
    if over > 1.0:
        red.append(f"I22 surfaces overlap {over:.1f} m2")
    if street_face is None and not patches:
        put("ground", *ground_fan(frame), stock.STOCK["grass"])
    else:
        # ONE FRAME FOR BOTH HEIGHTS. `z_edge` is the DRAWN street's own surface and carries the
        # DEM's metres above the sea; `frame.z` is relative to the datum under the camera. Handed
        # in as they stood, the sheet mixed the two and `drop` then took the datum off both: the
        # street came out right and the fan came out 432.2 m under it. Measured 2026-09-07 at
        # OldTown -- a ground vertex at (-7148, 8151) read -572.6 m where `frame.z` says -140.4,
        # and the difference IS the datum. In the picture it was a vertical green cliff with the
        # street floating on its edge, and every case was GREEN.
        sheet = lab_ground.surface(lambda x, y: frame.z(x, y) + frame.datum, street_face,
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
        # THE WEATHERING FIELD, and every channel of it is a consequence: a tyre polished the
        # wheel paths, water left its silt in the last half metre before the kerb. It rides WITH
        # the vertices, so it cannot be looked up under a name the part does not have.
        put("road", road, faces, stock.STOCK["asphalt"], field=wear.carriageway(mesh, mesh.map))
        took("wear")
        colour = {"kerb": stock.STOCK["kerbstone"], "gutter": stock.STOCK["asphalt"],
                  "walk": stock.STOCK["paving"], "paint": stock.STOCK["paint"],
                  "metal": stock.STOCK["iron"], "lamp": stock.STOCK["steel"],
                  "iron": stock.STOCK["iron"]}
        for (role, vv, tt) in kerbline.street_edge(mesh.map, mesh.st, surface, sites, FINE_ROAD_M):
            put(role, drop(vv), tt, colour.get(role) or stock.STOCK["concrete"])
        took("kerb ring")
        walk = kerbline.walk_area(mesh.map, mesh.st)
        # AND THE MARKINGS ARE DRAWN ON THE ROAD THAT WAS DRAWN. A line painted on a carriageway
        # that was never meshed is paint in the air over the terrain.
        for w in mesh.net.ways:
            if mesh.seen is not None and w["id"] not in mesh.seen[0]:
                continue
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

    # SIX CORES, AND THE BODIES ARE INDEPENDENT. Two performance cores and four efficiency ones
    # on this machine, and the GIL means a thread pool would use exactly one of them for work
    # that is arithmetic and shapely. A PROCESS pool uses all six, and meshing 1 330 bodies is
    # what a pool is for: nothing a body builds is read by another one.
    #
    # TWO RULES IT MAY NOT BREAK. `CLAUDE.md`: anything assembled from work that ran on more than
    # one worker is combined in a DECLARED order and never in completion order -- `Pool.imap` with
    # a chunked map keeps the input's order, so the parts go in exactly as they would have. And
    # `fork` rather than `spawn`: the children inherit the bodies, the frame and its DEM cache
    # copy-on-write, so only the finished ARRAYS cross a pipe. Pickling the inputs would cost
    # more than the pool buys.
    want = [at for at in range(len(bodies)) if at in keep]
    global _MESH_JOB
    _MESH_JOB = (bodies, lod, camera.fov_deg)
    done = None
    if len(want) >= PARALLEL_LEAST and not os.environ.get("OUTSHINE_NOPOOL"):
        try:
            import multiprocessing as mp
            with mp.get_context("fork").Pool(PARALLEL_CORES) as pool:
                done = pool.map(_mesh_one, want, chunksize=max(1, len(want) // (PARALLEL_CORES * 8)))
        except Exception as why:                       # a pool that will not start is not a defect
            print(f"    pool refused ({type(why).__name__}: {why}); one core", flush=True)
            done = None
    if done is None:
        done = [_mesh_one(at) for at in want]
    for at, made, mats, findings in done:
        for note in findings:
            red.append(note)
        foot = float(min(v[2] for (vv, _) in made.values() for v in vv)) if made else 0.0
        for role, (vv, tt) in made.items():
            up = np.asarray(vv, dtype=np.float32).reshape(-1, 3)[:, 2]
            field = np.zeros((len(up), 3), dtype=np.float32)
            field[:, 2] = np.clip((up - foot) / blend.HEIGHT_SCALE_M, 0.0, 1.0)
            put(f"{role}.{at}", vv, tt, mats.get(role) or (0.35, 0.33, 0.30), field=field)
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
    return parts, parts.looks, dict(ways=ways, buildings=len(bodies), dropped=dropped,
                                    fields=parts.fields)


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
    parts, looks, counts = cached_parts(place, frame, doc, red, lod=3, camera=camera)
    if os.environ.get("OUTSHINE_CLAY"):
        # A CLAY RENDER: every material the same matte grey, which is what a modeller looks at
        # when the question is the FORM. Twice in one session a defect was blamed on the
        # material and twice the material was innocent; a picture with no material in it cannot
        # be argued with (2026-09-07).
        clay = stock.Material("clay", (0.42, 0.41, 0.40), roughness=0.92)
        looks = {k: clay for k in looks}
    if any(not np.isfinite(v).all() for (v, _) in parts.of.values()):
        red.append("P finite")
    # AND EVERY VERTEX STANDS ON EARTH. `P finite` passes on -33 200 m: it is a perfectly finite
    # number. The claim that catches a no-data tile is a PHYSICAL one, and it is stated over what
    # was BUILT rather than over what was sampled, so a height that reaches the geometry by any
    # route at all is caught.
    low = Frame.LOWEST_M - frame.datum - 200.0
    high = Frame.HIGHEST_M - frame.datum + 200.0
    for role, (v, t) in parts.of.items():
        if not len(t) or not len(v):
            continue
        zmin, zmax = float(np.min(v[:, 2])), float(np.max(v[:, 2]))
        if zmin < low or zmax > high:
            red.append(f"P earth({role} {zmin:.0f}..{zmax:.0f} m)")
            break
    # AND THE GROUND IS NOT A WALL. A terrain face standing within a degree of vertical is not a
    # slope: the steepest ground on Earth is a cliff and a DEM whose postings are 25 m apart
    # cannot resolve one, so 90 degrees means two heights in one place rather than a mountain.
    # Measured 2026-09-07: the ground's tilt read p99 89.7 and max 90.0 while `ink`, `black`,
    # `P earth` and every geometric check stayed green and the picture was published.
    # [SET] 80 degrees: a 25 m posting would need a 142 m step between neighbours to reach it,
    # which is steeper than anything a DEM of this class carries.
    for role, (v, t) in parts.of.items():
        if not role.startswith("g_") or not len(t):
            continue
        a, b, c = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
        n = np.cross(b - a, c - a)
        run = np.linalg.norm(n, axis=1)
        live = run > 1e-12
        if not live.any():
            continue
        # A NEEDLE IS NOT A CLIFF, and telling them apart is the whole point. A face whose three
        # points are nearly collinear IN PLAN has a normal made of noise: measured 2026-09-07,
        # eight faces of 17 531 read 85 to 90 degrees while carrying 0.06 to 3.10 m of fall over
        # spans of 0.7 to 43 m -- slopes of two degrees. Judged by the normal alone the oracle
        # would report a wall that is not there and stay silent about the one that is. The tilt
        # is therefore read only where the face has AREA in plan, and the needles are counted.
        flat = 0.5 * np.abs(n[live, 2])
        edge = np.maximum(np.maximum(np.linalg.norm((b - a)[live, :2], axis=1),
                                     np.linalg.norm((c - b)[live, :2], axis=1)),
                          np.linalg.norm((a - c)[live, :2], axis=1))
        solid = flat > GROUND_NEEDLE * edge ** 2
        needles = int((~solid).sum())
        if solid.any():
            tilt = np.degrees(np.arccos(np.clip(np.abs(n[live, 2][solid]) / run[live][solid],
                                                0.0, 1.0)))
            if float(tilt.max()) > GROUND_TILT_MOST_DEG:
                red.append(f"P ground({role} tilt {tilt.max():.0f} deg, "
                           f"{int((tilt > GROUND_TILT_MOST_DEG).sum())} faces)")
                break
        rate = needles / max(int(live.sum()), 1)
        if rate > GROUND_NEEDLE_RATE:
            red.append(f"P needle({role} {needles} of {int(live.sum())}, {rate * 100:.3f} %)")
            break
    OUT.mkdir(parents=True, exist_ok=True)
    shot = OUT / f"{place['name']}.png"
    # AND THE EYE MAY STAND SOMEWHERE ELSE. A place's origin is a coordinate a surveyor chose,
    # and in a dense old town it is usually INSIDE a block: a camera at 1.7 m there renders 100 %
    # black, which is what a street-level look at OldTown gave until the eye could be moved.
    eye = os.environ.get("OUTSHINE_EYE", "")
    got = [float(v) for v in eye.split(",")] if eye else []
    if len(got) >= 6:
        dx, dy = got[4], got[5]
        parts.move(dx, dy)
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
