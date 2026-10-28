"""Whole actual solid and exact affine corner coverage checks, no source fitting."""

from fractions import Fraction

import numpy as np
import shapely
from surface_solid_checks import audit_closed


def rational_height(plane, origin, point):
    z, gx, gy = map(Fraction, plane)
    ox, oy = map(Fraction, origin)
    x, y = (Fraction(float(v)) for v in point)
    return z + gx * x + gy * y - gx * ox - gy * oy


def qualify_body(product, report):
    surface, body = product["surface"], product["body"]
    n, f = len(surface.vertices), len(surface.triangles)
    recipe = report["recipe"]
    if (
        body.vertices[:n].tobytes() != surface.vertices.tobytes()
        or body.triangles[:f].tobytes() != surface.triangles.tobytes()
        or not np.array_equal(body.vertices[n:, :2], surface.vertices[:, :2])
        or not np.array_equal(
            body.vertices[n:, 2], surface.vertices[:, 2] - recipe["depth_m"]
        )
        or not np.array_equal(body.triangles[f : 2 * f], surface.triangles[:, ::-1] + n)
    ):
        raise ValueError("Deck body does not preserve its complete indexed caps")
    triangles = body.vertices[body.triangles]
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    if (np.linalg.norm(normals, axis=1) == 0).any():
        raise ValueError("Zero area body face")
    volume = audit_closed(body)
    expected = product["domain"].area * recipe["depth_m"]
    if volume <= 0 or abs(volume - expected) > 1e-10 * expected:
        raise ValueError("Whole oriented deck volume disagrees with domain and depth")
    edges, counts = np.unique(
        np.sort(surface.triangles[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1),
        axis=0,
        return_counts=True,
    )
    expected_walls = {tuple(e) for e in edges[counts == 1]}
    actual_walls = {
        tuple(sorted(set(map(int, face % n)))) for face in body.triangles[2 * f :]
    }
    if actual_walls != expected_walls or len(body.triangles) != 2 * f + 2 * len(
        expected_walls
    ):
        raise ValueError("Deck includes missing or internal walls")
    rows = product["occupancies"]
    if not shapely.union_all([r.geometry for r in rows]).equals(product["domain"]):
        raise ValueError("Occupied domains disagree with actual deck domain")
    method = report["occupancy_work"]["method"]
    if len(rows) != (1 if method == "whole" else f):
        raise ValueError("Unexpected occupied-space partition")
    padding = Fraction(0)
    for i, face in enumerate(surface.triangles):
        row = rows[0 if method == "whole" else i]
        if not row.geometry.covers(shapely.Polygon(surface.vertices[face, :2])):
            raise ValueError("Whole projected cap is not covered")
        for index in face:
            lower, upper = body.vertices[int(index) + n], body.vertices[index]
            lo = rational_height(row.lower_plane_m, row.origin_xy_m, lower[:2])
            hi = rational_height(row.upper_plane_m, row.origin_xy_m, upper[:2])
            a = Fraction(float(lower[2])) - lo
            b = hi - Fraction(float(upper[2])) - Fraction(recipe["headroom_m"])
            if min(a, b) < 0:
                raise ValueError("Occupied space misses an actual affine cap")
            padding = max(padding, a, b)
    if padding > Fraction(1, 100000000):
        raise ValueError("Whole cap padding exceeds the retained envelope contract")
    return {
        "top_vertices_exact": n,
        "top_faces_exact": f,
        "closed_volume_m3": volume,
        "analytic_volume_m3": expected,
        "volume_error_m3": abs(volume - expected),
        "boundary_walls": len(expected_walls),
        "vertices": len(body.vertices),
        "faces": len(body.triangles),
        "covered_whole_top_bottom_cap_pairs": f,
        "occupied_volumes": len(rows),
        "maximum_exact_cap_padding_m": float(padding),
        "scope": "Indexed solid/cap and exact affine corner coverage implies whole triangle coverage; existing polygonal height query limitations remain, no general clearance/World/statics admission.",
    }
