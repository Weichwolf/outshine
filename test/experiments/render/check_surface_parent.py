"""Independent affine/convex witness audit, without trusting clustering decisions."""

import math

import numpy as np


def check_parent(positions, triangles, parent):
    positions = np.asarray(positions, dtype=np.float64)
    triangles = np.asarray(triangles)
    n = len(triangles)
    if (
        parent.mode.shape != (n,)
        or parent.corner_map.shape != (n, 3)
        or parent.covered_by.shape != (n,)
        or not np.isin(parent.mode, [0, 1, 2, 3]).all()
        or not np.isfinite(parent.error_m)
        or parent.error_m < 0
        or (parent.corner_map < 0).any()
        or (parent.corner_map >= len(positions)).any()
        or (parent.covered_by < 0).any()
        or (parent.covered_by >= len(parent.triangles)).any()
        or len(parent.original_triangle_ids) != len(parent.triangles)
    ):
        raise AssertionError("incomplete witness")
    candidate = parent.positions[parent.triangles]
    kept = parent.original_triangle_ids
    np.testing.assert_array_equal(parent.covered_by[kept], np.arange(len(kept)))
    np.testing.assert_array_equal(
        parent.positions, positions[parent.original_vertex_ids]
    )
    max_distance = 0.0
    for start in range(0, n, 8192):
        stop = min(n, start + 8192)
        modes = parent.mode[start:stop]
        original = positions[triangles[start:stop]]
        images = positions[parent.corner_map[start:stop]]
        targets = candidate[parent.covered_by[start:stop]]
        np.testing.assert_array_equal(targets[modes == 0], original[modes == 0])
        mapped = np.isin(modes, [1, 2])
        # Same oriented triangle, up to a cyclic permutation.
        matches = np.zeros(int(mapped.sum()), dtype=bool)
        for rotation in range(3):
            matches |= (
                np.roll(images[mapped], rotation, axis=1) == targets[mapped]
            ).all(axis=(1, 2))
        if not matches.all():
            raise AssertionError("nondegenerate image lost")
        for image, target in zip(images[modes == 3], targets[modes == 3], strict=True):
            unique = set(map(tuple, image))
            if len(unique) > 2 or not unique <= set(map(tuple, target)):
                raise AssertionError("collapsed point/segment has no convex witness")
        for a, b in zip(
            original[modes != 0].reshape(-1, 3),
            images[modes != 0].reshape(-1, 3),
            strict=True,
        ):
            max_distance = max(max_distance, math.dist(a, b))
    if max_distance > parent.error_m:
        raise AssertionError("corner displacement exceeds certified bound")
    return {
        "reference_triangles_checked": n,
        "parent_triangles_checked": len(kept),
        "corner_distance_lower_check_m": max_distance,
    }
