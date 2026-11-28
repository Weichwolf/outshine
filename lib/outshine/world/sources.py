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

from .road_terrain import (
    ROAD_LAYERS,
    VECTOR_ZOOM,
    RoadTerrain,
    crossfall_tiles,
    dem_longitude,
    elevation_tiles,
    local_metric,
    metres_at,
    terrain_halo,
    terrain_tiles,
)

LOCAL_LAYERS = (*ROAD_LAYERS, "landcover")


class LocalScene:
    def __init__(self, place, snapshot, cache, radius, *, pins=None):
        self.lat, self.lon = place["latitude"], place["longitude"]
        self.metres = metres_at(self.lat)
        self.receipts = []
        self.features = []
        self._radius = radius
        self.dem = Elevation([])
        transport_tiles = {}
        vectors = {}
        ground_tiles = terrain_halo(terrain_tiles(self.lat, self.lon, radius))
        support = {
            "elevation": elevation_tiles(ground_tiles, 12),
            "vector": crossfall_tiles(ground_tiles),
        }
        extent = box(-radius, -radius, radius, radius)
        shapely.prepare(extent)
        for kind, zoom in (("elevation", 12), ("vector", VECTOR_ZOOM)):
            local = set(covering_tiles(self.lat, self.lon, radius, zoom))
            tiles = local | support[kind]
            for tile in sorted(tiles):
                key = "/".join(map(str, tile))
                url = (
                    f"https://tiles.mapterhorn.com/{key}.webp"
                    if kind == "elevation"
                    else f"https://tiles.openfreemap.org/planet/{snapshot}/{key}.pbf"
                )
                if tile not in local and pins is not None and url not in pins:
                    continue
                receipt = acquire(
                    (kind, tile, url),
                    Path(cache),
                    fetch=True,
                    optional=tile not in local,
                )
                self.receipts.append(receipt)
                if kind == "vector":
                    if tile not in local and receipt.get("missing"):
                        continue
                    data = decode_vector(
                        receipt, layers=LOCAL_LAYERS if tile in local else ROAD_LAYERS
                    )
                    vectors[tile] = {
                        name: data[name] for name in ROAD_LAYERS if name in data
                    }
                    if tile in local:
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
        self.road_terrain = RoadTerrain(self, vectors)

    def height(self, xy):
        lon, lat = (np.asarray(xy) / self.metres + [self.lon, self.lat]).T
        return self.dem.sample(lat, dem_longitude(lon)) - self.datum

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
    def road_tunnel_approaches(self):
        approaches = {}
        for portal in self.transport_graph.portals:
            if portal.structure.domain[2] != "tunnel" or "motor" not in (
                portal.enter_modes | portal.exit_modes
            ):
                continue
            mouth, lead = self.metric(
                *np.asarray(
                    [geography(portal.ground.point), geography(portal.ground_next)]
                ).T
            )
            approaches[tuple(np.round(mouth, 6))] = tuple(lead)
        return approaches

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
        return local_metric(longitude, latitude, (self.lon, self.lat), self.metres)

    def metric_features(self, receipt, names, data=None, *, project=None):
        project = self.metric if project is None else project
        tile = receipt["tile"]
        if data is None:
            data = decode_vector(receipt, layers=names)
        for name in names:
            if name not in data:
                continue
            layer = data[name]

            def local(points, extent=layer["extent"]):
                lat, lon = tile_geography(points, extent, tile)
                return project(lon, lat)

            for feature in layer["features"]:
                geom = shapely.transform(shape(feature["geometry"]), local)
                tags = feature["properties"]
                if name == "water" and "id" not in tags and feature.get("id"):
                    tags = tags | {"id": feature["id"]}
                yield name, geom, tags
