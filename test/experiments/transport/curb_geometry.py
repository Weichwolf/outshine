"""Whole finite curb caps, unchanged traffic envelope and actual Float32 shell checks."""

import numpy as np
from bridge_support_contacts import replay_contact
from curb_plan import HEIGHT_M, WIDTH_M
from retained_surface import surface_domain
from surface_solid_checks import audit_closed


def audit(product, shape, ground, traffic, world, excluded, *, outside_limit=None):
    body, low, high = product.body, product.lower_contact, product.upper_contact
    n, f = len(low.vertices), len(low.triangles)
    if not (
        np.array_equal(low.vertices[:, :2], high.vertices[:, :2])
        and np.array_equal(low.triangles, high.triangles)
        and np.array_equal(body.vertices[:n], high.vertices)
        and np.array_equal(body.vertices[n:], low.vertices)
        and np.array_equal(body.triangles[:f], high.triangles)
        and np.array_equal(body.triangles[f : 2 * f], low.triangles[:, ::-1] + n)
    ):
        raise ValueError("curb actual shell/cap ownership mismatch")
    delta = high.vertices[:, 2] - low.vertices[:, 2]
    error = float(np.max(abs(delta - HEIGHT_M)))
    domain = surface_domain(low)
    if outside_limit is None:
        outside_limit = traffic.buffer(WIDTH_M, quad_segs=4)
    if error > 1e-8 or domain.symmetric_difference(shape).area > 1e-8:
        raise ValueError("curb actual height or complete footprint mismatch")
    if (
        domain.intersection(traffic).area > 1e-8
        or domain.intersection(excluded).area > 1e-8
        or domain.difference(world).area > 1e-8
        or domain.difference(outside_limit).area > 1e-8
    ):
        raise ValueError(
            "curb actual body violates traffic/width/port/world reservation"
        )
    contact_error = replay_contact(low, ground)
    volume = audit_closed(body)
    if abs(volume - domain.area * HEIGHT_M) > 1e-7:
        raise ValueError("curb closed volume differs from its finite section")
    p = body.vertices[body.triangles]
    q = body.vertices.astype(np.float32).astype(float)[body.triangles]
    normal = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    cast = np.cross(q[:, 1] - q[:, 0], q[:, 2] - q[:, 0])
    if np.any(np.linalg.norm(cast, axis=1) == 0) or np.any(
        np.einsum("ij,ij->i", normal, cast) <= 0
    ):
        raise ValueError("curb Float32 shell degenerates or inverts")
    cap = normal[:f]
    slope = float(np.max(np.linalg.norm(cap[:, :2], axis=1) / cap[:, 2]))
    if slope > 0.05 + 1e-7:
        raise ValueError("curb top slope exceeds procedural five percent")
    return {
        "height_error_m": error,
        "whole_ground_contact_error_m": contact_error,
        "volume_m3": volume,
        "volume_error_m3": abs(volume - domain.area * HEIGHT_M),
        "maximum_top_grade": slope,
        "area_m2": domain.area,
        "float32_faces": len(body.triangles),
    }
