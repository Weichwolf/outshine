"""Independent circle/helix/planar solutions, frame covariance and force replay."""

import json
import unittest
from dataclasses import replace

import numpy as np
from outshine_ref.vegetation import (
    CurlField,
    DirectionField,
    GnarlField,
    GrowthSpine,
    PlanarField,
    Profile,
    RadialFalloff,
    ReturnField,
)
from outshine_ref.vegetation.scalar_forces import compile_fields, scalar_force
from scipy.spatial.transform import Rotation


def rate(value):
    return Profile(np.full(4, value))


def helix(axis, initial, distance, strength):
    axis = np.asarray(axis, dtype=float)
    axis /= np.linalg.norm(axis)
    parallel = np.dot(initial, axis) * axis
    perpendicular = initial - parallel
    angle = distance * strength
    return (
        distance[:, None] * parallel
        + (distance * np.sinc(angle / np.pi))[:, None] * perpendicular
        + (distance * angle / 2 * np.sinc(angle / (2 * np.pi)) ** 2)[:, None]
        * np.cross(axis, perpendicular)
    )


class TurningFieldTests(unittest.TestCase):
    def test_fixed_gnarl_helix_and_moving_curl_circle_independently(self):
        axis = np.array((1, 1, 0.4), dtype=float)
        axis /= np.linalg.norm(axis)
        initial = np.array((0, 1, 0))
        t = np.linspace(0, 1, 33)
        distance = t * 1.3
        g = GrowthSpine(1.3, (GnarlField(tuple(axis), rate(0.8)),)).prepare()
        reference = helix(axis, initial, distance, 0.8)
        np.testing.assert_allclose(g.spine.evaluate(t), reference, rtol=0, atol=2e-9)
        tangent = Rotation.from_rotvec(distance[:, None] * 0.8 * axis).apply(
            np.broadcast_to(initial, (len(t), 3))
        )
        np.testing.assert_allclose(g.spine.sample(t)[1], tangent, rtol=0, atol=2e-9)
        curl = GrowthSpine(1.3, (CurlField(tuple(axis), rate(0.8)),)).prepare()
        effective = np.array((axis[0], 0, axis[2]))
        reference = helix(effective, initial, distance, 0.8 * np.linalg.norm(effective))
        np.testing.assert_allclose(curl.spine.evaluate(t), reference, rtol=0, atol=2e-9)
        self.assertGreater(
            float(np.linalg.norm(g.spine.evaluate(1) - curl.spine.evaluate(1))), 0.05
        )

    def test_planar_heading_against_closed_normal_component(self):
        normal = np.array((1, 0.4, 0), dtype=float)
        normal /= np.linalg.norm(normal)
        p = GrowthSpine(1.3, (PlanarField(tuple(normal), rate(2)),))
        result = p.prepare()
        t = np.linspace(0, 1, 33)
        distance = t * p.length
        c = normal[1]
        expected = c / np.sqrt(c * c + (1 - c * c) * np.exp(4 * distance))
        np.testing.assert_allclose(
            result.spine.sample(t)[1] @ normal, expected, rtol=0, atol=2e-9
        )
        w = c / np.sqrt(1 - c * c)
        depth = (np.arcsinh(w) - np.arcsinh(w * np.exp(-2 * distance))) / 2
        np.testing.assert_allclose(
            result.spine.evaluate(t) @ normal, depth, rtol=0, atol=2e-9
        )

    def test_return_recovers_heading_and_singular_axes_stay_finite(self):
        perturb = DirectionField((0, 0, 1), Profile((1.2, 0.8, 0, 0)))
        a = GrowthSpine(1.3, (perturb,)).prepare()
        b = GrowthSpine(1.3, (perturb, ReturnField(Profile((0, 0, 2, 2))))).prepare()
        self.assertGreater(b.spine.sample(1)[1][1], a.spine.sample(1)[1][1] + 0.1)
        for f in (
            CurlField((0, 1, 0), rate(1)),
            GnarlField((0, 1, 0), rate(1)),
            PlanarField((0, 1, 0), rate(1)),
            ReturnField(rate(1)),
        ):
            p = GrowthSpine(1.3, (f,)).prepare()
            np.testing.assert_allclose(p.spine.evaluate(1), (0, 1.3, 0), atol=1e-14)

    def test_scalar_and_batch_fields_match_for_arbitrary_frames(self):
        rng = np.random.default_rng(49)
        frames = Rotation.from_rotvec(rng.normal(0, 0.8, (100, 3))).as_matrix()
        headings, lateral = frames[:, :, 1], frames[:, :, 0]
        points = rng.uniform(-1, 1, (100, 3))
        fraction = rng.uniform(0, 1, 100)
        for mode in ("linear", "squared", "step"):
            fall = RadialFalloff((0.1, -0.1, 0.1), 0.2, 1.2, mode)
            p = GrowthSpine(
                1.3,
                (
                    CurlField((0.3, 0.5, 0.1), rate(0.2), fall),
                    GnarlField((0.5, 0.1, 0.4), rate(0.3)),
                    PlanarField((1, 0.3, 0.2), rate(0.1)),
                    ReturnField(rate(0.2)),
                ),
                initial_reference=(1, 0, 0),
            )
            compiled = compile_fields(p.fields, p.initial_tangent)
            actual = np.array(
                [
                    scalar_force(compiled, *point, f, *h, a)
                    for point, f, h, a in zip(
                        points, fraction, headings, lateral, strict=True
                    )
                ]
            )
            np.testing.assert_allclose(
                actual,
                p.evaluate(points, fraction, headings, lateral),
                rtol=0,
                atol=1e-14,
            )

    def test_coupled_frames_are_covariant_and_v2_replay_is_exact(self):
        p = GrowthSpine(
            1.3,
            (
                CurlField((1, 0.3, 0.1), rate(0.5)),
                GnarlField((0.2, 1, 0.3), rate(0.2)),
                ReturnField(rate(0.15)),
            ),
            initial_reference=(1, 0, 0.2),
        )
        a = p.prepare()
        q = GrowthSpine.from_recipe(json.loads(json.dumps(p.recipe())))
        self.assertEqual(p.identity, q.identity)
        np.testing.assert_array_equal(a.spine.controls, q.prepare().spine.controls)
        rotation = Rotation.from_rotvec((0.2, -0.4, 0.3)).as_matrix()
        moved = replace(
            p,
            initial_direction=tuple(rotation @ p.initial_tangent),
            initial_reference=tuple(rotation @ p.initial_reference),
            fields=(
                p.fields[0],
                replace(p.fields[1], axis=tuple(rotation @ p.fields[1].axis)),
                p.fields[2],
            ),
        )
        np.testing.assert_allclose(
            np.array(moved.prepare().spine.controls),
            np.array(a.spine.controls) @ rotation.T,
            rtol=0,
            atol=2e-9,
        )

    def test_missing_frame_bad_recipe_and_budget_are_explicit(self):
        p = GrowthSpine(1.3, (CurlField((1, 0, 0), rate(0.5)),))
        with self.assertRaises(ValueError):
            p.evaluate(np.zeros((1, 3)), 0.5)
        with self.assertRaises(ValueError):
            replace(p, initial_reference=(0, 1, 0))
        with self.assertRaises(ValueError):
            GrowthSpine.from_recipe(p.recipe() | {"version": "growth-spine-1"})
        self.assertIsNone(p.prepare(max_evaluations=1))
        self.assertIsNone(p.prepare(stations=2))


if __name__ == "__main__":
    unittest.main()
