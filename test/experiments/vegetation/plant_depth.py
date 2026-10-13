"""Root-anchored view meshes from conditional first-hit depth moments."""

import numpy as np
from leaf_sheet import sample_image
from scipy.ndimage import distance_transform_edt


def root_value(values, lower, upper):
    grid = values.shape[0] - 1
    root = np.clip((-lower) / (upper - lower) * grid, 0, grid)
    cell = np.minimum(root.astype(int), grid - 1)
    fx, fy = root - cell
    z = values[cell[1] : cell[1] + 2, cell[0] : cell[0] + 2]
    return (
        (1 - fx) * z[0, 0] + (fx - fy) * z[0, 1] + fy * z[1, 1]
        if fy <= fx
        else (1 - fy) * z[0, 0] + fx * z[1, 1] + (fy - fx) * z[1, 0]
    )


def depth_mesh(image, frame, lower, upper, grid):
    if type(grid) is not int or grid not in (1, 4, 8, 16):
        raise ValueError("bounded cached depth grid required")
    axis = np.linspace(0, 1, grid + 1)
    u, v = np.meshgrid(axis, axis)
    uv = np.column_stack((u.ravel(), v.ravel()))
    xy = lower + uv * (upper - lower)
    indices = np.arange((grid + 1) ** 2).reshape(grid + 1, grid + 1)
    a, b, c, d = (
        indices[:-1, :-1],
        indices[:-1, 1:],
        indices[1:, 1:],
        indices[1:, :-1],
    )
    faces = np.stack(
        (np.stack((a, b, c), axis=-1), np.stack((a, c, d), axis=-1)), axis=2
    ).reshape(-1, 3)
    depth = np.zeros(len(uv))
    if grid > 1:
        covered = image[..., 3] > 0
        if not np.any(covered):
            raise ValueError("nonempty plant projection required")
        nearest = distance_transform_edt(
            ~covered, return_distances=False, return_indices=True
        )
        field = image[..., 8] / np.maximum(image[..., 3], 1e-12)
        field = field[tuple(nearest)]
        depth = sample_image(field[..., None], uv)[:, 0]
        depth -= root_value(depth.reshape(grid + 1, grid + 1), lower, upper)
    return {
        "vertices": np.column_stack((xy, depth)) @ frame.T,
        "faces": faces.astype(np.int32),
        "normals": np.broadcast_to(frame[:, 2], (len(faces) * 3, 3)).copy(),
        "uv": uv,
    }
