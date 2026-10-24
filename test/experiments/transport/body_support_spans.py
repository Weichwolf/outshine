"""One-sided source-path support from actual indexed body caps, without refitting."""

from itertools import pairwise

import numpy as np
import shapely


def _input(body, path, allowed, max_faces, max_tests):
    vertices, faces, reference, masks, steps = (
        body[k]
        for k in (
            "body_vertices",
            "top_faces",
            "reference_faces",
            "face_sources",
            "face_step",
        )
    )
    path = np.asarray(path, dtype=float)
    reference_vertices = body["reference_vertices"]
    levels = body["levels_m"]
    if (
        type(allowed) is not int
        or not 0 < allowed < 2**64
        or not 2 <= len(path) <= 4097
        or path.shape[1:] != (2,)
        or not np.isfinite(path).all()
        or vertices.ndim != 2
        or vertices.shape[1:] != (3,)
        or vertices.dtype not in (np.dtype("float32"), np.dtype("float64"))
        or reference_vertices.ndim != 2
        or reference_vertices.shape[1:] != (3,)
        or faces.ndim != 2
        or faces.shape[1:] != (3,)
        or faces.dtype.kind not in "iu"
        or reference.shape != faces.shape
        or reference.dtype.kind not in "iu"
        or masks.shape != (len(faces),)
        or masks.dtype.kind not in "iu"
        or steps.shape != masks.shape
        or steps.dtype.kind not in "iu"
        or levels.ndim != 1
        or not np.isfinite(levels).all()
        or np.any(steps < -1)
        or np.any(steps >= len(levels))
        or len(faces) > max_faces
        or (
            len(faces)
            and (
                faces.min() < 0
                or faces.max() >= len(vertices)
                or reference.min() < 0
                or reference.max() >= len(reference_vertices)
            )
        )
        or np.any(masks <= 0)
    ):
        raise ValueError(
            "bounded indexed body caps, ownership and source path required"
        )
    ownership = masks.astype(np.uint64, copy=False)
    known = int(np.bitwise_or.reduce(ownership, initial=np.uint64(0)))
    if allowed & ~known:
        raise ValueError("unknown source bits cannot select body support")
    if len(np.unique(np.sort(faces, axis=1), axis=0)) != len(faces):
        raise ValueError("duplicate body caps are not distinct support")
    if not np.array_equal(
        vertices[faces, :2],
        reference_vertices[reference, :2].astype(vertices.dtype),
    ):
        raise ValueError("body/reference cap XY correspondence required")
    owned = steps >= 0
    if not np.array_equal(
        vertices[faces[owned], 2],
        np.repeat(levels[steps[owned]].astype(vertices.dtype)[:, None], 3, axis=1),
    ):
        raise ValueError("body step labels must retain their actual flat levels")
    selected = np.flatnonzero((ownership & allowed) != 0)
    if not len(selected) or len(selected) * (len(path) - 1) > max_tests:
        raise ValueError("body support face-test budget exhausted or no declared owner")
    gathered = vertices[faces[selected]]
    triangles = np.asarray(gathered, dtype=float)
    xy = triangles[:, :, :2]
    edges = xy[:, 1:] - xy[:, :1]
    determinant = edges[:, 0, 0] * edges[:, 1, 1] - edges[:, 0, 1] * edges[:, 1, 0]
    if not np.isfinite(triangles).all() or np.any(determinant <= 0):
        raise ValueError("finite positive projected body caps required")
    lengths = np.linalg.norm(np.diff(path, axis=0), axis=1)
    if np.any(lengths <= 0):
        raise ValueError("positive source-path segments required")
    return (
        path,
        selected,
        triangles,
        lengths,
        {
            "triangle_gather_bytes": gathered.nbytes,
            "triangle_cast_bytes": 0 if triangles is gathered else triangles.nbytes,
        },
    )


def _shared_edge(body, first, second, segment):
    common = sorted(set(body["top_faces"][first]) & set(body["top_faces"][second]))
    return len(common) == 2 and shapely.covers(
        shapely.LineString(body["body_vertices"][common, :2]), segment
    )


def _coordinates(pieces, strategy):
    if strategy == "batch":
        kinds = shapely.get_type_id(pieces)
        empty = shapely.is_empty(pieces)
        if np.any(~empty & (kinds != 0) & (kinds != 1)):
            raise ValueError("a convex cap must have one source-path interval")
        selected = np.flatnonzero(~empty & (kinds == 1))
        lines = pieces[selected]
        if np.any(shapely.get_num_coordinates(lines) != 2):
            raise ValueError("straight convex clipping must retain two endpoints")
        return selected, shapely.get_coordinates(lines).reshape(-1, 2, 2)
    selected, xy = [], []
    for i, piece in enumerate(pieces):
        if piece.is_empty:
            continue
        kind = piece.geom_type
        if kind == "Point":
            continue
        if kind != "LineString":
            raise ValueError("a convex cap must have one source-path interval")
        coordinates = np.asarray(piece.coords)
        if coordinates.shape != (2, 2):
            raise ValueError("straight convex clipping must retain two endpoints")
        selected.append(i)
        xy.append(coordinates)
    return np.asarray(selected, dtype=int), np.asarray(xy).reshape(-1, 2, 2)


def _ordered(body, selected, pieces, line, domain, a, b, remaining, strategy):
    raw = []
    delta = b - a
    indices, xy = _coordinates(pieces, strategy)
    values = (xy - a) @ delta / float(delta @ delta)
    values[np.all(xy == a, axis=2)] = 0
    values[np.all(xy == b, axis=2)] = 1
    order = np.argsort(values, axis=1)
    values = np.take_along_axis(values, order, axis=1)
    xy = np.take_along_axis(xy, order[:, :, None], axis=1)
    for at, row in enumerate(indices):
        lo, hi = map(float, values[at])
        if lo == hi:
            continue
        raw.append((lo, hi, int(selected[row]), xy[at], pieces[row]))
    raw.sort(key=lambda r: r[:3])
    result, cursor = [], 0.0
    for record in raw:
        lo, hi, face, _xy, piece = record
        if (
            result
            and record[:2] == result[-1][:2]
            and _shared_edge(body, result[-1][2], face, piece)
        ):
            continue
        if lo != cursor or not lo < hi <= 1:
            raise ValueError("source path has absent or ambiguous body support")
        result.append(record)
        if len(result) > remaining:
            raise ValueError("body support span budget exhausted")
        cursor = hi
    if cursor != 1 or not shapely.covers(domain, line):
        raise ValueError("source path lacks complete body support")
    return result


def _points(triangles, uv):
    return triangles[:, :1] + np.einsum(
        "nei,nij->nej", uv, triangles[:, 1:] - triangles[:, :1]
    )


def support_points(result):
    triangles = result["body"]["body_vertices"][
        result["body"]["top_faces"][result["body_cap_face_ids"]]
    ].astype(float)
    return _points(triangles, result["barycentric_uv"])


def _connections(body, faces, points):
    jumps, kind = [], []
    for i, (first, second) in enumerate(pairwise(faces)):
        if (
            first == second
            or len(set(body["top_faces"][first]) & set(body["top_faces"][second])) >= 2
        ):
            jumps.append(0.0)
            kind.append("continuous")
            continue
        common = set(body["reference_faces"][first]) & set(
            body["reference_faces"][second]
        )
        if len(common) < 2 or body["face_step"][first] == body["face_step"][second]:
            raise ValueError("disconnected body sheets do not create a navigation join")
        jumps.append(float(points[i + 1, 0, 2] - points[i, 1, 2]))
        kind.append("body_step")
    return np.asarray(jumps), kind


def trace(
    body,
    path,
    allowed_sources_mask,
    *,
    strategy="batch",
    max_faces=8192,
    max_face_tests=262144,
    max_spans=32768,
    cancelled=None,
):
    """Borrow an immutable body; return support, not agent traversal clearance."""
    if strategy not in ("batch", "scalar"):
        raise ValueError("known body clipping strategy required")
    if cancelled is not None and cancelled():
        raise ValueError("body support trace cancelled")
    if any(type(v) is not int or v < 0 for v in (max_faces, max_face_tests, max_spans)):
        raise ValueError("finite nonnegative support work budgets required")
    path, selected, triangles, lengths, copies = _input(
        body, path, allowed_sources_mask, max_faces, max_face_tests
    )
    polygons = shapely.polygons(triangles[:, :, :2])
    domain = shapely.union_all(polygons)
    segments, faces, bounds, endpoints = [], [], [], []
    for index, (a, b) in enumerate(pairwise(path)):
        if cancelled is not None and cancelled():
            raise ValueError("body support trace cancelled")
        line = shapely.LineString([a, b])
        pieces = (
            shapely.intersection(polygons, line)
            if strategy == "batch"
            else [shapely.intersection(p, line) for p in polygons]
        )
        for lo, hi, face, xy, _piece in _ordered(
            body, selected, pieces, line, domain, a, b, max_spans - len(faces), strategy
        ):
            segments.append(index)
            faces.append(face)
            bounds.append([lo, hi])
            endpoints.append(xy)
    segments = np.asarray(segments, dtype=np.uint32)
    faces = np.asarray(faces, dtype=np.uint32)
    bounds = np.asarray(bounds)
    gathered_cap = body["body_vertices"][body["top_faces"][faces]]
    cap = np.asarray(gathered_cap, dtype=float)
    copies.update(
        support_cap_gather_bytes=gathered_cap.nbytes,
        support_cap_cast_bytes=0 if cap is gathered_cap else cap.nbytes,
    )
    inverse = np.linalg.inv(cap[:, 1:, :2] - cap[:, :1, :2])
    uv = (np.asarray(endpoints) - cap[:, :1, :2]) @ inverse
    prefix = np.r_[0.0, np.cumsum(lengths)]
    arrays = {
        "source_segment_ids": segments,
        "body_cap_face_ids": faces,
        "parameter_bounds": bounds,
        "station_bounds_m": prefix[segments, None] + bounds * lengths[segments, None],
        "barycentric_uv": uv,
        "source_masks": body["face_sources"][faces].astype(np.uint64),
    }
    result = {"body": body, "path_xy_m": path, **arrays}
    jumps, kinds = _connections(body, faces, _points(cap, uv))
    result.update(
        height_jumps_m=jumps,
        connections=kinds,
        work={
            "selected_faces": len(selected),
            "source_segments": len(path) - 1,
            "face_intersections": len(selected) * (len(path) - 1),
            "intersection_dispatches": len(path) - 1
            if strategy == "batch"
            else len(selected) * (len(path) - 1),
            "support_spans": len(faces),
            "selected_array_bytes": sum(a.nbytes for a in arrays.values())
            + jumps.nbytes,
            **copies,
        },
    )
    return result
