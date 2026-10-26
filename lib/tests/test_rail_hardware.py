"""Public hardware API ownership, shared assets, independent bounds and errors."""

import unittest

import numpy as np
import shapely
from outshine_ref.transport.rail_hardware import (
    RailBudget,
    RailHardware,
    RailRecipe,
    bake_rail_sleepers,
    build_rail_hardware,
)


class RailHardwareAPI(unittest.TestCase):
    def test_shared_template_and_owned_immutable_products(self):
        axis = np.array([[0.0, 0], [10, 0]])
        first, report = build_rail_hardware(axis, 3.0)
        second, _ = build_rail_hardware([[0, 5], [10, 5]], 3.0)
        self.assertTrue(report["valid"])
        self.assertIs(first.sleeper, second.sleeper)
        self.assertIsInstance(first.footprints, tuple)
        before = first.rails.vertices.copy()
        axis[:] = 1000
        np.testing.assert_array_equal(first.rails.vertices, before)
        for values in (
            first.rails.vertices,
            first.rails.triangles,
            first.sleeper.vertices,
            first.sleeper_transforms,
        ):
            self.assertFalse(values.flags.writeable)
            with self.assertRaises(ValueError):
                values.flat[0] = 42

    def test_product_constructor_does_not_alias_caller_transforms(self):
        source, _ = build_rail_hardware([[0, 0], [2, 0]], 0.0)
        values = source.sleeper_transforms.copy()
        result = RailHardware(source.rails, source.sleeper, values, source.footprints)
        before = result.sleeper_transforms.copy()
        values[:] = 0
        np.testing.assert_array_equal(result.sleeper_transforms, before)

    def test_nondefault_gauge_and_bake_have_actual_closed_dimensions(self):
        product, report = build_rail_hardware(
            [[0, 0], [4, 0]], 6.0, RailRecipe(gauge_m=1.067)
        )
        self.assertTrue(report["valid"])
        vertices = product.rails.vertices
        self.assertAlmostEqual(vertices[:, 2].min(), 6.18)
        self.assertAlmostEqual(vertices[:, 2].max(), 6.352)
        rings = vertices.reshape(2, 2, 12, 3)
        np.testing.assert_allclose(
            rings[1, :, 7, 1] - rings[0, :, 6, 1], 1.067, atol=1e-12
        )
        baked = bake_rail_sleepers(product.sleeper_transforms)
        self.assertEqual(len(baked.triangles), 12 * report["sleepers"])
        self.assertTrue(
            shapely.covers(shapely.box(-1, -2, 5, 2), product.footprints).all()
        )

    def test_budget_failure_has_no_unchecked_partial_product(self):
        result, report = build_rail_hardware(
            [[0, 0], [4, 0]], 0.0, budget=RailBudget(maximum_sleepers=1)
        )
        self.assertIsNone(result)
        self.assertFalse(report["valid"])
        self.assertEqual(report["reason"], "rail_sleeper_budget")

    def test_large_view_is_rejected_before_expansion(self):
        view = np.broadcast_to(np.eye(3, 4), (262145, 3, 4))
        with self.assertRaises(ValueError):
            bake_rail_sleepers(view)
        with self.assertRaises(ValueError):
            bake_rail_sleepers(np.zeros((1, 3, 4)))

    def test_footprints_are_explicit_polygonal_xy_contacts(self):
        product, _ = build_rail_hardware([[0, 0], [2, 0]], 0.0)
        invalid = (shapely.LineString([[0, 0], [1, 1]]),) + product.footprints[1:]
        with self.assertRaises(ValueError):
            RailHardware(
                product.rails, product.sleeper, product.sleeper_transforms, invalid
            )


if __name__ == "__main__":
    unittest.main()
