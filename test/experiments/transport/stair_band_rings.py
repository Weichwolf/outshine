"""Remove redundant band-ring points while preserving shared indexed vertices."""

from collections import deque

import numpy as np


def _chain_error(points, kept):
    maximum = 0.0
    for a, b in zip(kept, np.roll(kept, -1), strict=True):
        indices = (
            np.arange(a, b + 1)
            if a < b
            else np.r_[np.arange(a, len(points)), np.arange(b + 1)]
        )
        edge = points[b] - points[a]
        norm = edge @ edge
        if norm == 0:
            raise ValueError("simplified ring contains a zero edge")
        ratio = np.clip((points[indices] - points[a]) @ edge / norm, 0, 1)
        distance = np.linalg.norm(
            points[indices] - (points[a] + ratio[:, None] * edge), axis=1
        )
        maximum = max(maximum, float(distance.max()))
    return maximum


def clean_ring(coords, protected, *, tolerance_m=1e-10):
    points = np.array(coords[:-1])
    count = len(points)
    if not 3 <= count <= 24576 or not 0 <= tolerance_m <= 1e-10:
        raise ValueError("bounded ring and numeric collinearity allowance required")
    previous = np.arange(count) - 1
    previous[0] = count - 1
    following = (np.arange(count) + 1) % count
    alive = np.ones(count, bool)
    remaining, visits = count, 0
    queue = deque(range(count))
    while queue and remaining > 3:
        i = queue.popleft()
        visits += 1
        if visits > 8 * count:
            raise ValueError("band ring simplification work budget")
        if not alive[i] or tuple(points[i]) in protected:
            continue
        a, b = previous[i], following[i]
        edge = points[b] - points[a]
        norm = edge @ edge
        if norm == 0:
            continue
        ratio = (points[i] - points[a]) @ edge / norm
        if not 0 <= ratio <= 1:
            continue
        distance = np.linalg.norm(points[i] - (points[a] + ratio * edge))
        if distance > tolerance_m:
            continue
        alive[i] = False
        remaining -= 1
        following[a], previous[b] = b, a
        queue.extend((a, b))
    error = _chain_error(points, np.flatnonzero(alive))
    if error > tolerance_m:
        raise ValueError("accumulated band boundary deviation exceeds its allowance")
    return points[alive], {
        "ring_visits": visits,
        "removed_ring_vertices": count - remaining,
        "maximum_chain_vertex_deviation_m": error,
    }
