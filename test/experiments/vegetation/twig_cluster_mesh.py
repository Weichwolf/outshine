"""Demanded new curved short-shoot bodies; existing source wood stays exact."""

import numpy as np
from outshine_ref.vegetation.axis_surface import axis_mesh
from twig_radius_basis import apply_basis, radius_basis


def scalar_twigs(plan, source):
    """Retained exact existing kernel control, never the default large expansion."""
    needed = plan.summary()["twigs"]
    if not needed:
        return dict(source)
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


def add_twigs(plan, source, max_twigs, *, method="batched"):
    needed = plan.summary()["twigs"]
    if (
        type(max_twigs) is not int
        or not 0 <= max_twigs <= 16384
        or method not in ("batched", "scalar")
    ):
        raise ValueError("bounded complete twig capacity and declared method required")
    if needed > max_twigs:
        return None
    if method == "scalar":
        return scalar_twigs(plan, source)
    if not needed:
        return dict(source)
    groups = [plan.twigs(int(node), 32) for node in plan.host_ids]
    rows = {k: np.concatenate([g[k] for g in groups]) for k in groups[0]}
    vertices = np.empty((needed, 18, 3))
    normals = np.empty((needed, 96, 3))
    keys = rows["counts"].astype(int) * 16 + rows["variants"]
    faces = None
    for key in np.unique(keys):
        select = np.flatnonzero(keys == key)
        count, variation = divmod(int(key), 16)
        shoot = plan.shoot(count, variation)
        basis = radius_basis(shoot.length, shoot.bend)
        v, n = apply_basis(basis, rows["radii"][select])
        vertices[select] = (
            np.einsum("nij,nvj->nvi", rows["frames"][select], v)
            + rows["origins"][select, None, :]
        )
        normals[select] = np.einsum("nij,nvj->nvi", rows["frames"][select], n)
        if faces is None:
            faces = basis["faces"]
        else:
            np.testing.assert_array_equal(faces, basis["faces"])
    f = (
        faces[None] + len(source["vertices"]) + np.arange(needed)[:, None, None] * 18
    ).reshape(-1, 3)
    return {
        "vertices": np.vstack((source["vertices"], vertices.reshape(-1, 3))),
        "faces": np.vstack((source["faces"], f)).astype(np.int32),
        "normals": np.vstack((source["normals"], normals.reshape(-1, 3))),
        "corner_uv": np.vstack((source["corner_uv"], np.zeros((f.size, 2)))),
        "surface": np.r_[source["surface"], np.zeros(len(f), np.uint8)],
    }
