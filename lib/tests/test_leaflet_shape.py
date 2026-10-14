"""Stable local leaflet profiles, analytical source bounds and matched prototypes."""

import json
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import Blade, LeafletShape, PinnatePattern, Profile


def shaped(seed=17):
    return PinnatePattern(
        seed=seed,
        pairs=6,
        leaflet_shape=LeafletShape(
            Profile(np.linspace(-0.3, 0.6, 4)),
            Profile(np.full(4, 0.16)),
            Profile(np.linspace(-0.3, 0.4, 4)),
            (0.08, 0.03, 0.09),
        ),
    )


class LocalLeaflets(unittest.TestCase):
    def test_recipe_default_compatibility_and_stable_reversed_shape_requests(self):
        self.assertEqual(PinnatePattern().recipe()["version"], "pinnate-source-1")
        plan = shaped(2**64 - 1)
        restored = PinnatePattern.from_recipe(json.loads(json.dumps(plan.recipe())))
        self.assertEqual(plan.identity, restored.identity)
        expected = plan.mesh(24, 2)
        actual = restored.mesh(24, 2)
        for name in expected:
            np.testing.assert_array_equal(expected[name], actual[name])
        shape = plan.leaflet_shape
        coordinates = np.linspace(0, 1, 13)
        samples = [shape.sample(t, plan.seed, i) for i, t in enumerate(coordinates)]
        for i in reversed(range(13)):
            self.assertEqual(samples[i], shape.sample(coordinates[i], plan.seed, i))
        self.assertNotEqual(samples, [shape.sample(i / 12, 0, i) for i in range(13)])

    def test_dense_independent_surface_sampling_stays_inside_guide_and_anchored(self):
        plan = shaped()
        for organ in plan.organs():
            b, f, t = organ["shape"]
            blade = Blade(
                length=organ["length"],
                width=organ["width"],
                widest=0.35,
                lobes=0,
                base_fill=0.08,
                bend=b,
                fold=f,
                twist=t,
                petiole=0,
            )
            u, v = np.meshgrid(np.linspace(0, 1, 97), np.linspace(-1, 1, 17))
            positions = (
                blade.evaluate(u, v, np.where(v < 0, -1, 1))[0] @ organ["frame"].T
                + organ["origin"]
            )
            self.assertGreaterEqual(float(positions[..., 1].min()), -1e-15)
            self.assertLessEqual(float(positions[..., 1].max()), plan.length + 1e-15)
            self.assertLessEqual(
                float(np.abs(positions[..., (0, 2)]).max()), plan.source_extent
            )
            root = blade.evaluate(0, 0, 1)[0] @ organ["frame"].T + organ["origin"]
            np.testing.assert_array_equal(root, organ["origin"])
        a = plan.mesh(48, 4).get("vertices").reshape(13, -1, 3)
        b = plan.mesh(12, 2).get("vertices").reshape(13, -1, 3)
        common = a[:, :-1].reshape(13, 48, 9, 3)[:, ::4, ::2].reshape(13, -1, 3)
        np.testing.assert_array_equal(common, b[:, :-1])
        np.testing.assert_array_equal(a[:, -1], b[:, -1])

    def test_shared_and_individual_prototypes_are_complete_exact_and_use_less_work(
        self,
    ):
        fixed = PinnatePattern(
            pairs=6,
            leaflet_shape=LeafletShape(
                Profile(np.full(4, 0.4)),
                Profile(np.full(4, 0.2)),
                Profile(np.full(4, 0.25)),
            ),
        )
        original = Blade.mesh
        calls = []

        def counted(blade, *args, **kwargs):
            calls.append(1)
            return original(blade, *args, **kwargs)

        with patch.object(Blade, "mesh", counted):
            a = fixed.mesh(24, 2)
            shared = len(calls)
            calls.clear()
            b = fixed.mesh(24, 2, method="individual")
            individual = len(calls)
        self.assertEqual(shared, 1)
        self.assertEqual(individual, 13)
        for name in a:
            np.testing.assert_array_equal(a[name], b[name])
        variable = shaped()
        a, b = variable.mesh(24, 2), variable.mesh(24, 2, method="individual")
        for name in a:
            np.testing.assert_array_equal(a[name], b[name])

    def test_budget_checks_precede_organ_and_prototype_work(self):
        plan = shaped()
        with (
            patch.object(
                PinnatePattern, "organs", side_effect=AssertionError("organs")
            ),
            patch.object(Blade, "mesh", side_effect=AssertionError("prototype")),
        ):
            self.assertIsNone(plan.mesh(24, 2, 0))
            self.assertIsNone(replace(plan, pairs=64).mesh(1024, 32, 2000000))

    def test_profile_variation_regularity_and_invalid_methods_refuse(self):
        for args in (
            {"variance": (0.1, -0.1, 0.0)},
            {"variance": (True, 0.0, 0.0)},
            {"bend": Profile(np.full(4, 4.0))},
            {"fold": Profile(np.full(4, 3.0))},
        ):
            with self.assertRaises(ValueError):
                LeafletShape(**args)
        with self.assertRaises(ValueError):
            replace(shaped(), aspect=3.0)
        with self.assertRaises(ValueError):
            shaped().mesh(method="unknown")
        with self.assertRaises(ValueError):
            LeafletShape().sample(0.5, 0, 129)
        with self.assertRaises(ValueError):
            LeafletShape.from_recipe({"version": "unknown"})


if __name__ == "__main__":
    unittest.main()
