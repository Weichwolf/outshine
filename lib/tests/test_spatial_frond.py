"""Independent derivatives, metric spine coordinates, normals and request boundaries."""

import json
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import (
    CubicSpine,
    FrondGuide,
    LeafletShape,
    PinnatePattern,
    Profile,
    SpatialFrondGuide,
)
from scipy.optimize import brentq


def guide(intervals=16):
    spine = CubicSpine(
        (((0, 0, 0), (0, 0.35, 0), (0.12, 0.85, 0.07), (-0.03, 1.3, 0.14)),)
    )
    return SpatialFrondGuide(
        spine,
        width=Profile(np.linspace(1, 0.8, 4)),
        height=Profile(np.linspace(0.9, 1.1, 4)),
        roll=Profile(np.linspace(-0.1, 0.7, 4)),
        fold_per_m=0.2,
        intervals_per_segment=intervals,
    )


class SpatialFrondTests(unittest.TestCase):
    def test_jacobian_determinant_and_independent_arc_inverse(self):
        for intervals in (2, 16):
            g = guide(intervals)
            np.testing.assert_array_equal(g.measure.fraction(np.array((0, 1))), (0, 1))
            rng = np.random.default_rng(87)
            points = rng.uniform(
                (-0.025, 0.02, -0.012), (0.025, g.length - 0.02, 0.012), (97, 3)
            )
            _, jacobian = g.evaluate(points)
            h = 1e-7
            reference = np.stack(
                [
                    (g.evaluate(points + h * row)[0] - g.evaluate(points - h * row)[0])
                    / (2 * h)
                    for row in np.eye(3)
                ],
                axis=-1,
            )
            np.testing.assert_allclose(jacobian, reference, rtol=0, atol=1e-8)
            sample = g.field.sample(points[:, 1] / g.length)
            q = points[:, 0] * g.width.evaluate(points[:, 1] / g.length)
            d = (
                points[:, 2] * g.height.evaluate(points[:, 1] / g.length)
                + g.fold_per_m * q * q
            )
            factor = 1 - np.sum(
                sample["curvature"]
                * (q[:, None] * sample["lateral"] + d[:, None] * sample["normal"]),
                axis=1,
            )
            expected = (
                g.width.evaluate(points[:, 1] / g.length)
                * g.height.evaluate(points[:, 1] / g.length)
                * sample["speed"]
                / sample["arc_speed"]
                * factor
            )
            np.testing.assert_allclose(np.linalg.det(jacobian), expected, rtol=1e-14)
            self.assertGreater(float(expected.min()), 0)
            fractions = np.linspace(0, 1, 17)
            parameters = [
                brentq(
                    lambda t, g=g, f=f: g.measure.evaluate(t) - f * g.length,
                    0,
                    1,
                    xtol=1e-14,
                )
                for f in fractions
            ]
            spine_points = g.evaluate(
                np.column_stack((fractions * 0, fractions * g.length, fractions * 0))
            )[0]
            np.testing.assert_allclose(
                spine_points, g.spine.evaluate(parameters), atol=3e-14
            )

    def test_straight_guide_matches_circle_and_origin(self):
        g = SpatialFrondGuide(
            CubicSpine((((0, 0, 0), (0, 0.4, 0), (0, 0.9, 0), (0, 1.3, 0)),))
        )
        p = PinnatePattern(length=g.length)
        source = p.mesh(24, 2)
        a = g.deform(source)
        b = FrondGuide(length=g.length, bend=0, fold_per_m=0).deform(source)
        for key in ("vertices", "normals"):
            np.testing.assert_allclose(a[key], b[key], atol=1e-14)
        np.testing.assert_array_equal(g.evaluate(np.zeros((1, 3)))[0], np.zeros((1, 3)))

    def test_connected_segments_preserve_joint_and_have_regular_surfaces(self):
        spine = CubicSpine.from_hermite(
            ((0, 0, 0), (0.04, 0.65, 0.05), (0.09, 1.3, -0.04)),
            ((0, 1.3, 0), (0.12, 1.3, 0.03), (0.08, 1.3, -0.12)),
        )
        g = SpatialFrondGuide(spine)
        fraction = g.measure.fraction(0.5)
        point = np.array(((0, fraction * g.length, 0),))
        np.testing.assert_allclose(
            g.evaluate(point)[0][0], spine.evaluate(0.5), atol=1e-14
        )
        points = np.tile((0.02, 0, 0.01), (97, 1))
        points[:, 1] = np.linspace(0.02, g.length - 0.02, len(points))
        _, jacobian = g.evaluate(points)
        h = 1e-7
        numerical = np.stack(
            [
                (g.evaluate(points + h * row)[0] - g.evaluate(points - h * row)[0])
                / (2 * h)
                for row in np.eye(3)
            ],
            axis=-1,
        )
        np.testing.assert_allclose(jacobian, numerical, rtol=0, atol=1e-8)
        self.assertGreater(float(np.linalg.det(jacobian).min()), 0)
        p = PinnatePattern(length=g.length, leaflet_length=0.12)
        self.assertEqual(len(g.deform(p.mesh(24, 2))["faces"]), 6204)

    def test_owned_mesh_normals_anchors_common_points_and_recipe(self):
        g = guide()
        p = PinnatePattern(
            length=g.length,
            pairs=6,
            leaflet_length=0.16,
            leaflet_shape=LeafletShape(variance=(0.1, 0.03, 0.1)),
        )
        source = p.mesh(24, 2)
        a = g.deform(source)
        flat = source["faces"].ravel()
        j = g.evaluate(source["vertices"])[1][flat]
        normals = np.linalg.solve(j.transpose(0, 2, 1), source["normals"][..., None])[
            ..., 0
        ]
        normals /= np.linalg.norm(normals, axis=1, keepdims=True)
        np.testing.assert_allclose(a["normals"], normals, atol=1e-14)
        for key in source:
            self.assertFalse(np.shares_memory(source[key], a[key]))
            if key not in ("vertices", "normals"):
                np.testing.assert_array_equal(source[key], a[key])
        loaded = SpatialFrondGuide.from_recipe(json.loads(json.dumps(g.recipe())))
        self.assertEqual(loaded.identity, g.identity)
        for key, value in loaded.deform(source).items():
            np.testing.assert_array_equal(value, a[key])
        coarse = g.deform(p.mesh(12, 1))
        fine = a["vertices"].reshape(13, -1, 3)
        common = fine[:, :-1].reshape(13, 24, 5, 3)[:, ::2, ::2].reshape(13, -1, 3)
        np.testing.assert_array_equal(
            common, coarse["vertices"].reshape(13, -1, 3)[:, :-1]
        )
        origins = np.array([o["origin"] for o in p.organs()])
        centres = g.field.sample(origins[:, 1] / g.length)["centre"]
        np.testing.assert_array_equal(g.evaluate(origins)[0], centres)

    def test_source_invalid_or_budget_precedes_field_sampling(self):
        g = guide()
        p = PinnatePattern(length=g.length, pairs=1, leaflet_length=0.1)
        m = p.mesh(4, 1)
        for vertices in (
            m["vertices"].astype(complex),
            m["vertices"] + np.array((0, g.length, 0)),
            np.broadcast_to((0, 0, 0), (1_000_001, 3)),
        ):
            with (
                patch.object(
                    type(g.field), "sample", side_effect=AssertionError("field work")
                ),
                self.assertRaises(ValueError),
            ):
                g.deform(m | {"vertices": vertices})
        for args in (
            {"width": Profile(np.zeros(4))},
            {"fold_per_m": np.nan},
            {"intervals_per_segment": 0},
        ):
            with self.assertRaises(ValueError):
                replace(g, **args)
        with self.assertRaises(ValueError):
            g.evaluate(np.array(((10, g.length / 2, 10),)))
        with self.assertRaises(ValueError):
            g.deform(m, method="undeclared")


if __name__ == "__main__":
    unittest.main()
