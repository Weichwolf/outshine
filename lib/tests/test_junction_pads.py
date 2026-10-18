"""Bounded immutable whole-profile primitive, source-independent geometry controls."""

import unittest

import numpy as np
import shapely
from outshine_ref.transport.junction_pads import profile_junction_pad


class ProfilePrimitive(unittest.TestCase):
    def test_known_two_and_three_arm_profile_hulls_are_complete(self):
        two = np.array([[[0, -1], [0, 0], [0, 1]], [[-2, 0], [0, 0], [2, 0]]], float)
        three = np.concatenate([two, [[[-1, -1], [0, 0], [1, 1]]]])
        four = np.concatenate([three, two[:1]])
        for points, hull_area in ((two, 4), (three, 5), (four, 5)):
            for method, area in (("profile_hull", hull_area), ("profile_box", 8)):
                pad = profile_junction_pad(points, method=method)
                self.assertAlmostEqual(pad.area, area)
                self.assertFalse(pad.interiors)
                for profile in points:
                    self.assertTrue(pad.covers(shapely.LineString(profile)))

    def test_input_mutation_cannot_change_the_owned_polygon(self):
        points = np.array([[[0, -1], [0, 0], [0, 1]], [[-2, 0], [0, 0], [2, 0]]], float)
        original = points.copy()
        pad = profile_junction_pad(points)
        np.testing.assert_array_equal(points, original)
        saved = pad.wkb
        points[:] = 0
        self.assertEqual(pad.wkb, saved)

    def test_maximum_width_rotates_in_a_translated_local_frame(self):
        points = np.array(
            [[[0, -15], [0, 0], [0, 15]], [[-15, 0], [0, 0], [15, 0]]], float
        )
        for theta in (0.1, 0.7, 1.3):
            rotation = np.array(
                [[np.cos(theta), -np.sin(theta)], [np.sin(theta), np.cos(theta)]]
            )
            pad = profile_junction_pad(points @ rotation.T + [500, -900])
            self.assertAlmostEqual(pad.area, 450)

    def test_nonfinite_uncentred_distant_or_oversized_profiles_are_rejected(self):
        good = np.array([[[0, -1], [0, 0], [0, 1]], [[-2, 0], [0, 0], [2, 0]]], float)
        for name in ("nan", "centre", "midpoint", "width", "frame", "zero"):
            points = good.copy()
            if name == "nan":
                points[0, 0, 0] = np.nan
            elif name == "centre":
                points[1] += [0.01, 0]
            elif name == "midpoint":
                points[0, 0, 0] += 0.01
            elif name == "width":
                points *= 100
            elif name == "frame":
                points += 1e20
            else:
                points[0] = 0
            with self.subTest(name=name), self.assertRaises(ValueError):
                profile_junction_pad(points)
        for points in (good[:1], np.tile(good[:1], (5, 1, 1)), good[:, :2]):
            with self.assertRaises(ValueError):
                profile_junction_pad(points)
        with self.assertRaises(ValueError):
            profile_junction_pad(good, method="unknown")


if __name__ == "__main__":
    unittest.main()
