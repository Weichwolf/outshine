"""THE ENGINE'S OWN CACHE, READ FROM THE LAB.

CLAUDE.md asks the lab to compare against the C++ on THIS TREE'S INPUTS. Until now it fetched its
own: its own Overpass extracts, its own terrarium tiles, its own cache keyed its own way -- so a
number from the lab and a number from the client were computed from two downloads that merely
ought to agree. They do not have to: `src/world/data/ContentStore.cpp` already keeps every byte
the engine ever fetched, under a key it derives itself, and that store is sitting on this disk
with 7 738 files and 599 MB in it.

THE KEY, from `ContentKey(const SourceDecl &, const Address &)`:

    sha256(Id + "\\n" + Version + "\\n" + Name(Kind) + "\\n" + Address.Text())

and for `Scheme::TileZxy` the address is `z/x/y`. Two sources are declared:

    terrarium.s3     v1  elevation   zoom 0..15, ancestor fill    Terrarium PNG
    versatiles.osm   v1  vector      zoom 0..14                   Mapbox Vector Tile

Verified 2026-09-07 against the store on this machine: every zoom from 8 to 15 of Rothenburg's
own DEM column, and the vector tiles at 11 and 14, all hit.

WHAT THIS BUYS BESIDES SPEED. The lab stops being a second downloader and becomes an ORACLE on
the same bytes: if the client and the lab disagree about a height, it is not because they read
different tiles. And the OSM arrives GRIDDED, which is the shape the geometry has to be built in
anyway.
"""
import hashlib
import os
import pathlib

ROOT = pathlib.Path(os.environ.get("OUTSHINE_CACHE", "/tmp/outshine-drive-cache"))

# `src/world/data/TerrariumDem.cpp` and `VersatilesVector.cpp`, field for field
SOURCES = {
    "elevation": ("terrarium.s3", 1, "elevation", 0, 15),
    "vector": ("versatiles.osm", 1, "vector", 0, 14),
}


def key(kind, zoom, x, y):
    """`ContentKey`, in Python. Same string, same order, same digest."""
    ident, version, name, _, _ = SOURCES[kind]
    return hashlib.sha256(f"{ident}\n{version}\n{name}\n{zoom}/{x}/{y}".encode()).hexdigest()


def path(kind, zoom, x, y):
    return ROOT / key(kind, zoom, x, y)


def read(kind, zoom, x, y):
    """The bytes the engine kept, or None. Never fetches: a lab that downloads is a lab that can
    disagree with the client about what it read."""
    got = path(kind, zoom, x, y)
    try:
        return got.read_bytes()
    except OSError:
        return None


def held(kind, zoom, x, y):
    return path(kind, zoom, x, y).exists()


def ancestor(kind, zoom, x, y):
    """THE TILE OR THE NEAREST ANCESTOR THAT IS HELD, as `AncestorFill` declares for elevation:
    a DEM tile missing at zoom 15 is its parent at 14 magnified, which is what the engine does
    rather than leaving a hole. Returns (zoom, x, y, bytes) or None."""
    z, cx, cy = int(zoom), int(x), int(y)
    while z >= SOURCES[kind][3]:
        got = read(kind, z, cx, cy)
        if got is not None:
            return z, cx, cy, got
        z, cx, cy = z - 1, cx // 2, cy // 2
    return None


def heights(png_bytes):
    """A terrarium tile decoded to metres: `(r * 256 + g + b / 256) - 32768`, which is the same
    arithmetic `roads/data.py` already does and the same the engine's `TerrainTiles` does."""
    import io

    import numpy as np
    from PIL import Image
    rgb = np.asarray(Image.open(io.BytesIO(png_bytes)).convert("RGB")).astype(np.float64)
    return rgb[:, :, 0] * 256.0 + rgb[:, :, 1] + rgb[:, :, 2] / 256.0 - 32768.0


def ledger():
    """What the store holds, so a run can say whether it is reading or guessing."""
    n = bytes_ = 0
    try:
        for got in ROOT.iterdir():
            if got.is_file() and len(got.name) == 64:
                n += 1
                bytes_ += got.stat().st_size
    except OSError:
        pass
    return {"files": n, "bytes": bytes_, "where": str(ROOT)}


def covered(kind, to_zoom=15, base=9, under=None):
    """WHERE THE STORE ACTUALLY IS, without inverting a hash. The key is a digest, so the store
    cannot be listed -- but it can be PROBED, and the probe is cheap because the client fetches a
    COLUMN. WHAT THIS CANNOT SEE, measured 2026-09-07: the column does not always start at the
    same zoom -- Venice's begins at 8 and Shibuya's at 9 -- so the sweep starts at 9, and a place
    the client only ever reached BELOW zoom 9 is invisible to it. `holds_place` probes one
    coordinate directly and has no such blind spot; this is for drawing the map, not for deciding
    whether one place is held."""
    high = min(int(to_zoom), SOURCES[kind][4])
    if under is None:
        front = [(base, x, y) for x in range(2 ** base) for y in range(2 ** base) if held(kind, base, x, y)]
        out = list(front)
    else:
        front, out = list(under), []
    for z in range(base + 1, high + 1):
        nxt = [(z, cx, cy) for _, x, y in front for cx in (2 * x, 2 * x + 1) for cy in (2 * y, 2 * y + 1) if held(kind, z, cx, cy)]
        out += nxt
        front = nxt
        if not front:
            break
    return out


def under(kind, seeds, zoom):
    """Every held tile at `zoom` inside these coarser addresses. A jump rather than a walk,
    because the vector source is held at 11 and 14 with nothing between -- a level-by-level
    descent finds nothing and reports an empty planet, which is the measure that cannot see."""
    out = []
    for z, x, y in seeds:
        step = 2 ** (int(zoom) - z)
        for cx in range(x * step, (x + 1) * step):
            for cy in range(y * step, (y + 1) * step):
                if held(kind, zoom, cx, cy):
                    out.append((int(zoom), cx, cy))
    return out


def reach(kind, to_zoom=15):
    """The store's coverage for any source. Elevation is fetched as a COLUMN from zoom 8 and can
    be swept; the vector source is fetched only at the zooms a scenario asks for -- measured on
    this store, 11 and 14 and nothing between -- so it has no chain to walk and is probed under
    the elevation's roots, which is everywhere the client has ever stood."""
    dem = covered("elevation", to_zoom)
    if kind == "elevation":
        return dem
    roots = [a for a in dem if a[0] == 9]
    coarse = under(kind, roots, 11)
    return coarse + under(kind, coarse, min(int(to_zoom), SOURCES[kind][4]))


def box(zoom, x, y):
    """A tile's geodetic box, (west, south, east, north) in degrees."""
    import math
    n = 2.0 ** zoom
    w, e = x / n * 360.0 - 180.0, (x + 1) / n * 360.0 - 180.0
    north = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * y / n))))
    south = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * (y + 1) / n))))
    return w, south, e, north


def holds_place(lat, lon, zoom, kind="elevation"):
    """Does the store already answer at this place and zoom? The lab prefers places it does."""
    import math
    n = 2 ** int(zoom)
    x = int((lon + 180.0) / 360.0 * n)
    y = int((1.0 - math.log(math.tan(math.radians(lat)) + 1.0 / math.cos(math.radians(lat))) / math.pi) / 2.0 * n)
    got = ancestor(kind, zoom, x, y)
    return None if got is None else got[0]
