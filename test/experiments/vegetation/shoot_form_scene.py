"""Original procedural leaf material and source wood for equal form comparisons."""

import bpy
import numpy as np
from crown_draw_batches import batch_geometry, posed_bounds
from fern_material import attach_normals, leaf_material
from outshine_ref.rendering.blender_mesh import mesh_object
from outshine_ref.vegetation.axis_surface import axis_mesh


def constant_material(name, colour, roughness):
    result = bpy.data.materials.new(name)
    result.use_nodes = True
    shader = result.node_tree.nodes["Principled BSDF"]
    shader.inputs["Base Color"].default_value = (*colour, 1)
    shader.inputs["Roughness"].default_value = roughness
    return result


def leaf_objects(source, form, camera, inputs, fine_by_id, *, selected=None):
    requested = camera.visible(form.lower, form.upper) if selected is None else selected
    poses = form.evaluate(requested, max_leaves=100000)
    material = leaf_material(inputs / "spatial")
    count, triangles, payload, fine_count = 0, 0, 0, 0
    for variant, blade in enumerate(source.blades):
        chosen = poses["variants"] == variant
        if not chosen.any():
            continue
        detail_prototype = blade.mesh(32, 4)
        origins, frames = poses["origins"][chosen], poses["frames"][chosen]
        visible = camera.visible(*posed_bounds(detail_prototype, origins, frames))
        fine = fine_by_id[poses["ids"][chosen]]
        for detailed in (False, True):
            take = visible & (fine == detailed)
            if not take.any():
                continue
            product = detail_prototype if detailed else blade.mesh(8, 1)
            product["uv"] = np.column_stack(
                ((product["parameters"][:, 1] + 1) / 2, product["parameters"][:, 0])
            )
            mesh = batch_geometry(product, origins[take], frames[take])
            obj = mesh_object(
                f"source leaf variant{variant} fine{detailed}",
                mesh["vertices"],
                mesh["faces"],
                mesh["normals"],
                material,
                True,
                uv=mesh["uv"],
            )
            attach_normals(obj, mesh["normals"])
            count += int(take.sum())
            fine_count += int(take.sum()) if detailed else 0
            triangles += len(mesh["faces"])
            payload += sum(v.nbytes for v in mesh.values())
    return {
        "requested_leaf_poses": len(poses["ids"]),
        "drawn_leaves": count,
        "leaf_triangles": triangles,
        "expanded_leaf_array_bytes": payload,
        "fine_leaves": fine_count,
    }


def wood_objects(form, camera, study, *, near):
    material = constant_material("same generic wood", (0.12, 0.065, 0.025), 0.8)
    with np.load(study / "wood.npz", allow_pickle=False) as z:
        mesh_object(
            "unchanged complete C1 scaffold",
            z["vertices"],
            z["faces"],
            z["normals"],
            material,
            True,
        )
        body_triangles = len(z["faces"])
    vertices, normals, faces = [], [], []
    offset = 0
    for host in np.flatnonzero(camera.visible(form.lower, form.upper)):
        # Only demanded shoots receive a tube; their anchors/length match leaf poses.
        length = 0.35 * form.scale[host]
        p = axis_mesh(
            length,
            0.4,
            0.0005,
            0.00005,
            segments=12 if near else 3,
            sides=6 if near else 4,
        )
        frame, origin = form.base.data["frames"][host], form.base.data["origins"][host]
        vertices.append(p["vertices"] @ frame.T + origin)
        normals.append(p["normals"] @ frame.T)
        faces.append(p["faces"] + offset)
        offset += len(p["vertices"])
    twig_triangles = 0
    if faces:
        f = np.concatenate(faces)
        mesh_object(
            "matching demanded short shoots",
            np.concatenate(vertices),
            f,
            np.concatenate(normals),
            material,
            True,
        )
        twig_triangles = len(f)
    return {"body_triangles": body_triangles, "twig_triangles": twig_triangles}
