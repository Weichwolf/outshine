"""Read finished connected products without generating source shapes or placements."""

from pathlib import Path

import numpy as np


def read_host(inputs, node, policy="connected32"):
    inputs = Path(inputs)
    with np.load(inputs / f"host-{node}/source.npz", allow_pickle=False) as z:
        source = dict(z)
    with np.load(inputs / f"host-{node}/{policy}-poses.npz", allow_pickle=False) as z:
        poses = dict(z)
    for key, other in (("ids", "ids"), ("variants", "leaf_variants")):
        np.testing.assert_array_equal(poses[key], source[other])
    np.testing.assert_array_equal(
        poses["physical_leaf_area_m2"], source["physical_leaf_area_m2"]
    )
    prototypes = {}
    for variant in np.unique(poses["variants"]):
        with np.load(
            inputs / policy / f"prototype-{variant}.npz", allow_pickle=False
        ) as z:
            prototypes[int(variant)] = dict(z)
    surfaces = []
    frame, origin = source["anchor_frame"], source["anchor_origin"]
    for index, variant in enumerate(poses["variants"]):
        p = prototypes[int(variant)]
        r = frame.T @ poses["frames"][index]
        mesh = {k: p[k] for k in ("vertices", "faces", "normals", "uv")}
        mesh |= {
            "vertices": (
                p["vertices"] @ poses["frames"][index].T
                + poses["origins"][index]
                - origin
            )
            @ frame,
            "normals": p["normals"] @ r.T,
        }
        surfaces.append(
            {
                "mesh": mesh,
                "colour": p["colour"],
                "normal": p["normal"],
                "basis": r,
                "owner": poses["ids"][index],
            }
        )
    return source, tuple(surfaces)
