"""Own reciprocal diffuse thin-sheet baseline, without a specular or BSSRDF claim."""

import numpy as np

from .leaf_optics import bounded_optics
from .material_arrays import real_array


def unit_directions(value, count):
    array = real_array(value, 3)
    if array.shape not in ((3,), (count, 3)):
        raise ValueError("one direction or matching leaf direction rows required")
    if np.any(abs(array) > 1 + 1e-10) or np.any(
        abs(np.linalg.norm(array, axis=-1) - 1) > 1e-10
    ):
        raise ValueError("unit leaf direction required")
    return np.broadcast_to(array, (count, 3))


def leaf_diffuse_response(optical, normals, views, lights):
    optical = bounded_optics(optical)
    count = len(optical)
    normals, views, lights = (
        unit_directions(v, count) for v in (normals, views, lights)
    )
    facing = np.where(np.sum(normals * views, axis=1) < 0, -1, 1)
    cosine = facing * np.sum(normals * lights, axis=1)
    return (
        optical[:, :3] * np.maximum(cosine, 0)[:, None]
        + optical[:, 3:] * np.maximum(-cosine, 0)[:, None]
    ) / np.pi
