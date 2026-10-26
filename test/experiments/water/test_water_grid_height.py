"""Sparse coverage without fine geometry, source work caps and masked queries."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
import shapely
from native_water_observation import COURSE_V3
from outshine_ref.contracts import Mesh
from outshine_ref.places.sources import tile_geography
from river_profiles import RiverProfile
from test_water_cell_height import inputs
from water_grid_height import normal_extension, prepare_grid, sample_water, sparse_grid


def source_data():
    latitude, longitude = tile_geography(
        [[8628.2, 5745.5], [8629, 5745.5], [8629, 5745.5], [8629.8, 5745.5]],
        1,
        (14, 0, 0),
    )
    courses = np.zeros(1, dtype=COURSE_V3)
    courses["point_count"] = 4
    return {
        "courses": courses,
        "points_lat_lon": np.c_[latitude, longitude],
        "levels": np.array([500, 499.5, 499.5, 499]),
        "inputs": [
            {
                "kind": "river",
                "layer": "water_lines",
                "bridge": False,
                "tunnel": False,
                "crossing": "",
            }
        ],
    }


class WaterGridHeight(unittest.TestCase):
    def test_precise_holes_and_components_do_not_require_a_contour_mesh(self):
        mask = shapely.box(0, 0, 10, 10).difference(shapely.box(3, 3, 5, 5))
        with patch(
            "shapely.constrained_delaunay_triangles",
            side_effect=AssertionError("fine geometry before demand"),
        ):
            flat, returned, report = sparse_grid([mask], 16)
        self.assertTrue(returned.equals(mask))
        self.assertEqual(report["selection_area_m2"], 96)
        owner = Mesh(
            np.c_[flat.vertices[:, :2], np.full(len(flat.vertices), 500)],
            flat.triangles,
        )
        samples = sample_water(owner, returned, [[1, 1], [4, 4], [12, 12]])
        self.assertEqual(
            samples.reasons, ("", "outside_water_mask", "outside_water_mask")
        )
        self.assertEqual(samples.points_enu_m[0, 2], 500)
        self.assertTrue(np.isnan(samples.points_enu_m[1:]).all())

    def test_disconnected_sources_do_not_enumerate_their_global_bounding_box(self):
        polygons = [shapely.box(0, 0, 4, 4), shapely.box(1e6, 1e6, 1e6 + 4, 1e6 + 4)]
        mesh, mask, report = sparse_grid(polygons, 16, candidate_limit=2)
        self.assertEqual(report["source_bbox_grid_cell_visits"], 2)
        self.assertEqual(len(mesh.triangles), 4)
        self.assertEqual(mask.area, 32)

    def test_complete_producer_avoids_cdt_and_keeps_common_source_bounds(self):
        _mesh, _anchor, records, _knots = inputs()
        data = source_data()
        original = data["points_lat_lon"].tobytes()
        with patch(
            "shapely.constrained_delaunay_triangles",
            side_effect=AssertionError("contour dependency"),
        ):
            coarse = prepare_grid(records, data, 512)
            fine = prepare_grid(records[::-1], data, 256)
            reversed_coarse = prepare_grid(records[::-1], data, 512)
        self.assertEqual(coarse.bounds, fine.bounds)
        self.assertEqual(
            coarse.report["physical_support_area_m2"],
            fine.report["physical_support_area_m2"],
        )
        np.testing.assert_array_equal(
            coarse.mesh.vertices, reversed_coarse.mesh.vertices
        )
        np.testing.assert_array_equal(
            coarse.mesh.triangles, reversed_coarse.mesh.triangles
        )
        self.assertEqual(coarse.mask.wkb, reversed_coarse.mask.wkb)
        self.assertEqual(
            coarse.report["sources"]["omitted_zero_length_segment_addresses"], [[0, 1]]
        )
        self.assertEqual(data["points_lat_lon"].tobytes(), original)

    def test_normal_extension_has_known_affine_and_clamped_endpoint_values(self):
        profile = RiverProfile(np.array([[[0, 0], [1, 0]]]), np.array([[10, 12]]))
        heights, report = normal_extension(
            profile, [[0.5, 1000], [0.5, -1000], [-0.5, 1000], [1.5, -1000]]
        )
        np.testing.assert_array_equal(heights, [11, 11, 10, 12])
        self.assertEqual(report["maximum_profile_references"], 1)

    def test_source_reference_caps_and_crossing_ambiguity_do_not_become_defaults(self):
        profile = RiverProfile(
            np.repeat([[[0, 0], [1, 0]]], 65, axis=0), np.repeat([[10, 12]], 65, axis=0)
        )
        with self.assertRaisesRegex(ValueError, "unchanged work caps"):
            normal_extension(profile, [[0.5, 1000]])
        _mesh, _anchor, records, _knots = inputs()
        data = copy.deepcopy(source_data())
        data["inputs"][0]["bridge"] = True
        with self.assertRaisesRegex(ValueError, "no admitted"):
            prepare_grid(records, data, 512)

    def test_bad_spacing_and_candidate_or_query_work_refuse(self):
        for spacing in (0, np.nan, -1):
            with self.assertRaises(ValueError):
                sparse_grid([shapely.box(0, 0, 10, 10)], spacing)
        with self.assertRaisesRegex(ValueError, "work budget"):
            sparse_grid([shapely.box(0, 0, 10, 10)], 1, candidate_limit=1)
        flat, mask, _ = sparse_grid([shapely.box(0, 0, 10, 10)], 16)
        with self.assertRaisesRegex(ValueError, "bounded water height"):
            sample_water(flat, mask, [[0, 0], [1, 1]], query_limit=1)


if __name__ == "__main__":
    unittest.main()
