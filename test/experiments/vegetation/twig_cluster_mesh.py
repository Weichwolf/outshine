"""Demanded new curved short-shoot bodies; existing source wood stays exact."""

import numpy as np
from outshine_ref.vegetation.axis_surface import axis_mesh


def add_twigs(plan, source, max_twigs):
    needed = plan.summary()["twigs"]
    if type(max_twigs) is not int or not 0 <= max_twigs <= 16384:
        raise ValueError("bounded whole-twig capacity required")
    if needed > max_twigs:
        return None
    vertices, faces, normals = (
        [source["vertices"]],
        [source["faces"]],
        [source["normals"]],
    )
    offset = len(source["vertices"])
    for host in plan.host_ids:
        twigs = plan.twigs(int(host), 32)
        for i, count in enumerate(twigs["counts"]):
            shoot = plan.shoot(int(count), int(twigs["variants"][i]))
            radius = float(twigs["radii"][i])
            mesh = axis_mesh(shoot.length, shoot.bend, radius, radius * 0.3, 3, 4)
            frame = twigs["frames"][i]
            vertices.append(mesh["vertices"] @ frame.T + twigs["origins"][i])
            normals.append(mesh["normals"] @ frame.T)
            faces.append(mesh["faces"] + offset)
            offset += len(mesh["vertices"])
    f = np.concatenate(faces).astype(np.int32)
    old = len(source["faces"])
    return {
        "vertices": np.concatenate(vertices),
        "faces": f,
        "normals": np.concatenate(normals),
        "corner_uv": np.concatenate(
            (source["corner_uv"], np.zeros(((len(f) - old) * 3, 2)))
        ),
        "surface": np.r_[source["surface"], np.zeros(len(f) - old, np.uint8)],
    }
