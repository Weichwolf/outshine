"""Independent analytic, ODE, topology and original-function checks for spatial spines."""

import json
import subprocess
import sys

import numpy as np
from scipy.integrate import solve_ivp
from scipy.spatial.transform import Rotation
from spine_curve import CubicSpine
from spine_frames import orthonormalize, reflection_loop, transport_frames, unit
from spine_surface import spine_mesh


def helix(parameter):
    parameter = np.asarray(parameter)
    angle = 2 * np.pi * parameter
    radius, height, omega = 0.26, 1.8, 2 * np.pi
    centre = np.stack(
        (radius * (np.cos(angle) - 1), height * parameter, radius * np.sin(angle)),
        axis=-1,
    )
    velocity = np.stack(
        (
            -radius * omega * np.sin(angle),
            np.full_like(angle, height),
            radius * omega * np.cos(angle),
        ),
        axis=-1,
    )
    speed = np.sqrt((radius * omega) ** 2 + height**2)
    tangent = velocity / speed
    normal = np.stack((-np.cos(angle), np.zeros_like(angle), -np.sin(angle)), axis=-1)
    binormal = np.cross(tangent, normal)
    roll = height / speed * angle
    reference = -np.cos(roll)[..., None] * normal - np.sin(roll)[..., None] * binormal
    return centre, velocity, reference


def ode_reference(spine, parameter, initial=(1, 0, 0)):
    _centre, tangent, _speed, _curvature = spine.sample(0)
    initial = unit(initial - np.dot(initial, tangent) * tangent)

    def derivative(position, reference):
        _centre, tangent, speed, curvature = spine.sample(position)
        return -np.dot(speed * curvature, reference) * tangent

    result = solve_ivp(
        derivative,
        (0, 1),
        initial,
        method="DOP853",
        t_eval=parameter,
        rtol=2e-12,
        atol=2e-13,
        max_step=1 / (8 * len(spine.controls)),
    )
    assert result.success
    return result.y.T, result.nfev


def native_frame_check(oracle):
    rng = np.random.default_rng(172)
    matrices = []
    for index in range(32):
        rotation = Rotation.from_rotvec(rng.normal(size=3)).as_matrix()
        skew = np.array(((1.2, 0, 0), (0.25, 0.8, 0), (-0.1, 0.3, 1.5)))
        skew[2, 2] *= -1 if index % 2 else 1
        matrices.append((skew @ rotation).astype(np.float32).tolist())
    result = subprocess.run(
        [sys.executable, str(oracle)],
        input=json.dumps({"matrices": matrices}),
        text=True,
        capture_output=True,
        check=True,
        timeout=30,
    )
    native = json.loads(result.stdout)
    actual = orthonormalize(matrices)
    expected = np.asarray(native.pop("matrices"))
    np.testing.assert_allclose(actual, expected, rtol=1e-6, atol=5e-7)
    native["case_count"] = len(matrices)
    native["maximum_absolute_error"] = float(np.max(np.abs(actual - expected)))
    return native


def frame_checks():
    parameter = np.linspace(0, 1, 129)
    centre, velocity, exact = helix(parameter)
    tangent = unit(velocity)
    actual = transport_frames(centre, tangent)
    np.testing.assert_allclose(actual, reflection_loop(centre, tangent), atol=1e-13)
    error = np.max(np.linalg.norm(actual[:, 0] - exact, axis=1))
    assert error < 2e-7
    np.testing.assert_allclose(np.linalg.det(actual), 1, atol=2e-14)
    np.testing.assert_allclose(
        actual @ actual.transpose(0, 2, 1),
        np.broadcast_to(np.eye(3), actual.shape),
        atol=2e-14,
    )
    transform = Rotation.from_rotvec((0.4, -0.8, 0.3))
    shifted = transform.apply(centre * 7) + (2, -5, 9)
    rotated = transport_frames(
        shifted, transform.apply(tangent), transform.apply((1, 0, 0))
    )
    np.testing.assert_allclose(
        rotated,
        transform.apply(actual.reshape(-1, 3)).reshape(actual.shape),
        atol=2e-14,
    )
    reverse = transport_frames(centre[::-1], -tangent[::-1], actual[-1, 0])
    expected = actual[::-1].copy()
    expected[:, 1:] *= -1
    np.testing.assert_allclose(reverse, expected, atol=2e-14)
    straight = np.column_stack(
        (np.zeros_like(parameter), parameter, np.zeros_like(parameter))
    )
    flat = transport_frames(straight, np.tile((0, 1, 0), (len(parameter), 1)))
    np.testing.assert_allclose(flat, np.broadcast_to(np.eye(3), flat.shape), atol=1e-14)
    rolled = transport_frames(straight, flat[:, 1], roll=2 * np.pi * parameter)
    np.testing.assert_allclose(
        rolled[:, 0, 0], np.cos(2 * np.pi * parameter), atol=2e-14
    )
    return {
        "helix_frame_maximum_vector_error": float(error),
        "stations": len(parameter),
    }


def curve_check(spine):
    rng = np.random.default_rng(17)
    position = rng.uniform(0, 1, 1000)
    scaled = position * len(spine.controls)
    segment = scaled.astype(int)
    fraction = scaled - segment
    controls = np.asarray(spine.controls)[segment]
    weights = np.stack(
        (
            (1 - fraction) ** 3,
            3 * fraction * (1 - fraction) ** 2,
            3 * fraction**2 * (1 - fraction),
            fraction**3,
        ),
        axis=1,
    )
    polynomial = np.sum(weights[..., None] * controls, axis=1)
    np.testing.assert_allclose(spine.evaluate(position), polynomial, atol=5e-14)
    _centre, _tangent, _speed, curvature = spine.sample(position)
    assert np.max(np.linalg.norm(curvature, axis=-1)) <= spine.curvature_bound
    parameter = np.linspace(0, 1, 129)
    reference, evaluations = ode_reference(spine, parameter)
    centre, tangent, _speed, _curvature = spine.sample(parameter)
    frame = transport_frames(centre, tangent)
    error = np.max(np.linalg.norm(frame[:, 0] - reference, axis=1))
    assert error < 1e-6
    return {
        "curvature_bound_per_m": spine.curvature_bound,
        "ode_maximum_frame_vector_error": float(error),
        "ode_evaluations": evaluations,
    }


def surface_check(spine, profile, roll=(0, 0), ellipse=(1, 0.6)):
    mesh = spine_mesh(
        spine, profile, 0.13, segments=128, sides=24, ellipse=ellipse, roll=roll
    )
    result = mesh_check(mesh)
    result["maximum_differential_normal_error"] = differential_check(
        spine, profile, roll, ellipse, mesh
    )
    return result


def mesh_check(mesh):
    triangles = mesh["vertices"][mesh["faces"]]
    area = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    corners = mesh["normals"].reshape(-1, 3, 3)
    assert np.isfinite(triangles).all() and np.isfinite(corners).all()
    assert np.min(np.linalg.norm(area, axis=1)) > 1e-10
    assert np.min(np.sum(area * np.mean(corners, axis=1), axis=1)) > 0
    np.testing.assert_allclose(np.linalg.norm(corners, axis=-1), 1, atol=1e-14)
    edges = np.concatenate(
        [mesh["faces"][:, [a, b]] for a, b in ((0, 1), (1, 2), (2, 0))]
    )
    signs = np.where(edges[:, 0] < edges[:, 1], 1, -1)
    _unique, inverse, counts = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    assert np.all(counts == 2) and np.all(np.bincount(inverse, weights=signs) == 0)
    volume = (
        np.sum(
            np.einsum(
                "ij,ij->i", triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])
            )
        )
        / 6
    )
    assert volume > 0
    return {
        "vertices": len(mesh["vertices"]),
        "triangles": len(triangles),
        "signed_volume_m3": float(volume),
        "product_bytes": sum(array.nbytes for array in mesh.values()),
    }


def differential_check(spine, profile, roll, ellipse, mesh):
    stations = np.array((13, 39, 71, 89, 111))
    sides = np.array((1, 4, 7, 10, 13))
    parameter = stations / 128
    angle = sides * 2 * np.pi / 24
    epsilon = 1e-7
    queries = (parameter[:, None] + (-epsilon, 0, epsilon)).reshape(-1)
    reference, _evaluations = ode_reference(spine, queries)
    centre, tangent, _speed, _curvature = spine.sample(queries)
    rotation = np.interp(queries, (0, 1), roll)
    reference = Rotation.from_rotvec(-rotation[:, None] * tangent).apply(reference)
    normal = np.cross(reference, tangent)
    radius = 0.13 * profile.evaluate(queries)

    def positions(angles):
        return centre + radius[:, None] * (
            ellipse[0] * np.cos(angles)[:, None] * reference
            + ellipse[1] * np.sin(angles)[:, None] * normal
        )

    repeated = np.repeat(angle, 3)
    points = positions(repeated).reshape(-1, 3, 3)
    longitudinal = (points[:, 2] - points[:, 0]) / (2 * epsilon)
    angular = (
        positions(repeated + epsilon).reshape(-1, 3, 3)[:, 1]
        - positions(repeated - epsilon).reshape(-1, 3, 3)[:, 1]
    ) / (2 * epsilon)
    expected = unit(np.cross(longitudinal, angular))
    corner = mesh["normals"].reshape(-1, 3, 3)[2 * (stations * 24 + sides), 0]
    error = np.max(np.linalg.norm(corner - expected, axis=1))
    assert error < 3e-6
    return float(error)


def rejection_checks(spine, profile):
    cases = [
        lambda: CubicSpine(np.zeros((1, 4, 3))),
        lambda: CubicSpine(np.full((1, 4, 3), np.nan)),
        lambda: CubicSpine.from_hermite(
            [[0, 0, 0], [0, 1, 0]], [[0, -1, 0], [0, 1, 0]]
        ),
        lambda: spine.evaluate(-0.1),
        lambda: spine.evaluate(np.nan),
        lambda: spine.evaluate(0.5, 3),
        lambda: orthonormalize(np.zeros((3, 3))),
        lambda: orthonormalize(((1, 0, 0), (1, 0, 0), (0, 0, 1))),
        lambda: transport_frames([[0, 0, 0], [0, 0, 0]], [[0, 1, 0], [0, 1, 0]]),
        lambda: transport_frames([[0, 0, 0], [0, 1, 0]], [[0, 1, 0], [0, -1, 0]]),
        lambda: transport_frames(
            [[0, 0, 0], [0, 1, 0]], [[0, 1, 0], [0, 1, 0]], initial=(0, 1, 0)
        ),
        lambda: spine_mesh(spine, profile, -1),
        lambda: spine_mesh(spine, profile, 10),
        lambda: spine_mesh(spine, profile, 0.13, sides=2),
        lambda: spine_mesh(spine, profile, 0.13, segments=1025),
        lambda: spine_mesh(spine, profile, 0.13, ellipse=(0, 1)),
        lambda: spine_mesh(spine, profile, 0.13, roll=(0, np.inf)),
    ]
    for case in cases:
        try:
            case()
        except ValueError:
            continue
        raise AssertionError("invalid spatial curve/frame/surface accepted")
    return len(cases)
