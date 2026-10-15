"""Independent ENU, camera handedness, pixel projection and reference controls."""

import unittest
from dataclasses import replace
from pathlib import Path

import numpy as np
from place_camera import GeographicFrame, PlaceCamera, selected_cameras


def north_camera():
    return PlaceCamera(
        "control",
        "",
        GeographicFrame(0, 0),
        0,
        0,
        0,
        False,
        0,
        0,
        90,
        1000,
        "2026-06-21T12:00:00Z",
    )


class PlaceCameraTests(unittest.TestCase):
    def test_wgs84_equator_matches_independent_circular_section(self):
        frame = GeographicFrame(0, 0)
        delta = 0.001
        angle = np.deg2rad(delta)
        expected = (6378137 * np.sin(angle), 0, 6378137 * (np.cos(angle) - 1))
        np.testing.assert_allclose(frame.to_local(delta, 0, 0), expected, atol=1e-9)
        self.assertLess(frame.to_local(delta, 0, 0)[2], 0)

    def test_geographic_roundtrip_and_one_common_height_datum(self):
        frame = GeographicFrame(47.256121, 10.115857)
        lon = np.array((10.115857, 10.2, 9.9))
        lat = np.array((47.256121, 47.3, 47.1))
        height = np.array((1772, 1350, 2300))
        local = frame.to_local(lon, lat, height)
        np.testing.assert_allclose(local[0], (0, 0, 1772), atol=1e-9)
        replay = frame.to_geographic(local)
        np.testing.assert_allclose(
            replay[:, :2], np.column_stack((lon, lat)), atol=1e-11
        )
        np.testing.assert_allclose(replay[:, 2], height, atol=2e-6)

    def test_cardinal_cameras_are_right_handed_and_pitch_is_upward(self):
        camera = north_camera()
        for bearing, direction in ((0, (0, 1, 0)), (90, (1, 0, 0)), (180, (0, -1, 0))):
            basis = replace(camera, bearing_deg=bearing).basis
            np.testing.assert_allclose(-basis[:, 2], direction, atol=2e-15)
            np.testing.assert_allclose(basis.T @ basis, np.eye(3), atol=2e-15)
            self.assertAlmostEqual(np.linalg.det(basis), 1)
        up = -replace(camera, pitch_deg=30).basis[:, 2]
        np.testing.assert_allclose(up, (0, np.sqrt(3) / 2, 0.5), atol=2e-15)

    def test_pixel_edges_use_vertical_fov_and_top_left_origin(self):
        camera = north_camera()
        points = np.array(((0, 10, 0), (0, 10, 10), (160 / 9, 10, 0), (0, -10, 0)))
        pixels, depth = camera.project(points, 0, (1600, 900))
        np.testing.assert_allclose(
            pixels[:3], ((800, 450), (800, 0), (1600, 450)), atol=3e-13
        )
        self.assertTrue(np.isnan(pixels[3]).all())
        np.testing.assert_array_equal(depth, (10, 10, 10, -10))

    def test_real_places_preserve_geodetic_height_semantics_and_lens(self):
        root = Path(__file__).resolve().parents[3]
        places = {camera.name: camera for camera in selected_cameras(root)}
        self.assertEqual(
            set(places), {"CentralPark", "Koerbersee", "DarmstadtWest", "Malcesine"}
        )
        self.assertTrue(places["CentralPark"].samples_height)
        self.assertAlmostEqual(places["CentralPark"].eye(35)[2], 95, places=8)
        self.assertFalse(places["Koerbersee"].samples_height)
        self.assertAlmostEqual(places["Koerbersee"].eye(35)[2], 1772, places=8)
        self.assertEqual(places["Koerbersee"].fov_y_deg, 33.97)
        self.assertEqual(places["DarmstadtWest"].pitch_deg, 2.6)
        self.assertEqual(places["Malcesine"].bearing_deg, 290)


if __name__ == "__main__":
    unittest.main()
