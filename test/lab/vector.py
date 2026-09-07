"""THE ENGINE'S OWN GRIDDED OSM, READ FROM ITS STORE.

`store.py` reads the bytes the client already fetched; this decodes the VECTOR ones. The source
is `versatiles.osm` -- Mapbox Vector Tiles at zoom 11 and 14 -- and it is what the goal means by
"kachelweise": OSM arrives on a fixed grid fixed to the ground, complete per tile, with its
neighbours addressable, and nothing about it depends on where a camera stands.

WHY IT MATTERS BEYOND SPEED. The lab's extract is an Overpass query on a RADIUS, so the ground
now reaches 240 km (`Frame.rung`) over land that OSM never described: a uniform green plain to the
horizon where Franconia is a patchwork of forest, farmland, meadow and vineyard. The far field
cannot be fetched by radius -- it is a hundred and fifty tiles across -- and it does not need to
be: at that distance a `land` polygon at zoom 11 is finer than a pixel.

    z11   land 22, water_polygons 19, streets 11        59 kB
    z14   buildings 3228, streets 92, land 18, ...     258 kB

The `land` layer's `kind` is already the vocabulary `surfaces.py` sorts by, one word instead of a
key and a value, so the two tables meet at their own names rather than at a translation.
"""
import math

import numpy as np
from shapely.geometry import MultiPolygon, Polygon
from shapely.ops import unary_union

import store

EXTENT = 4096                    # the MVT's own tile-local grid, stated per layer and constant here

# `versatiles.osm`'s `land.kind` against `surfaces.py`'s roles. A kind this does not name is not
# a surface the twin draws -- it is a label, a boundary or a POI.
LAND = {
    "forest": "crop", "wood": "crop", "meadow": "crop", "grass": "crop", "grassland": "crop",
    "park": "crop", "garden": "crop", "cemetery": "crop", "scrub": "crop", "heath": "crop",
    "farmland": "soil", "farmyard": "soil", "orchard": "soil", "vineyard": "soil",
    "allotments": "soil",
    "quarry": "gravel", "bare_rock": "gravel", "scree": "gravel",
    "sand": "sand", "beach": "sand",
    "residential": None, "commercial": None, "industrial": None, "retail": None,
    "military": None, "railway": "ballast",
}


def tile_of(lat, lon, zoom):
    n = 2.0 ** zoom
    x = (lon + 180.0) / 360.0 * n
    y = (1.0 - math.log(math.tan(math.radians(lat)) + 1.0 / math.cos(math.radians(lat)))
         / math.pi) / 2.0 * n
    return x, y


def _geo(zoom, tx, ty, px, py):
    """One MVT point to degrees. The tile's own y runs DOWN from its north edge."""
    n = 2.0 ** zoom
    lon = (tx + px / EXTENT) / n * 360.0 - 180.0
    yy = (ty + (EXTENT - py) / EXTENT) / n
    lat = math.degrees(math.atan(math.sinh(math.pi * (1.0 - 2.0 * yy))))
    return lat, lon


def layers(zoom, tx, ty):
    """The decoded tile, or None when the store has never seen it. Never fetches: the vector
    source is the CLIENT's and the lab reads it, which is the whole point of `store.py`."""
    raw = store.read("vector", zoom, tx, ty)
    if raw is None:
        return None
    try:
        import mapbox_vector_tile
        return mapbox_vector_tile.decode(raw)
    except Exception:
        return None


def _shape(frame, zoom, tx, ty, geom):
    """One MVT geometry in the frame's own metres, or None if it is not an area."""
    kind = geom.get("type")
    if kind not in ("Polygon", "MultiPolygon"):
        return None
    rings = geom["coordinates"] if kind == "MultiPolygon" else [geom["coordinates"]]
    out = []
    for parts in rings:
        made = []
        for ring in parts:
            pts = [frame.xy(*_geo(zoom, tx, ty, px, py)) for (px, py) in ring]
            if len(pts) >= 4:
                made.append(pts)
        if made:
            try:
                out.append(Polygon(made[0], made[1:]).buffer(0))
            except Exception:
                pass
    live = [q for q in out if not q.is_empty and q.area > 0]
    if not live:
        return None
    return live[0] if len(live) == 1 else unary_union(live)


def land(frame, reach_m, zoom=11, least_m2=400.0):
    """WHAT THE GROUND IS, out to `reach_m`, from the store's own tiles.

    Returns [(role, polygon)] in the frame's metres, in `surfaces.py`'s vocabulary, so the near
    field's patches and the far field's speak one language and a reader meets no translation."""
    lat, lon = frame.lat0, frame.lon0
    span = 40075016.686 * math.cos(math.radians(lat)) / (2.0 ** zoom)
    reach = int(math.ceil(reach_m / max(span, 1.0)))
    cx, cy = tile_of(lat, lon, zoom)
    got = []
    for tx in range(int(cx) - reach, int(cx) + reach + 1):
        for ty in range(int(cy) - reach, int(cy) + reach + 1):
            held = layers(zoom, tx, ty)
            if held is None:
                continue
            for name in ("land", "water_polygons"):
                for feature in held.get(name, {}).get("features", ()):
                    role = ("water" if name == "water_polygons"
                            else LAND.get(feature["properties"].get("kind")))
                    if role is None:
                        continue
                    made = _shape(frame, zoom, tx, ty, feature["geometry"])
                    if made is not None and made.area >= least_m2:
                        got.append((role, made))
    return got


def held(frame, reach_m, zoom=11):
    """How many of the tiles a reach needs the store actually has -- the number that says whether
    a far field is DRAWN or merely hoped for."""
    lat = frame.lat0
    span = 40075016.686 * math.cos(math.radians(lat)) / (2.0 ** zoom)
    reach = int(math.ceil(reach_m / max(span, 1.0)))
    cx, cy = tile_of(lat, frame.lon0, zoom)
    want = have = 0
    for tx in range(int(cx) - reach, int(cx) + reach + 1):
        for ty in range(int(cy) - reach, int(cy) + reach + 1):
            want += 1
            have += 1 if store.held("vector", zoom, tx, ty) else 0
    return have, want
