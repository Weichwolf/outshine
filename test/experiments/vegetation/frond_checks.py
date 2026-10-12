"""Independent frond positions, Jacobians, normals and stable source anchors."""

from dataclasses import replace
from unittest.mock import patch

import numpy as np
from frond_pattern import PinnatePattern
from frond_surface import FrondGuide
from profile_curve import Profile
from scipy.spatial.transform import Rotation


def guide_cases():
    t = np.linspace(0, 1, 1000)
    return (
        FrondGuide(bend=0, fold_per_m=0),
        FrondGuide(bend=0, fold_per_m=1.8),
        FrondGuide(bend=-0.85, fold_per_m=1.2, roll=Profile(0.7 * t)),
        FrondGuide(
            bend=0.55,
            fold_per_m=-0.6,
            width=Profile(0.7 + 0.4 * np.sin(np.pi * t)),
            height=Profile(1 - 0.4 * t),
            roll=Profile(-0.5 + 0.8 * t),
        ),
    )


def check_differentials(guide):
    rng = np.random.default_rng(711)
    points = rng.uniform((-0.2, 0.04, -0.02), (0.2, guide.length - 0.04, 0.02), (97, 3))
    t = points[:, 1] / guide.length
    angle = t * guide.bend
    centre = (
        np.column_stack((t * 0, points[:, 1], t * 0))
        if guide.bend == 0
        else np.column_stack(
            (
                t * 0,
                guide.length / guide.bend * np.sin(angle),
                guide.length / guide.bend * (1 - np.cos(angle)),
            )
        )
    )
    frame = (
        Rotation.from_rotvec(np.column_stack((angle, t * 0, t * 0)))
        * Rotation.from_rotvec(np.column_stack((t * 0, -guide.roll.evaluate(t), t * 0)))
    ).as_matrix()
    transverse = points[:, 0] * guide.width.evaluate(t)
    depth = points[:, 2] * guide.height.evaluate(t) + guide.fold_per_m * transverse**2
    reference = centre + np.einsum(
        "nij,nj->ni", frame, np.column_stack((transverse, t * 0, depth))
    )
    actual, jacobian = guide.evaluate(points)
    np.testing.assert_allclose(actual, reference, atol=1e-14)
    step = 1e-7
    difference = np.stack(
        [
            (
                guide.evaluate(points + np.eye(3)[axis] * step)[0]
                - guide.evaluate(points - np.eye(3)[axis] * step)[0]
            )
            / (2 * step)
            for axis in range(3)
        ],
        axis=-1,
    )
    error = float(np.max(abs(jacobian - difference)))
    assert error < 1e-8, error
    assert np.linalg.det(jacobian).min() > 0
    return error


def check_mesh(guide, source):
    batch, scalar = guide.deform(source), guide.deform(source, "scalar")
    for key in source:
        np.testing.assert_array_equal(batch[key], scalar[key])
    for key in source.keys() - {"vertices", "normals"}:
        np.testing.assert_array_equal(batch[key], source[key])
    indices = source["faces"].ravel()[::19]
    jacobian = guide.evaluate(source["vertices"][indices])[1]
    independent = np.linalg.solve(
        jacobian.transpose(0, 2, 1), source["normals"][::19, :, None]
    )[..., 0]
    independent /= np.linalg.norm(independent, axis=1, keepdims=True)
    np.testing.assert_allclose(batch["normals"][::19], independent, atol=1e-14)
    vertices = batch["vertices"][batch["faces"]]
    geometry = np.cross(
        vertices[:, 1] - vertices[:, 0], vertices[:, 2] - vertices[:, 0]
    )
    assert np.linalg.norm(geometry, axis=1).min() > 0
    assert (
        np.sum(geometry[:, None] * batch["normals"].reshape(-1, 3, 3), axis=-1).min()
        > 0
    )
    assert FrondGuide.from_recipe(guide.recipe()).identity == guide.identity
    return batch


def check_stability():
    pattern = PinnatePattern()
    coarse, fine = pattern.mesh(12, 1), pattern.mesh(24, 2)
    lookup = {
        (int(i), *p): v
        for i, p, v in zip(fine["organ_ids"], fine["parameters"], fine["vertices"])
    }
    for identity, parameter, vertex in zip(
        coarse["organ_ids"], coarse["parameters"], coarse["vertices"]
    ):
        np.testing.assert_array_equal(vertex, lookup[(int(identity), *parameter)])
    assert pattern.mesh(1024, 32, 0) is None
    with patch.object(
        PinnatePattern,
        "organs",
        side_effect=AssertionError("unexpected detail expansion"),
    ):
        assert PinnatePattern(pairs=64).mesh(1024, 32, 100_000_000) is None
        assert PinnatePattern(pairs=60).mesh(1024, 4, 100_000_000) is None
    anchors = np.array([o["origin"] for o in pattern.organs()])
    for guide in guide_cases():
        guide.validate(anchors)
        source = guide.deform(coarse)
        for identity, origin in enumerate(anchors):
            root = (
                (source["organ_ids"] == identity)
                & (source["parameters"][:, 0] == 0)
                & (source["parameters"][:, 1] == 0)
            )
            np.testing.assert_allclose(
                source["vertices"][root][0],
                guide.evaluate(origin[None])[0][0],
                atol=1e-14,
            )
    for guide in (replace(guide_cases()[0], bend=np.pi),):
        try:
            guide.validate(np.array([[guide.length, 0.5, 0]]))
        except ValueError:
            pass
        else:
            raise AssertionError("singular guide support accepted")
    return {
        "pinnae": len(anchors),
        "coarse_triangles": len(coarse["faces"]),
        "fine_triangles": len(fine["faces"]),
    }
