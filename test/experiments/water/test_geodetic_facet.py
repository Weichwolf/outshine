"""Closed forms, independent PROJ coordinates and geodesic finite differences."""

import unittest

import numpy as np
from outshine_ref.water.geodetic import WGS84_A, local_points, query_facet
from pyproj import Geod, Transformer


class GeodeticFacetTests(unittest.TestCase):
    def test_equatorial_constant_enu_plane_has_nonconstant_geodetic_height(self):
        triangle = np.array(
            [[-20000, -20000, 500], [20000, -20000, 500], [0, 20000, 500.0]]
        )
        longitude = np.array([0, 0.001, 0.01, 0.03])
        result = query_facet(
            [0, 0], triangle, np.c_[longitude, np.zeros(4)], metric_height_m=0
        )
        angle = np.deg2rad(longitude)
        expected = (WGS84_A + 500) / np.cos(angle) - WGS84_A
        grade = (1 + 500 / WGS84_A) * np.sin(angle) / np.cos(angle) ** 2
        np.testing.assert_allclose(
            result["plane_height_m"], expected, rtol=0, atol=4 * np.spacing(WGS84_A)
        )
        np.testing.assert_allclose(
            result["grade_m_per_m"][:, 0], grade, rtol=0, atol=2e-16
        )
        np.testing.assert_array_equal(result["enu_up_grade"], [0, 0])
        self.assertGreater(result["plane_height_m"][2] - 500, 0.09)
        self.assertTrue(result["inside_numeric"].all())

    def test_shared_frame_matches_independent_proj_at_dateline_and_high_latitude(self):
        for anchor in ([0, 0], [10, 47], [179.99, 75], [18, -35]):
            geod = Geod(ellps="WGS84")
            lon, lat, _ = geod.fwd(
                np.repeat(anchor[0], 3),
                np.repeat(anchor[1], 3),
                [0, 120, 240],
                [3000] * 3,
            )
            heights = np.array([0, 500, -100.0])
            ours = local_points(anchor, np.c_[lon, lat], heights)
            # PROJ's independent EPSG:9837 pipeline includes its own cart/ENU stages.
            enu = Transformer.from_pipeline(
                f"+proj=pipeline +step +proj=cart +ellps=WGS84 +step +proj=topocentric +ellps=WGS84 +lon_0={anchor[0]} +lat_0={anchor[1]} +h_0=0"
            )
            expected = np.array(enu.transform(lon, lat, heights)).T
            np.testing.assert_allclose(
                ours, expected, rtol=0, atol=8 * np.spacing(WGS84_A)
            )

    def test_grade_matches_independent_geodesic_finite_differences(self):
        geod = Geod(ellps="WGS84")
        triangle = np.array([[-2000, -2000, 500], [2000, -2000, 540], [0, 2000, 504.0]])
        step = 2.0
        for anchor in ([0, 0], [10, 47], [179.99, 75], [18, -35]):
            lon, lat, _ = geod.fwd(anchor[0], anchor[1], 35, 300)
            query = np.array([[lon, lat]])
            value = query_facet(anchor, triangle, query, metric_height_m=0)
            differences = []
            for azimuth in (90, 0):
                east = geod.fwd(lon, lat, azimuth, step)[:2]
                west = geod.fwd(lon, lat, azimuth, -step)[:2]
                samples = query_facet(anchor, triangle, [west, east], metric_height_m=0)
                self.assertTrue(samples["inside_numeric"].all())
                differences.append(np.diff(samples["plane_height_m"])[0] / (2 * step))
            np.testing.assert_allclose(
                value["grade_m_per_m"][0],
                differences,
                rtol=0,
                atol=16 * np.spacing(WGS84_A) / step,
            )

    def test_metric_height_is_explicit_and_not_an_implicit_grade_reinterpretation(self):
        triangle = np.array([[-2000, -2000, 500], [2000, -2000, 540], [0, 2000, 504.0]])
        zero = query_facet([0, 0], triangle, [[0, 0]], metric_height_m=0)
        value = zero["plane_height_m"][0]
        at_surface = query_facet([0, 0], triangle, [[0, 0]], metric_height_m=value)
        self.assertGreater(
            abs(zero["grade_m_per_m"][0, 0] - at_surface["grade_m_per_m"][0, 0]), 1e-7
        )
        np.testing.assert_allclose(
            at_surface["grade_m_per_m"][0], at_surface["enu_up_grade"], atol=1e-16
        )

    def test_outside_and_parallel_are_not_valid_triangle_heights(self):
        horizontal = np.array([[-1, -1, 500], [1, -1, 500], [0, 1, 500.0]])
        outside = query_facet([0, 0], horizontal, [[0.01, 0]], metric_height_m=0)
        self.assertTrue(outside["valid_plane_query"][0])
        self.assertFalse(outside["inside_numeric"][0])
        vertical = np.array([[0, -1, 0], [0, 1, 0], [0, 0, 1.0]])
        parallel = query_facet([0, 0], vertical, [[0, 0]], metric_height_m=0)
        self.assertFalse(parallel["valid_plane_query"][0])
        self.assertTrue(np.isnan(parallel["plane_height_m"][0]))


if __name__ == "__main__":
    unittest.main()
