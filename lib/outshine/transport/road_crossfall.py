"""Fit a crowned carriageway to mapped road axes on a shared world lattice."""

from collections import defaultdict
from dataclasses import dataclass

import numpy as np
import shapely

from outshine.terrain.ground_surface import ROAD_WIDTH, at_grade_width

from .crown_constraints import crown_constraints
from .road_grades import GRADE_LIMITS
from .route_tiles import WORLD_CELLS

MERCATOR_RADIUS_M = 6378137
WORLD_METRES = 2 * np.pi * MERCATOR_RADIUS_M
METRES_PER_CELL = WORLD_METRES / WORLD_CELLS
EARTHWORK_TAPER_M = 18.0
PAVEMENT_CROSS_SLOPE = 0.025
CROSSFALL_ONSET = 0.08
CROSSFALL_FULL = 0.25
GRADE_TRANSITION = 0.04
MAX_ROAD_WIDTH_M = max(ROAD_WIDTH.values())
BEND_COSINE = np.cos(np.deg2rad(15.0))
GROUND = ("land", 0, "", "", False)


def _unwrap(east, centre, period):
    return centre + (east - centre + period / 2) % period - period / 2


@dataclass(frozen=True, slots=True)
class MercatorTile:
    tile: tuple[int, int, int]

    @property
    def west(self):
        return self.tile[1] * (WORLD_METRES / (1 << self.tile[0]))

    @property
    def span(self):
        return WORLD_METRES / (1 << self.tile[0])

    def project(self, longitude, latitude):
        east = (longitude + 180) * (WORLD_METRES / 360)
        east = _unwrap(east, self.west + self.span / 2, WORLD_METRES)
        south = MERCATOR_RADIUS_M * (np.pi - np.arcsinh(np.tan(np.deg2rad(latitude))))
        return np.column_stack((east, south))

    def route_points(self, coordinates):
        projected = np.asarray(coordinates, dtype=float) * METRES_PER_CELL
        projected[..., 0] = _unwrap(
            projected[..., 0], self.west + self.span / 2, WORLD_METRES
        )
        return projected

    def grid(self, shape):
        rows, columns = shape
        zoom, x, y = self.tile
        stride = WORLD_CELLS >> zoom
        east = (x * stride + np.linspace(0, stride, columns)) * METRES_PER_CELL
        south = (y * stride + np.linspace(stride, 0, rows)) * METRES_PER_CELL
        xy = np.stack(np.meshgrid(east, south), axis=-1).reshape(-1, 2)
        diagonal = self.span * np.hypot(1 / (rows - 1), 1 / (columns - 1))
        return xy, diagonal

    def box(self, tile):
        zoom, x, y = tile
        span = WORLD_METRES / (1 << zoom)
        west = _unwrap(x * span, self.west + self.span / 2, WORLD_METRES)
        return shapely.box(west, y * span, west + span, (y + 1) * span)


def geodetic(xy):
    east, south = np.moveaxis(np.asarray(xy), -1, 0)
    longitude = (east / WORLD_METRES % 1) * 360 - 180
    latitude = np.rad2deg(np.arctan(np.sinh(np.pi - south / MERCATOR_RADIUS_M)))
    return longitude, latitude


def _road_layout(graph):
    edges = {}
    neighbours = defaultdict(set)
    for path in graph.paths:
        if path.attributes.domain != GROUND:
            continue
        grade = GRADE_LIMITS.get(path.attributes.kind)
        width = (
            at_grade_width(dict(path.attributes.tags)) if grade is not None else None
        )
        for a, b in zip(path.nodes[:-1], path.nodes[1:], strict=True):
            if a.point == b.point:
                continue
            neighbours[a.point].add(b.point)
            neighbours[b.point].add(a.point)
            if width is not None:
                pair = tuple(sorted((a.point, b.point)))
                before = edges.get(pair, (0, 1))
                edges[pair] = max(before[0], width), min(before[1], grade)
    junctions = {node for node, adjacent in neighbours.items() if len(adjacent) != 2}
    junctions.update(contact.road.point for contact in graph.contacts)
    bends = []
    for node, adjacent in neighbours.items():
        if len(adjacent) == 2:
            direction = np.array(list(adjacent), dtype=float) - node
            direction[:, 0] = _unwrap(direction[:, 0], 0, WORLD_CELLS)
            direction /= np.linalg.norm(direction, axis=1)[:, None]
            if direction[0] @ direction[1] > -BEND_COSINE:
                bends.append(node)
    approaches = {
        tuple(sorted((portal.ground.point, portal.ground_next)))
        for portal in graph.portals
    }
    return edges, sorted(junctions), sorted(bends), approaches


def _smooth(weight):
    weight = np.clip(weight, 0, 1)
    return weight * weight * (3 - 2 * weight)


def _support_weight(xy, scale, diagonal, geometries, margin, taper):
    if not len(geometries):
        return 1.0
    points = shapely.points(xy)
    tree = shapely.STRtree(geometries)
    nearest = tree.geometries[tree.nearest(points)]
    distance = shapely.distance(points, nearest) * scale
    return _smooth((distance - diagonal * scale - margin) / taper)


def grade_crossfall(tile, latitude, solved, graph, elevation, reserved=(), absent=()):
    edges, junctions, bends, approaches = _road_layout(graph)
    if not edges:
        return solved, None
    keys = sorted(edges)
    segments = tile.route_points(keys)
    ways = shapely.linestrings(segments)
    index = shapely.STRtree(ways)
    widths, limits = np.array([edges[key] for key in keys]).T
    xy, diagonal = tile.grid(solved.shape)
    scale = np.repeat(np.cos(np.deg2rad(latitude)), solved.shape[1])
    reach = diagonal + (EARTHWORK_TAPER_M + MAX_ROAD_WIDTH_M / 2) / scale.min()
    vertex, owner = index.query_nearest(shapely.points(xy), max_distance=reach)
    if not len(vertex):
        return solved, None
    order = np.lexsort((owner, vertex))
    unique = order[np.r_[True, np.diff(vertex[order]) != 0]]
    vertex, owner = vertex[unique], owner[unique]
    start, end = segments[owner].transpose(1, 0, 2)
    direction = end - start
    length = np.linalg.norm(direction, axis=1)
    direction /= length[:, None]
    along = np.clip(((xy[vertex] - start) * direction).sum(axis=1), 0, length)
    anchor = start + along[:, None] * direction
    local_scale = scale[vertex]
    offset = np.linalg.norm(xy[vertex] - anchor, axis=1) * local_scale
    half_width = widths[owner] / 2
    blend = _smooth(
        1 - (offset - half_width - diagonal * local_scale) / EARTHWORK_TAPER_M
    )
    blocked = [
        *reserved,
        *(way for key, way in zip(keys, ways, strict=True) if key in approaches),
    ]
    if junctions:
        blocked.extend(shapely.points(tile.route_points(junctions)))
    a, b = index.query(ways, predicate="crosses")
    unique_crossings = a < b
    if unique_crossings.any():
        blocked.extend(
            shapely.intersection(ways[a[unique_crossings]], ways[b[unique_crossings]])
        )
    blend *= _support_weight(
        xy[vertex], local_scale, diagonal, blocked, MAX_ROAD_WIDTH_M, EARTHWORK_TAPER_M
    )
    corners = shapely.points(tile.route_points(bends)) if bends else ()
    blend *= _support_weight(
        xy[vertex],
        local_scale,
        diagonal,
        corners,
        2 * MAX_ROAD_WIDTH_M,
        EARTHWORK_TAPER_M,
    )
    blend *= _support_weight(
        xy[vertex],
        local_scale,
        diagonal,
        absent,
        EARTHWORK_TAPER_M + MAX_ROAD_WIDTH_M / 2,
        MAX_ROAD_WIDTH_M,
    )
    keep = blend > 0
    if not keep.any():
        return solved, None
    vertex, owner, anchor, direction, local_scale, offset, half_width, blend = (
        values[keep]
        for values in (
            vertex,
            owner,
            anchor,
            direction,
            local_scale,
            offset,
            half_width,
            blend,
        )
    )
    normal = direction[:, [1, 0]] * [-1, 1]
    shoulder = normal * (half_width / local_scale)[:, None]
    fore = direction * (half_width / local_scale)[:, None]
    lateral, centre, far, back, forward = elevation(
        np.stack(
            (anchor - shoulder, anchor, anchor + shoulder, anchor - fore, anchor + fore)
        )
    )
    width = 2 * half_width
    crossfall = np.abs(far - lateral) / width
    grade = np.abs(forward - back) / width
    blend *= _smooth((crossfall - CROSSFALL_ONSET) / (CROSSFALL_FULL - CROSSFALL_ONSET))
    blend *= _smooth(1 - (grade - limits[owner]) / GRADE_TRANSITION)
    valid = np.isfinite(blend) & np.isfinite(centre) & (blend > 0)
    result = solved.copy().ravel()
    selected = vertex[valid]
    crown = centre[valid] - PAVEMENT_CROSS_SLOPE * offset[valid]
    result[selected] += blend[valid] * (crown - result[selected])
    target = result.reshape(solved.shape)
    return target, crown_constraints(
        xy, latitude, solved, target, segments, widths, limits
    )
