"""Shared bounded mesh deformation through prepared frond guide maps."""

import numpy as np

MAX_VERTICES = 1_000_000
MAX_TRIANGLES = 2_000_000


def deform_frond(guide, mesh, method="batch"):
    vertices, faces, normals = mesh["vertices"], mesh["faces"], mesh["normals"]
    if (
        method not in ("batch", "scalar")
        or np.iscomplexobj(vertices)
        or np.iscomplexobj(normals)
        or vertices.ndim != 2
        or vertices.shape[1:] != (3,)
        or faces.ndim != 2
        or faces.shape[1:] != (3,)
        or normals.shape != (faces.size, 3)
        or len(vertices) > MAX_VERTICES
        or len(faces) > MAX_TRIANGLES
    ):
        raise ValueError("bounded complete frond mesh and declared evaluator required")
    if not np.issubdtype(faces.dtype, np.integer) or np.any(
        (faces < 0) | (faces >= len(vertices))
    ):
        raise ValueError("valid source triangle indices required")
    if not np.isfinite(normals).all() or np.any(np.linalg.norm(normals, axis=1) == 0):
        raise ValueError("finite nonzero source corner normals required")
    guide.validate(vertices)
    if method == "batch" or not len(vertices):
        positions, jacobian = guide.evaluate(vertices)
    else:
        samples = [guide.evaluate(point[None]) for point in vertices]
        positions = np.concatenate([sample[0] for sample in samples])
        jacobian = np.concatenate([sample[1] for sample in samples])
    columns = jacobian.transpose(0, 2, 1)
    cofactor = np.stack(
        (
            np.cross(columns[:, 1], columns[:, 2]),
            np.cross(columns[:, 2], columns[:, 0]),
            np.cross(columns[:, 0], columns[:, 1]),
        ),
        axis=-1,
    )
    mapped = np.einsum("nij,nj->ni", cofactor[faces.ravel()], normals)
    mapped /= np.linalg.norm(mapped, axis=1, keepdims=True)
    if not np.isfinite(positions).all() or not np.isfinite(mapped).all():
        raise ValueError("finite deformed frond product required")
    p = positions[faces]
    cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    if not np.all(np.sum(cross[:, None] * mapped.reshape(-1, 3, 3), axis=2) > 0):
        raise ValueError("frond detail does not resolve the oriented surface")
    return {key: np.array(value, copy=True) for key, value in mesh.items()} | {
        "vertices": positions,
        "normals": mapped,
    }
