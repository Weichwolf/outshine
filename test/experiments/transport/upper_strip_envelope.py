"""Upper hull of a convex ribbon span; a finite-mesh experiment, not runtime LOD."""

import numpy as np


def cross(a, b):
    return a[..., 0] * b[..., 1] - a[..., 1] * b[..., 0]


def upper_quad(first, second):
    """Keep four outer corners, bounding displacement from two ordered source rings.

    Each ring projects onto its outer segment with residual at most delta. The
    corresponding ideal strip lies inside the tetrahedron of its four corners.
    Its upper/lower roofs differ most at the crossing of their XY diagonals.
    Thus delta + roof gap bounds symmetric finite-mesh distance. This floating
    point probe is not a directed-rounding or continuous-curve certificate.
    """
    rings = np.asarray([first, second], dtype=np.float64)
    if rings.shape != (2, 4, 3) or not np.isfinite(rings).all():
        raise ValueError("two finite ordered four-point rings required")
    outer = rings[[0, 0, 1, 1], [0, 3, 3, 0]]
    xy = outer[:, [0, 2]]
    edges = np.roll(xy, -1, axis=0) - xy
    turns = cross(edges, np.roll(edges, -1, axis=0))
    if not (np.all(turns > 0) or np.all(turns < 0)):
        return None
    direction = rings[:, 3] - rings[:, 0]
    length2 = np.sum(direction * direction, axis=1)
    if np.any(length2 <= 0):
        return None
    fraction = np.sum((rings - rings[:, :1]) * direction[:, None], axis=2)
    fraction = np.clip(fraction / length2[:, None], 0, 1)
    if np.any(np.diff(fraction, axis=1) <= 0):
        return None
    ideal = rings[:, :1] + fraction[..., None] * direction[:, None]
    residual = float(np.linalg.norm(rings - ideal, axis=2).max())
    ac, bd, ab = xy[2] - xy[0], xy[3] - xy[1], xy[1] - xy[0]
    denominator = cross(ac, bd)
    if denominator == 0:
        return None
    t, u = cross(ab, bd) / denominator, cross(ab, ac) / denominator
    if not (0 < t < 1 and 0 < u < 1):
        return None
    heights = (
        outer[0, 1] + t * (outer[2, 1] - outer[0, 1]),
        outer[1, 1] + u * (outer[3, 1] - outer[1, 1]),
    )
    triangles = np.array(
        [[0, 3, 2], [0, 2, 1]] if heights[0] >= heights[1] else [[0, 3, 1], [1, 3, 2]],
        dtype=np.uint32,
    )
    if turns[0] < 0:
        triangles = triangles[:, ::-1].copy()
    return {
        "positions": outer,
        "triangles": triangles,
        "roof_gap_m": float(abs(heights[0] - heights[1])),
        "ring_residual_m": residual,
        "position_bound_m": float(abs(heights[0] - heights[1]) + residual),
    }


def lower_quad(first, second):
    """Reflected counterpart for a shell underside; outer side/cap geometry stays separate."""
    reflected = np.asarray([first, second], dtype=np.float64).copy()
    reflected[..., 1] *= -1
    result = upper_quad(*reflected)
    if result is not None:
        result["positions"][:, 1] *= -1
        result["triangles"] = result["triangles"][:, ::-1].copy()
    return result


def stitch_quad(candidate, first, second, *, start=False, end=False):
    """Split outer roof edges at exact source contacts at each end of a coarse run."""
    positions = candidate["positions"].tolist()
    triangles = candidate["triangles"].tolist()
    for keep, edge, ring in [(start, (0, 1), first), (end, (3, 2), second)]:
        if not keep:
            continue
        next_triangles = []
        for triangle in triangles:
            for at in range(3):
                a, b, opposite = (
                    triangle[at],
                    triangle[(at + 1) % 3],
                    triangle[(at + 2) % 3],
                )
                if {a, b} != set(edge):
                    continue
                inner = np.asarray(ring)[1:3]
                if (a, b) != edge:
                    inner = inner[::-1]
                begin = len(positions)
                positions.extend(inner.tolist())
                chain = [a, begin, begin + 1, b]
                next_triangles.extend(
                    [[chain[i], chain[i + 1], opposite] for i in range(3)]
                )
                break
            else:
                next_triangles.append(triangle)
        triangles = next_triangles
    return {
        **candidate,
        "positions": np.asarray(positions),
        "triangles": np.asarray(triangles, dtype=np.uint32),
        "position_bound_m": candidate["position_bound_m"]
        + candidate["ring_residual_m"],
    }
