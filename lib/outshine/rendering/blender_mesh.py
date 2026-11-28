"""Blender import of supplied triangles, normals and optional UVs without reconstruction."""

import numpy as np


def mesh_object(
    name, vertices, faces, normals, surface, corner_normals=False, *, uv=None
):
    import bpy

    vertices, faces, normals = (
        np.asarray(vertices),
        np.asarray(faces),
        np.asarray(normals),
    )
    if (
        vertices.ndim != 2
        or vertices.shape[1] != 3
        or not len(vertices)
        or faces.ndim != 2
        or faces.shape[1] != 3
        or not len(faces)
        or faces.dtype.kind not in "iu"
        or faces.min() < 0
        or faces.max() >= len(vertices)
        or not np.isfinite(vertices).all()
    ):
        raise ValueError("Finite vertices and valid triangle indices required")
    expected = len(faces) * 3 if corner_normals else len(vertices)
    if (
        normals.shape != (expected, 3)
        or not np.isfinite(normals).all()
        or not np.allclose(np.linalg.norm(normals, axis=1), 1.0, rtol=0, atol=1e-4)
    ):
        raise ValueError(
            "Supplied unit normals must match the declared point/corner domain"
        )
    if uv is not None:
        uv = np.asarray(uv)
        if uv.shape != (len(vertices), 2) or not np.isfinite(uv).all():
            raise ValueError("Supplied point UVs must match the vertex domain")
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices.tolist(), [], faces.tolist())
    mesh.materials.append(surface)
    mesh.update()
    mesh.polygons.foreach_set("use_smooth", np.ones(len(faces), dtype=bool))
    if corner_normals:
        mesh.normals_split_custom_set(normals.tolist())
    else:
        mesh.normals_split_custom_set_from_vertices(normals.tolist())
    if uv is not None:
        layer = mesh.uv_layers.new(name="source UV")
        layer.data.foreach_set("uv", np.asarray(uv[faces], dtype=np.float32).ravel())
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj
