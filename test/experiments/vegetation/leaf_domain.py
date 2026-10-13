"""Conservative segment containment in the analytic blade outline."""

import numpy as np


def width_derivative_bound(blade):
    p = blade.widest
    warp_bound = max(p / (1 - p), (1 - p) / p)
    return (
        blade.width
        / 2
        * (
            np.pi * (1 - blade.base_fill + blade.lobe_depth * blade.lobes) * warp_bound
            + blade.base_fill
        )
    )


def contains_segment(blade, first, last, depth=12):
    if type(depth) is not int or not 0 <= depth <= 12:
        raise ValueError("bounded integer contour subdivision depth required")
    first, last = np.asarray(first), np.asarray(last)
    if not np.isfinite([first, last]).all():
        return False
    u = np.array([first[1], last[1]]) / blade.length
    if np.any((u < 0) | (u > 1)):
        return False
    widths, _ = blade.width_profile(u)
    if np.any(np.abs([first[0], last[0]]) > widths):
        return False
    width_bound = width_derivative_bound(blade)
    delta = last - first
    lipschitz = width_bound * abs(delta[1]) / blade.length + abs(delta[0])
    pending = [(0.0, 1.0, 0)]
    while pending:
        low, high, level = pending.pop()
        mid = (low + high) / 2
        point = first + mid * delta
        width, _ = blade.width_profile(point[1] / blade.length)
        clearance = width - abs(point[0])
        if clearance >= lipschitz * (high - low) / 2:
            continue
        if clearance < 0 or level == depth:
            return False
        pending.extend(((low, mid, level + 1), (mid, high, level + 1)))
    return True
