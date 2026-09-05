"""The map's inputs, fetched once and cached: OSM ways by Overpass, the DEM as terrarium tiles.

The engine reads the same OSM through versatiles and the same terrarium tiles at zoom 14
(GroundStack: the finest elevation zoom, 15, minus one), so what this lab sees is what the
engine sees, decoded by numpy instead of by the tree.
"""
import json
import math
import os
import pathlib
import urllib.parse
import urllib.request

import numpy as np
from PIL import Image

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

    def tile(self, tx, ty):
        key = (tx, ty)
        if key not in self.tiles:
            png = fetch(TERRARIUM.format(z=self.zoom, x=tx, y=ty), CACHE / "terrarium" / str(self.zoom) / str(tx) / f"{ty}.png")
            rgb = np.asarray(Image.open(CACHE / "terrarium" / str(self.zoom) / str(tx) / f"{ty}.png").convert("RGB")).astype(np.float64)
            self.tiles[key] = rgb[:, :, 0] * 256.0 + rgb[:, :, 1] + rgb[:, :, 2] / 256.0 - 32768.0
        return self.tiles[key]

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
