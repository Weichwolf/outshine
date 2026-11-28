"""Bake the mapped land cover of the geodetic horizon into a shared material map."""

from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass

import numpy as np
import shapely
from horizon import HORIZON_RANGE_M
from materials import SURFACES
from outshine_ref.places.camera import axes, ecef
from outshine_ref.places.sources import acquire, covering_tiles, decode_vector
from PIL import Image, ImageDraw
from regional import tile_rect, view_sector

COVER_TILE_ZOOM = 10
COVER_SIZE = 2048
COVER_LAYERS = ("landuse", "landcover")
CROP_VARIANTS = ("green crop",) * 4 + ("dry crop",) * 3 + ("field soil",)
SETTLEMENT = {
    "commercial",
    "industrial",
    "neighbourhood",
    "quarter",
    "residential",
    "retail",
    "suburb",
}
# Paint more specific surface cover over the underlying land use.
COVER_MATERIALS = {
    "distant terrain": SURFACES["distant terrain"],
    "settlement": ((0.145, 0.126, 0.113), 0.94),
    "grass": SURFACES["meadow"],
    "scrub": ((0.075, 0.119, 0.048), 0.91),
    "wetland": ((0.062, 0.103, 0.069), 0.93),
    "green crop": ((0.086, 0.137, 0.041), 0.92),
    "dry crop": ((0.171, 0.164, 0.079), 0.94),
    "field soil": ((0.142, 0.104, 0.066), 0.95),
    "vineyard": ((0.102, 0.128, 0.058), 0.93),
    "orchard": ((0.070, 0.111, 0.045), 0.92),
    "wood": SURFACES["regional wood"],
    "quarry": ((0.242, 0.211, 0.172), 0.93),
    "sand": ((0.285, 0.237, 0.161), 0.96),
    "scree": ((0.199, 0.186, 0.162), 0.94),
    "bare rock": ((0.244, 0.233, 0.213), 0.91),
    "ice": ((0.544, 0.635, 0.678), 0.82),
}
COVER_CODES = {name: i for i, name in enumerate(COVER_MATERIALS)}


@dataclass(frozen=True)
class HorizonCover:
    pixels: np.ndarray
    transform: tuple[float, float, float, float]


def cover_source_specs(source, place, snapshot, focal):
    sector = view_sector(place, focal, HORIZON_RANGE_M)
    for tile in covering_tiles(
        source.lat, source.lon, HORIZON_RANGE_M, COVER_TILE_ZOOM
    ):
        nw, se = tile_rect(source, tile)
        if sector.intersects(shapely.box(nw[0], se[1], se[0], nw[1])):
            path = "/".join(map(str, tile))
            url = f"https://tiles.openfreemap.org/planet/{snapshot}/{path}.pbf"
            yield "vector", tile, url


def cover_material(layer, tags, feature_id=None):
    kind, detail = tags.get("class"), tags.get("subclass")
    if layer == "landuse":
        if kind in SETTLEMENT:
            return "settlement"
        return "quarry" if kind == "quarry" else None
    if kind == "farmland":
        if detail in ("vineyard", "orchard"):
            return detail
        if not feature_id:
            return "green crop"
        variation = ((int(feature_id) * 0x9E3779B1) & 0xFFFFFFFF) >> 29
        return CROP_VARIANTS[variation]
    if kind == "rock":
        return "scree" if detail == "scree" else "bare rock"
    if kind == "grass" and detail in ("scrub", "heath"):
        return "scrub"
    if kind in ("wood", "grass", "wetland", "sand", "ice"):
        return kind
    return None


def tile_cover(source, receipt, project):
    data = decode_vector(receipt, layers=COVER_LAYERS)
    nw, se = tile_rect(source, receipt["tile"])
    bounds = shapely.box(nw[0], se[1], se[0], nw[1])
    for layer in COVER_LAYERS:
        if layer not in data:
            continue
        codes, geometries = [], []
        decoded = source.metric_features(receipt, (layer,), data)
        for (_, geometry, tags), feature in zip(
            decoded, data[layer]["features"], strict=True
        ):
            if material := cover_material(layer, tags, feature.get("id")):
                codes.append(COVER_CODES[material])
                geometries.append(geometry)
        if geometries:
            clipped = shapely.intersection(shapely.make_valid(geometries), bounds)
            projected = shapely.make_valid(shapely.transform(clipped, project))
            yield from zip(codes, projected, strict=True)


def distant_cover(source, place, snapshot, focal, cache, pins):
    specs = list(cover_source_specs(source, place, snapshot, focal))
    for _, _, url in specs:
        if url not in pins:
            raise ValueError(f"Unpinned horizon land cover source: {url}")
    with ThreadPoolExecutor(max_workers=6) as workers:
        receipts = list(workers.map(lambda row: acquire(row, cache, fetch=True), specs))
    anchor = ecef(source.lat, source.lon, source.datum)
    horizontal = axes(source.lat, source.lon)[:2].T

    def project(xy):
        # Ring closure needs bit-identical coordinates even across vectorized tails.
        points, inverse = np.unique(xy, axis=0, return_inverse=True)
        lon, lat = (points / source.metres + [source.lon, source.lat]).T
        projected = (ecef(lat, lon, source.datum) - anchor) @ horizontal
        return projected[inverse]

    footprints = []
    for receipt in receipts:
        if pins[receipt["url"]] != receipt["sha256"]:
            raise ValueError(f"Changed horizon land cover source: {receipt['url']}")
        footprints.extend(tile_cover(source, receipt, project))
    return rasterize_cover(
        footprints, view_sector(place, focal, HORIZON_RANGE_M).bounds
    )


def rasterize_cover(footprints, bounds, size=COVER_SIZE):
    lower, upper = np.asarray(bounds).reshape(2, 2)
    scale = 1.0 / (upper - lower)
    span = 2 * size
    values = Image.new("L", (span, span))

    def coordinates(ring):
        points = (np.asarray(ring.coords) - lower) * scale
        return np.floor(points * span).astype(int)

    for code, geometry in sorted(footprints, key=lambda row: row[0]):
        polygons = shapely.get_parts(shapely.get_parts(geometry))
        for polygon in polygons:
            if polygon.geom_type != "Polygon" or polygon.is_empty:
                continue
            perimeter = coordinates(polygon.exterior)
            origin = np.maximum(0, perimeter.min(axis=0))
            stop = np.minimum(span, perimeter.max(axis=0) + 1)
            if np.any(stop <= origin):
                continue
            mask = Image.new("L", tuple(stop - origin))
            pen = ImageDraw.Draw(mask)
            pen.polygon([tuple(p) for p in perimeter - origin], fill=255)
            for hole in polygon.interiors:
                pen.polygon([tuple(p) for p in coordinates(hole) - origin], fill=0)
            values.paste(code, (*origin, *stop), mask)

    palette = np.zeros((256, 4), dtype=np.uint8)
    for code, (colour, roughness) in enumerate(COVER_MATERIALS.values()):
        palette[code] = np.rint(np.asarray((*colour, roughness)) * 255)
    # Filter material channels independently. The fourth channel is roughness.
    channels = [
        values.point(channel.tolist()).resize((size, size), Image.Resampling.BOX)
        for channel in palette.T
    ]
    pixels = np.asarray(Image.merge("RGBA", channels)).copy()
    pixels[[0, -1], :, :] = palette[0]
    pixels[:, [0, -1], :] = palette[0]
    return HorizonCover(pixels, (*lower, *scale))
