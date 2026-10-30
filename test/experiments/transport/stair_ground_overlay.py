"""Complete piecewise-affine contact partitions under existing metric tolerances."""

import numpy as np
import shapely


def contact_parts(xyz, owners, source, polygons, gradients, radius, height_tolerance):
    if len(owners) > 64:
        raise ValueError("piecewise ground parent budget exhausted")
    normal = np.cross(xyz[1] - xyz[0], xyz[2] - xyz[0])
    if normal[2] <= 0:
        raise ValueError("positive piecewise ground contact facet required")
    gradient = -normal[:2] / normal[2]
    shape = shapely.Polygon(xyz[:, :2])
    parts, maximum, vertices = [], 0.0, 0
    for owner in owners:
        part = shape.intersection(polygons[owner])
        if part.area <= 0:
            continue
        xy = shapely.get_coordinates(part)
        vertices += len(xy)
        if vertices > 1024:
            raise ValueError("piecewise ground intersection vertex budget exhausted")
        expected = source[owner, 0, 2] + (xy - source[owner, 0, :2]) @ gradients[owner]
        actual = xyz[0, 2] + (xy - xyz[0, :2]) @ gradient
        error = float(abs(actual - expected).max())
        if error > height_tolerance:
            raise ValueError(
                "piecewise ground contact exceeds its original height bound"
            )
        maximum = max(maximum, error)
        parts.append((int(owner), part))
    if not parts or not shapely.union_all([p for _, p in parts]).buffer(
        radius, quad_segs=32
    ).covers(shape):
        raise ValueError("piecewise ground contact lacks complete original coverage")
    return parts, maximum, vertices
