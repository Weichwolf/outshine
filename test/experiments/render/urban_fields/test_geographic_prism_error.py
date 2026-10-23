"""Shared frame convention and geometric correspondence controls."""

import unittest
from types import SimpleNamespace

import numpy as np
from geographic_prism_error import WGS84_A, frame_axes, measure, reference
from outshine_ref.places.camera import axes


def product(longitude, side=1):
    first_x = int(np.deg2rad(longitude) * WGS84_A / 32)
    first_y = int(np.arcsinh(np.tan(np.deg2rad(35))) * WGS84_A / 32)
    return SimpleNamespace(
        chunks=np.array([[first_x // side, first_y // side]]),
        side=side,
        tiers=np.ones((1, side, side), np.uint8),
        heights=np.array([0, 48]),
        frame=np.array([0, 0, 1, 32, 0.75]),
    )


class GeographicPrismErrorTest(unittest.TestCase):
    def test_batched_axes_match_shared_camera_convention(self):
        latitude = np.array([-75.0, 0.0, 35.0, 75.0])
        longitude = np.array([-179.999, 0.0, 139.0, 179.999])
        actual = frame_axes(np.deg2rad(latitude), np.deg2rad(longitude))
        expected = np.array(
            [axes(lat, lon) for lat, lon in zip(latitude, longitude, strict=True)]
        )
        np.testing.assert_array_equal(actual, expected)

    def test_single_cell_and_wrapped_longitude_keep_exact_centre(self):
        for longitude in (0, 139, 180.001, -180.001):
            result = measure(reference(product(longitude)), 1, 0, 0)
            self.assertLess(result["max_vertex_error_m"], 1e-8)
            self.assertEqual(result["patches"], 1)

    def test_height_interval_endpoints_bound_the_middle(self):
        data = product(139, 8)
        source = reference(data)
        low = measure(source, 8, 0, 1000)["max_vertex_error_m"]
        high = measure(source, 8, 2000, 1000)["max_vertex_error_m"]
        mid = measure(source, 8, 1000, 1000)["max_vertex_error_m"]
        self.assertLessEqual(mid, max(low, high) + 1e-8)

    def test_dateline_crossing_preserves_longitude_invariant_error(self):
        source = reference(product(179.9999, 8))
        self.assertGreater(np.ptp(np.rad2deg(source["longitude"])), 300)
        crossing = measure(source, 8, 0, 0)["max_vertex_error_m"]
        ordinary = measure(reference(product(0.0001, 8)), 8, 0, 0)["max_vertex_error_m"]
        self.assertAlmostEqual(crossing, ordinary, delta=1e-8)


if __name__ == "__main__":
    unittest.main()
