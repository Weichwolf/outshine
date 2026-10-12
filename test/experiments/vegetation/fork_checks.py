"""Independent optimization, ray/triangle and topology checks for organic fork surfaces."""

import numpy as np
from fork_field import ForkField, smooth_maximum
from scipy.optimize import brentq, minimize
from scipy.sparse import coo_array
from scipy.sparse.csgraph import connected_components
from scipy.spatial.transform import Rotation
from spine_frames import unit


def blend_checks():
    rng = np.random.default_rng(73)
    cases = 0
    maximum_error = 0.0
    for count in (1, 2, 3, 8, 16):
        scores = rng.uniform(-3, 3, (8, count))
        actual, weights = smooth_maximum(scores, 0.7)
        np.testing.assert_allclose(weights.sum(axis=1), 1, atol=2e-14)
        assert np.min(weights) >= 0
        for values, result in zip(scores, actual, strict=True):
            reference = minimize(
                lambda w, values=values: (
                    -np.dot(values, w) + 0.35 * np.dot(w, w) - 0.35
                ),
                np.full(count, 1 / count),
                method="SLSQP",
                bounds=[(0, 1)] * count,
                constraints={"type": "eq", "fun": lambda w: np.sum(w) - 1},
                options={"ftol": 1e-13, "maxiter": 200},
            )
            assert reference.success
            error = abs(result + reference.fun)
            assert error < 2e-10
            maximum_error = max(maximum_error, error)
            cases += 1
        assert np.all(actual >= np.max(scores, axis=1) - 1e-14)
        assert np.all(actual <= np.max(scores, axis=1) + 0.35 * (1 - 1 / count) + 1e-14)
    return {"independent_qp_cases": cases, "maximum_value_error_m2": maximum_error}


def scalar_scores(field, point):
    point = np.asarray(point) - field.origin
    values = []
    for endpoint, radius in zip(field.endpoints, field.radii, strict=True):
        endpoint = np.asarray(endpoint)
        square = np.dot(endpoint, endpoint)
        projection = (
            min(max(float(np.dot(point, endpoint) / square), 0), 1) if square else 0
        )
        delta = point - projection * endpoint
        values.append(radius**2 - np.dot(delta, delta))
    return np.asarray(values)


def field_checks(field):
    rng = np.random.default_rng(91)
    points = rng.normal(size=(37, 3)) * field.ray_extent / 3 + field.origin
    scores = np.stack([scalar_scores(field, point) for point in points])
    values, gradient = field.evaluate(points, with_gradient=True)
    expected, _weights = smooth_maximum(scores, field.blend)
    np.testing.assert_allclose(values, expected, atol=2e-14)
    epsilon = field.ray_extent * 1e-6
    numerical = np.stack(
        [
            (
                field.evaluate(points + np.eye(3)[axis] * epsilon)
                - field.evaluate(points - np.eye(3)[axis] * epsilon)
            )
            / (2 * epsilon)
            for axis in range(3)
        ],
        axis=1,
    )
    np.testing.assert_allclose(gradient, numerical, atol=2e-7, rtol=2e-7)
    directions = unit(rng.normal(size=(41, 3)))
    actual, stats = field.radial_radius(directions)
    reference = np.array(
        [
            brentq(
                lambda distance, direction=direction: float(
                    field.evaluate([np.asarray(field.origin) + distance * direction])[0]
                ),
                0,
                field.ray_extent,
                xtol=1e-13,
            )
            for direction in directions
        ]
    )
    np.testing.assert_allclose(actual, reference, atol=field.ray_extent * 2e-10)
    surface = np.asarray(field.origin) + directions * actual[:, None]
    normals = field.normals(surface)
    assert np.min(np.sum(normals * directions, axis=1)) > 0
    order = tuple(reversed(range(len(field.endpoints))))
    permuted = ForkField(
        tuple(field.endpoints[i] for i in order),
        tuple(field.radii[i] for i in order),
        field.blend,
        field.origin,
    )
    np.testing.assert_allclose(permuted.evaluate(points), values, atol=2e-14)
    transform = Rotation.from_rotvec((0.7, -0.3, 0.4))
    rotated = ForkField(
        transform.apply(field.endpoints) * 3,
        np.asarray(field.radii) * 3,
        field.blend * 9,
        transform.apply(field.origin) * 3 + (4, -2, 7),
    )
    np.testing.assert_allclose(
        rotated.evaluate(transform.apply(points) * 3 + (4, -2, 7)),
        values * 9,
        atol=2e-13,
    )
    return stats | {
        "independent_scalar_points": len(points),
        "independent_brent_rays": len(directions),
        "maximum_ray_error_m": float(np.max(np.abs(actual - reference))),
        "maximum_gradient_error_m": float(np.max(np.abs(gradient - numerical))),
    }


def sphere_checks():
    field = ForkField(((0, 0, 0),), (0.2,), 0.04)
    directions = unit(np.random.default_rng(7).normal(size=(101, 3)))
    radius, _cost = field.radial_radius(directions)
    np.testing.assert_allclose(radius, 0.2, atol=1e-11)
    repeated = ForkField(((0, 0, 0),) * 3, (0.2,) * 3, 0.04)
    radius, _cost = repeated.radial_radius(directions)
    np.testing.assert_allclose(radius, np.sqrt(0.2**2 + 0.04 / 3), atol=1e-11)
    return {"analytic_sphere_rays": len(directions) * 2}


def mesh_checks(mesh, field):
    vertices, faces = mesh["vertices"], mesh["faces"]
    assert np.isfinite(vertices).all() and np.isfinite(mesh["normals"]).all()
    assert np.min(faces) >= 0 and np.max(faces) < len(vertices)
    triangles = vertices[faces] - field.origin
    cross = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    assert np.min(np.linalg.norm(cross, axis=1)) > field.ray_extent**2 * 1e-15
    corners = mesh["normals"].reshape(-1, 3, 3)
    np.testing.assert_allclose(np.linalg.norm(corners, axis=2), 1, atol=1e-14)
    edges = np.concatenate([faces[:, pair] for pair in ((0, 1), (1, 2), (2, 0))])
    sign = np.where(edges[:, 0] < edges[:, 1], 1, -1)
    unique, inverse, count = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    assert np.all(count == 2) and np.all(np.bincount(inverse, weights=sign) == 0)
    assert len(vertices) - len(unique) + len(faces) == 2
    adjacency = coo_array(
        (np.ones(len(edges)), (edges[:, 0], edges[:, 1])),
        shape=(len(vertices), len(vertices)),
    ).tocsr()
    assert connected_components(adjacency, directed=False, return_labels=False) == 1
    volume = (
        np.sum(
            np.einsum(
                "ij,ij->i", triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])
            )
        )
        / 6
    )
    assert volume > 0
    face_dot = np.sum(cross * np.mean(corners, axis=1), axis=1)
    return {
        "triangles": len(faces),
        "vertices": len(vertices),
        "closed_single_component": True,
        "euler_characteristic": 2,
        "signed_volume_m3": float(volume),
        "product_bytes": sum(array.nbytes for array in mesh.values()),
        "faces_disagreeing_with_analytic_normals": int(np.count_nonzero(face_dot <= 0)),
    }


def ray_mesh_radii(mesh, directions, origin):
    directions = unit(directions)
    triangles = mesh["vertices"][mesh["faces"]] - origin
    result = np.full(len(directions), np.inf)
    for start in range(0, len(directions), 32):
        rays = directions[start : start + 32]
        distance = np.full(len(rays), np.inf)
        for begin in range(0, len(triangles), 1024):
            triangle = triangles[begin : begin + 1024]
            first, second = (
                triangle[:, 1] - triangle[:, 0],
                triangle[:, 2] - triangle[:, 0],
            )
            perpendicular = np.cross(rays[:, None], second[None])
            determinant = np.sum(first[None] * perpendicular, axis=2)
            usable = np.abs(determinant) > 1e-15
            inverse = np.divide(
                1, determinant, out=np.zeros_like(determinant), where=usable
            )
            u = np.sum(-triangle[None, :, 0] * perpendicular, axis=2) * inverse
            other = np.cross(-triangle[:, 0], first)
            v = (rays @ other.T) * inverse
            t = np.sum(second * other, axis=1)[None] * inverse
            hit = (
                usable & (u >= -1e-10) & (v >= -1e-10) & (u + v <= 1 + 1e-10) & (t > 0)
            )
            distance = np.minimum(distance, np.min(np.where(hit, t, np.inf), axis=1))
        result[start : start + len(rays)] = distance
    if not np.isfinite(result).all():
        raise ValueError(
            "closed mesh must intersect every tested ray from the fork origin"
        )
    return result


def rejection_checks():
    cases = [
        lambda: ForkField((), (), 0.02),
        lambda: ForkField(((0, 1, 0),) * 17, (0.1,) * 17, 0.02),
        lambda: ForkField(((0, 1, 0),), (-0.1,), 0.02),
        lambda: ForkField(((0, 1, 0),), (1e-200,), 0.02),
        lambda: ForkField(((0, np.nan, 0),), (0.1,), 0.02),
        lambda: ForkField(((0, 1, 0),), (0.1,), -0.02),
        lambda: ForkField(((0, 1, 0),), (0.1,), np.inf),
        lambda: ForkField(((0, 1, 0),), (0.1,), 0.02, origin=(0, 0, np.nan)),
        lambda: ForkField(((0, 1, 0),), (0.1,), 0.02).radial_radius(((0, 0, 0),)),
    ]
    for case in cases:
        try:
            case()
        except ValueError:
            continue
        raise AssertionError("invalid fork field input accepted")
    return len(cases)
