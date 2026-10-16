"""Independent Float64 plane/Gram solar visibility through open blade triangles."""

import numpy as np


def direct_visibility(vertices, faces, origins, direction, lengths):
    triangles = vertices[faces]
    projection = (
        triangles[..., :2] - triangles[..., 2, None] * direction[:2] / direction[2]
    )
    lower, upper = projection.min(1), projection.max(1)
    result = np.ones(len(origins), bool)
    tested = 0
    for index, (origin, length) in enumerate(zip(origins, lengths, strict=True)):
        projected = origin[:2] - origin[2] * direction[:2] / direction[2]
        possible = np.all(lower <= projected, axis=1) & np.all(
            upper >= projected, axis=1
        )
        t = triangles[possible]
        tested += len(t)
        if not len(t):
            continue
        a, b = t[:, 1] - t[:, 0], t[:, 2] - t[:, 0]
        n = np.cross(a, b)
        denominator = n @ direction
        distance = np.divide(
            np.einsum("ij,ij->i", t[:, 0] - origin, n),
            denominator,
            out=np.full(len(t), -1.0),
            where=denominator != 0,
        )
        hit = origin + distance[:, None] * direction
        relative = hit - t[:, 0]
        aa, ab, bb = (np.einsum("ij,ij->i", x, y) for x, y in ((a, a), (a, b), (b, b)))
        determinant = aa * bb - ab * ab
        ra, rb = np.einsum("ij,ij->i", relative, a), np.einsum("ij,ij->i", relative, b)
        u = np.divide(
            bb * ra - ab * rb,
            determinant,
            out=np.full(len(t), -1.0),
            where=determinant > 0,
        )
        v = np.divide(
            aa * rb - ab * ra,
            determinant,
            out=np.full(len(t), -1.0),
            where=determinant > 0,
        )
        hit = (
            (distance > 0)
            & (distance <= length)
            & (u >= -1e-12)
            & (v >= -1e-12)
            & (u + v <= 1 + 1e-12)
        )
        result[index] = not hit.any()
    return result, {
        "reference_rays": len(origins),
        "reference_box_tests": len(origins) * len(faces),
        "reference_triangle_tests": tested,
    }
