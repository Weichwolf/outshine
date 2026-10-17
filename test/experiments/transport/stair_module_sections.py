"""Actual triangle/line heights and piecewise vertical strips, without remeshing."""

from itertools import pairwise

import numpy as np
import shapely


def cap_edge_constraints(caps, domains):
    covered = shapely.union_all(domains)
    boundaries = []
    for cap in caps:
        selected = shapely.dwithin(
            shapely.polygons(cap.vertices[cap.triangles, :2]), covered, 1e-7
        )
        f = cap.triangles[selected]
        edges = np.unique(
            np.sort(np.vstack([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]]), axis=1),
            axis=0,
        )
        boundaries.extend(shapely.linestrings(cap.vertices[edges, :2]))
    return boundaries


def interpolate(triangles, xy, values):
    a = triangles[:, 0, :2]
    u, v = triangles[:, 1, :2] - a, triangles[:, 2, :2] - a
    d = xy - a
    det = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
    if np.any(det == 0):
        raise ValueError("contact cap has degenerate projected face")
    s = (d[:, 0] * v[:, 1] - d[:, 1] * v[:, 0]) / det
    t = (u[:, 0] * d[:, 1] - u[:, 1] * d[:, 0]) / det
    return (
        values[:, 0]
        + s * (values[:, 1] - values[:, 0])
        + t * (values[:, 2] - values[:, 0])
    )


def line_pieces(vertices, faces, line):
    intersections = shapely.intersection(
        shapely.polygons(vertices[faces, :2]), shapely.LineString(line)
    )
    pieces, parents = shapely.get_parts(intersections, return_index=True)
    keep = (shapely.get_type_id(pieces) == 1) & (shapely.length(pieces) > 1e-10)
    return pieces[keep], parents[keep]


def metric_line_coverage(polygons, pieces, parents, line, reserve):
    query = shapely.LineString(line)
    points, parts = shapely.get_coordinates(pieces, return_index=True)
    stations = shapely.line_locate_point(query, shapely.points(points))
    canonical = shapely.get_coordinates(shapely.line_interpolate_point(query, stations))
    low, high = np.full(len(pieces), np.inf), np.full(len(pieces), -np.inf)
    np.minimum.at(low, parts, stations)
    np.maximum.at(high, parts, stations)
    positions = [points, canonical]
    faces = [parents[parts], parents[parts]]
    for point in line:
        station = shapely.line_locate_point(query, shapely.Point(point))
        selected = np.flatnonzero((low <= station) & (station <= high))
        positions.append(np.repeat(point[None], len(selected), axis=0))
        faces.append(parents[selected])
    distance = shapely.distance(
        shapely.points(np.vstack(positions)), polygons[np.concatenate(faces)]
    )
    if np.max(distance, initial=0) + reserve > 1e-7:
        raise ValueError("whole line exceeds its existing metric bound")
    cursor, missing = 0.0, 0.0
    for index in np.argsort(low):
        missing += max(0.0, low[index] - cursor)
        cursor = max(cursor, high[index])
    missing += max(0.0, query.length - cursor)
    if missing > reserve:
        raise ValueError("whole line has missing intervals in its metric surface")


def whole_line_pieces(vertices, faces, line):
    """Retain parent planes while bounding the complete line by the existing metric."""
    if len(faces) > 32768:
        raise ValueError("bounded whole metric line query required")
    line = np.asarray(line, dtype=float)
    pieces, parents = line_pieces(vertices, faces, line)
    query = shapely.LineString(line)
    if (
        len(pieces)
        and query.difference(shapely.union_all(pieces).buffer(1e-7)).length <= 1e-7
    ):
        return pieces, parents
    used = np.unique(faces)
    scale = max(
        1.0, float(abs(vertices[used, :2]).max(initial=0)), float(abs(line).max())
    )
    reserve = 64 * np.finfo(float).eps * scale
    if 2 * reserve >= 1e-7:
        raise ValueError("bounded whole metric line query required")
    polygons = shapely.polygons(vertices[faces, :2])
    cuts = shapely.intersection(shapely.buffer(polygons, 1e-7 - 2 * reserve), query)
    pieces, parents = shapely.get_parts(cuts, return_index=True)
    keep = (shapely.get_type_id(pieces) == 1) & (shapely.length(pieces) > 1e-10)
    pieces, parents = pieces[keep], parents[keep]
    if not len(pieces):
        raise ValueError("whole line has missing intervals in its metric surface")
    metric_line_coverage(polygons, pieces, parents, line, reserve)
    return pieces, parents


def profile_levels(vertices, faces, line):
    pieces, parents = line_pieces(vertices, faces, line)
    xy, parts = shapely.get_coordinates(pieces, return_index=True)
    triangles = vertices[faces[parents[parts]]]
    return interpolate(triangles, xy, triangles[:, :, 2])


def wall_strip(cap, segment, cast, xy_guard):
    pieces, parents = whole_line_pieces(cap.vertices, cap.triangles, segment)
    if (
        not len(pieces)
        or shapely.LineString(segment)
        .difference(shapely.union_all(pieces).buffer(1e-7))
        .length
        > 1e-7
    ):
        raise ValueError("whole contact line lacks original cap reference")
    a, b = segment
    axis = (b - a) / np.linalg.norm(b - a)
    high = cap.vertices.astype("f4").astype(float) if cast else cap.vertices
    low = cap.vertices - [0, 0, 0.3]
    low = low.astype("f4").astype(float) if cast else low
    datum = float(low[np.unique(cap.triangles), 2].min())
    polygons, gradients = [], []
    for piece, parent in zip(pieces, parents, strict=True):
        ids = cap.triangles[parent]
        for x, y in pairwise(piece.coords):
            xy = np.array([x, y])
            tri = np.repeat(high[ids][None], 2, axis=0)
            top_z = interpolate(tri, xy, tri[:, :, 2]) - datum
            bottom_z = (
                interpolate(tri, xy, np.repeat(low[ids, 2][None], 2, axis=0)) - datum
            )
            stations = (xy - a) @ axis
            polygons.append(
                shapely.Polygon(
                    [
                        (stations[0], bottom_z[0]),
                        (stations[1], bottom_z[1]),
                        (stations[1], top_z[1]),
                        (stations[0], top_z[0]),
                    ]
                )
            )
        for vertices in (high, low):
            p = vertices[ids]
            gradients.append(
                float(
                    np.linalg.norm(
                        np.linalg.solve(p[1:, :2] - p[0, :2], p[1:, 2] - p[0, 2])
                    )
                )
            )
    return shapely.union_all(polygons), datum, 1e-7 + max(gradients) * xy_guard
