"""Independent tropism solutions, frame equivariance, replay and bounded preparation."""

import json
import math
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import (
    AttractionField,
    DirectionField,
    GrowthSpine,
    Profile,
    RadialFalloff,
    SpatialFrondGuide,
)
from outshine_ref.vegetation.growth_integrate import constant_samples
from outshine_ref.vegetation.scalar_forces import compile_fields, scalar_force
from scipy.spatial.transform import Rotation


def rate(value):
    return Profile(np.full(4, value))


class GrowthSpineTests(unittest.TestCase):
    def test_constant_direction_against_closed_integrals_and_two_solvers(self):
        initial = np.array((0.0, 1.0, 0.0))
        for strength, cosine in ((0.05, 0.3), (0.8, -0.6), (10.0, -0.99)):
            direction = np.array((math.sqrt(1 - cosine * cosine), cosine, 0))
            vector = strength * direction
            distances = np.linspace(0, 1.3, 33)
            actual, tangents = constant_samples(vector, initial, distances)
            offset = math.atanh(cosine)
            transverse = (initial - cosine * direction) / math.sqrt(1 - cosine * cosine)
            a = (
                np.array(
                    [
                        math.log(math.cosh(offset + strength * s))
                        - math.log(math.cosh(offset))
                        for s in distances
                    ]
                )
                / strength
            )
            b = (
                np.array(
                    [
                        math.atan(math.sinh(offset + strength * s))
                        - math.atan(math.sinh(offset))
                        for s in distances
                    ]
                )
                / strength
            )
            expected = a[:, None] * direction + b[:, None] * transverse
            np.testing.assert_allclose(actual, expected, rtol=0, atol=1e-12)
            np.testing.assert_allclose(np.linalg.norm(tangents, axis=1), 1, atol=1e-14)
            p = GrowthSpine(1.3, (DirectionField(tuple(direction), rate(strength)),))
            analytic = p.prepare(stations=65).spine
            for method in ("DOP853", "RK45"):
                reference = p.prepare(stations=65, method=method).spine
                np.testing.assert_allclose(
                    reference.controls, analytic.controls, atol=2e-9, rtol=0
                )

    def test_zero_parallel_antiparallel_and_tiny_fields(self):
        s = np.linspace(0, 2, 33)
        for direction in ((0, 1, 0), (0, -1, 0), (0, 0, 0)):
            p, t = constant_samples(np.array(direction), np.array((0, 1, 0)), s)
            np.testing.assert_array_equal(p, np.column_stack((s * 0, s, s * 0)))
            np.testing.assert_array_equal(t, np.tile((0, 1, 0), (len(s), 1)))
        p = GrowthSpine(2).prepare()
        np.testing.assert_array_equal(p.spine.evaluate(1), (0, 2, 0))
        result = GrowthSpine(
            2, (DirectionField((1, 0.2, -0.1), rate(1e-14)),)
        ).prepare()
        np.testing.assert_allclose(result.spine.evaluate(1), (0, 2, 0), atol=1e-13)
        self.assertEqual(p.evaluations, 0)

    def test_falloff_modes_and_soft_attraction_at_centre(self):
        points = np.column_stack((np.array((0, 0.2, 0.7, 1.2, 1.3)), np.zeros((5, 2))))
        fall = RadialFalloff((0, 0, 0), 0.2, 1.2)
        np.testing.assert_allclose(fall.weights(points), (1, 1, 0.5, 0, 0))
        np.testing.assert_allclose(
            replace(fall, mode="squared").weights(points), (1, 1, 0.25, 0, 0)
        )
        np.testing.assert_array_equal(
            replace(fall, mode="step").weights(points), (1, 1, 1, 1, 0)
        )
        field = AttractionField((0, 0, 0), rate(1), softening=0.1)
        np.testing.assert_array_equal(
            field.evaluate(np.zeros((1, 3)), 0.5), np.zeros((1, 3))
        )
        h = 1e-7
        positive = field.evaluate(np.eye(3) * h, 0.5)
        negative = field.evaluate(-np.eye(3) * h, 0.5)
        np.testing.assert_allclose(
            (positive - negative) / (2 * h), -np.eye(3) / 0.1, atol=1e-10
        )

    def test_profiles_rigid_rotation_and_differential_equation(self):
        p = GrowthSpine(
            1.3,
            (
                DirectionField((0, -0.3, -1), Profile((0.8, 0.5, -0.1, -0.5))),
                AttractionField((0.25, 1.1, -0.3), rate(0.7)),
            ),
        )
        result = p.prepare()
        g = SpatialFrondGuide(result.spine)
        self.assertGreater(g.length, 1.29)
        t = np.linspace(0.02, 0.98, 97)
        velocity = result.spine.evaluate(t, 1)
        heading = velocity / np.linalg.norm(velocity, axis=1, keepdims=True)
        acceleration = result.spine.evaluate(t, 2)
        projected = (
            acceleration - np.sum(acceleration * heading, axis=1)[:, None] * heading
        ) / np.linalg.norm(velocity, axis=1, keepdims=True) ** 2
        force = p.evaluate(result.spine.evaluate(t), t)
        expected = force - np.sum(force * heading, axis=1)[:, None] * heading
        np.testing.assert_allclose(projected, expected, rtol=0, atol=4e-3)
        rotation = Rotation.from_rotvec((0.2, -0.5, 0.3)).as_matrix()
        fields = tuple(
            replace(
                f,
                **(
                    {"direction": tuple(rotation @ f.direction)}
                    if isinstance(f, DirectionField)
                    else {"target": tuple(rotation @ f.target)}
                ),
            )
            for f in p.fields
        )
        moved = GrowthSpine(
            p.length, fields, tuple(rotation @ np.array((0, 1, 0)))
        ).prepare()
        np.testing.assert_allclose(
            np.array(moved.spine.controls),
            np.array(result.spine.controls) @ rotation.T,
            rtol=0,
            atol=2e-9,
        )

    def test_json_replay_preserves_raw_directions_and_prepared_curve(self):
        fall = RadialFalloff((0.1, 0.2, -0.1), 0.2, 1.2, "squared")
        p = GrowthSpine(
            1.3,
            (
                DirectionField((0.23, 0.11, -0.8), rate(0.5), fall),
                AttractionField((0.2, 1, -0.4), rate(0.9)),
            ),
        )
        a = p.prepare()
        for _ in range(3):
            p = GrowthSpine.from_recipe(json.loads(json.dumps(p.recipe())))
            np.testing.assert_array_equal(p.prepare().spine.controls, a.spine.controls)
        self.assertEqual(p.fields[0].direction, (0.23, 0.11, -0.8))

    def test_budget_refusal_and_invalid_preflight(self):
        p = GrowthSpine(1.3, (DirectionField((0, -0.2, -1), rate(0.8)),))
        with patch(
            "outshine_ref.vegetation.growth_integrate.compile_fields",
            side_effect=AssertionError("field preparation"),
        ):
            self.assertIsNone(p.prepare(max_evaluations=0))
            for args in ({"stations": 1}, {"stations": 66}, {"method": "unknown"}):
                with self.assertRaises(ValueError):
                    p.prepare(**args)
        original = scalar_force
        calls = []

        def counted(*args):
            calls.append(1)
            return original(*args)

        with patch("outshine_ref.vegetation.growth_integrate.scalar_force", counted):
            self.assertIsNone(p.prepare(method="DOP853", max_evaluations=100))
        self.assertEqual(len(calls), 100)
        with patch(
            "outshine_ref.vegetation.growth_integrate.compile_fields",
            side_effect=AssertionError("field preparation"),
        ):
            self.assertIsNone(p.prepare(method="DOP853", max_evaluations=1))
            dense = replace(
                p,
                fields=(
                    DirectionField((1, 0, 0), Profile(np.tile((-0.5, 0.5), 2048))),
                ),
            )
            self.assertIsNone(dense.prepare(method="DOP853"))
        self.assertIsNone(p.prepare(stations=2))
        for args in (
            {"length": 0},
            {"length": 1001},
            {"fields": p.fields * 17},
            {"fields": (DirectionField((1, 0, 0), rate(20)),)},
        ):
            with self.assertRaises(ValueError):
                replace(p, **args)
        with self.assertRaises(ValueError):
            RadialFalloff((0, 0, 0), 1, 1)
        with self.assertRaises(ValueError):
            AttractionField((0, 0, 0), rate(1), softening=0)
        with self.assertRaises(ValueError):
            DirectionField((0, 0, 0), rate(1))

    def test_compiled_scalar_fields_match_independent_public_batch(self):
        rng = np.random.default_rng(43)
        points = rng.uniform(-1, 1, (100, 3))
        fraction = rng.uniform(-0.1, 1.1, len(points))
        for mode in ("linear", "squared", "step"):
            falloff = RadialFalloff((0.1, -0.1, 0.1), 0.2, 1.2, mode)
            for kind in ("custom", "maximum", "minimum", "linear", "inverse_linear"):
                p = GrowthSpine(
                    1.3,
                    (
                        DirectionField(
                            (0.3, 0.1, -1),
                            Profile(
                                {
                                    "custom": (0.8, 0.4, -0.2, 0.5),
                                    "maximum": np.ones(4),
                                    "minimum": np.zeros(4),
                                    "linear": np.linspace(0, 1, 4),
                                    "inverse_linear": np.linspace(1, 0, 4),
                                }[kind],
                                kind,
                            ),
                            falloff,
                        ),
                        AttractionField((0.2, 1, -0.1), rate(0.9)),
                    ),
                )
                compiled = compile_fields(p.fields)
                actual = np.array(
                    [
                        scalar_force(compiled, *point, f)
                        for point, f in zip(points, fraction, strict=True)
                    ]
                )
                np.testing.assert_allclose(
                    actual, p.evaluate(points, fraction), rtol=0, atol=1e-14
                )


if __name__ == "__main__":
    unittest.main()
