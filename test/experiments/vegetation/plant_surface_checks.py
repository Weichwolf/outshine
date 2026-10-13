"""Independent solid-angle occupancy probes and analytic union cases."""

from types import SimpleNamespace

import manifold3d as manifold
import numpy as np
from curve_frame import arc_frame
from fork_checks import mesh_checks
from plant_surface import solid_from_mesh, union_mesh


def topology(mesh):
    extent = np.linalg.norm(np.ptp(mesh["vertices"], axis=0))
    checks = mesh_checks(mesh, SimpleNamespace(origin=np.zeros(3), ray_extent=extent))
    checks["faces_against_corner_normals"] = checks.pop(
        "faces_disagreeing_with_analytic_normals"
    )
    return checks


def winding_inside(mesh, points):
    triangles = mesh["vertices"][mesh["faces"]]
    result = []
    for start in range(0, len(points), 8):
        v = triangles[None] - points[start : start + 8, None, None]
        lengths = np.linalg.norm(v, axis=-1)
        a, b, c = v[:, :, 0], v[:, :, 1], v[:, :, 2]
        numerator = np.einsum("ijk,ijk->ij", a, np.cross(b, c))
        denominator = np.prod(lengths, axis=2)
        denominator += np.sum(a * b, axis=2) * lengths[:, :, 2]
        denominator += np.sum(b * c, axis=2) * lengths[:, :, 0]
        denominator += np.sum(c * a, axis=2) * lengths[:, :, 1]
        winding = np.sum(2 * np.arctan2(numerator, denominator), axis=1) / (4 * np.pi)
        assert np.max(abs(winding - np.rint(winding))) < 1e-7
        result.extend(abs(winding) > 0.5)
    return np.asarray(result)


def plant_checks(plant, products, mesh):
    points = []
    for path in plant.paths():
        state = plant.axis(path)
        local = arc_frame(state.length, state.bend, 0, np.array([0.5, 0.97]))[0]
        points.extend(local @ state.frame.T + state.origin)
    inside_points = len(points)
    bounds = np.vstack([m["vertices"] for m in products])
    low, high = np.min(bounds, axis=0), np.max(bounds, axis=0)
    points.extend(np.random.default_rng(2353).uniform(low - 0.1, high + 0.1, (64, 3)))
    points = np.asarray(points)
    expected = np.zeros(len(points), dtype=bool)
    for product in products:
        expected |= winding_inside(product, points)
    actual = winding_inside(mesh, points)
    np.testing.assert_array_equal(actual, expected)
    assert actual[:inside_points].all()
    return topology(mesh) | {
        "independent_occupancy_probes": len(points),
        "thin_axis_interior_probes": inside_points,
    }


def analytic_checks():
    first = manifold.Manifold.cube((1, 1, 1)).to_mesh64()
    base = {
        "vertices": np.asarray(first.vert_properties)[:, :3],
        "faces": np.asarray(first.tri_verts),
    }
    cases = 0
    for shift, volume in ((0.0, 1.0), (0.5, 1.5), (1.0, 2.0)):
        other = {"vertices": base["vertices"] + (shift, 0, 0), "faces": base["faces"]}
        for method in ("batch", "sequential"):
            mesh = union_mesh([base, other], method)
            checks = topology(mesh)
            np.testing.assert_allclose(checks["signed_volume_m3"], volume, atol=1e-13)
            assert solid_from_mesh(mesh).status() == manifold.Error.NoError
            cases += 1
    return {"analytic_union_cases": cases}
