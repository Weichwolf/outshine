"""Draw linked whole-plant view meshes with per-site affine frames and light roles."""

from pathlib import Path

import bpy
import numpy as np
from blender_preview import mesh_object
from mathutils import Matrix
from sheet_material import sheet_material


def populate_views(recipe, directory, sites):
    surfaces, sources, triangles, objects = {}, [], 0, 0
    sources.append(Path(recipe["plant_product"]))
    for product in recipe["products"]:
        role = product["pass_role"]
        mask = sites[f"{product['selection']}_views"] == product["view"]
        indices = np.flatnonzero(mask)
        if not len(indices):
            continue
        path = directory / product["mesh"]
        sources.append(path)
        sources.extend(
            directory / filename for filename in product["textures"].values()
        )
        if role not in surfaces:
            surfaces[role] = sheet_material(
                role, directory, product["textures"], True, role
            )
        with np.load(path, allow_pickle=False) as mesh:
            original = mesh_object(
                f"{role}-{path.stem}",
                mesh["vertices"],
                mesh["faces"],
                mesh["normals"],
                surfaces[role],
                corner_normals=True,
            )
            uv = mesh["uv"][mesh["faces"].ravel()]
            original.data.uv_layers.new(name="plant atlas").data.foreach_set(
                "uv", uv.ravel()
            )
        for offset, index in enumerate(indices):
            obj = original if offset == 0 else original.copy()
            if offset:
                obj.data = original.data
                bpy.context.collection.objects.link(obj)
            c, s = np.cos(sites["angles"][index]), np.sin(sites["angles"][index])
            rotation = np.array(((c, -s, 0), (s, c, 0), (0, 0, 1)))
            transform = np.eye(4)
            transform[:3, :3] = rotation * sites["scales"][index]
            transform[:3, 3] = sites["positions"][index]
            obj.matrix_world = Matrix(transform.tolist())
            obj["vegetation_pass_role"] = role
            if "shadow_light" in product:
                obj["shadow_light"] = product["shadow_light"]
        triangles += len(indices) * product["triangles"]
        objects += len(indices)
    return sources, triangles, objects
