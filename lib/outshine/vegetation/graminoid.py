"""Bounded unidentified graminoid tuft meshes from analytical folded blades."""

from dataclasses import replace

import numpy as np
from scipy.spatial.transform import Rotation

from .blade_surface import Blade

GRAMINOID_BLADE = Blade(
    length=0.45,
    width=0.009,
    widest=0.1,
    lobes=0,
    lobe_depth=0,
    base_fill=0.7,
    bend=1.15,
    fold=0.25,
    fold_power=1,
    twist=0,
    petiole=0,
)


def graminoid_tuft_mesh(seed, blades=12, segments=8):
    if (
        type(seed) is not int
        or not 0 <= seed < 2**64
        or type(blades) is not int
        or not 1 <= blades <= 64
    ):
        raise ValueError("uint64 seed and one to64 authored tuft blades required")
    rng = np.random.Generator(np.random.PCG64(seed))
    vertices, faces, normals, uv, blade_ids = [], [], [], [], []
    offset = 0
    for blade_index in range(blades):
        blade = replace(
            GRAMINOID_BLADE,
            length=rng.uniform(0.25, 0.48),
            width=rng.uniform(0.006, 0.012),
            bend=rng.uniform(0.6, 1.4),
            twist=rng.uniform(-0.2, 0.2),
        )
        mesh = blade.mesh(segments, across=1)
        # Uppercase intrinsic axes give world-Z azimuth after upright lean.
        frame = Rotation.from_euler(
            "ZX", (rng.uniform(0, 2 * np.pi), np.pi / 2 - rng.uniform(0.08, 0.4))
        ).as_matrix()
        angle, radius = rng.uniform(0, 2 * np.pi), 0.06 * np.sqrt(rng.random())
        origin = np.array((radius * np.cos(angle), radius * np.sin(angle), -0.008))
        vertices.append(mesh["vertices"] @ frame.T + origin)
        blade_ids.append(np.full(len(mesh["vertices"]), blade_index, dtype=np.uint8))
        faces.append(mesh["faces"] + offset)
        normals.append(mesh["normals"] @ frame.T)
        uv.append(
            np.column_stack(
                ((mesh["parameters"][:, 1] + 1) / 2, mesh["parameters"][:, 0])
            )
        )
        offset += len(mesh["vertices"])
    return {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
        "uv": np.concatenate(uv),
        "blade_index": np.concatenate(blade_ids),
    }
