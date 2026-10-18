"""Bounded whole line traces through actual face planes, including all breakpoints."""

from itertools import pairwise

import numpy as np
import shapely
from stair_module_sections import interpolate
from surface_height_checks import normal_gradients


def trace_bounds(groups, line, cast):
    line_error = (
        float(np.linalg.norm(line.astype("f4").astype(float) - line, axis=1).max())
        if cast
        else 0
    )
    selected, xy_errors, z_errors, slopes = [], [], [], []
    for group in groups:
        caps, group_z = [], []
        for vertices, faces, domain in group:
            if not len(faces):
                continue
            ids = np.unique(faces)
            xyz_error = vertices[ids].astype("f4").astype(float) - vertices[ids]
            broad = (
                1e-7
                + line_error
                + (float(np.linalg.norm(xyz_error[:, :2], axis=1).max()) if cast else 0)
            )
            near = shapely.dwithin(
                shapely.polygons(vertices[faces, :2]), shapely.LineString(line), broad
            )
            faces = faces[near]
            if not len(faces):
                continue
            ids = np.unique(faces)
            xyz_error = vertices[ids].astype("f4").astype(float) - vertices[ids]
            xy_errors.append(
                float(np.linalg.norm(xyz_error[:, :2], axis=1).max()) if cast else 0
            )
            group_z.append(float(abs(xyz_error[:, 2]).max()) if cast else 0)
            actual = vertices.astype("f4").astype(float) if cast else vertices
            slopes.append(
                float(
                    np.linalg.norm(
                        normal_gradients(actual[:, :2], faces, actual[:, 2]), axis=1
                    ).max()
                )
            )
            caps.append((actual, faces, domain))
        selected.append(caps)
        z_errors.append(max(group_z, default=0))
    xy_guard = 1e-7 + line_error + max(xy_errors, default=0)
    z_guard = 1e-7 + sum(z_errors) + 2 * max(slopes, default=0) * xy_guard
    return selected, {
        "xy_guard_m": xy_guard,
        "z_guard_m": z_guard,
        "group_vertex_z_rounding_m": z_errors,
        "maximum_local_face_grade": max(slopes, default=0),
    }


def trace(caps, line, xy_guard):
    length = float(np.linalg.norm(line[-1] - line[0]))
    axis = (line[-1] - line[0]) / length
    result = []
    for vertices, faces, domain in caps:
        polygons = shapely.buffer(shapely.polygons(vertices[faces, :2]), xy_guard)
        if domain is not None:
            polygons = shapely.intersection(polygons, domain.buffer(xy_guard))
        cuts, parents = shapely.get_parts(
            shapely.intersection(polygons, shapely.LineString(line)), return_index=True
        )
        keep = (shapely.get_type_id(cuts) == 1) & (shapely.length(cuts) > 1e-10)
        for cut, parent in zip(cuts[keep], parents[keep], strict=True):
            for a, b in pairwise(cut.coords):
                xy = np.array([a, b])
                station = (xy - line[0]) @ axis
                if abs(station[1] - station[0]) <= 1e-10:
                    continue
                triangles = np.repeat(vertices[faces[parent]][None], 2, axis=0)
                heights = interpolate(triangles, xy, triangles[:, :, 2])
                order = np.argsort(station)
                result.append(np.r_[station[order], heights[order]])
                if len(result) > 4096:
                    raise ValueError("profile trace piece budget exhausted")
    return np.array(result).reshape(-1, 4)


def agree(a, b, length, *, xy_guard, z_guard, max_tests=65536):
    if not len(a) or not len(b):
        raise ValueError("whole profile has no actual owned trace")
    knots = np.unique(
        np.clip(np.r_[0, length, a[:, :2].ravel(), b[:, :2].ravel()], 0, length)
    )
    intervals = [(x, y) for x, y in pairwise(knots) if y - x > 1e-10]
    tests = len(intervals) * (len(a) + len(b))
    if tests > max_tests:
        raise ValueError("whole profile interval comparison budget exhausted")
    maximum = 0.0
    for low, high in intervals:
        middle = (low + high) / 2
        values = []
        for pieces in (a, b):
            selected = pieces[(pieces[:, 0] <= middle) & (pieces[:, 1] >= middle)]
            if not len(selected):
                raise ValueError("whole profile has an uncovered interval")
            fraction = (np.array([low, high])[None] - selected[:, 0, None]) / (
                selected[:, 1] - selected[:, 0]
            )[:, None]
            values.append(
                selected[:, 2, None]
                + fraction * (selected[:, 3] - selected[:, 2])[:, None]
            )
        spread = float(np.ptp(np.vstack(values), axis=0).max())
        maximum = max(maximum, spread)
        if spread > z_guard:
            raise ValueError("whole profile heights disagree between actual owners")
    return {
        "intervals": len(intervals),
        "piece_tests": tests,
        "maximum_height_disagreement_m": maximum,
        "source_trace_pieces": [len(a), len(b)],
    }


def compare(groups, line, cast):
    selected, bounds = trace_bounds(groups, line, cast)
    actual_line = line.astype("f4").astype(float) if cast else line
    pieces = [trace(caps, actual_line, bounds["xy_guard_m"]) for caps in selected]
    proof = agree(
        *pieces,
        float(np.linalg.norm(actual_line[-1] - actual_line[0])),
        xy_guard=bounds["xy_guard_m"],
        z_guard=bounds["z_guard_m"],
    )
    return {"float32": cast, **bounds, **proof}
