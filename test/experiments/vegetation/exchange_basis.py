"""Camera-independent face/ground collocation basis on the unchanged finite mesh."""

import hashlib

import numpy as np
from grass_projection_reader import read_product
from scatter_scene import ScatterScene


def finite_mesh(inputs):
    report, stand, _ = read_product(inputs)
    ground = np.array(((-3, -3, 0), (3, -3, 0), (3, 3, 0), (-3, 3, 0)), float)
    vertices = np.concatenate((stand["vertices"], ground))
    faces = np.concatenate(
        (stand["faces"], np.array(((0, 1, 2), (0, 2, 3))) + len(stand["vertices"]))
    )
    digest = hashlib.sha256()
    digest.update(vertices.view(np.uint8))
    digest.update(faces.view(np.uint8))
    return report, vertices, faces, digest.hexdigest()


def prepare_stand(inputs, grid):
    if type(grid) is not int or not 1 <= grid <= 128:
        raise ValueError("bounded ground basis resolution required")
    report, vertices, faces, digest = finite_mesh(inputs)
    leaf_nodes = len(faces) - 2
    triangles = vertices[faces[:leaf_nodes]]
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    axis = -3 + (np.arange(grid) + 0.5) * 6 / grid
    x, y = np.meshgrid(axis, axis)
    points = np.concatenate(
        (triangles.mean(1), np.c_[x.ravel(), y.ravel(), np.zeros(grid * grid)])
    )
    normals = np.concatenate((normals, np.tile((0, 0, 1.0), (grid * grid, 1))))
    basis = {
        "points": points,
        "normals": normals,
        "leaf_nodes": leaf_nodes,
        "ground_grid": grid,
        "geometry_sha256": digest,
        "stand_identity": report["stand_identity"],
    }
    return ScatterScene(vertices, faces), basis


def source_nodes(ids, points, basis):
    result = ids.copy()
    ground = ids >= basis["leaf_nodes"]
    grid = basis["ground_grid"]
    xy = np.clip(np.floor((points[ground, :2] + 3) * grid / 6).astype(int), 0, grid - 1)
    result[ground] = basis["leaf_nodes"] + xy[:, 1] * grid + xy[:, 0]
    return result
