"""Bound every affine face overlap at all polygon corners, including crossed edges."""

import time

import numpy as np
import shapely


def overlap_bounds(
    xy,
    faces,
    barrier,
    *,
    offset_m=0,
    sign=1,
    seconds=4,
    max_pairs=65536,
    max_faces=8192,
):
    if (
        sign not in (-1, 1)
        or not np.isfinite(offset_m)
        or not np.isfinite(seconds)
        or seconds < 0
        or type(max_pairs) is not int
        or not 0 <= max_pairs <= 65536
    ):
        raise ValueError("finite signed height offset and time budget required")
    if type(max_faces) is not int or not 1 <= max_faces <= 262144:
        raise ValueError("affine bound input allocation cap")
    if not 1 <= len(faces) <= max_faces or not 1 <= len(barrier.triangles) <= max_faces:
        raise ValueError("affine bound face budget")
    began = time.monotonic()
    shapes = shapely.polygons(xy[faces])
    tree = shapely.STRtree(shapes)
    columns, coefficients, limits = [], [], []
    checks = 0
    for tri in barrier.vertices[barrier.triangles]:
        if time.monotonic() - began > seconds:
            raise ValueError("affine bound time budget")
        polygon = shapely.Polygon(tri[:, :2])
        gradient = np.linalg.solve(tri[1:, :2] - tri[0, :2], tri[1:, 2] - tri[0, 2])
        candidates = tree.query(polygon, predicate="intersects")
        checks += len(candidates)
        if checks > max_pairs:
            raise ValueError("affine bound pair budget")
        for face in np.sort(candidates):
            if time.monotonic() - began > seconds:
                raise ValueError("affine bound time budget")
            overlap = polygon.intersection(shapes[face])
            if overlap.area <= 0:
                continue
            points = shapely.get_coordinates(overlap)[:-1]
            if len(limits) + len(points) > 65536:
                raise ValueError("affine bound row budget")
            vertices = xy[faces[face]]
            uv = np.linalg.solve(
                (vertices[1:] - vertices[0]).T, (points - vertices[0]).T
            ).T
            barycentric = np.c_[1 - uv.sum(axis=1), uv]
            heights = tri[0, 2] + (points - tri[0, :2]) @ gradient + offset_m
            columns.extend(np.broadcast_to(faces[face], (len(points), 3)))
            coefficients.extend(sign * barycentric)
            limits.extend(sign * heights)
    return (
        np.asarray(columns, dtype=int).reshape(-1, 3),
        np.asarray(coefficients).reshape(-1, 3),
        np.asarray(limits),
    ), {"pairs": checks, "rows": len(limits), "ms": (time.monotonic() - began) * 1000}


def vertex_caps(indices, heights):
    return (
        np.repeat(np.asarray(indices)[:, None], 3, axis=1),
        np.tile([1.0, 0, 0], (len(indices), 1)),
        np.array(heights, dtype=float, copy=True),
    )


def joined(*bounds):
    return tuple(np.concatenate(values) for values in zip(*bounds))
