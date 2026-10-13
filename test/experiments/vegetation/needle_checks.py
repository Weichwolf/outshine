"""Independent differential, manifold, nested-detail and physical-volume checks."""

import numpy as np
from needle_surface import Needle


def check_mesh(mesh):
    faces = mesh["faces"]
    p = mesh["vertices"][faces]
    cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    areas = np.linalg.norm(cross, axis=1) / 2
    assert np.isfinite(p).all() and areas.min() > 0
    n = mesh["normals"].reshape(-1, 3, 3)
    assert np.all(np.sum(cross[:, None] * n, axis=2) > 0)
    assert np.allclose(np.linalg.norm(n, axis=2), 1, atol=1e-12, rtol=0)
    edges = np.concatenate((faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]))
    sorted_edges = np.sort(edges, axis=1)
    unique, counts = np.unique(sorted_edges, axis=0, return_counts=True)
    assert np.all(counts == 2) and len(mesh["vertices"]) - len(unique) + len(faces) == 2
    _, inverse = np.unique(sorted_edges, axis=0, return_inverse=True)
    orientation = np.bincount(
        inverse, weights=np.where(edges[:, 0] < edges[:, 1], 1, -1)
    )
    assert np.all(orientation == 0)
    volume = float(
        np.sum(np.einsum("ij,ij->i", p[:, 0], np.cross(p[:, 1], p[:, 2]))) / 6
    )
    assert volume > 0
    return volume


def needle_checks():
    from axis_surface import profiled_axis_mesh
    from generator_specs import TubeShape
    from graph_sources import source_parts
    from profile_curve import Profile

    cylinder = TubeShape(0.005, 0, Profile(np.full(4, 0.0006)))
    reduced = source_parts(cylinder)["wood"]
    reference = profiled_axis_mesh(cylinder.length, 0, 1, cylinder.radius, 48, 12)
    assert abs(check_mesh(reduced) / check_mesh(reference) - 1) < 1e-12
    assert len(reduced["faces"]) == 72 and len(reference["faces"]) == 1176
    cases = []
    maximum = 0.0
    for shape in (
        Needle(),
        Needle(bend=-0.8, twist=2),
        Needle(section="rhombus", length=0.018, radius=0.0005),
        Needle(bend=0, twist=0),
    ):
        t = np.linspace(0.01, 0.98, 89)
        angles = np.full_like(t, 0.37)
        p, n, density = shape.evaluate(t, angles)
        e = 1e-6
        dt = (shape.evaluate(t + e, angles)[0] - shape.evaluate(t - e, angles)[0]) / (
            2 * e
        )
        da = (shape.evaluate(t, angles + e)[0] - shape.evaluate(t, angles - e)[0]) / (
            2 * e
        )
        cross = np.cross(dt, da)
        norm = cross / np.linalg.norm(cross, axis=1)[:, None]
        error = float(np.max(abs(norm - n)))
        maximum = max(maximum, error)
        assert error < 1e-8 and np.allclose(
            density, np.linalg.norm(cross, axis=1), rtol=1e-7, atol=1e-14
        )
        near, coarse = shape.mesh(), shape.mesh(16, 8)
        volumes = [check_mesh(m) for m in (near, coarse)]
        shared = []
        for p in coarse["vertices"]:
            rows = np.flatnonzero(np.all(near["vertices"] == p, axis=1))
            assert len(rows)
            shared.append(int(rows[0]))
        scalar = shape.mesh(method="scalar")
        for key in near:
            assert np.array_equal(near[key], scalar[key]), key
        polynomial = np.polynomial.Polynomial([0.55, 0, 1.35, -0.9])
        squared = (polynomial * polynomial).integ()
        profile = (
            shape.taper_start * (squared(1) - squared(0))
            + (1 - shape.taper_start) * 8 / 15
        )
        area = (
            (np.pi if shape.section == "ellipse" else 2)
            * shape.aspect
            * shape.radius**2
        )
        reference = shape.length * area * profile
        cases.append(
            {
                "section": shape.section,
                "near_triangles": len(near["faces"]),
                "coarse_triangles": len(coarse["faces"]),
                "common_vertices": len(shared),
                "volume_relative_errors": [v / reference - 1 for v in volumes],
            }
        )
    from needle_sector_checks import sector_checks

    sectors = sector_checks()
    return {
        "differential_points": 356 + sectors["differential_points"],
        "normal_max_error": max(maximum, sectors["normal_max_error"]),
        "closed_oriented_meshes": 16,
        "constant_cylinder_same_volume": True,
        "cases": cases + sectors["cases"],
        "sector_edges_and_fascicles": sectors,
    }
