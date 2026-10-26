"""Native scenario contract and independent cardinal/projection/height checks."""

import unittest
from dataclasses import replace

import numpy as np
from outshine_ref.places import axes, ecef
from place_reference import PLACES, read


class PlaceReferences(unittest.TestCase):
    def test_fixed_native_camera_values_and_vertical_lens(self):
        controls = {
            "Husum": (20.0, 35.0, -7.0, 38.04),
            "Feldkirch": (614.0, 1.0, -11.5, 80.72),
            "ZuerichHauptbahnhof": (510.0, 285.0, -20.0, 85.0),
            "DarmstadtWest": (205.0, 252.0, 2.6, 38.04),
            "Flensburg": (66.0, 1.0, -7.0, 33.966661),
            "Rosenheim": (492.0, 185.0, -1.6, 30.68),
            "Tokyo": (60.0, 40.0, -6.0, 55.0),
        }
        for name, expected in controls.items():
            value = read(name)
            self.assertEqual(
                (
                    value.declared_height_m,
                    value.bearing_deg,
                    value.pitch_deg,
                    value.vertical_fov_deg,
                ),
                expected,
            )
            view = value.view(ground_height_m=42.0)
            self.assertEqual((view.width, view.height), (1920, 1080))
            self.assertEqual(view.fov_deg, expected[3])
            self.assertAlmostEqual(np.linalg.norm(view.target - view.eye), 1.0)
            self.assertEqual(len(value.scenario_sha256), 64)

    def test_tokyo_adds_dem_height_but_absolute_stations_do_not(self):
        for name in PLACES:
            reference = read(name)
            self.assertEqual(reference.samples_height, name == "Tokyo")
            a, b = reference.view(10.0), reference.view(80.0)
            expected = 70.0 if name == "Tokyo" else 0.0
            np.testing.assert_allclose(b.eye - a.eye, [0, 0, expected], atol=2e-9)
        with self.assertRaises(ValueError):
            read("Tokyo").view()

    def test_cardinal_bearings_and_positive_pitch_follow_native_enu(self):
        reference = replace(read("Flensburg"), pitch_deg=0.0)
        for bearing, expected in (
            (0.0, [0, 1, 0]),
            (90.0, [1, 0, 0]),
            (180.0, [0, -1, 0]),
            (270.0, [-1, 0, 0]),
        ):
            view = replace(reference, bearing_deg=bearing).view()
            np.testing.assert_allclose(view.target - view.eye, expected, atol=1e-12)
        view = replace(reference, bearing_deg=0.0, pitch_deg=30.0).view()
        np.testing.assert_allclose(
            view.target - view.eye, [0, np.sqrt(3) / 2, 0.5], atol=1e-12
        )

    def test_ecef_axes_and_height_are_metrically_consistent(self):
        np.testing.assert_allclose(ecef(0, 0, 0), [6378137.0, 0, 0])
        for name in PLACES:
            value = read(name)
            matrix = axes(value.latitude_deg, value.longitude_deg)
            np.testing.assert_allclose(matrix @ matrix.T, np.eye(3), atol=1e-15)
            self.assertAlmostEqual(np.linalg.det(matrix), 1.0)
            np.testing.assert_allclose(
                value.local(value.latitude_deg, value.longitude_deg, 100),
                [0, 0, 100],
                atol=2e-9,
            )

    def test_aspect_and_unknown_place_are_not_silently_replaced(self):
        with self.assertRaises(ValueError):
            read("Tokyo").view(30.0, (1920, 1200))
        with self.assertRaises(ValueError):
            read("Hameln")


if __name__ == "__main__":
    unittest.main()
