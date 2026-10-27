import unittest

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.water import fit_physical_surface


class WaterVertexBounds(unittest.TestCase):
    def setUp(self):
        self.mesh = Mesh(
            np.array([[0, 0, 5], [10, 0, 5], [0, 10, 5]], float), np.array([[0, 1, 2]])
        )
        self.rows = np.array([[0, 1, 0, 0, -0.7, 0.7], [0, 0, 1, 0, -0.7, 0.7]])

    def fit(self, **options):
        return fit_physical_surface(
            self.mesh, [0, 0], self.rows, up_bounds_m=(0, 10), **options
        )

    def test_upper_and_lower_vertex_bounds_are_enforced_and_owned(self):
        for low, high in ((3, 4), (6, 7)):
            bounds = np.tile([low, high], (3, 1)).astype(float)
            original = bounds.copy()
            result = self.fit(vertex_up_bounds_m=bounds)
            self.assertIsNotNone(result.mesh)
            self.assertLessEqual(result.report["vertex_height_box_violation_m"], 1e-9)
            self.assertTrue((result.mesh.vertices[:, 2] >= low - 1e-9).all())
            self.assertTrue((result.mesh.vertices[:, 2] <= high + 1e-9).all())
            np.testing.assert_array_equal(bounds, original)

    def test_none_keeps_the_original_problem_and_explicit_global_boxes_match(self):
        baseline = self.fit()
        none = self.fit(vertex_up_bounds_m=None)
        explicit = self.fit(vertex_up_bounds_m=np.tile([0.0, 10.0], (3, 1)))
        np.testing.assert_array_equal(baseline.mesh.vertices, none.mesh.vertices)
        np.testing.assert_array_equal(baseline.mesh.vertices, explicit.mesh.vertices)
        self.assertNotIn("vertex_height_box_violation_m", baseline.report)

    def test_invalid_or_conflicting_bounds_never_publish_geometry(self):
        for bounds in (
            np.zeros((2, 2)),
            np.tile([4.0, 3.0], (3, 1)),
            np.tile([-1.0, 11.0], (3, 1)),
            np.full((3, 2), np.nan),
        ):
            with self.assertRaises(ValueError):
                self.fit(vertex_up_bounds_m=bounds)
        failed = self.fit(
            vertex_up_bounds_m=np.tile([3.0, 4.0], (3, 1)), fixed={0: 5.0}
        )
        self.assertIsNone(failed.mesh)
        self.assertEqual(failed.report["reason"], "fixed_contact_outside_vertex_bounds")


if __name__ == "__main__":
    unittest.main()
