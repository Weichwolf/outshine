"""Matched carrier and demanded child-tube drawing from complete stored coefficients."""

import numpy as np
from outshine_ref.rendering.blender_mesh import mesh_object
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.twig_radius_basis import apply_basis, radius_basis
from shoot_form_scene import constant_material


def isolated_carrier(dense, host):
    mesh = axis_mesh(0.35, 0.4, 0.0005, 0.00005, segments=12, sides=6)
    frame, origin = dense.base.data["frames"][host], dense.base.data["origins"][host]
    mesh_object(
        "same isolated parent carrier",
        mesh["vertices"] @ frame.T + origin,
        mesh["faces"],
        mesh["normals"] @ frame.T,
        constant_material("same generic wood", (0.12, 0.065, 0.025), 0.8),
        True,
    )
    return {"body_triangles": 0, "twig_triangles": len(mesh["faces"])}


def child_tubes(form, selected):
    data = form.data
    rows = np.flatnonzero(
        np.repeat(selected, np.diff(data["host_offsets"]).astype(int))
    )
    count = len(rows)
    if not count:
        return {"child_tubes": 0, "child_triangles": 0, "child_mesh_array_bytes": 0}
    vertices, normals = np.empty((count, 18, 3)), np.empty((count, 96, 3))
    topology = None
    for key in np.unique(data["keys"][rows]):
        targets = np.flatnonzero(data["keys"][rows] == key)
        chosen = rows[targets]
        basis = radius_basis(float(data[f"p{key}_length"]), 0.35)
        v, n = apply_basis(basis, data["radii"][chosen])
        vertices[targets] = (
            np.einsum("nij,nvj->nvi", data["frames"][chosen], v)
            + data["origins"][chosen, None]
        )
        normals[targets] = np.einsum("nij,nvj->nvi", data["frames"][chosen], n)
        topology = basis["faces"]
    faces = (topology[None] + np.arange(count)[:, None, None] * 18).reshape(-1, 3)
    mesh_object(
        "attached matching short shoots",
        vertices.reshape(-1, 3),
        faces,
        normals.reshape(-1, 3),
        constant_material("same generic wood", (0.12, 0.065, 0.025), 0.8),
        True,
    )
    return {
        "child_tubes": count,
        "child_triangles": len(faces),
        "child_mesh_array_bytes": vertices.nbytes + normals.nbytes + faces.nbytes,
    }
