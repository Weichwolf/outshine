"""Refine nearby ground triangles that carry an interpolated irradiance field."""

import bmesh
import numpy as np

GROUND_RECEIVERS = {"paving", "sidewalk", "road", "ground", "grass"}


def refine_receiver_mesh(obj, eye, reach, spacing):
    mesh = obj.data
    eye_local = obj.matrix_world.inverted() @ eye
    linear = obj.matrix_world.to_3x3()
    work = bmesh.new()
    try:
        work.from_mesh(mesh)
        bmesh.ops.remove_doubles(work, verts=list(work.verts), dist=1e-6)
        revised = False
        while True:
            edges = set()
            for face in work.faces:
                centre = face.calc_center_median()
                radius = max((linear @ (v.co - centre)).length for v in face.verts)
                if (linear @ (centre - eye_local)).length > reach + radius:
                    continue
                edges.update(
                    edge
                    for edge in face.edges
                    if (linear @ (edge.verts[1].co - edge.verts[0].co)).length > spacing
                )
            if not edges:
                break
            revised = True
            bmesh.ops.subdivide_edges(
                work, edges=list(edges), cuts=1, use_grid_fill=True
            )
            bmesh.ops.triangulate(
                work, faces=[face for face in work.faces if len(face.verts) > 3]
            )
        if revised:
            work.to_mesh(mesh)
            mesh.update()
        return revised
    finally:
        work.free()


def refine_ground_receivers(scene, batches, reach, spacing):
    candidates = set()
    for batch in batches:
        material = batch.mesh.materials[0].name.partition(" #")[0]
        if material in GROUND_RECEIVERS:
            corners = batch.positions.reshape(-1, 3, 3)
            edges = corners - np.roll(corners, -1, axis=1)
            if np.any(np.sum(edges * edges, axis=2) > spacing * spacing):
                candidates.add(batch.mesh)
    if not candidates:
        return False
    eye = scene.camera.matrix_world.translation
    revised = False
    for obj in scene.objects:
        if obj.type == "MESH" and obj.data in candidates:
            candidates.remove(obj.data)
            revised |= refine_receiver_mesh(obj, eye, reach, spacing)
    return revised
