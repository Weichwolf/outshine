"""Conservative quantized normal cones and unbounded six-face visibility masks.

Only geometric, static, one-sided facing is considered. Unknown product state
keeps all indices. Native eligibility, query arithmetic and shader contracts
still need their own agreement and target-device validation.
"""

import struct
from fractions import Fraction

import numpy as np
from facing_intervals import (
    add,
    divide_positive,
    dot,
    facing,
    geometric_normals,
    length,
    mul,
    point,
    sub,
)


def exact_facing(triangles, camera):
    """Adaptive dyadic determinant oracle: zero/tangent stays potentially visible."""
    triangles = np.asarray(triangles, dtype=np.float64)
    camera = np.asarray(camera, dtype=np.float64)
    if (
        not np.isfinite(triangles).all()
        or camera.shape != (3,)
        or not np.isfinite(camera).all()
    ):
        raise ValueError("finite represented geometry and camera required")
    lower, upper = facing(triangles, camera)
    signs = np.where(lower > 0, 1, np.where(upper < 0, -1, 0)).astype(np.int8)
    for index in np.flatnonzero(~((lower > 0) | (upper < 0))):
        p = [[Fraction(float(x)) for x in row] for row in triangles[index]]
        c = [Fraction(float(x)) for x in camera]
        a, b, view = (
            [p[1][j] - p[0][j] for j in range(3)],
            [p[2][j] - p[0][j] for j in range(3)],
            [c[j] - p[0][j] for j in range(3)],
        )
        value = sum(
            (a[(j + 1) % 3] * b[(j + 2) % 3] - a[(j + 2) % 3] * b[(j + 1) % 3])
            * view[j]
            for j in range(3)
        )
        signs[index] = (value > 0) - (value < 0)
    return signs


def eligible(static, one_sided, trusted_winding, model):
    if (
        static is not True
        or one_sided is not True
        or trusted_winding is not True
        or model is None
    ):
        return False
    model = np.asarray(model, dtype=np.float64)
    if (
        model.shape != (4, 4)
        or not np.isfinite(model).all()
        or not np.array_equal(model[3], [0, 0, 0, 1])
    ):
        return False
    linear = model[:3, :3]
    try:
        return bool(np.linalg.det(linear) > 0 and np.linalg.cond(linear) < 1e6)
    except np.linalg.LinAlgError:
        return False


def bake(triangles, sphere):
    triangles = np.asarray(triangles, dtype=np.float64)
    sphere = np.asarray(sphere, dtype=np.float32).astype(np.float64)
    if (
        triangles.ndim != 3
        or triangles.shape[1:] != (3, 3)
        or not 1 <= len(triangles) <= 128
    ):
        raise ValueError("one bounded original triangle cluster required")
    if (
        sphere.shape != (4,)
        or not np.isfinite(sphere).all()
        or sphere[3] < 0
        or not np.isfinite(triangles).all()
    ):
        raise ValueError("finite original geometry/sphere required")
    all_bits = (1 << len(triangles)) - 1
    result = {
        "payload": bytes(8),
        "masks": [all_bits] * 6,
        "sphere": sphere,
        "triangles": len(triangles),
        "reason": "sphere-unproved",
    }
    _, radii = length(sub(point(triangles), point(sphere[:3])))
    if not np.isfinite(radii).all() or (radii > sphere[3]).any():
        return result
    normal = geometric_normals(triangles)
    normal_length = length(normal)
    known = (normal_length[0] > 0) & np.isfinite(normal_length[1])
    offset = dot(normal, sub(point(sphere[:3]), point(triangles[:, 0])))
    lower_radius = np.nextafter(sphere[3] / np.nextafter(np.sqrt(3.0), np.inf), -np.inf)
    masks = []
    for axis in range(3):
        for sign in (-1, 1):
            support = normal[1][:, axis] if sign > 0 else -normal[0][:, axis]
            for other in range(3):
                if other != axis:
                    support = np.nextafter(
                        support
                        + np.maximum(
                            np.abs(normal[0][:, other]), np.abs(normal[1][:, other])
                        ),
                        np.inf,
                    )
            upper = add(offset, mul(point(max(0, lower_radius)), point(support)))[1]
            visible = (
                ~known
                | ~np.isfinite(upper)
                | ~np.isfinite(support)
                | (support > 0)
                | (upper >= 0)
            )
            masks.append(sum(1 << int(i) for i in np.flatnonzero(visible)))
    result.update(masks=masks, reason="wide-or-degenerate")
    if not known.all():
        return result
    middle = (normal[0] + normal[1]) / 2
    unit = middle / np.linalg.norm(middle, axis=1)[:, None]
    axis = unit.sum(axis=0)
    if np.linalg.norm(axis) < 1e-12:
        return result
    axis = np.rint(127 * axis / np.linalg.norm(axis)).astype(np.int64)
    projected = dot(normal, point(axis))
    if (projected[0] <= 0).any() or not np.isfinite(projected[1]).all():
        return result
    least = divide_positive(projected, normal_length)[0].min()
    square = np.nextafter(
        127**2 + int(axis @ axis) - np.nextafter(254 * least, -np.inf), np.inf
    )
    cutoff = int(np.ceil(np.nextafter(np.sqrt(max(0, square)), np.inf)))
    if cutoff >= np.linalg.norm(axis) or cutoff > 255:
        return result
    apex = divide_positive(offset, projected)[1].max()
    encoded = np.float32(apex)
    if encoded < apex:
        encoded = np.nextafter(encoded, np.float32(np.inf))
    if not np.isfinite(encoded):
        return result
    result.update(
        payload=struct.pack("<3bBf", *map(int, axis), cutoff, encoded), reason="cone"
    )
    return result


def query(
    record,
    camera,
    *,
    static=None,
    one_sided=None,
    trusted_winding=None,
    model=None,
    counters=None,
):
    """camera is explicitly supplied in the cluster's original object space.

    Production must account for obtaining this camera/transform consistently;
    no native inverse transform, layout or binding is implied by the reference.
    """
    count = record["triangles"]
    camera = np.asarray(camera, dtype=np.float64)
    all_bits = (1 << count) - 1
    if (
        camera.shape != (3,)
        or not np.isfinite(camera).all()
        or not eligible(static, one_sided, trusted_winding, model)
    ):
        return True, all_bits
    if record["reason"] == "sphere-unproved":
        return True, all_bits
    delta = sub(point(camera), point(record["sphere"][:3]))
    lower, upper_radius = length(delta)
    if not np.isfinite(upper_radius) or lower <= record["sphere"][3]:
        return True, all_bits
    if counters is not None:
        counters.update(
            cone_tests=1,
            cone_payload_read_bytes=8,
            cone_dot_products=2,
            cone_vector_lengths=1,
            mask_triangle_bit_tests=count,
        )
    ax, ay, az, cutoff, apex = struct.unpack("<3bBf", record["payload"])
    axis = np.array([ax, ay, az], dtype=float)
    vector = add(delta, mul(point(axis), point(apex)))
    upper = add(dot(point(axis), vector), mul(point(cutoff), length(vector)))[1]
    keep_cone = bool(not np.isfinite(upper) or upper >= 0)
    least_abs = np.where(delta[0] > 0, delta[0], np.where(delta[1] < 0, -delta[1], 0))
    greatest_abs = np.maximum(np.abs(delta[0]), np.abs(delta[1]))
    masks = 0
    for dimension in range(3):
        if greatest_abs[dimension] < least_abs.max():
            continue
        for sign in (-1, 1):
            if (sign < 0 and delta[0][dimension] <= 0) or (
                sign > 0 and delta[1][dimension] >= 0
            ):
                masks |= record["masks"][2 * dimension + (sign > 0)]
                if counters is not None:
                    counters.update(
                        mask_face_words=(count + 63) // 64,
                        mask_payload_read_bytes=8 * ((count + 63) // 64),
                    )
    return keep_cone, masks
