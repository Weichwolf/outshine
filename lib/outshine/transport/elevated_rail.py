"""Dry rail decks inferred from mapped track centrelines."""

from collections import defaultdict
from dataclasses import dataclass

import numpy as np
import shapely
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components

from outshine.terrain.ground_surface import (
    at_grade_width,
    route_area,
    terrain_triangles,
)
from outshine.transport.bridges import edge_guards, slab
from outshine.transport.rail_hardware import bake_rail_sleepers, build_rail_hardware
from outshine.transport.viaducts import LOWER_CLEARANCE, SLAB_DEPTH

TRACK_HALF_WIDTH = 2.2
BALLAST_WIDTH = 3.3
BALLAST_DEPTH = 0.18
MAX_TRACK_SPACING = 10.0
SUPPORT_SPACING = 36.0
SUPPORT_RADIUS = 0.75
VEHICLE_CLEARANCE = 4.8


@dataclass(frozen=True)
class ElevatedRailDeck:
    footprint: shapely.Polygon
    tracks: tuple
    supports: tuple
    direction: np.ndarray
    elevation: float
    layer: int

    def height(self, xy):
        return np.full(len(xy), self.elevation)


def parallel_groups(tracks):
    lines = np.asarray(tracks, dtype=object)
    ends = shapely.get_coordinates(shapely.get_point(lines[:, None], [0, -1]).ravel())
    ends = ends.reshape(-1, 2, 2)
    axes = ends[:, 1] - ends[:, 0]
    lengths = np.linalg.norm(axes, axis=1)
    axes /= lengths[:, None]
    a, b = shapely.STRtree(lines).query(lines, "dwithin", MAX_TRACK_SPACING)
    aligned = abs(np.einsum("ij,ij->i", axes[a], axes[b])) >= 0.95
    overlap = np.einsum("ijk,ik->ij", ends[b] - ends[a, :1], axes[a])
    overlap.sort(axis=1)
    stations = np.minimum(lengths[a], overlap[:, 1]) - np.maximum(0, overlap[:, 0])
    paired = aligned & (stations >= 0.75 * np.maximum(lengths[a], lengths[b]))
    graph = coo_matrix(
        (np.ones(paired.sum()), (a[paired], b[paired])), shape=(len(lines), len(lines))
    )
    count, labels = connected_components(graph, directed=False)
    for label in range(count):
        yield tuple(lines[labels == label])


def lower_corridors(features, extent):
    for name, geometry, tags in features:
        if name != "transportation":
            continue
        kind = tags.get("class")
        width = at_grade_width(tags)
        if width is not None:
            area = route_area(geometry.intersection(extent), width)
            if not area.is_empty:
                yield area, LOWER_CLEARANCE.get(kind, VEHICLE_CLEARANCE), frozenset()
        elif (
            kind in {"rail", "transit"}
            and not tags.get("brunnel")
            and not tags.get("layer")
        ):
            for line in shapely.get_parts(geometry):
                if line.geom_type != "LineString" or line.is_empty:
                    continue
                area = route_area(line.intersection(extent), TRACK_HALF_WIDTH * 2)
                if not area.is_empty:
                    yield (
                        area,
                        LOWER_CLEARANCE[kind],
                        frozenset((line.coords[0], line.coords[-1])),
                    )


def terrain_upper_bound(area, cells, height):
    overlaps = shapely.intersection(
        area, cells.geometries[cells.query(area, "intersects")]
    )
    samples = shapely.get_coordinates(overlaps)
    return float(np.max(height(samples)))


def support_footprints(footprint, direction, occupied):
    lateral = direction[::-1] * [-1, 1]
    points = shapely.get_coordinates(footprint)
    first, last = (points @ direction).min(), (points @ direction).max()
    left, right = (points @ lateral).min(), (points @ lateral).max()
    rows = max(2, int(np.ceil((last - first) / SUPPORT_SPACING)) + 1)
    spans = np.linspace(first + 2, last - 2, rows)
    across = np.linspace(left + SUPPORT_RADIUS, right - SUPPORT_RADIUS, 17)
    supports = []
    for station in spans:
        centres = (station + np.array([0.0, 1.5, -1.5, 3.0, -3.0]))[
            :, None, None
        ] * direction
        centres = centres + across[None, :, None] * lateral
        piers = shapely.buffer(
            shapely.points(centres.reshape(-1, 2)), SUPPORT_RADIUS, quad_segs=3
        )
        clear = piers[
            shapely.covers(footprint, piers) & ~shapely.intersects(occupied, piers)
        ]
        if not len(clear):
            return ()
        supports.append(clear[np.argmin(shapely.distance(clear, footprint.centroid))])
        if right - left > 8:
            spread = shapely.distance(clear, supports[-1])
            if spread.max() > 4:
                supports.append(clear[spread.argmax()])
    return tuple(supports)


def plan_elevated_rail(
    features, extent, water, buildings, reserved, vertices, quads, height
):
    tracks = defaultdict(set)
    for name, geometry, tags in features:
        if (
            name == "transportation"
            and tags.get("class") == "rail"
            and tags.get("brunnel") == "bridge"
        ):
            level = tags.get("layer", 1)
            if level > 0:
                tracks[level].update(
                    shapely.normalize(p)
                    for p in shapely.get_parts(geometry.intersection(extent))
                    if p.geom_type == "LineString" and p.length >= 8 and not p.is_ring
                )
    if not tracks:
        return []

    cells = shapely.STRtree(shapely.polygons(vertices[terrain_triangles(quads), :2]))
    structures = shapely.STRtree(buildings)
    corridors = tuple(lower_corridors(features, extent))
    lower = shapely.STRtree([area for area, _, _ in corridors])
    decks = []
    for level, lines in sorted(tracks.items()):
        dry = sorted(
            (line for line in lines if line.is_simple and not line.intersects(water)),
            key=lambda line: line.wkb,
        )
        if not dry:
            continue
        for group in parallel_groups(dry):
            beds = [
                line.buffer(TRACK_HALF_WIDTH, cap_style="flat", join_style="mitre")
                for line in group
            ]
            footprint = shapely.union_all(beds)
            if len(group) > 1:
                footprint = footprint.convex_hull
            occupied = structures.geometries[structures.query(footprint, "intersects")]
            footprint = footprint.intersection(extent).difference(
                shapely.union_all(occupied)
            )
            if (
                footprint.geom_type != "Polygon"
                or footprint.is_empty
                or footprint.intersects(water)
            ):
                continue
            if footprint.intersection(reserved).area > 1e-6:
                continue
            joined = [
                (i, footprint.intersection(corridors[i][0]))
                for i in lower.query(footprint, "intersects")
            ]
            ports = {
                point for line in group for point in (line.coords[0], line.coords[-1])
            }
            required = [
                terrain_upper_bound(area, cells, height) + corridors[index][1]
                for index, area in joined
                if area.area > 0 and not ports.intersection(corridors[index][2])
            ]
            if not required:
                continue
            endpoints = np.asarray(max(group, key=lambda line: line.length).coords)[
                [0, -1]
            ]
            axis = endpoints[1] - endpoints[0]
            direction = axis / np.linalg.norm(axis)
            supports = support_footprints(
                footprint, direction, shapely.union_all([area for _, area in joined])
            )
            if not supports:
                continue
            carried = tuple(
                part
                for line in group
                for part in shapely.get_parts(line.intersection(footprint))
                if part.geom_type == "LineString" and part.length >= 1
            )
            if not carried:
                continue
            elevation = (
                max(terrain_upper_bound(footprint, cells, height), max(required))
                + SLAB_DEPTH
                + 0.15
            )
            decks.append(
                ElevatedRailDeck(
                    footprint, carried, supports, direction, elevation, level
                )
            )
            reserved = reserved.union(footprint)
    return decks


def track_centreline(line, start, end, station=(None, None)):
    first = start.centreline if start is not None else station[0]
    last = end.centreline if end is not None else station[1]
    return np.concatenate(
        (
            first[:0:-1] if first is not None else np.empty((0, 2)),
            np.asarray(line.coords),
            last[1:] if last is not None else np.empty((0, 2)),
        )
    )


def draw_track_hardware(meshes, line, elevation, approaches, station):
    hardware, _ = build_rail_hardware(
        track_centreline(line, *approaches, station), elevation
    )
    if hardware is None:
        return
    sleepers = bake_rail_sleepers(hardware.sleeper_transforms)
    for material, product in (
        ("rail head", hardware.rails),
        ("rail sleeper", sleepers),
    ):
        vertices = product.vertices.copy()
        for approach in approaches:
            if approach is not None:
                on_bed = shapely.dwithin(
                    shapely.points(vertices[:, :2]), approach.footprint, 1e-6
                )
                vertices[on_bed, 2] += approach.heights(vertices[on_bed]) - elevation
        meshes.add(material, vertices, product.triangles)


def draw_elevated_rail(meshes, deck, height, approaches, station_tracks):
    slab(meshes, "bridge deck", deck.footprint, deck.height, SLAB_DEPTH)
    edge_guards(meshes, shapely.orient_polygons(deck.footprint), deck)
    for support in deck.supports:
        top = deck.elevation - SLAB_DEPTH + 0.02
        samples = shapely.get_coordinates(support)
        depth = top - float(np.min(height(samples))) + 0.3
        slab(
            meshes, "bridge deck", support, lambda xy, z=top: np.full(len(xy), z), depth
        )
    for line in deck.tracks:
        bed = line.buffer(
            BALLAST_WIDTH / 2, cap_style="flat", join_style="mitre"
        ).intersection(deck.footprint)
        top = deck.elevation + BALLAST_DEPTH
        for polygon in shapely.get_parts(bed):
            if polygon.geom_type == "Polygon" and not polygon.is_empty:
                slab(
                    meshes,
                    "rail ballast",
                    polygon,
                    lambda xy, z=top: np.full(len(xy), z),
                    BALLAST_DEPTH,
                )
        draw_track_hardware(
            meshes,
            line,
            top,
            (approaches.get((line, 0)), approaches.get((line, -1))),
            (station_tracks.get((line, 0)), station_tracks.get((line, -1))),
        )
