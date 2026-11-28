"""Local metric source layers and elevation for a pedestrian reference scene."""

from functools import cached_property
from pathlib import Path

import numpy as np
import shapely
from shapely.geometry import box, shape

from outshine.buildings.building_footprints import building_footprints
from outshine.places.height import Elevation
from outshine.places.sources import (
    acquire,
    covering_tiles,
    decode_vector,
    tile_geography,
)
from outshine.terrain.ground_surface import ROAD_WIDTH, at_grade_width
from outshine.transport.route_graph import route_graph
from outshine.transport.route_tiles import geography, grid_point

LOCAL_LAYERS = ("building", "transportation", "water", "landcover")


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
        self._radius = radius
        self.dem = Elevation([])
        transport_tiles = {}
        extent = box(-radius, -radius, radius, radius)
        shapely.prepare(extent)
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
                    data = decode_vector(receipt, layers=LOCAL_LAYERS)
                    if "transportation" in data:
                        transport_tiles[tile] = data["transportation"]
                    self.add_features(receipt, extent, data)
            if kind == "elevation":
                self.dem = Elevation(self.receipts)
        self.datum = float(self.dem.sample(self.lat, self.lon))
        east, north = (radius + max(ROAD_WIDTH.values()) / 2) / self.metres
        bounds = (
            *grid_point(self.lon - east, self.lat + north),
            *grid_point(self.lon + east, self.lat - north),
        )
        self.transport_graph = route_graph(transport_tiles, bounds=bounds)

    def height(self, xy):
        lon, lat = (np.asarray(xy) / self.metres + [self.lon, self.lat]).T
        return self.dem.sample(lat, lon) - self.datum

    def add_features(self, receipt, extent, data=None):
        for name, geometry, properties in self.metric_features(
            receipt, LOCAL_LAYERS, data
        ):
            if not extent.intersects(geometry):
                continue
            if name == "building":
                footprints, _ = building_footprints(geometry)
                nearby = footprints[shapely.intersects(extent, footprints)]
                self.features.extend(
                    (name, footprint, properties) for footprint in nearby
                )
            else:
                self.features.append((name, geometry, properties))

    @cached_property
    def ground_tile_joins(self):
        graph = self.transport_graph
        joined = []
        for first, second in graph.seam_pairs:
            a, b = graph.paths[first], graph.paths[second]
            if a.attributes.domain != ("land", 0, "", "", False):
                continue
            tags = dict(a.attributes.tags)
            width = at_grade_width(tags)
            if width is None:
                continue
            ports = {a.nodes[0], a.nodes[-1]} & {b.nodes[0], b.nodes[-1]}
            if not any(
                np.abs(self.metric(*geography(port.point))).max()
                <= self._radius + width / 2
                for port in ports
            ):
                continue
            coords = [
                self.metric(
                    *np.asarray([geography(node.point) for node in path.nodes]).T
                )
                for path in (a, b)
            ]
            axis = shapely.line_merge(shapely.MultiLineString(coords))
            if axis.geom_type == "LineString":
                joined.append((axis, tags))
        return tuple(joined)

    def metric(self, longitude, latitude):
        east = longitude - self.lon
        east = np.where(np.abs(east) > 180, east - np.sign(east) * 360, east)
        return np.column_stack((east, latitude - self.lat)) * self.metres

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
                return self.metric(lon, lat)

            for feature in layer["features"]:
                geom = shapely.transform(shape(feature["geometry"]), local)
                tags = feature["properties"]
                if name == "water" and "id" not in tags and feature.get("id"):
                    tags = tags | {"id": feature["id"]}
                yield name, geom, tags
