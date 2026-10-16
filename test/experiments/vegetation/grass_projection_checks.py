"""Independent geometry/gap/guard/moment counterexamples for #462."""

import unittest
from unittest.mock import patch

import numpy as np
from grass_projection import contacts, footprint_mips, guard_bounds
from grass_projection_instances import instanced_contacts
from grass_projection_reference import continuous_union, verify
from grass_projection_stand import (
    DIRECTIONS,
    LOWER,
    UPPER,
    build_stand,
    require_support,
)


def square(x=-0.25, height=0.2):
    v = np.array(
        (
            (x, -0.25, height),
            (x + 0.5, -0.25, height),
            (x + 0.5, 0.25, height),
            (x, 0.25, height),
        )
    )
    return v, np.array(((0, 1, 2), (0, 2, 3)))


class GrassProjectionChecks(unittest.TestCase):
    def test_instance_bounds_before_expansion_preserve_full_contacts(self):
        recipe, _, stand = build_stand(5)
        for d in DIRECTIONS.values():
            full = contacts(stand["vertices"], stand["faces"], LOWER, UPPER, d, 64)
            actual = instanced_contacts(recipe, stand, LOWER, UPPER, d, 64)
            for key in ("face", "height", "normal", "coverage"):
                np.testing.assert_array_equal(actual[key], full[key])
            self.assertLess(actual["work"]["expanded_triangles"], len(stand["faces"]))
        with patch(
            "grass_projection_instances.expand_selected",
            side_effect=AssertionError("expanded invalid demand"),
        ):
            with self.assertRaises(ValueError):
                instanced_contacts(
                    recipe, stand, LOWER, UPPER, (0, 0, 1), 64, pair_budget=1
                )
            with self.assertRaises(ValueError):
                instanced_contacts(
                    recipe, stand, LOWER + 100, UPPER + 100, (0, 0, 1), 64
                )

    def test_overlap_is_not_summed_area_or_poisson_coverage(self):
        v, f = square()
        direction, lower, upper = (
            np.array((0, 0, 1.0)),
            np.array((-0.5, -0.5)),
            np.array((0.5, 0.5)),
        )
        vv, ff = np.r_[v, v], np.r_[f, f + 4]
        union, stats = continuous_union(vv, ff, lower, upper, direction)
        self.assertEqual(union.area, 0.25)
        self.assertEqual(stats["summed_projected_area_m2"], 0.5)
        self.assertGreater(-np.expm1(-0.5) - union.area, 0.14)
        shifted, _ = square(x=0)
        union2, stats2 = continuous_union(
            np.r_[v, shifted], ff, lower, upper, direction
        )
        self.assertEqual(stats2["summed_projected_area_m2"], 0.5)
        self.assertEqual(union2.area, 0.375)

    def test_first_from_sky_not_first_from_ground(self):
        v, f = square()
        vv, ff = np.r_[v, v + (0, 0, 0.3)], np.r_[f, f + 4]
        product = contacts(vv, ff, (-0.5, -0.5), (0.5, 0.5), (0, 0, 1), 32)
        covered = product["coverage"] > 0
        np.testing.assert_array_equal(product["height"][covered], 0.5)
        self.assertTrue(np.all(product["face"][covered] >= 2))

    def test_ground_clipping_and_double_sided_union(self):
        v = np.array(((0, 0, -1.0), (1, 0, 1.0), (0, 1, 1.0)))
        f = np.array(((0, 1, 2),))
        union, _ = continuous_union(v, f, (0, 0), (1, 1), (0, 0, 1))
        self.assertEqual(union.area, 0.375)
        a = contacts(v, f, (0, 0), (1, 1), (0, 0, 1), 64)
        b = contacts(v, f[:, ::-1], (0, 0), (1, 1), (0, 0, 1), 64)
        np.testing.assert_array_equal(a["coverage"], b["coverage"])
        self.assertTrue(np.all(a["height"] >= 0))

    def test_outside_roots_can_cover_inside_ground(self):
        direction = np.array((np.sqrt(3) / 2, 0, 0.5))
        v, f = square(x=0.7, height=0.5)
        product = contacts(v, f, (-0.5, -0.5), (0.5, 0.5), direction, 64)
        self.assertGreater(product["coverage"].sum(), 0)
        low, high = guard_bounds((-0.5, -0.5), (0.5, 0.5), direction, 0.1, 0.5)
        self.assertLessEqual(low[0], -0.6)
        self.assertGreater(high[0], 1.4)
        recipe = {
            "maximum_radius_m": 0.1,
            "maximum_height_m": 0.5,
            "root_bounds_m": [[-0.5, -0.5], [0.5, 0.5]],
        }
        with self.assertRaises(ValueError):
            require_support(recipe, (-0.5, -0.5), (0.5, 0.5), direction)

    def test_partition_reversal_and_independent_rays(self):
        rng = np.random.Generator(np.random.PCG64(462))
        v, f = rng.uniform(-0.5, 0.5, (60, 3)), np.arange(60).reshape(-1, 3)
        args = ((-0.5, -0.5), (0.5, 0.5), np.array((0, 0, 1)), 32)
        a = contacts(v, f, *args, pair_budget=1024)
        b = contacts(v, f, *args, pair_budget=4096)
        c = contacts(v, f[::-1], *args)
        for key in ("face", "height", "normal", "coverage"):
            np.testing.assert_array_equal(a[key], b[key])
        np.testing.assert_array_equal(a["coverage"], c["coverage"])
        np.testing.assert_array_equal(a["height"], c["height"])
        verify(v, f, np.array(args[0]), np.array(args[1]), args[2], a)

    def test_mips_preserve_area_and_unnormalized_moments(self):
        v, f = square()
        product = contacts(v, f, (-0.5, -0.5), (0.5, 0.5), (0, 0, 1), 32)
        levels = footprint_mips(
            product["coverage"], product["height"], product["normal"]
        )
        for level in levels:
            np.testing.assert_allclose(
                level.mean((0, 1)), (0.25, 0.05, 0, 0, 0.25), atol=1e-15
            )

    def test_empty_and_invalid_inputs_before_fragment_work(self):
        empty = contacts(
            np.empty((0, 3)), np.empty((0, 3), int), (-1, -1), (1, 1), (0, 0, 1), 16
        )
        self.assertEqual(empty["coverage"].sum(), 0)
        self.assertTrue(np.all(empty["face"] == -1))
        v, f = square()
        with patch(
            "grass_projection.fragments", side_effect=AssertionError("invalid work")
        ):
            for direction, size, budget in [
                ((0, 0, -1), 32, 1024),
                ((0, 0, 2), 32, 1024),
                ((0, 0, 1), 31, 1024),
                ((0, 0, 1), 32, 1),
            ]:
                with self.assertRaises(ValueError):
                    contacts(v, f, (-1, -1), (1, 1), direction, size, budget)


if __name__ == "__main__":
    unittest.main()
