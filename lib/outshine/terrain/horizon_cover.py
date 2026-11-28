"""Bake the mapped land cover of the geodetic horizon into a shared material map."""

from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, replace

import numpy as np
import shapely
from PIL import Image

from outshine.materials.palette import SURFACES
from outshine.places.camera import axes, ecef
from outshine.places.sources import acquire, covering_tiles, decode_vector
from outshine.terrain.horizon import HORIZON_RANGE_M
from outshine.terrain.land_cover import (
    COVER_LAYERS,
    COVER_ORDER,
    cover_material,
    paint_cover,
)
from outshine.world.regional import tile_rect, view_sector

COVER_TILE_ZOOM = 10
COVER_SIZE = 2048
COVER_MATERIALS = {
    "distant terrain": SURFACES["distant terrain"],
    **{
        name: SURFACES["regional wood" if name == "wood" else name]
        for name in COVER_ORDER
    },
}
COVER_CODES = {name: i for i, name in enumerate(COVER_MATERIALS)}


@dataclass(frozen=True)
class HorizonCover:
    pixels: np.ndarray
    transform: tuple[float, float, float, float]
    receipts: tuple[dict, ...] = ()


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
    if pins is not None:
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
        if pins is not None and pins[receipt["url"]] != receipt["sha256"]:
            raise ValueError(f"Changed horizon land cover source: {receipt['url']}")
        footprints.extend(tile_cover(source, receipt, project))
    cover = rasterize_cover(
        footprints, view_sector(place, focal, HORIZON_RANGE_M).bounds
    )
    return replace(cover, receipts=tuple(receipts))


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
            paint_cover(
                values,
                coordinates(polygon.exterior),
                [coordinates(hole) for hole in polygon.interiors],
                code,
            )

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
