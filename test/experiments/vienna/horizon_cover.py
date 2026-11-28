"""Project coarse mapped woodland onto the unchanged geodetic height field."""

from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass

import numpy as np
import shapely
from horizon import HORIZON_RANGE_M
from outshine_ref.places.camera import axes, ecef
from outshine_ref.places.sources import acquire, covering_tiles
from PIL import Image, ImageChops, ImageDraw
from regional import tile_rect, view_sector

WOODLAND_TILE_ZOOM = 10
WOODLAND_MASK_SIZE = 2048


@dataclass(frozen=True)
class WoodlandCoverage:
    pixels: np.ndarray
    transform: tuple[float, float, float, float]


def woodland_source_specs(source, place, snapshot, focal):
    sector = view_sector(place, focal, HORIZON_RANGE_M)
    for tile in covering_tiles(
        source.lat, source.lon, HORIZON_RANGE_M, WOODLAND_TILE_ZOOM
    ):
        nw, se = tile_rect(source, tile)
        if sector.intersects(shapely.box(nw[0], se[1], se[0], nw[1])):
            path = "/".join(map(str, tile))
            url = f"https://tiles.openfreemap.org/planet/{snapshot}/{path}.pbf"
            yield "vector", tile, url


def distant_woodland(source, place, snapshot, focal, cache, pins):
    specs = list(woodland_source_specs(source, place, snapshot, focal))
    for _, _, url in specs:
        if url not in pins:
            raise ValueError(f"Unpinned horizon woodland source: {url}")
    with ThreadPoolExecutor(max_workers=6) as workers:
        receipts = list(workers.map(lambda row: acquire(row, cache, fetch=True), specs))
    anchor = ecef(source.lat, source.lon, source.datum)
    horizontal = axes(source.lat, source.lon)[:2].T

    def project(xy):
        points, inverse = np.unique(xy, axis=0, return_inverse=True)
        lon, lat = (points / source.metres + [source.lon, source.lat]).T
        coordinates = (ecef(lat, lon, source.datum) - anchor) @ horizontal
        return coordinates[inverse]

    footprints = []
    for receipt in receipts:
        if pins[receipt["url"]] != receipt["sha256"]:
            raise ValueError(f"Changed horizon woodland source: {receipt['url']}")
        nw, se = tile_rect(source, receipt["tile"])
        bounds = shapely.box(nw[0], se[1], se[0], nw[1])
        for _, geometry, props in source.metric_features(receipt, ("landcover",)):
            if props.get("class") == "wood":
                clipped = shapely.make_valid(geometry).intersection(bounds)
                footprints.append(
                    shapely.make_valid(shapely.transform(clipped, project))
                )
    bounds = view_sector(place, focal, HORIZON_RANGE_M).bounds
    return rasterize_woodland(footprints, bounds)


def rasterize_woodland(footprints, bounds, size=WOODLAND_MASK_SIZE):
    lower, upper = np.asarray(bounds).reshape(2, 2)
    scale = 1.0 / (upper - lower)
    mask = Image.new("L", (2 * size, 2 * size))

    def pixels(ring):
        points = (np.asarray(ring.coords) - lower) * scale
        return np.floor(points * mask.width).astype(int)

    for polygon in shapely.get_parts(shapely.get_parts(footprints)):
        if polygon.geom_type != "Polygon" or polygon.is_empty:
            continue
        exterior = pixels(polygon.exterior)
        origin = np.maximum(0, exterior.min(axis=0))
        stop = np.minimum(mask.width, exterior.max(axis=0) + 1)
        if np.any(stop <= origin):
            continue
        tile = Image.new("L", tuple(stop - origin))
        pen = ImageDraw.Draw(tile)
        pen.polygon([tuple(p) for p in exterior - origin], fill=255)
        for hole in polygon.interiors:
            pen.polygon([tuple(p) for p in pixels(hole) - origin], fill=0)
        current = mask.crop((*origin, *stop))
        mask.paste(ImageChops.lighter(current, tile), tuple(origin))
    coverage = np.asarray(mask.resize((size, size), Image.Resampling.BOX))
    return WoodlandCoverage(coverage, (*lower, *scale))
