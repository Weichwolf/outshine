"""Independent continuous polygon union and Float64 first-contact checks."""

import numpy as np
import shapely


def clip_ground(triangle):
    polygon = []
    for i in range(3):
        a, b = triangle[i - 1], triangle[i]
        if (a[2] >= 0) != (b[2] >= 0):
            polygon.append(a + (b - a) * (-a[2] / (b[2] - a[2])))
        if b[2] >= 0:
            polygon.append(b)
    return [
        np.array([polygon[0], polygon[i], polygon[i + 1]])
        for i in range(1, len(polygon) - 1)
    ]


def continuous_union(vertices, faces, lower, upper, direction):
    triangles = vertices[faces]
    projected = (
        triangles[..., :2]
        - triangles[..., 2, None] * np.asarray(direction[:2]) / direction[2]
    )
    possible = (
        (triangles[..., 2].max(1) >= 0)
        & np.all(projected.max(1) >= lower, axis=1)
        & np.all(projected.min(1) <= upper, axis=1)
    )
    triangles = triangles[possible]
    positive = np.all(triangles[..., 2] >= 0, axis=1)
    parts = [triangles[positive]]
    clipped = [p for t in triangles[~positive] for p in clip_ground(t)]
    if clipped:
        parts.append(np.array(clipped))
    triangles = np.concatenate(parts)
    projected = (
        triangles[..., :2]
        - triangles[..., 2, None] * np.asarray(direction[:2]) / direction[2]
    )
    polygons = shapely.polygons(projected)
    polygons = shapely.intersection(polygons, shapely.box(*lower, *upper))
    polygons = polygons[shapely.area(polygons) > 0]
    # Explicit metric precision handles almost-coincident shared blade edges.
    union = shapely.union_all(polygons, grid_size=1e-12)
    return union, {
        "union_area_m2": float(union.area),
        "summed_projected_area_m2": float(shapely.area(polygons).sum()),
        "candidate_triangles": int(possible.sum()),
        "clipped_polygon_count": len(polygons),
        "geos": shapely.geos_version_string,
        "union_precision_grid_m": 1e-12,
    }


def ray_heights(vertices, faces, points, direction):
    """Independent per-ray plane/Gram intersection; max positive height is first from sky."""
    triangles = vertices[faces]
    a, b = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    n = np.cross(a, b)
    aa, ab, bb = (np.einsum("ij,ij->i", x, y) for x, y in ((a, a), (a, b), (b, b)))
    determinant = aa * bb - ab * ab
    denominator = n @ direction
    possible = (determinant > 0) & (abs(denominator) > 1e-18)
    result = []
    for xy in points:
        origin = np.r_[xy, 0.0]
        distance = np.divide(
            np.einsum("ij,ij->i", triangles[:, 0] - origin, n),
            denominator,
            out=np.full(len(faces), -np.inf),
            where=possible,
        )
        hit = origin + np.where(possible, distance, 0)[:, None] * direction
        relative = hit - triangles[:, 0]
        ra, rb = np.einsum("ij,ij->i", relative, a), np.einsum("ij,ij->i", relative, b)
        u = np.divide(
            bb * ra - ab * rb, determinant, out=np.zeros(len(faces)), where=possible
        )
        v = np.divide(
            aa * rb - ab * ra, determinant, out=np.zeros(len(faces)), where=possible
        )
        inside = (
            possible
            & (distance >= 0)
            & (u >= -1e-12)
            & (v >= -1e-12)
            & (u + v <= 1 + 1e-12)
        )
        result.append(float(np.max(distance[inside], initial=-1)) * direction[2])
    return np.asarray(result)


def verify(vertices, faces, lower, upper, direction, product):
    size = len(product["coverage"])
    union, report = continuous_union(vertices, faces, lower, upper, direction)
    axis = (np.arange(size) + 0.5) / size
    x, y = np.meshgrid(
        lower[0] + axis * (upper[0] - lower[0]), lower[1] + axis * (upper[1] - lower[1])
    )
    membership = shapely.intersects_xy(union, x, y)
    np.testing.assert_array_equal(membership, product["coverage"].astype(bool))
    fraction = report["union_area_m2"] / np.prod(np.asarray(upper) - lower)
    error = abs(float(product["coverage"].mean()) - fraction)
    if error > 0.01:
        raise ValueError("fixed one-percentage-point sampled area criterion failed")
    indices = np.random.Generator(np.random.PCG64(462)).choice(
        size * size, 64, replace=False
    )
    points = np.c_[x.ravel()[indices], y.ravel()[indices]]
    heights = ray_heights(vertices, faces, points, direction)
    expected_hit = heights >= 0
    np.testing.assert_array_equal(
        expected_hit, product["coverage"].ravel()[indices].astype(bool)
    )
    np.testing.assert_allclose(
        heights[expected_hit],
        product["height"].ravel()[indices][expected_hit],
        atol=2e-11,
        rtol=0,
    )
    report.update(
        sampled_fraction=float(product["coverage"].mean()),
        absolute_area_fraction_error=error,
        independent_ray_count=64,
        membership_points=size * size,
    )
    return report
