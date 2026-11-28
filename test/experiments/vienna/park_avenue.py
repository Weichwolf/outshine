"""Infer managed tree rows from shared park routes and mapped lawns."""

import numpy as np
import shapely
from ground_surface import at_grade_width, route_area
from plant_sites import patch_density
from woodland_sites import spaced_sites

ROW_SPACING_M = 9.5
VERGE_DEPTH_M = 3.0
LAWN_SUBCLASSES = {"recreation_ground", "village_green"}


def shared_park_route(tags):
    return tags.get("class") in {"minor", "service"} and all(
        tags.get(mode) in {"yes", "designated"} for mode in ("foot", "bicycle", "horse")
    )


def row_points(route, offset):
    line = shapely.normalize(route)
    stations = np.arange(ROW_SPACING_M / 2, line.length - 1, ROW_SPACING_M)
    centre = shapely.get_coordinates(shapely.line_interpolate_point(line, stations))
    ahead = shapely.get_coordinates(shapely.line_interpolate_point(line, stations + 1))
    behind = shapely.get_coordinates(shapely.line_interpolate_point(line, stations - 1))
    tangent = ahead - behind
    norm = np.linalg.norm(tangent, axis=1)
    distinct = norm > 1e-9
    across = tangent[distinct, ::-1] * [1, -1] / norm[distinct, None]
    displaced = across * offset
    return np.concatenate((centre[distinct] + displaced, centre[distinct] - displaced))


def avenue_sites(source, half_extent, seed):
    lawns, obstructions, woods, transport, routes = [], [], [], [], []
    for layer, shape, tags in source.features:
        if layer == "landcover" and tags.get("subclass") in LAWN_SUBCLASSES:
            lawns.append(shape)
        elif layer in {"building", "water"}:
            obstructions.append(shape)
        elif layer == "landcover" and tags.get("class") == "wood":
            woods.append(shape)
        elif layer == "transportation" and (width := at_grade_width(tags)) is not None:
            transport.append((shape, width))
            if shared_park_route(tags):
                routes.extend(
                    (line, width)
                    for line in shapely.get_parts(shapely.line_merge(shape))
                    if line.geom_type == "LineString"
                    and line.length > 3 * ROW_SPACING_M
                )
    if not lawns or not routes:
        return shapely.Polygon(), np.empty((0, 2)), np.empty((0, 3))
    reservations = [
        *shapely.buffer(shapely.make_valid(obstructions), 4.5),
        *shapely.make_valid(woods),
        *(route_area(shape, width) for shape, width in transport),
    ]
    lawn = shapely.union_all(shapely.make_valid(lawns)).difference(
        shapely.union_all(reservations)
    )
    planting = lawn.buffer(-0.85)
    xy = np.concatenate(
        [row_points(line, width / 2 + VERGE_DEPTH_M) for line, width in routes]
    )
    keep = np.all(np.abs(xy) < half_extent, axis=1) & shapely.contains_xy(
        planting, *xy.T
    )
    xy = xy[keep]
    rank = patch_density(source, xy, seed ^ 0xA4D8, scale=0.1)
    distinct = spaced_sites(xy, rank, ROW_SPACING_M * 0.60)
    xy = xy[distinct]
    marks = np.column_stack(
        (
            rank[distinct],
            patch_density(source, xy, seed ^ 0x79A1, scale=0.1),
            1.15 + 0.5 * patch_density(source, xy, seed ^ 0xDB76, scale=0.2),
        )
    )
    return lawn, xy, marks
