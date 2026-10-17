"""Actual immutable owner-cap ground, depth and D/Q whole-facet checks."""

import numpy as np


def cap_properties(asset, indices, limit):
    data = asset.data
    top = data["body_vertices"][data["top_faces"][indices]]
    bottom = data["body_vertices"][data["bottom_faces"][indices]]
    checks = []
    for dtype in (np.float64, np.float32):
        xyz = top.astype(dtype).astype(float)
        normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        valid = bool(np.isfinite(normal).all() and (normal[:, 2] > 0).all())
        maximum = (
            float(np.max(np.linalg.norm(normal[:, :2] / normal[:, 2, None], axis=1)))
            if valid
            else None
        )
        checks.append(
            {
                "dtype": str(np.dtype(dtype)),
                "valid_projected_faces": valid,
                "maximum_whole_facet_gradient": maximum,
                "isotropic_bound_met": bool(valid and maximum <= limit),
            }
        )
    return (
        {
            "old_ground_faces": bool(data["ground"][indices].all()),
            "old_depth_exactly_0_3m": bool(
                np.array_equal(bottom[:, :, 2], top[:, :, 2] - 0.3)
            ),
            "intersecting_whole_facet_height_range_m": [
                float(top[:, :, 2].min()),
                float(top[:, :, 2].max()),
            ],
            "old_cap_gradient_checks": checks,
        },
        top,
        bottom,
    )
