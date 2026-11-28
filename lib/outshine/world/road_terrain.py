"""Build absolute terrain heights in world tiles before selecting a view."""

from dataclasses import dataclass

import numpy as np
import shapely

from outshine.buildings.building_footprints import building_footprints
from outshine.places.sources import covering_tiles, tile_geography
from outshine.terrain.ground_surface import ROAD_WIDTH, route_area
from outshine.terrain.lattice import lattice
from outshine.transport.elevated_rail import BALLAST_WIDTH
from outshine.transport.rail_routes import surface_rail_lines
from outshine.transport.road_crossfall import (
    WORLD_METRES,
    MercatorTile,
    geodetic,
    grade_crossfall,
)
from outshine.transport.road_grades import (
    carriageway_axes,
    fit_road_grades,
    ground_carriageways,
)
from outshine.transport.route_graph import route_graph
from outshine.transport.route_tiles import WORLD_CELLS
from outshine.transport.tram_grades import fit_tram_grades

from .crown_surface import CrownPatch, CrownSurface

TERRAIN_ZOOM = 16
VECTOR_ZOOM = 14
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


def terrain_halo(tiles):
    side = 1 << TERRAIN_ZOOM
    return {
        (TERRAIN_ZOOM, xx % side, yy)
        for _, x, y in tiles
        for xx in range(x - 1, x + 2)
        for yy in range(max(0, y - 1), min(side, y + 2))
    }


def crossfall_tiles(tiles):
    shift = TERRAIN_ZOOM - VECTOR_ZOOM
    return {(VECTOR_ZOOM, x >> shift, y >> shift) for _, x, y in terrain_halo(tiles)}


def elevation_tiles(tiles, zoom):
    shift = TERRAIN_ZOOM - zoom
    side = 1 << zoom
    parents = set()
    for _, x, y in tiles:
        xx = {x >> shift, (x + 1) >> shift}
        yy = {y >> shift, (y + 1) >> shift}
        if x % (1 << shift) == 0:
            xx.add((x >> shift) - 1)
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


def _reservations(features, *, mercator=False, fit_tram=False):
    for name, geometry, tags in features:
        if name == "building":
            yield from building_footprints(geometry)[0]
        elif name == "water":
            yield shapely.make_valid(geometry)
        elif name == "transportation" and (
            tags.get("brunnel")
            or tags.get("layer")
            or tags.get("class") in {"rail", "transit"}
        ):
            street_tram = (
                tags.get("class") == "transit"
                and tags.get("subclass") == "tram"
                and not any(
                    tags.get(key) for key in ("brunnel", "layer", "indoor", "level")
                )
            )
            road_tunnel = (
                tags.get("brunnel") == "tunnel"
                and tags.get("class") in ROAD_WIDTH
                and tags.get("class") != "path"
            )
            if fit_tram and (street_tram or road_tunnel):
                continue
            width = ROAD_WIDTH.get(tags.get("class"), BALLAST_WIDTH)
            if mercator and not geometry.is_empty:
                _, lo, _, hi = geometry.bounds
                _, latitude = geodetic([(0, lo), (0, hi)])
                width /= np.cos(np.deg2rad(latitude)).min()
            yield route_area(geometry, width)


class RoadTerrain:
    def __init__(self, source, vectors):
        self.source = source
        self.vectors = vectors
        self.tiles = {}
        self.parents = {}
        self.reservations = {}
        self.prepared = {}
        self.crowns = CrownSurface(1 << TERRAIN_ZOOM, CELLS_PER_TILE, self._patch)

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

    def _crossfall_reservations(self, parent, tile):
        frame = MercatorTile(parent)
        if parent not in self.reservations:
            features = self.source.metric_features(
                {"tile": parent},
                ROAD_LAYERS,
                self.vectors[parent],
                project=frame.project,
            )
            self.reservations[parent] = tuple(
                geom
                for geom in _reservations(features, mercator=True)
                if not geom.is_empty
            )
        geometries = self.reservations[parent]
        turn = round((tile.west - frame.west) / WORLD_METRES)
        return (
            shapely.transform(geometries, lambda xy: xy + [turn * WORLD_METRES, 0])
            if turn and geometries
            else geometries
        )

    def _crossfall(self, tile, latitude, heights):
        _, x, y = tile
        frame = MercatorTile(tile)
        parents = crossfall_tiles((tile,))
        present = sorted(parents & self.vectors.keys())
        transport = {
            key: self.vectors[key]["transportation"]
            for key in present
            if "transportation" in self.vectors[key]
        }
        if not transport:
            return heights, None
        stride = WORLD_CELLS >> TERRAIN_ZOOM
        graph = route_graph(
            transport,
            bounds=(
                ((x - 1) * stride) % WORLD_CELLS,
                (y - 1) * stride,
                ((x + 2) * stride) % WORLD_CELLS,
                (y + 2) * stride,
            ),
        )
        reserved = [
            geom for key in present for geom in self._crossfall_reservations(key, frame)
        ]
        absent = [frame.box(key) for key in sorted(parents - self.vectors.keys())]

        def elevation(xy):
            lon, lat = geodetic(xy)
            values, _ = self.source.dem.sample_with_zoom(
                np.clip(lat, -85.051128779, 85.051128779),
                dem_longitude(lon),
                allow_missing=True,
            )
            return values

        return grade_crossfall(
            frame, latitude, heights, graph, elevation, reserved, absent
        )

    def _prepare_tile(self, tile):
        _, x, y = tile
        shift = TERRAIN_ZOOM - VECTOR_ZOOM
        parent = (VECTOR_ZOOM, x >> shift, y >> shift)
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
        trams = tuple(surface_rail_lines(graph, project, kind="transit", mode="tram"))
        carriageways = tuple(carriageway_axes(graph.paths, project, bounds))
        roads = ground_carriageways(carriageways, bounds, local)
        if roads or trams:
            vertices, quads = lattice(axes[:, 0], axes[:, 1], heights)
            if roads:
                vertices = fit_road_grades(
                    vertices, quads, roads, tuple(_reservations(local))
                ).vertices
            if trams:
                vertices = fit_tram_grades(
                    vertices,
                    quads,
                    trams,
                    carriageways,
                    tuple(_reservations(local, fit_tram=True)),
                )
            heights = vertices[:, 2].reshape(heights.shape)
        target, constraints = self._crossfall(tile, latitude, heights)
        return (longitude, latitude, CrownPatch(heights, target, constraints))

    def _patch(self, tile):
        if tile not in self.prepared:
            self.prepared[tile] = self._prepare_tile(tile)
        data = self.prepared[tile]
        return data[2] if data is not None else None

    def _tile(self, tile):
        if tile not in self.tiles:
            patch = self._patch(tile)
            if patch is None:
                self.tiles[tile] = None
            else:
                longitude, latitude, _ = self.prepared[tile]
                self.tiles[tile] = TerrainTile(
                    longitude, latitude, self.crowns.fit(tile, patch)
                )
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
