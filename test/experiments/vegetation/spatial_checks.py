"""Independent arc integrals, transported frames, geometry and graph checks."""

import numpy as np
from scipy.integrate import quad
from spatial_examples import spatial_axis, spatial_graph
from spine_checks import ode_reference
from spine_surface import sweep_stations


def true_length(spine, end=1.0):
    points = np.linspace(0, 1, len(spine.controls) + 1)
    inside = points[(points > 0) & (points < end)]
    return quad(
        lambda t: float(np.linalg.norm(spine.evaluate(t, 1))),
        0,
        end,
        points=inside,
        epsabs=1e-11,
        epsrel=1e-11,
    )[0]


def point(axis, u, angle):
    s = axis.field.sample(u)
    r = axis.radius.evaluate(u)
    offset = (
        axis.ellipse[0] * np.cos(angle)[..., None] * s["lateral"]
        + axis.ellipse[1] * np.sin(angle)[..., None] * s["normal"]
    )
    return s["centre"] + r[..., None] * offset


def check_axis(axis):
    u = np.linspace(0, 1, 73)
    s = axis.field.sample(u)
    exact = true_length(axis.spine)
    length_error = abs(axis.length - exact)
    arc_error = max(
        abs(true_length(axis.spine, t) - q * exact)
        for q, t in zip(u[::6], s["parameter"][::6])
    )
    assert length_error < 2e-11 and arc_error < 1e-7, (length_error, arc_error)
    reference, _ = ode_reference(axis.spine, s["parameter"])
    normal = np.cross(reference, s["tangent"])
    angle = axis.roll.evaluate(u)
    reference = np.cos(angle)[:, None] * reference + np.sin(angle)[:, None] * normal
    frame_error = np.linalg.norm(reference - s["lateral"], axis=-1).max()
    assert frame_error < 1e-5, frame_error
    frames = np.stack((s["lateral"], s["tangent"], s["normal"]), axis=1)
    np.testing.assert_allclose(
        frames @ frames.transpose(0, 2, 1),
        np.broadcast_to(np.eye(3), frames.shape),
        atol=2e-14,
    )
    assert np.linalg.det(frames).min() > 1 - 2e-14
    near, coarse = axis.mesh(48, 12), axis.mesh(12, 6)
    indices = np.arange(49 * 12).reshape(49, 12)[::4, ::2].ravel()
    indices = np.r_[indices, len(near["vertices"]) - 2, len(near["vertices"]) - 1]
    np.testing.assert_allclose(
        coarse["vertices"], near["vertices"][indices], atol=2e-12, rtol=0
    )
    for name in ("bark_coordinates", "bark_radius"):
        np.testing.assert_allclose(
            coarse[name], near[name][indices], atol=2e-12, rtol=0
        )
    for mesh in (near, coarse):
        np.testing.assert_allclose(
            np.linalg.norm(mesh["normals"], axis=1), 1, atol=2e-14
        )
        vertices = mesh["vertices"][mesh["faces"]]
        assert (
            np.linalg.norm(
                np.cross(
                    vertices[:, 1] - vertices[:, 0], vertices[:, 2] - vertices[:, 0]
                ),
                axis=1,
            ).min()
            > 1e-12
        )
        edges = np.sort(
            np.vstack(
                (
                    mesh["faces"][:, [0, 1]],
                    mesh["faces"][:, [1, 2]],
                    mesh["faces"][:, [2, 0]],
                )
            ),
            axis=1,
        )
        _, counts = np.unique(edges, axis=0, return_counts=True)
        assert (counts == 2).all()
    return {
        "length_m": exact,
        "length_error_m": length_error,
        "arc_location_error_m": arc_error,
        "frame_vector_error": float(frame_error),
        "shared_vertices": len(indices),
    }


def check_normals(axis):
    u = np.linspace(0.017, 0.983, 37)
    angle = np.full(len(u), 0.731)
    h = 1e-6
    longitudinal = (point(axis, u + h, angle) - point(axis, u - h, angle)) / (2 * h)
    angular = (point(axis, u, angle + h) - point(axis, u, angle - h)) / (2 * h)
    reference = np.cross(longitudinal, angular)
    reference /= np.linalg.norm(reference, axis=1)[:, None]
    s = axis.field.sample(u)
    frames = np.stack((s["lateral"], s["tangent"], s["normal"]), axis=1)
    _, normals = sweep_stations(
        s["centre"],
        s["tangent"],
        s["speed"],
        s["curvature"],
        frames,
        axis.radius.evaluate(u),
        axis.radius.derivative(u) * s["arc_speed"] / axis.length,
        np.asarray(axis.ellipse),
        s["tau"],
        np.array([0.731]),
    )
    error = np.linalg.norm(normals[:, 0] - reference, axis=1).max()
    assert error < 2e-7, error
    return float(error)


def check_graph():
    graph = spatial_graph()
    cap = sum(graph.capacities.values())
    assert graph.instances(3, cap - 1) is None and not graph._poses
    leaves = graph.instances(3, cap)
    assert leaves and graph.instances(3, 0) == leaves
    parent = graph.instances(1, 0)[0]
    branches = graph.instances(2, 0)
    sites = graph._sites[(parent.path, 2)]
    assert len(branches) == len(sites) > 0
    for pose, site in zip(branches, sites):
        expected = parent.origin + parent.frame @ (
            graph.by_id[1].shape.field.sample(site[0])["centre"] * parent.scale
        )
        np.testing.assert_allclose(pose.anchor, expected, atol=2e-14)
    restored = type(graph).from_recipe(graph.recipe())
    assert restored.identity == graph.identity
    for a, b in zip(leaves, restored.instances(3, cap)):
        np.testing.assert_array_equal(a.origin, b.origin)
        np.testing.assert_array_equal(a.frame, b.frame)
    return graph.summary()


def check_all():
    axes = {
        name: spatial_axis(name) for name in ("straight", "inflection", "space", "roll")
    }
    result = {
        name: check_axis(axis) | {"normal_error": check_normals(axis)}
        for name, axis in axes.items()
    }
    return {"axes": result, "graph": check_graph()}
