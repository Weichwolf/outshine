"""Build absolute terrain heights in world tiles before selecting a view."""

from dataclasses import dataclass

import numpy as np
import shapely

from outshine.places.sources import covering_tiles, tile_geography
from outshine.terrain.ground_surface import ROAD_WIDTH, route_area
from outshine.terrain.lattice import lattice
from outshine.transport.elevated_rail import BALLAST_WIDTH
from outshine.transport.road_grades import fit_road_grades, ground_carriageways
from outshine.transport.route_graph import route_graph
from outshine.transport.route_tiles import WORLD_CELLS

TERRAIN_ZOOM = 16
CELLS_PER_TILE = 128
ROAD_LAYERS = ("building", "transportation", "water")


def metres_at(latitude):
    latitude = np.deg2rad(latitude)
    return np.array(
        [
            111412.84 * np.cos(latitude) - 93.5 * np.cos(3 * latitude),
            111132.92 - 559.82 * np.cos(2 * latitude),
        ]
    )


def local_metric(longitude, latitude, origin, metres):
    east = longitude - origin[0]
    east = np.where(np.abs(east) > 180, east - np.sign(east) * 360, east)
    return np.column_stack((east, latitude - origin[1])) * metres


def dem_longitude(longitude):
    wrapped = np.where(longitude >= 180.0, longitude - 360.0, longitude)
    wrapped = np.where(wrapped < -180.0, wrapped + 360.0, wrapped)
    return np.clip(wrapped, np.nextafter(-180.0, 0.0), np.nextafter(180.0, 0.0))


def terrain_tiles(latitude, longitude, radius):
    return covering_tiles(latitude, longitude, radius, TERRAIN_ZOOM)


def elevation_tiles(tiles, zoom):
    shift = TERRAIN_ZOOM - zoom
    side = 1 << zoom
    parents = set()
    for _, x, y in tiles:
        xx = {x >> shift, (x + 1) >> shift}
        yy = {y >> shift, (y + 1) >> shift}
        if y % (1 << shift) == 0 and y:
            yy.add((y >> shift) - 1)
        parents.update((zoom, i % side, j) for i in xx for j in yy if j < side)
    return parents


@dataclass(frozen=True, slots=True)
class TerrainTile:
    longitude: np.ndarray
    latitude: np.ndarray
    height: np.ndarray


def _geography(tile):
    stations = np.arange(CELLS_PER_TILE + 1, dtype=float)
    lat, lon = tile_geography(
        np.column_stack((stations, stations[::-1])), CELLS_PER_TILE, tile
    )
    return lon, lat


def _enclosing(axis, radius):
    left = max(0, np.searchsorted(axis, -radius, side="right") - 1)
    right = np.searchsorted(axis, radius, side="left") + 1
    return slice(left, right)


def _reservations(features):
    for name, geometry, tags in features:
        if name in {"building", "water"}:
            yield shapely.make_valid(geometry)
        elif name == "transportation" and (
            tags.get("brunnel")
            or tags.get("layer")
            or tags.get("class") in {"rail", "transit"}
        ):
            yield route_area(geometry, ROAD_WIDTH.get(tags.get("class"), BALLAST_WIDTH))


class RoadTerrain:
    def __init__(self, source, vectors):
        self.source = source
        self.vectors = vectors
        self.tiles = {}
        self.parents = {}

    def _parent(self, parent):
        if parent not in self.parents:
            lat, lon = tile_geography([[1, 1]], 2, parent)
            origin = (float(lon[0]), float(lat[0]))
            metres = metres_at(origin[1])

            def project(longitude, latitude):
                return local_metric(longitude, latitude, origin, metres)

            data = self.vectors[parent]
            features = tuple(
                self.source.metric_features(
                    {"tile": parent}, ROAD_LAYERS, data, project=project
                )
            )
            index = shapely.STRtree([g for _, g, _ in features])
            transport = (
                {parent: data["transportation"]} if "transportation" in data else {}
            )
            self.parents[parent] = project, features, index, transport
        return self.parents[parent]

    def _fit_tile(self, tile):
        _, x, y = tile
        parent = (14, x >> (TERRAIN_ZOOM - 14), y >> (TERRAIN_ZOOM - 14))
        if parent not in self.vectors:
            return None
        longitude, latitude = _geography(tile)
        lam, phi = np.meshgrid(longitude, latitude)
        heights, _ = self.source.dem.sample_with_zoom(
            np.clip(phi, -85.051128779, 85.051128779),
            dem_longitude(lam),
            allow_missing=True,
        )
        if not np.isfinite(heights).all():
            return None
        project, features, index, transport = self._parent(parent)
        axes = project(longitude, latitude)
        bounds = shapely.box(*axes[0], *axes[-1])
        nearby = bounds.buffer(max(ROAD_WIDTH.values()) / 2, quad_segs=1)
        local = tuple(
            features[i] for i in sorted(index.query(nearby, predicate="intersects"))
        )
        stride = WORLD_CELLS >> TERRAIN_ZOOM
        graph = route_graph(
            transport,
            bounds=(x * stride, y * stride, (x + 1) * stride, (y + 1) * stride),
        )
        roads = ground_carriageways(graph.paths, project, bounds, local)
        if roads:
            vertices, quads = lattice(axes[:, 0], axes[:, 1], heights)
            product = fit_road_grades(
                vertices, quads, roads, tuple(_reservations(local))
            )
            heights = product.vertices[:, 2].reshape(heights.shape)
        return TerrainTile(longitude, latitude, heights)

    def _tile(self, tile):
        if tile not in self.tiles:
            self.tiles[tile] = self._fit_tile(tile)
        return self.tiles[tile]

    def grid(self, radius):
        source = self.source
        rows = {}
        for key in terrain_tiles(source.lat, source.lon, radius):
            tile = self._tile(key)
            if tile is None:
                return None
            west = source.metric(tile.longitude[0], source.lat)[0, 0]
            rows.setdefault(key[2], []).append((west, tile))
        strips = [
            [tile for _, tile in sorted(rows[key], key=lambda part: part[0])]
            for key in sorted(rows, reverse=True)
        ]
        first = strips[0]
        lon = np.concatenate(
            [p.longitude[:-1] for p in first[:-1]] + [first[-1].longitude]
        )
        lat = np.concatenate(
            [row[0].latitude[:-1] for row in strips[:-1]] + [strips[-1][0].latitude]
        )
        east = source.metric(lon, np.full(len(lon), source.lat))[:, 0]
        north = (lat - source.lat) * source.metres[1]
        zs = [
            np.hstack([p.height[:, :-1] for p in row[:-1]] + [row[-1].height])
            for row in strips
        ]
        height = np.vstack([z[:-1] for z in zs[:-1]] + zs[-1:])
        x, y = _enclosing(east, radius), _enclosing(north, radius)
        return lattice(east[x], north[y], height[y, x] - source.datum)
