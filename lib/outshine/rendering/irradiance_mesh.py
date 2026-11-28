"""Refine nearby surface triangles that carry an interpolated irradiance field."""

import numpy as np

GROUND_RECEIVERS = {"paving", "sidewalk", "road", "ground", "grass"}
# Triangle corners are 0–2, with midpoints on AB, BC, and CA at 3–5.
SPLIT_TRIANGLES = (
    ((0, 1, 2),),
    ((0, 3, 2), (3, 1, 2)),
    ((1, 4, 0), (4, 2, 0)),
    ((0, 3, 2), (3, 1, 4), (3, 4, 2)),
    ((2, 5, 1), (5, 0, 1)),
    ((0, 3, 5), (3, 1, 2), (3, 2, 5)),
    ((0, 1, 4), (0, 4, 5), (5, 4, 2)),
    ((0, 3, 5), (3, 1, 4), (5, 4, 2), (3, 4, 5)),
)


def mesh_points(mesh, transform=None):
    points = np.empty(3 * len(mesh.vertices), dtype=np.float32)
    mesh.vertices.foreach_get("co", points)
    points = points.reshape(-1, 3)
    if transform is not None:
        return points @ transform[:3, :3].T + transform[:3, 3]
    return points


def mesh_triangles(mesh):
    mesh.calc_loop_triangles()
    indices = np.empty(3 * len(mesh.loop_triangles), dtype=np.int32)
    mesh.loop_triangles.foreach_get("vertices", indices)
    return indices.reshape(-1, 3)


def refine_triangles(points, faces, eye, reach, spacing, field=None):
    while True:
        corners = points[faces]
        centres = corners.mean(axis=1)
        radius = np.linalg.norm(corners - centres[:, None, :], axis=2).max(axis=1)
        nearby = np.linalg.norm(centres - eye, axis=1) < reach + radius
        length = np.linalg.norm(corners - np.roll(corners, -1, axis=1), axis=2)
        cuts = nearby[:, None] & (length > spacing)
        if not cuts.any():
            return points, faces, field

        edges = np.sort(np.stack((faces, np.roll(faces, -1, axis=1)), axis=2), axis=2)
        stride = len(points)
        keys = stride * edges[:, :, 0].astype(np.int64) + edges[:, :, 1]
        selected = np.unique(keys[cuts])
        endpoints = np.column_stack(np.divmod(selected, stride))
        points = np.concatenate((points, points[endpoints].mean(axis=1)))
        if field is not None:
            field = np.concatenate((field, field[endpoints].mean(axis=1)))

        # Every incident triangle uses the same midpoint, including across the range limit.
        lookup = np.minimum(np.searchsorted(selected, keys), len(selected) - 1)
        split = selected[lookup] == keys
        pattern = split @ np.array([1, 2, 4])
        corners_and_midpoints = np.column_stack((faces, stride + lookup))
        faces = np.concatenate(
            [
                corners_and_midpoints[pattern == mask][:, triangles].reshape(-1, 3)
                for mask, triangles in enumerate(SPLIT_TRIANGLES)
            ]
        ).astype(np.int32)


def refine_receiver_mesh(obj, eye, reach, spacing):
    mesh = obj.data
    transform = np.asarray(obj.matrix_world)
    points = mesh_points(mesh, transform)
    faces = mesh_triangles(mesh)
    litter = mesh.attributes.get("trail litter")
    field = None
    if litter is not None:
        field = np.empty(len(points), dtype=np.float32)
        litter.data.foreach_get("value", field)
    vertices, triangles, field = refine_triangles(
        points, faces, eye, reach, spacing, field
    )
    if len(vertices) == len(points):
        return False
    inverse = np.asarray(obj.matrix_world.inverted())
    vertices = vertices @ inverse[:3, :3].T + inverse[:3, 3]
    mesh.clear_geometry()
    mesh.from_pydata(vertices.tolist(), [], triangles.tolist())
    if field is not None:
        mesh.attributes.new("trail litter", "FLOAT", "POINT").data.foreach_set(
            "value", field
        )
    mesh.update()
    return True


def refine_receivers(scene, batches, reach, spacing, materials):
    candidates = set()
    for batch in batches:
        material = batch.mesh.materials[0].name.partition(" #")[0]
        if material in materials:
            corners = batch.positions.reshape(-1, 3, 3)
            edges = corners - np.roll(corners, -1, axis=1)
            if np.any(np.sum(edges * edges, axis=2) > spacing * spacing):
                candidates.add(batch.mesh)
    if not candidates:
        return False
    eye = np.asarray(scene.camera.matrix_world.translation)
    revised = False
    for obj in scene.objects:
        if obj.type == "MESH" and obj.data in candidates:
            candidates.remove(obj.data)
            revised |= refine_receiver_mesh(obj, eye, reach, spacing)
    return revised
