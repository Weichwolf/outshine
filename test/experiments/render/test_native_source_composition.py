"""Sparse native readbacks must retain clear samples and exact World binding."""

import copy
import unittest

import numpy as np
from compose_native_source_materials import bound_source, source_points


class NativeSourceComposition(unittest.TestCase):
    def setUp(self):
        self.pose = "near-direction"
        self.crop = [3, 5, 6, 7]
        self.samples = {self.pose + "/pixel_xy": np.array([[3, 5], [5, 6]], "<i4")}
        for mode in ("whole", "selected"):
            self.samples[self.pose + "/" + mode + "_rgba"] = np.array(
                [[2, 1, 0, 1], [0, 3, 1, 1]], "<f4"
            )
            self.samples[self.pose + "/" + mode + "_depth"] = np.array(
                [0.5, 0.25], "<f4"
            )
        self.samples[self.pose + "/selected_rgba"][1] = 0
        self.samples[self.pose + "/selected_depth"][1] = 0

    def test_missing_candidate_stays_clear_at_the_global_sample_phase(self):
        model = source_points(self.samples, self.pose, "selected", self.crop)
        self.assertEqual(model.rgba.shape, (2, 3, 4))
        np.testing.assert_array_equal(model.rgba[0, 0], [2, 1, 0, 1])
        self.assertEqual(np.count_nonzero(model.reverse_z), 1)
        self.assertTrue(model.evaluated.all())
        self.assertTrue((model.rgba[model.reverse_z == 0] == 0).all())

    def test_ambiguous_or_off_crop_sample_coordinates_are_rejected(self):
        for xy in ([[3, 5], [3, 5]], [[2, 5], [5, 6]], [[3, 5], [6, 6]]):
            with self.subTest(xy=xy), self.assertRaises(ValueError):
                source_points(
                    {**self.samples, self.pose + "/pixel_xy": np.array(xy, "<i4")},
                    self.pose,
                    "whole",
                    self.crop,
                )

    def test_nonfinite_or_inconsistent_depth_does_not_become_coverage(self):
        for bad in (np.nan, -0.1, 1.1, 0.0):
            samples = copy.deepcopy(self.samples)
            samples[self.pose + "/whole_depth"][0] = bad
            with self.subTest(depth=bad), self.assertRaises(ValueError):
                source_points(samples, self.pose, "whole", self.crop)

    def test_changed_camera_light_or_original_pixels_are_rejected(self):
        whole = source_points(self.samples, self.pose, "whole", self.crop)
        original = {"capture": {}, "lighting_camera": {}, "lens_operands": {}}
        cases = {("original", self.pose): original}
        arrays = {
            "original/" + self.pose + "/rgba": whole.rgba,
            "original/" + self.pose + "/depth": whole.reverse_z,
        }
        report = {
            "native_reports": {
                mode: {"cases": [{"query": self.pose, **copy.deepcopy(original)}]}
                for mode in ("whole", "selected")
            }
        }
        bound_source(self.samples, report, arrays, cases, self.pose, self.crop)
        for key in original:
            bad = copy.deepcopy(report)
            bad["native_reports"]["selected"]["cases"][0][key] = {"changed": True}
            with self.subTest(operand=key), self.assertRaises(ValueError):
                bound_source(self.samples, bad, arrays, cases, self.pose, self.crop)
        arrays["original/" + self.pose + "/rgba"][0, 0, 0] += 1
        with self.assertRaises(ValueError):
            bound_source(self.samples, report, arrays, cases, self.pose, self.crop)


if __name__ == "__main__":
    unittest.main()
