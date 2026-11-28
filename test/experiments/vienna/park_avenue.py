"""Infer managed tree rows from shared park routes and mapped lawns."""

import numpy as np
import shapely
from ground_surface import at_grade_width, route_area
from plant_sites import patch_density
from woodland_sites import spaced_sites

ROW_SPACING_M = 9.5
PARALLEL_ROW_SPACING_M = 7.6
VERGE_DEPTH_M = 3.0
SIDE_PATH_VERGE_M = 3.4
SIDE_PATH_MAX_M = 28.0
SIDE_PATH_MIN_GAP_M = 5.0
DISTINCT_PATHS_M = 2.0
LAWN_SUBCLASSES = {"recreation_ground", "village_green"}
SIDE_PATHS = {"footway", "bridleway", "cycleway"}


def shared_park_route(tags):
    return tags.get("class") in {"minor", "service"} and all(
        tags.get(mode) in {"yes", "designated"} for mode in ("foot", "bicycle", "horse")
    )


def row_sections(line, spacing):
    stations = np.arange(spacing / 2, line.length - 1, spacing)
    centre = shapely.get_coordinates(shapely.line_interpolate_point(line, stations))
    ahead = shapely.get_coordinates(shapely.line_interpolate_point(line, stations + 1))
    behind = shapely.get_coordinates(shapely.line_interpolate_point(line, stations - 1))
    tangent = ahead - behind
    norm = np.linalg.norm(tangent, axis=1)
    distinct = norm > 1e-9
    return centre[distinct], tangent[distinct] / norm[distinct, None]


def parallel_path_offsets(path, centres, tangent, across):
    station = shapely.line_locate_point(path, shapely.points(centres))
    side = shapely.get_coordinates(shapely.line_interpolate_point(path, station))
    ends = [
        shapely.get_coordinates(
            shapely.line_interpolate_point(
                path, np.clip(station + step, 0, path.length)
            )
        )
        for step in (-5, 5)
    ]
    direction = ends[1] - ends[0]
    separation = np.einsum("ij,ij->i", side - centres, across)
    forward = np.einsum("ij,ij->i", side - centres, tangent)
    reach = np.linalg.norm(direction, axis=1)
    alignment = abs(np.einsum("ij,ij->i", direction, tangent))
    near = (
        (abs(separation) >= 7.0)
        & (abs(separation) <= SIDE_PATH_MAX_M)
        & (abs(forward) <= 2.5)
        & (reach >= 5.0)
        & (alignment >= 0.94 * reach)
    )
    # A usable verge follows the road at successive stations.
    continuous = near[1:] & near[:-1] & (abs(np.diff(separation)) < 1.7)
    return np.where(
        np.r_[continuous, False] | np.r_[False, continuous], separation, np.nan
    )


def verge_offsets(paths, bounds, inner):
    if not len(paths):
        return (inner,)
    first = paths[0]
    second = paths[paths >= first + SIDE_PATH_MIN_GAP_M]
    inner = max(inner, first - SIDE_PATH_VERGE_M)
    outer = second[0] if len(second) else first
    limit = bounds[bounds >= outer + DISTINCT_PATHS_M]
    verge = outer + SIDE_PATH_VERGE_M
    if len(limit):
        verge = min(verge, 0.5 * (outer + limit[0]))
    if not len(second):
        return inner, verge
    return inner, 0.5 * (first + outer), verge


def row_points(route, width, side_paths):
    line = shapely.normalize(route)
    inner = width / 2 + VERGE_DEPTH_M
    centres, tangent = row_sections(line, PARALLEL_ROW_SPACING_M / 2)
    across = tangent[:, ::-1] * [1, -1]
    nearby = [
        (path, supports_rows)
        for path, supports_rows in side_paths
        if path.dwithin(line, SIDE_PATH_MAX_M)
    ]
    anchors = np.array([supports_rows for _, supports_rows in nearby], dtype=bool)
    offsets = np.array(
        [parallel_path_offsets(path, centres, tangent, across) for path, _ in nearby]
    )
    if not np.isfinite(offsets[anchors]).any():
        centres, tangent = row_sections(line, ROW_SPACING_M)
        displaced = tangent[:, ::-1] * [1, -1] * inner
        return np.concatenate((centres + displaced, centres - displaced))

    candidates = []
    for index, (centre, axis, sides) in enumerate(
        zip(centres, across, offsets.T, strict=True)
    ):
        for sign in (-1, 1):
            signed = sides * sign
            paths = np.sort(signed[(signed > 0) & anchors])
            bounds = np.sort(signed[signed > 0])
            for row, distance in enumerate(verge_offsets(paths, bounds, inner)):
                if index % 2 == row % 2:
                    candidates.append(centre + sign * distance * axis)
    return np.asarray(candidates).reshape(-1, 2)


def avenue_sites(source, half_extent, seed):
    lawns, obstructions, woods, transport, routes, side_paths = [], [], [], [], [], []
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
            elif tags.get("class") == "path":
                supports_rows = tags.get("subclass") in SIDE_PATHS
                side_paths.extend(
                    (line, supports_rows)
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
    xy = np.concatenate([row_points(line, width, side_paths) for line, width in routes])
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
