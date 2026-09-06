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

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE / "roads"))
import importlib.util as _util  # noqa: E402

import data as roaddata  # noqa: E402
import publish  # noqa: E402
import render as lab_render  # noqa: E402

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
BUILT_REACH_M = 2500.0           # buildings: as far as the lens resolves a body at all
ROAD_REACH_M = 700.0             # roads: one convex solve over the extract, and it grows with it
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
    """The lab camera that stands exactly where the client's does."""
    if place["plan"]:
        return lab_render.Camera(place["lat"], place["lon"], bearing_deg=place["bearing"],
                                 pitch_deg=CAM["kOverheadPitchDeg"], fov_deg=CAM["kFovDeg"],
                                 width=int(CAM["kWidePx"]), height=int(CAM["kHighPx"]),
                                 plan_above_m=CAM["kPlanAboveM"], span_m=place["span"])
    return lab_render.Camera(place["lat"], place["lon"], agl_m=CAM["kEyeAglM"],
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
    query = ("[out:json][timeout:300];("
             f'way["building"]({bbox});way["building:part"]({bbox});'
             f'way["highway"]({bbox});way["railway"]({bbox});'
             ");(._;>;);out body;")
    name = f"place-{place['name']}-{int(reach_m)}.json"
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

def ground_fan(frame, scene):
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
    scene.add(verts, tris, GROUND_COLOUR)
    up = 0
    for (ia, ib, ic) in tris:
        pa, pb, pc = (np.asarray(verts[i], dtype=float) for i in (ia, ib, ic))
        up += float(np.cross(pb - pa, pc - pa)[2]) > 0.0
    if up != len(tris):
        raise RuntimeError(f"the ground fan has {len(tris) - up} of {len(tris)} faces turned "
                           f"away from the sky -- a ground the camera sees the underside of")
    return len(tris)


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
    bodies = []
    for (poly, tags) in made:
        b = bldbed.Building(poly, tags, ground, where=where)
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
    return roadbed.Mesh(roadbed.Structure(m)), kept


# ------------------------------------------------------------------ the picture

class Parts:
    """GEOMETRY BY ROLE, not by body. A wall and the roof over it are two materials and that one
    split is most of what a town reads as: measured by looking, 2026-09-06, the outshine client's
    own OldTown carries terracotta over cream and the lab twin carried one brick colour for
    everything, which is why the twin looked a generation behind its own C++."""

    def __init__(self):
        self.of = {}

    def add(self, role, verts, tris):
        v, t = self.of.setdefault(role, ([], []))
        base = len(v)
        v.extend([tuple(map(float, p)) for p in verts])
        t.extend([(a + base, b + base, c + base) for (a, b, c) in tris])

    def scene(self, colours):
        scene = lab_render.Scene()
        for role, (v, t) in self.of.items():
            scene.add(v, t, colours[role])
        return scene

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


def scene_of(place, frame, doc, red):
    scene = lab_render.Scene()
    ground_fan(frame, scene)
    mesh, ways = roads_of(place, frame, red)
    if mesh is not None:
        # THE ROAD BED'S DATUM IS SEA LEVEL AND THE FRAME'S IS THE CAMERA'S GROUND. Two datums in
        # one scene is the second holder CLAUDE.md forbids, so the surface is brought to the
        # frame's here, once, at the boundary -- and lifted a hair so a carriageway at grade wins
        # the depth test against the terrain that carries it rather than fighting it per pixel.
        lift = 0.05
        # AND THE ROAD BED'S WINDING IS THE ROAD BED'S. Its surface is a strip per leg and a fan
        # per junction, wound however each was built; drawn here with one normal per face, the
        # ones turned away from the sky came out BLACK -- hairlines of ink across the terrain in
        # the first OldTown twin (measured 2026-09-06). A carriageway is ground: every face of it
        # faces up, and the flip happens once, here, at the boundary between the two beds.
        road = [(v[0], v[1], v[2] - frame.datum + lift) for v in mesh.vertices]
        faces = []
        for (ia, ib, ic) in mesh.tris:
            pa, pb, pc = (np.asarray(road[i], dtype=float) for i in (ia, ib, ic))
            faces.append((ia, ib, ic) if float(np.cross(pb - pa, pc - pa)[2]) > 0.0
                         else (ia, ic, ib))
        scene.add(road, faces, ROAD_COLOUR)
    bodies, dropped = buildings_of(place, frame, doc, red)
    for b in bodies:
        scene.add(b.vertices, b.tris, WALL_COLOUR.get(b.style.wall, WALL_COLOUR["brick"]))
    return scene, dict(ways=ways, buildings=len(bodies), dropped=dropped)


def look(place, scene, camera):
    """The picture, with the sun where the place and the client's own instant put it."""
    sun = lab_render.sun_direction(place["lat"], place["lon"], place["when"])
    if sun[2] < 0.02:
        # the client's instant is local NOON at every place, so a sun below the horizon means the
        # instant and the longitude disagree -- worth saying rather than rendering a black frame
        print(f"   note: the sun stands {math.degrees(math.asin(max(-1.0, min(1.0, sun[2])))):.1f} "
              f"deg at {place['when']}")
    return lab_render.render(scene, camera, sun, ambient=0.32, near_m=0.5)


def ink_share(img):
    """How much of the frame is NOT the sky. A twin whose extract failed renders a clean gradient
    and every geometric check stays green, because there is no geometry to be wrong -- which is
    the trap CLAUDE.md names as `a gate blind to a path`."""
    flat = img.reshape(-1, 3).astype(np.int16)
    rows = np.repeat(np.arange(img.shape[0]), img.shape[1])
    band = (lab_render.SKY_LOW[None, :] * (1.0 - (rows / max(img.shape[0] - 1, 1))[:, None])
            + lab_render.SKY_HIGH[None, :] * (rows / max(img.shape[0] - 1, 1))[:, None]) * 255.0
    return float(np.mean(np.abs(flat - band).max(axis=1) > 6))


INK_LEAST = 0.02
DARK_MOST = 0.06          # [SET] a face turned from the sky renders near black; a twin has few


def dark_share(img):
    """How much of the frame is near BLACK. `ink_share` counts everything that is not the sky and
    a black ground is not the sky, so it read 59.6 % on a twin whose whole terrain was unlit
    (measured 2026-09-06). Flat shading gives an up-facing face at least the ambient term, so a
    large black area means faces turned away from the sky -- a mesh handed over inside out."""
    return float(np.mean(img.reshape(-1, 3).max(axis=1) < 24))


def one(place):
    red = []
    frame = Frame(place)
    doc = overpass(place, BUILT_REACH_M)
    scene, counts = scene_of(place, frame, doc, red)
    camera = camera_for(place)
    if place["plan"]:
        # the plan camera is stated ABOVE SEA LEVEL (`SamplesHeight` is false for it), so the
        # frame's own datum is what turns that into a height over this ground
        camera.agl_m = CAM["kPlanAboveM"] - frame.datum
    img = look(place, scene, camera)
    share = ink_share(img)
    dark = dark_share(img)
    if share < INK_LEAST:
        red.append(f"empty({share * 100:.1f}% ink)")
    if dark > DARK_MOST:
        red.append(f"unlit({dark * 100:.1f}% black)")
    verts = np.asarray(scene.vertices, dtype=float)
    if verts.size and not np.isfinite(verts).all():
        red.append("P finite")
    OUT.mkdir(parents=True, exist_ok=True)
    shot = OUT / f"{place['name']}.png"
    lab_render.save(img, shot)
    publish.take("places", place["name"], shot, red)
    print(f"{place['name']:14s} {'RED ' + ','.join(red) if red else 'ok':30s} "
          f"{'PLAN ' + str(int(place['span'])) + ' m' if place['plan'] else 'EYE  '} "
          f"bearing {place['bearing']:6.2f}  buildings {counts['buildings']:5d} "
          f"(-{counts['dropped']:3d})  ways {counts['ways']:4d}  "
          f"tris {len(scene.tris):7d}  ink {share * 100:5.1f}%  black {dark * 100:4.1f}%  "
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
            print(f"{place['name']:14s} REFUSED {type(why).__name__}: {why}")
            reds += 1
    print(f"\n{len(picked)} place(s), {reds} red; pictures under {OUT}")
    return 1 if reds else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
