"""The map's inputs, fetched once and cached: OSM ways by Overpass, the DEM as terrarium tiles.

The engine reads the same OSM through versatiles and the same terrarium tiles at zoom 14
(GroundStack: the finest elevation zoom, 15, minus one), so what this lab sees is what the
engine sees, decoded by numpy instead of by the tree.
"""
import json
import math
import os
import pathlib
import sys
import urllib.parse
import urllib.request

import numpy as np
from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import store  # noqa: E402

CACHE = pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-lab"
ZOOM = int(os.environ.get("OUTSHINE_LAB_ZOOM", "12"))  # the engine samples FinestZoomOf(Elevation) - 1
TILE_PX = 256
OVERPASS = "https://overpass-api.de/api/interpreter"
TERRARIUM = "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png"

PLACES = {
    "OldTown": (49.3777, 10.179),
    "Heidelberg": (49.4147, 8.6968),
    "Kaiserberg": None,
}


def fetch(url, into, data=None):
    into.parent.mkdir(parents=True, exist_ok=True)
    if into.exists():
        return into.read_bytes()
    request = urllib.request.Request(url, data=data, headers={"User-Agent": "outshine-lab"})
    with urllib.request.urlopen(request, timeout=300) as answer:
        held = answer.read()
    into.write_bytes(held)
    return held


def overpass_ways(lat, lon, half_m):
    dlat = half_m / 111320.0
    dlon = half_m / (111320.0 * math.cos(math.radians(lat)))
    bbox = f"{lat - dlat},{lon - dlon},{lat + dlat},{lon + dlon}"
    query = f'[out:json][timeout:180];way["highway"]({bbox});(._;>;);out body;'
    name = f"overpass-{lat:.4f}-{lon:.4f}-{int(half_m)}.json"
    held = fetch(OVERPASS, CACHE / name, data=urllib.parse.urlencode({"data": query}).encode())
    doc = json.loads(held)
    nodes = {e["id"]: (e["lat"], e["lon"]) for e in doc["elements"] if e["type"] == "node"}
    ways = [e for e in doc["elements"] if e["type"] == "way"]
    return nodes, ways


def tile_of(lat, lon, zoom=ZOOM):
    n = 2 ** zoom
    x = (lon + 180.0) / 360.0 * n
    y = (1.0 - math.log(math.tan(math.radians(lat)) + 1.0 / math.cos(math.radians(lat))) / math.pi) / 2.0 * n
    return x, y


class Dem:
    """Terrarium tiles at one zoom, bilinear between postings -- the engine's TileHeightAslM."""

    def __init__(self, zoom=ZOOM):
        self.zoom = zoom
        self.tiles = {}

    # TERRARIUM'S OWN NO-DATA VALUE. `(0,0,0)` decodes to `0*256 + 0 + 0/256 - 32768`, so a tile
    # the server answers with a black placeholder reads as 32 768 metres below the sea. Measured
    # 2026-09-07: three tiles west of Rothenburg came back as 270-byte black PNGs, were cached as
    # if they were heights, and put the terrain 33 200 m under the town -- the picture was a
    # funnel and the frame rendered 0.7 % ink. `Generate.h` states the rule this broke: "An empty
    # answer is not zero: a tile that has not arrived and a sea-level plain are different answers."
    NO_DATA_M = -32768.0

    def _sane(self, grid):
        """A tile that is ENTIRELY no-data is not a tile. One no-data posting in a real tile is a
        void the neighbours fill; a whole tile of them is the server saying it has nothing."""
        return grid is not None and not bool(np.all(grid <= self.NO_DATA_M + 1e-6))

    def _magnified(self, got, tx, ty):
        """ANCESTOR FILL, as `terrarium.s3` declares it and `ContentStore` performs it: the tile's
        window inside the nearest held ancestor, magnified. Nearest neighbour, because a DEM
        posting is a SAMPLE and inventing a smoother one between two of them is a height nobody
        measured."""
        az, ax, ay, raw = got
        step = 2 ** (self.zoom - az)
        parent = store.heights(raw)
        if not self._sane(parent):
            return None
        wide = TILE_PX // step
        if wide < 1:
            return None
        x0 = (tx - ax * step) * wide
        y0 = (ty - ay * step) * wide
        window = parent[y0:y0 + wide, x0:x0 + wide]
        if window.shape != (wide, wide):
            return None
        return np.kron(window, np.ones((step, step)))

    def tile(self, tx, ty):
        """THE ENGINE'S OWN BYTES FIRST. `src/world/data/ContentStore.cpp` already keeps every
        tile the client ever fetched, under a key the lab can derive itself -- so the lab reads
        THAT rather than downloading a second copy, and a height the lab and the client disagree
        about cannot be blamed on two different downloads.

        THE ORDER IS: the store at this zoom, then a fetch, then the store's nearest ANCESTOR
        magnified. The last is not a nicety -- it is what stands between a black placeholder and
        a hole 33 km deep in the ground."""
        key = (tx, ty)
        if key in self.tiles:
            return self.tiles[key]
        got = store.ancestor("elevation", self.zoom, tx, ty)
        if got is not None and got[0] == self.zoom:
            grid = store.heights(got[3])
            if self._sane(grid):
                self.tiles[key] = grid
                return grid
        held = CACHE / "terrarium" / str(self.zoom) / str(tx) / f"{ty}.png"
        grid = None
        try:
            fetch(TERRARIUM.format(z=self.zoom, x=tx, y=ty), held)
            rgb = np.asarray(Image.open(held).convert("RGB")).astype(np.float64)
            grid = rgb[:, :, 0] * 256.0 + rgb[:, :, 1] + rgb[:, :, 2] / 256.0 - 32768.0
        except Exception:
            grid = None
        if not self._sane(grid):
            # AND IT IS NOT KEPT. A placeholder written into the cache is served for ever after,
            # which is how three bad tiles survived a whole day of runs.
            held.unlink(missing_ok=True)
            grid = self._magnified(got, tx, ty) if got is not None else None
        if grid is None:
            raise RuntimeError(f"no elevation at {self.zoom}/{tx}/{ty}, and no ancestor to fill it")
        self.tiles[key] = grid
        return grid

    def posting_m(self, lat):
        return 40075016.686 * math.cos(math.radians(lat)) / (2 ** self.zoom) / TILE_PX

    def at(self, lat, lon):
        x, y = tile_of(lat, lon, self.zoom)
        px = x * TILE_PX - 0.5
        py = y * TILE_PX - 0.5
        x0, y0 = math.floor(px), math.floor(py)
        fx, fy = px - x0, py - y0
        def sample(ix, iy):
            tx, iy_in = divmod(iy, TILE_PX)
            tX, ix_in = divmod(ix, TILE_PX)
            return self.tile(tX, tx)[iy_in, ix_in]
        h00 = sample(x0, y0)
        h10 = sample(x0 + 1, y0)
        h01 = sample(x0, y0 + 1)
        h11 = sample(x0 + 1, y0 + 1)
        return (h00 * (1 - fx) + h10 * fx) * (1 - fy) + (h01 * (1 - fx) + h11 * fx) * fy


def haversine_m(a, b):
    r = 6378137.0
    la1, lo1 = map(math.radians, a)
    la2, lo2 = map(math.radians, b)
    h = math.sin((la2 - la1) / 2) ** 2 + math.cos(la1) * math.cos(la2) * math.sin((lo2 - lo1) / 2) ** 2
    return 2 * r * math.asin(math.sqrt(h))


def class_table(tree):
    """highway tag -> the type table's row (maxGradient, sealed, widthM) from vegetation.json."""
    doc = json.load(open(pathlib.Path(tree) / "src/assets/world/vegetation.json"))
    rows = {}

    def walk(o):
        if isinstance(o, dict):
            if "speedMps" in o and "kind" in o:
                rows[o["kind"]] = o
            for v in o.values():
                walk(v)
        elif isinstance(o, list):
            for v in o:
                walk(v)

    walk(doc)
    return rows
