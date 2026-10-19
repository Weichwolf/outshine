"""Independent finite segment-intersection controls for original umbra witnesses."""

import numpy as np
from viewcell_visibility import corners


def check_witnesses(source, support, observer_box):
    triangle = source["positions"][source["indices"]].astype(float)
    eyes = corners(observer_box)
    random = np.random.default_rng(8992000)
    eyes = np.concatenate(
        (
            eyes,
            eyes.mean(0, keepdims=True),
            random.uniform(observer_box[0], observer_box[1], (16, 3)),
        )
    )
    weights = np.concatenate((np.eye(3), [[1 / 3] * 3], random.dirichlet([1, 1, 1], 4)))
    faces = np.flatnonzero(support.witness >= 0)
    checks, min_barycentric, min_t, max_t = 0, np.inf, np.inf, -np.inf
    for begin in range(0, len(faces), 64):
        ids = faces[begin : begin + 64]
        blockers = triangle[support.witness[ids]]
        target = np.einsum("wv,bvj->bwj", weights, triangle[ids])
        directions = target[:, None, :, :] - eyes[None, :, None, :]
        shape = directions.shape
        e1, e2 = blockers[:, 1] - blockers[:, 0], blockers[:, 2] - blockers[:, 0]
        matrix = np.stack(
            (
                directions,
                np.broadcast_to(-e1[:, None, None], shape),
                np.broadcast_to(-e2[:, None, None], shape),
            ),
            axis=-1,
        )
        rhs = blockers[:, None, None, 0, :] - eyes[None, :, None, :]
        solution = np.linalg.solve(matrix, np.broadcast_to(rhs, shape)[..., None])[
            ..., 0
        ]
        t, u, v = solution.transpose(3, 0, 1, 2)
        barycentric = np.minimum(np.minimum(u, v), 1 - u - v)
        if not (np.all(t > 0) and np.all(t < 1) and np.all(barycentric >= -1e-10)):
            raise AssertionError(
                "independent segment test rejects a hidden-face witness"
            )
        checks += t.size
        min_barycentric = min(min_barycentric, float(barycentric.min()))
        min_t, max_t = min(min_t, float(t.min())), max(max_t, float(t.max()))
    back = triangle[support.witness == -2]
    normals = np.cross(back[:, 1] - back[:, 0], back[:, 2] - back[:, 0])
    distance = np.einsum("bej,bj->be", eyes[None] - back[:, None, 0], normals)
    if not np.all(distance < 0):
        raise AssertionError("discarded face points towards a controlled eye")
    return {
        "segment_triangle_tests": checks,
        "tested_camera_positions": len(eyes),
        "points_per_occluded_triangle": len(weights),
        "minimum_blocker_barycentric": min_barycentric if checks else None,
        "minimum_segment_t": min_t if checks else None,
        "maximum_segment_t": max_t if checks else None,
        "backface_eye_tests": distance.size,
        "scope": "finite independent controls, continuous conservatism additionally relies on convex geometry and numeric margins",
    }
