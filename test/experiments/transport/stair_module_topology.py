"""Actual Double/Float32 XY height-field topology before fitting or admission."""

import numpy as np
import shapely


def check_surface(xy, faces):
    used = np.unique(faces)
    for dtype in (np.float64, np.float32):
        actual = xy.astype(dtype).astype(float)
        if len(np.unique(actual[used], axis=0)) != len(used):
            raise ValueError(
                "actual surface vertices merge after coordinate conversion"
            )
        p = actual[faces]
        a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
        if np.any(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] <= 0):
            raise ValueError(
                "actual surface facet collapses or inverts after coordinate conversion"
            )
        if not shapely.coverage_is_valid(shapely.polygons(p)):
            raise ValueError("actual surface has overlaps or unmatched edges")
