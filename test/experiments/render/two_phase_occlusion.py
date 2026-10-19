"""Current-frame second-phase HZB control on a fixed finite ray lattice."""

import itertools
from dataclasses import dataclass

import numpy as np

CORNERS = np.array(list(itertools.product((0, 1), repeat=3)))


def project(bounds, view):
    eye, basis, focal, width, height = view
    corners = np.where(CORNERS, bounds[1], bounds[0])
    p = (corners - eye) @ basis.T
    x, y, z = p.T
    planes = np.array(
        [
            z - 0.1,
            z * width / (2 * focal) + x,
            z * width / (2 * focal) - x,
            z * height / (2 * focal) + y,
            z * height / (2 * focal) - y,
        ]
    )
    visible = not (planes.max(1) < 0).any()
    if (z <= 0.1).any():
        return visible, None
    xy = np.array([width / 2, height / 2]) + focal * p[:, :2] / z[:, None] * [1, -1]
    return visible, (np.array([xy.min(0), xy.max(0)]), float(z.min()))


@dataclass
class DepthHierarchy:
    levels: list
    view: tuple
    origin: np.ndarray
    supersample: int
    reduced_values: int


def pyramid(depth, view, box, supersample=2):
    depth = np.asarray(depth, float)
    if (
        depth.ndim != 2
        or not depth.size
        or depth.size > 4194304
        or np.isnan(depth).any()
        or (depth < 0.1).any()
    ):
        raise ValueError(
            "bounded positive camera-depth field or positive infinity required"
        )
    if type(supersample) is not int or not 1 <= supersample <= 4:
        raise ValueError("1..4 fixed samples per pixel axis required")
    expected = np.rint((box[1] - box[0]) * supersample).astype(int)
    if tuple(expected[::-1]) != depth.shape:
        raise ValueError("depth field must cover its declared same-resolution crop")
    levels = [depth.copy()]
    reduced = 0
    while max(levels[-1].shape) > 1:
        old = levels[-1]
        h, w = old.shape
        padded = np.full((h + h % 2, w + w % 2), np.inf)
        padded[:h, :w] = old
        new = padded.reshape((h + 1) // 2, 2, (w + 1) // 2, 2).max((1, 3))
        reduced += new.size * 4
        levels.append(new)
    return DepthHierarchy(levels, view, np.array(box[0], float), supersample, reduced)


def classify(bounds, history, *, eligible=None):
    """Old history may defer; only a current-pose history can finally discard.

    Geometry and materials are never inferred from depth samples. Each queried
    rectangle includes every lattice location in the projected group AABB.
    Missing depth, incomplete crop, near-plane crossings and ties keep the group.
    Floating-point guards are engineering allowances, not interval certification.
    """
    mask = np.zeros(len(bounds), bool)
    eligible = (
        np.ones(len(bounds), bool) if eligible is None else np.asarray(eligible, bool)
    )
    work = {"groups_tested": 0, "projected_rectangles": 0, "depth_taps": 0}
    if history is None:
        return mask, work
    for i in np.flatnonzero(eligible):
        work["groups_tested"] += 1
        _, result = project(bounds[i], history.view)
        if result is None:
            continue
        rectangle, nearest = result
        sample = (rectangle - history.origin) * history.supersample - 0.5
        # Include ambiguous boundary samples rather than assume them absent.
        low = np.ceil(sample[0] - 1e-7).astype(int)
        high = np.floor(sample[1] + 1e-7).astype(int)
        shape = np.array(history.levels[0].shape[::-1])
        if (low < 0).any() or (high >= shape).any() or (low > high).any():
            continue
        level = 0
        while (
            level + 1 < len(history.levels)
            and ((high >> level) - (low >> level) + 1 > 4).any()
        ):
            level += 1
        a, b = low >> level, high >> level
        values = history.levels[level][a[1] : b[1] + 1, a[0] : b[0] + 1]
        work["projected_rectangles"] += 1
        work["depth_taps"] += values.size
        farthest = values.max()
        guard = 1e-7 + 128 * np.finfo(float).eps * max(abs(nearest), 1)
        mask[i] = np.isfinite(farthest) and nearest > farthest + guard
    return mask, work
