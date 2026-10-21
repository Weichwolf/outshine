"""Existing perspective rasterizer at an explicit regular ray grid; opaque only."""

import numpy as np
from current_visibility import raster


def samples(source, eye, basis, start, step, shape):
    width, height = map(int, shape)
    step = np.asarray(step, float)
    if (
        width <= 0
        or height <= 0
        or width * height > 4194304
        or np.linalg.det(basis) * step.prod() <= 0
    ):
        raise ValueError("bounded regular front-wound opaque ray grid required")
    points = (source["positions"].astype(float) - eye) @ basis.T
    if not np.isfinite(points).all() or points[:, 2].min() <= 0.2:
        raise ValueError("raster sample grid crosses source near plane")
    projected = np.column_stack(
        ((points[:, :2] / points[:, 2, None] - start) / step, 1 / points[:, 2])
    )
    work = {}
    inverse, faces = raster(
        projected,
        points[:, 2],
        source["indices"],
        width,
        height,
        with_faces=True,
        counters=work,
    )
    depth = np.divide(1, inverse, out=np.full_like(inverse, np.inf), where=inverse > 0)
    work["projected_vertices"] = len(points)
    work["raster_output_bytes"] = depth.nbytes + faces.nbytes
    return depth.ravel(), faces.ravel(), work
