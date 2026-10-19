"""Independent Python control of BatchVisibility.h at 0eb2e739d (#819).

This models the existing sphere/affine/four-side guards, not native execution.
Unknown bounds keep drawing. Positions are not scanned by these functions.
"""

import math

import numpy as np

F32 = np.finfo(np.float32)


def normal_or_zero(values):
    values = np.asarray(values, dtype=float)
    return bool(
        (np.isfinite(values) & ((values == 0) | (abs(values) >= F32.tiny))).all()
    )


def packed(values):
    with np.errstate(over="ignore", invalid="ignore"):
        return np.asarray(values, dtype=np.float32)


def pack_sphere(centre, radius):
    centre = np.asarray(centre, dtype=float)
    result = packed(centre)
    if not normal_or_zero(result):
        return None
    delta = result.astype(float) - centre
    with np.errstate(over="ignore", invalid="ignore"):
        r = np.nextafter(packed(radius + math.hypot(*delta)), np.float32(np.inf))
    if not normal_or_zero(r) or r < 0:
        return None
    return np.array([*result, r], dtype=np.float32)


def cluster_sphere(spheres, ranges, index_count):
    spheres = np.asarray(spheres, dtype=float)
    if not len(spheres) or index_count <= 0 or index_count % 3:
        return None
    lower, upper, covered = np.full(3, np.inf), np.full(3, -np.inf), 0
    for (first, count), sphere in zip(ranges, spheres, strict=True):
        if (
            first > covered
            or first < 0
            or first > index_count
            or first % 3
            or count <= 0
            or count % 3
            or count > index_count - first
            or sphere[3] < 0
            or not normal_or_zero(sphere)
        ):
            return None
        covered = max(covered, first + count)
        lower = np.minimum(lower, sphere[:3] - sphere[3])
        upper = np.maximum(upper, sphere[:3] + sphere[3])
    if covered != index_count:
        return None
    centre = (lower + upper) / 2
    radius = max(math.hypot(*(s[:3] - centre)) + s[3] for s in spheres)
    return pack_sphere(centre, radius)


def transformed_sphere(local, row):
    if local is None:
        return None
    row = packed(row).astype(float)
    if row[3, 3] != 1 or (row[3, :3] != 0).any() or not normal_or_zero(row[:3]):
        return None
    guard = 8 * F32.eps
    matrix = row[:3]
    centre, error = matrix[:, 3].copy(), abs(matrix[:, 3]) + 1
    for column in range(3):
        centre += matrix[:, column] * local[column]
        error += abs(matrix[:, column]) * (abs(float(local[column])) + float(local[3]))
    if (error > F32.max).any():
        return None
    gram = np.zeros((3, 3))
    for a in range(3):
        for b in range(3):
            for axis in range(3):
                gram[a, b] += matrix[axis, a] * matrix[axis, b]
    scale = math.sqrt(max(sum(abs(v) for v in r) for r in gram))
    radius = float(local[3]) * scale * (1 + guard) + math.hypot(*(error * guard))
    return pack_sphere(centre, radius)


def world_sphere(local, rows):
    if local is None or not len(rows):
        return None
    spheres = [transformed_sphere(local, row) for row in rows]
    if any(s is None for s in spheres):
        return None
    if len(spheres) == 1:
        return spheres[0]
    spheres = np.asarray(spheres, dtype=float)
    lower = (spheres[:, :3] - spheres[:, 3:]).min(0)
    upper = (spheres[:, :3] + spheres[:, 3:]).max(0)
    return pack_sphere((lower + upper) / 2, math.hypot(*((upper - lower) / 2)))


def outside(sphere, light, shift):
    light, shift = np.asarray(light, dtype=float), packed(shift).astype(float)
    if sphere is None or sphere[3] < 0 or not normal_or_zero(sphere):
        return False
    if not normal_or_zero(shift) or not normal_or_zero(light[[3, 0, 1]]):
        return False
    rejected = False
    for side in range(4):
        a, b = light[3], light[side // 2]
        equation = a + (1 if side % 2 == 0 else -1) * b
        error = abs(a) + abs(b)
        maximum = equation[3] + float(sphere[3]) * math.hypot(*equation[:3])
        uncertainty = error[3] + 1
        for axis in range(3):
            maximum += equation[axis] * (float(sphere[axis]) + shift[axis])
            uncertainty += error[axis] * (
                abs(float(sphere[axis])) + float(sphere[3]) + abs(shift[axis])
            )
        if uncertainty > F32.max:
            return False
        rejected |= maximum < -32 * F32.eps * uncertainty
    return bool(rejected)
