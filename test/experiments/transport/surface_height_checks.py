"""Independent normal-based grade checks and constrained-profile counterexamples."""

import unittest

import numpy as np

from surface_height_methods import checked_fit
from surface_height_frames import frames_for_surface


def normal_gradients(xy, faces, heights):
    vertices = np.c_[xy, heights][faces]
    normal = np.cross(vertices[:, 1] - vertices[:, 0], vertices[:, 2] - vertices[:, 0])
    return -normal[:, :2] / normal[:, 2:3]


class SurfaceHeights(unittest.TestCase):
    def setUp(self):
        self.xy = np.array([[0., 0.], [1, 0], [0, 1]])
        self.faces = np.array([[0, 1, 2]])

    def test_edge_only_bound_is_insufficient_and_cone_matches_analytic_case(self):
        target = np.array([0., .08, .08])
        for a, b in ((0, 1), (0, 2), (1, 2)):
            self.assertLessEqual(abs(target[b] - target[a]) / np.linalg.norm(self.xy[b] - self.xy[a]), .08)
        self.assertGreater(float(np.linalg.norm(normal_gradients(self.xy, self.faces, target))), .08)
        result, report = checked_fit("cone", self.xy, self.faces, target)
        self.assertTrue(report["valid"])
        expected = (.08 - .08 / np.sqrt(2)) * .5
        self.assertAlmostEqual(np.max(abs(result - target)), expected, places=7)
        self.assertLessEqual(float(np.linalg.norm(normal_gradients(self.xy, self.faces, result))), .08000001)
        np.testing.assert_array_equal(target, [0., .08, .08])

    def test_anisotropic_bounds_and_polygon_reference(self):
        frames = np.array([[[1 / .08, 0.], [0., 1 / .025]]])
        target = np.array([0., .04, .04])
        for method in ("cone", "polygon"):
            result, report = checked_fit(method, self.xy, self.faces, target, frames=frames)
            self.assertTrue(report["valid"])
            gradient = normal_gradients(self.xy, self.faces, result)
            self.assertLessEqual(np.linalg.norm(frames[0] @ gradient[0]), 1.0000001)
            self.assertLessEqual(abs(gradient[0, 1]), .02500001)

    def test_contacts_and_infeasible_contacts(self):
        target = np.array([0., .04, .01])
        result, report = checked_fit("cone", self.xy, self.faces, target, pins={0: 0.})
        self.assertTrue(report["valid"])
        self.assertAlmostEqual(result[0], 0., places=7)
        result, report = checked_fit("cone", self.xy, self.faces, target, pins={0: 0., 1: 2.})
        self.assertIsNone(result)
        self.assertFalse(report["valid"])

    def test_tiny_deadline_yields_no_unchecked_surface(self):
        result, report = checked_fit("cone", self.xy, self.faces, np.array([0., 1., 1.]), seconds=1e-12)
        if result is None:
            self.assertFalse(report["valid"])
        else:
            self.assertTrue(report["valid"])
            self.assertLessEqual(float(np.linalg.norm(normal_gradients(self.xy, self.faces, result))), .08000001)
            self.assertEqual(report["solver_status"], "MaxTime")

    def test_uncertain_source_direction_uses_safe_local_recipe(self):
        xy = np.array([[-.01, -.01], [.01, -.01], [0., .02]])
        raw = dict(roads=[dict(source_id="tiny-loop", coordinates=[[0., 0.], [.02, 0.], [0., 0.]],
                               properties={"class": "minor"})])
        frames, report = frames_for_surface(xy, self.faces, np.array([0]), raw, {"tiny-loop"})
        np.testing.assert_array_equal(frames[0], np.eye(2) / .025)
        self.assertEqual(report["isotropic_uncertain_faces"], [0])
        self.assertEqual(report["source_ids"], ["tiny-loop"])
        self.assertEqual(raw["roads"][0]["coordinates"], [[0., 0.], [.02, 0.], [0., 0.]])


if __name__ == "__main__":
    unittest.main()
