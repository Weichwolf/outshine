"""Prepared Double backface-cone reference; no quantized/native admission."""

import numpy as np


def prepare_cone(points, normals):
    points, normals = np.asarray(points, float), np.asarray(normals, float)
    if (
        not len(points)
        or not np.isfinite(points).all()
        or not np.isfinite(normals).all()
        or ((normals * normals).sum(axis=1) < 0.5).any()
    ):
        return None
    axis = normals.sum(axis=0)
    length = np.linalg.norm(axis)
    if length < 1e-12:
        return None
    axis /= length
    dots = normals @ axis
    cutoff = float(dots.min())
    if cutoff <= 1e-6:
        return None
    centre = (points.min(axis=(0, 1)) + points.max(axis=(0, 1))) * 0.5
    offset = np.max(np.einsum("ij,ij->i", normals, centre - points[:, 0]) / dots)
    apex = centre - axis * offset
    return np.r_[axis, apex, max(0, 1 - cutoff * cutoff), np.max(np.abs(points))]


def cone_excludes(cone, eye, margin):
    if (
        cone is None
        or not np.isfinite(cone).all()
        or not np.isfinite(eye).all()
        or not np.isfinite(margin)
        or margin < 0
    ):
        return False
    direction = eye - cone[3:6]
    along = float(direction @ cone[:3])
    return bool(
        along < -margin
        and along * along
        > np.dot(direction, direction) * cone[6] + margin * (2 * abs(along) + margin)
    )
