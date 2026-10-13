"""Independent local-normal checks, split invariance and sharp-boundary preservation."""

from types import SimpleNamespace

import numpy as np
from join_normals import average_directions, smooth_join_normals


def scalar_angles(mesh):
    sums = np.zeros_like(mesh["vertices"])
    normals = mesh["normals"].reshape(-1, 3, 3)
    for indices, corners in zip(mesh["faces"], normals):
        p = mesh["vertices"][indices]
        for corner in range(3):
            a = p[(corner + 1) % 3] - p[corner]
            b = p[(corner + 2) % 3] - p[corner]
            angle = np.arctan2(np.linalg.norm(np.cross(a, b)), np.dot(a, b))
            sums[indices[corner]] += angle * corners[corner]
    return sums / np.maximum(np.linalg.norm(sums, axis=1)[:, None], 1e-30)


def split_fixture():
    vertices = np.array(
        [(0, 0, 0), (1, 0, 0), (0, 1, 0), (-1, 0, 0), (0, -1, 0)], dtype=float
    )
    faces = np.array([(0, 1, 2), (0, 2, 3), (0, 3, 4), (0, 4, 1)])
    n = average_directions(np.array([(0.3, 0, 1), (-0.3, 0, 1)]), 0)
    normals = np.repeat(n[np.array([0, 0, 1, 1])], 3, axis=0)
    initial = {"vertices": vertices, "faces": faces, "normals": normals}
    vertices = np.vstack((vertices, (0.6, 0.4, 0)))
    faces = np.vstack(([(0, 1, 5), (0, 5, 2)], faces[1:]))
    normals = np.vstack((normals[:3], normals))
    return initial, {"vertices": vertices, "faces": faces, "normals": normals}


def check_weighting():
    meshes = split_fixture()
    axis = SimpleNamespace(path=(1,), pose=SimpleNamespace(anchor=np.zeros(3)))
    plans = [{"path": (1,), "span_m": 2}]
    outputs = {}
    for method in ("angle", "area", "uniform"):
        outputs[method] = [
            smooth_join_normals(m, [axis], plans, method)[0] for m in meshes
        ]
    reference = scalar_angles(meshes[0])
    np.testing.assert_allclose(
        outputs["angle"][0]["normals"],
        reference[meshes[0]["faces"]].reshape(-1, 3),
        atol=1e-15,
    )
    # Boolean splits add corners but partition the same angular contribution.
    np.testing.assert_allclose(
        outputs["angle"][0]["normals"][0], outputs["angle"][1]["normals"][0], atol=1e-15
    )
    uniform_error = np.linalg.norm(
        outputs["uniform"][0]["normals"][0] - outputs["uniform"][1]["normals"][0]
    )
    assert uniform_error > 0.01
    # A local hard split rejects a vertex as a whole, without weakening orientation.
    sharp = meshes[0] | {"normals": meshes[0]["normals"].copy()}
    sharp["normals"][:3] = (0.995, 0, 0.1)
    sharp["normals"][:3] /= np.linalg.norm(sharp["normals"][:3], axis=1)[:, None]
    out, _ = smooth_join_normals(sharp, [axis], plans)
    selected = sharp["faces"].ravel() == 0
    np.testing.assert_array_equal(out["normals"][selected], sharp["normals"][selected])
    np.testing.assert_array_equal(
        average_directions([1, 0, 0], [-1, 0, 0]), np.zeros(3)
    )
    return {
        "scalar_angle_match": True,
        "split_invariant": True,
        "uniform_split_error": float(uniform_error),
        "hard_split_preserved": True,
    }


def check_local_normals(source, result, axes, plans):
    for key in source:
        if key != "normals":
            np.testing.assert_array_equal(source[key], result[key])
    corners = result["normals"].reshape(-1, 3, 3)
    np.testing.assert_allclose(np.linalg.norm(corners, axis=-1), 1, atol=2e-14)
    changed = np.linalg.norm(source["normals"] - result["normals"], axis=1) > 1e-12
    local = np.zeros(len(result["vertices"]), dtype=bool)
    lookup = {a.path: a for a in axes}
    for plan in plans:
        local |= (
            np.linalg.norm(
                result["vertices"] - lookup[plan["path"]].pose.anchor, axis=1
            )
            < 2 * plan["span_m"]
        )
    assert not np.any(changed & ~local[result["faces"].ravel()])
    return {
        "unchanged_geometry_material": True,
        "outside_normals_exact": True,
        "changed_corners": int(changed.sum()),
    }
