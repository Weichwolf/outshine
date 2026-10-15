"""Terminal foliage keeps its actual woody parents and prepared area budgets."""

import unittest

import numpy as np
from biome_models import Assembly, plant_mesh, woody_plan
from broadleaf_crowns import architecture, match_area


class CrownChecks(unittest.TestCase):
    def test_area_preparation_builds_no_axes_or_organ_poses(self):
        for key in ("oak", "beech"):
            terminal = match_area(architecture(key, 718, True), 55)
            area = terminal.summary()["leaf_area_m2"]
            control = match_area(architecture(key, 718, False), area)
            self.assertLess(abs(control.summary()["leaf_area_m2"] / area - 1), 0.0025)
            self.assertLess(abs(area / 55 - 1), 0.01)
            for plant in (control, terminal):
                self.assertFalse(plant._axes or plant._leaves)

    def test_new_terminal_order_keeps_the_entire_existing_woody_skeleton(self):
        old, new = architecture("oak", 718), architecture("oak", 718, True)
        for path in old.paths():
            a, b = old.axis(path), new.axis(path)
            self.assertEqual(a.id, b.id)
            np.testing.assert_array_equal(a.origin, b.origin)
            np.testing.assert_array_equal(a.frame, b.frame)
            self.assertEqual(a.length, b.length)

    def test_terminal_partial_query_is_exactly_the_same_as_full_expansion(self):
        plant = match_area(architecture("beech", 718, True), 18)
        path = tuple(plant.paths())[-1]
        first = plant.leaves(path, 100000)
        for other in reversed(tuple(plant.paths())[::29]):
            plant.leaves(other, 100000)
        tables = plant.tables(10000, 100000)
        selected = tables["leaf_axis_ids"] == plant.axis(path).id
        for key in ("ids", "origins", "frames", "variants"):
            np.testing.assert_array_equal(first[key], tables["leaf_" + key][selected])
        np.testing.assert_allclose(np.linalg.det(first["frames"]), 1, atol=2e-14)

    def test_leaf_parameters_survive_into_the_shared_render_product_uvs(self):
        plan = woody_plan("oak", 718)
        mesh = plan.blades[0].mesh(8)
        assembly = Assembly()
        assembly.add(mesh, np.zeros((1, 3)), np.eye(3)[None], 1)
        product = assembly.finish()
        p = mesh["parameters"][mesh["faces"].ravel()]
        np.testing.assert_array_equal(product["uv"], np.c_[(p[:, 1] + 1) / 2, p[:, 0]])
        self.assertGreater(np.ptp(product["uv"][:, 0]), 0.9)
        self.assertGreater(np.ptp(product["uv"][:, 1]), 0.9)
        self.assertEqual(plant_mesh(plan)["uv"].shape[1], 2)


if __name__ == "__main__":
    unittest.main()
