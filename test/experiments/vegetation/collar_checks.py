"""Independent normal/volume cases and local approximation contracts."""

from dataclasses import replace

import manifold3d as manifold
import numpy as np
from branch_collar import collar_field, collar_products
from collar_union import local_union
from plant_experiment import sapling
from plant_surface_checks import topology
from spine_frames import unit


def analytic_checks():
    products = []
    for shift in (0.0, 0.5):
        raw = manifold.Manifold.cube((1, 1, 1)).refine(3).to_mesh64()
        vertices = np.asarray(raw.vert_properties)[:, :3] + (shift, 0, 0)
        faces = np.asarray(raw.tri_verts)
        products.append(
            {
                "vertices": vertices,
                "faces": faces,
                "normals": unit(vertices[faces].reshape(-1, 3) - (0.75, 0.5, 0.5)),
            }
        )
    mesh, work = local_union(products, with_work=True)
    checks = topology(mesh)
    np.testing.assert_allclose(checks["signed_volume_m3"], 1.5, atol=1e-13)
    expected = unit(mesh["vertices"][mesh["faces"]].reshape(-1, 3) - (0.75, 0.5, 0.5))
    error = float(np.max(abs(expected - mesh["normals"])))
    assert error < 1e-3 and work["projected_corners"] == 36
    plant = sapling()
    levels = (
        plant.levels[0],
        replace(plant.levels[1], branches=64),
        replace(plant.levels[2], branches=64),
    )
    large = replace(plant, levels=levels)
    try:
        collar_products(large)
    except ValueError:
        pass
    else:
        raise AssertionError("unbounded collar request accepted")
    assert not large._axes and not large._leaves
    return {
        "analytic_union_volume_m3": 1.5,
        "maximum_radial_normal_error": error,
        "reference_projected_corners": work["projected_corners"],
        "oversized_request_rejected_before_expansion": True,
    }


def check_local_meshes(plant, meshes, rows):
    from fork_checks import mesh_checks

    checks = []
    for mesh, row in zip(meshes, rows, strict=True):
        field, _plan = collar_field(plant, row["path"])
        checked = mesh_checks(mesh, field)
        assert checked["faces_disagreeing_with_analytic_normals"] == 0
        checks.append(checked)
    return {
        "closed_local_surfaces": len(checks),
        "triangles": sum(c["triangles"] for c in checks),
        "product_bytes": sum(c["product_bytes"] for c in checks),
        "maximum_source_chord_bound_m": max(
            max(r["source_chord_bounds_m"]) for r in rows
        ),
    }
