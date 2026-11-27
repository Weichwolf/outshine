"""Local metric source layers and elevation for a pedestrian reference scene."""

from pathlib import Path

import numpy as np
import shapely
from outshine_ref.places.height import Elevation
from outshine_ref.places.sources import (
    acquire,
    covering_tiles,
    decode_vector,
    tile_geography,
)
from shapely.geometry import Point, shape


class LocalScene:
    def __init__(self, place, snapshot, cache, radius):
        self.lat, self.lon = place["latitude"], place["longitude"]
        latitude = np.deg2rad(self.lat)
        self.metres = np.array(
            [
                111412.84 * np.cos(latitude) - 93.5 * np.cos(3 * latitude),
                111132.92 - 559.82 * np.cos(2 * latitude),
            ]
        )
        self.receipts = []
        self.features = []
        self.dem = Elevation([])
        for kind, zoom in (("elevation", 12), ("vector", 14)):
            for tile in covering_tiles(self.lat, self.lon, radius, zoom):
                key = "/".join(map(str, tile))
                url = (
                    f"https://tiles.mapterhorn.com/{key}.webp"
                    if kind == "elevation"
                    else f"https://tiles.openfreemap.org/planet/{snapshot}/{key}.pbf"
                )
                receipt = acquire((kind, tile, url), Path(cache), fetch=True)
                self.receipts.append(receipt)
                if kind == "vector":
                    self.add_features(receipt, radius)
            if kind == "elevation":
                self.dem = Elevation(self.receipts)
        self.datum = float(self.dem.sample(self.lat, self.lon))

    def height(self, xy):
        lon, lat = (np.asarray(xy) / self.metres + [self.lon, self.lat]).T
        return self.dem.sample(lat, lon) - self.datum

    def add_features(self, receipt, radius):
        centre = Point(0, 0)
        for record in self.metric_features(
            receipt, ("building", "transportation", "water", "landcover")
        ):
            if record[1].distance(centre) <= radius:
                self.features.append(record)

    def metric_features(self, receipt, names, data=None):
        tile = receipt["tile"]
        if data is None:
            data = decode_vector(receipt, layers=names)
        for name in names:
            if name not in data:
                continue
            layer = data[name]

            def local(points, extent=layer["extent"]):
                lat, lon = tile_geography(points, extent, tile)
                return np.column_stack((lon - self.lon, lat - self.lat)) * self.metres

            for feature in layer["features"]:
                geom = shapely.transform(shape(feature["geometry"]), local)
                tags = feature["properties"]
                if name == "water" and "id" not in tags and feature.get("id"):
                    tags = tags | {"id": feature["id"]}
                yield name, geom, tags
