"""Conforming adaptive cube patches for origin-star-shaped organic fork surfaces."""

from bisect import bisect_left, bisect_right

import numpy as np
from cube_surface import face_axes
from fork_surface import mesh_product
from spine_frames import unit


class SurfaceSamples:
    def __init__(self, field):
        self.field = field
        self.values = {}
        self.evaluations = 0

    def radii(self, keys):
        shape = keys.shape[:-1]
        unique, inverse = np.unique(keys.reshape(-1, 3), axis=0, return_inverse=True)
        tuples = [tuple(int(v) for v in key) for key in unique]
        missing = [key for key in tuples if key not in self.values]
        if missing:
            radii, stats = self.field.radial_radius(np.asarray(missing, dtype=float))
            self.values.update(zip(missing, radii.tolist(), strict=True))
            self.evaluations += stats["field_point_evaluations"]
        return np.array([self.values[key] for key in tuples])[inverse].reshape(shape)


def patch_points(cells, extent):
    cells = np.asarray(cells, dtype=int)
    frames = np.array(
        [face_axes(axis, sign) for axis in range(3) for sign in (-1, 1)], dtype=int
    )
    frames = frames[2 * cells[:, 0] + (cells[:, 1] > 0)]
    normal, first, second = frames[:, 0], frames[:, 1], frames[:, 2]
    offsets = np.array(((0, 0), (1, 0), (1, 1), (0, 1)))
    corners = cells[:, None, 2:4] + offsets * cells[:, None, 4:5]
    spatial = (
        extent * normal[:, None]
        + corners[:, :, :1] * first[:, None]
        + corners[:, :, 1:] * second[:, None]
    )
    centre = (
        extent * normal
        + (cells[:, 2] + cells[:, 4] // 2)[:, None] * first
        + (cells[:, 3] + cells[:, 4] // 2)[:, None] * second
    )
    return np.concatenate((spatial, centre[:, None]), axis=1)


def patch_errors(points, samples):
    bases = points * 6
    triangles = bases[:, ((4, 0, 1), (4, 1, 2), (4, 2, 3), (4, 3, 0))]
    weights = np.array(
        ((0.5, 0.5, 0), (0, 0.5, 0.5), (0.5, 0, 0.5), (1 / 3, 1 / 3, 1 / 3))
    )
    probes = np.rint(np.einsum("pijc,kj->pikc", triangles, weights)).astype(int)
    radii = samples.radii(bases)
    scale = radii / np.linalg.norm(bases, axis=2)
    triangle_scale = scale[:, ((4, 0, 1), (4, 1, 2), (4, 2, 3), (4, 3, 0))]
    approximate = np.linalg.norm(probes, axis=3) / np.einsum(
        "pij,kj->pik", 1 / triangle_scale, weights
    )
    exact = samples.radii(probes)
    return np.max(np.abs(exact - approximate), axis=(1, 2))


def split_cells(cells):
    children = []
    for axis, sign, u, v, size, depth in cells:
        half = size // 2
        children.extend(
            (axis, sign, u + a * half, v + b * half, half, depth + 1)
            for a, b in ((0, 0), (1, 0), (1, 1), (0, 1))
        )
    return children


def conforming_triangles(points):
    starts = points[:, :4]
    ends = np.roll(starts, -1, axis=1)
    axes = np.argmax(starts != ends, axis=2)
    fixed_columns = np.array(((1, 2), (0, 2), (0, 1)))[axes]
    fixed = np.take_along_axis(starts, fixed_columns, axis=2)
    start_coordinates = np.take_along_axis(starts, axes[:, :, None], axis=2)[:, :, 0]
    end_coordinates = np.take_along_axis(ends, axes[:, :, None], axis=2)[:, :, 0]
    lines = {}
    edge_data = list(
        zip(
            axes.tolist(),
            fixed.tolist(),
            start_coordinates.tolist(),
            end_coordinates.tolist(),
            strict=True,
        )
    )
    for axis_row, fixed_row, start_row, end_row in edge_data:
        for axis, pair, a, b in zip(
            axis_row, fixed_row, start_row, end_row, strict=True
        ):
            lines.setdefault((axis, tuple(pair)), set()).update((a, b))
    lines = {key: sorted(values) for key, values in lines.items()}
    vertices, indices, faces = [], {}, []

    def vertex(key):
        key = tuple(key)
        if key not in indices:
            indices[key] = len(vertices)
            vertices.append(key)
        return indices[key]

    for patch, data in zip(points.tolist(), edge_data, strict=True):
        centre = vertex(patch[4])
        boundary = []
        for index, (axis, pair, a, b) in enumerate(zip(*data, strict=True)):
            line = lines[axis, tuple(pair)]
            values = line[bisect_left(line, min(a, b)) : bisect_right(line, max(a, b))]
            if b < a:
                values = values[::-1]
            point = tuple(patch[index])
            for coordinate in values[:-1]:
                boundary.append(
                    vertex(point[:axis] + (coordinate,) + point[axis + 1 :])
                )
        faces.extend(
            (centre, a, b)
            for a, b in zip(boundary, boundary[1:] + boundary[:1], strict=True)
        )
    return np.asarray(vertices, dtype=int), np.asarray(faces, dtype=np.int32)


def adaptive_mesh(field, tolerance, max_depth=7, max_patches=4096):
    if not np.isfinite(tolerance) or tolerance <= 0:
        raise ValueError("positive world-space radial sample tolerance required")
    if not isinstance(max_depth, int) or not 1 <= max_depth <= 9:
        raise ValueError("bounded adaptive subdivision depth required")
    if not isinstance(max_patches, int) or not 6 <= max_patches <= 16384:
        raise ValueError("bounded patch budget required")
    extent = 1 << max_depth
    cells = [
        (axis, sign, -extent, -extent, 2 * extent, 0)
        for axis in range(3)
        for sign in (-1, 1)
    ]
    samples = SurfaceSamples(field)
    leaves, residual = [], 0.0
    while cells:
        points = patch_points(cells, extent)
        errors = patch_errors(points, samples)
        refined = []
        capacity = max_patches - len(leaves) - len(cells)
        for cell, error in zip(cells, errors, strict=True):
            if error > tolerance and cell[-1] < max_depth and capacity >= 3:
                refined.append(cell)
                capacity -= 3
            else:
                leaves.append(cell)
                residual = max(residual, float(error))
        cells = split_cells(refined)
    parameters, faces = conforming_triangles(patch_points(leaves, extent))
    radius = samples.radii(parameters * 6)
    vertices = unit(parameters) * radius[:, None] + field.origin
    return mesh_product(vertices, faces, field), {
        "method": "adaptive",
        "patches": len(leaves),
        "directions_solved": len(samples.values),
        "field_point_evaluations": samples.evaluations,
        "volume_bytes": 0,
        "maximum_patch_probe_error_m": residual,
        "requested_patch_probe_tolerance_m": tolerance,
        "patch_probe_tolerance_met": residual <= tolerance,
        "depth_limit": max_depth,
        "patch_budget": max_patches,
    }
