"""Weld nearby stems to their first-order limbs and round the joins."""

import numpy as np
from scipy.spatial import cKDTree


def joined_wood(stem, scaffolds, position, rotation, scale, stem_radius):
    from manifold3d import Error, Manifold, Mesh64, OpType

    c, s = np.cos(rotation), np.sin(rotation)
    frame = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])
    parts = [(stem["vertices"] - position, stem["faces"])]
    parts.extend((vertices @ frame.T * scale, faces) for vertices, faces in scaffolds)
    solids = [
        Manifold(
            Mesh64(
                np.ascontiguousarray(vertices, dtype=np.float64),
                np.ascontiguousarray(faces, dtype=np.uint64),
            )
        )
        for vertices, faces in parts
    ]
    joined = Manifold.batch_boolean(solids, OpType.Add)
    if joined.status() != Error.NoError or joined.is_empty():
        raise ValueError(f"cannot join woodland scaffold surfaces: {joined.status()}")
    shell = joined.to_mesh64()
    vertices, faces = shell.vert_properties, shell.tri_verts
    changes = joined_vertices(shell)
    if not len(changes):
        return vertices + position, faces

    # Extra position channels interpolate linearly over the input surface.
    reference = np.column_stack((vertices, vertices))
    rounded = Manifold(Mesh64(reference, faces.copy())).smooth_out(115, 0.45)
    result = rounded.refine_to_tolerance(0.014 * scale).to_mesh64()
    surface, source = result.vert_properties.reshape(-1, 2, 3).transpose(1, 0, 2)
    distance = cKDTree(changes).query(source)[0]
    blend = np.clip(1 - distance / (2.2 * stem_radius * scale), 0, 1)
    weight = (blend**2 * (3 - 2 * blend))[:, None]
    return source + (surface - source) * weight + position, result.tri_verts


def joined_vertices(mesh):
    faces = mesh.tri_verts
    provenance = np.repeat(mesh.run_original_id, np.diff(mesh.run_index) // 3)
    largest = np.full(len(mesh.vert_properties), -1, dtype=np.int64)
    smallest = np.full_like(largest, np.iinfo(np.int64).max)
    sources = np.repeat(provenance, 3)
    np.maximum.at(largest, faces.ravel(), sources)
    np.minimum.at(smallest, faces.ravel(), sources)
    return mesh.vert_properties[(largest >= 0) & (largest != smallest)]
