"""Actual triangle/line heights and piecewise vertical strips, without remeshing."""

from itertools import pairwise

import numpy as np
import shapely


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


def profile_levels(vertices, faces, line):
    pieces, parents = line_pieces(vertices, faces, line)
    xy, parts = shapely.get_coordinates(pieces, return_index=True)
    triangles = vertices[faces[parents[parts]]]
    return interpolate(triangles, xy, triangles[:, :, 2])


def wall_strip(cap, segment, cast, xy_guard):
    pieces, parents = line_pieces(cap.vertices, cap.triangles, segment)
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
