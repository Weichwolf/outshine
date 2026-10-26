"""Geometry/coverage counterexamples for the native roof-plan consumer."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
import shapely
from building_positions import millimetres
from building_roof_domains import (
    checked_union,
    footprint,
    planar_roof_domains,
    triangle_orientation,
    triangulate_domain,
)
from building_roof_positions import roof_positions
from verify_building_roof_domains import controls


def shape():
    return {
        "ring": [[-3, -2], [3, -2], [3, 2], [-3, 2]],
        "holes": [],
        "centre": [0, 0],
        "axis_u": [1, 0],
        "roof_kind": 0,
        "roof_deck_m": 10,
        "rise_m": 0,
        "height_controls": [{"point": [0, 0], "inside": True, "wall_top_m": 10}],
        "roof_bands": [
            {
                "v_interval": {
                    "min": None,
                    "max": None,
                    "min_closed": False,
                    "max_closed": False,
                },
                "slope_v": 0,
                "height_at_v0_m": 10,
                "normal_enu": [0, 0, 1],
            }
        ],
    }


class RoofDomains(unittest.TestCase):
    def test_millimetre_ties_are_away_from_zero_without_half_addition(self):
        values = np.array(
            [
                0.0005,
                -0.0005,
                np.nextafter(0.0005, 0),
                np.nextafter(-0.0005, 0),
                0.0015,
                -0.0015,
            ]
        )
        np.testing.assert_array_equal(
            millimetres(values), [0.001, -0.001, 0, 0, 0.002, -0.002]
        )

    def test_native_recipe_is_batch_independent_and_does_not_mutate_input(self):
        local = np.array(
            [[[1.2345, -3.4565, 0], [9.1234, 2.9876, 0], [1.2345, -3.4565, 0]]]
        )
        before = local.copy()
        plane = np.array([0.31, -0.17, 4.0005])
        surface = {
            "origin": [12345.678, -2222.3, 999.1],
            "east": [0.6, 0.8, 0],
            "north": [-0.8, 0.6, 0],
            "up": [0, 0, 1],
        }
        batch = roof_positions(local, plane, surface, recipe="snapped-f32")
        scalar = np.array(
            [
                roof_positions(point[None], plane, surface, recipe="snapped-f32")[0]
                for point in local.reshape(-1, 3)
            ]
        )
        np.testing.assert_array_equal(batch.reshape(-1, 3), scalar)
        np.testing.assert_array_equal(local, before)
        np.testing.assert_array_equal(
            batch, batch.astype(np.float32).astype(np.float64)
        )

    def test_float64_recipe_preserves_existing_affine_emission(self):
        local = np.array([[[1.0, 2.0, 3.0], [-1.0, 5.0, 2.0], [3.0, 7.0, 11.0]]])
        surface = {
            "origin": [99.0, -8.0, 7.0],
            "east": [0.6, 0.8, 0],
            "north": [-0.8, 0.6, 0],
            "up": [0, 0, 1],
        }
        expected = np.array(surface["origin"]) + local @ np.array(
            [surface[k] for k in ("east", "north", "up")]
        )
        actual = roof_positions(local, np.array([0.0, 0.0, 0.0]), surface)
        self.assertEqual(actual.tobytes(), expected.tobytes())

    def test_real_thin_triangle_is_not_rejected_by_cross_cancellation(self):
        xy = np.array(
            [
                [-0.8955213273406581, 20.56467645221061],
                [0.7955486026398972, 37.327515319472354],
                [0.7955486026398974, 37.327515319472354],
            ]
        )
        self.assertLess(triangle_orientation(xy), 0)
        self.assertGreater(triangle_orientation(xy[[0, 2, 1]]), 0)
        self.assertEqual(
            triangle_orientation(np.array([[0.0, 0.0], [1.0, 1.0], [2.0, 2.0]])), 0
        )
        # Original #801 bbd9 parent, source ordinal 152, shape 0, facet 2.
        piece = {
            "domain": shapely.Polygon(
                [
                    [0.7955486026398974, 37.327515319472354],
                    [13.922100546198203, 21.44349619582772],
                    [4.207082959163197, 20.049028650974716],
                    [-0.3489143829580481, 19.906561444866714],
                    [-0.8955213273406581, 20.56467645221061],
                    [0.7955486026398972, 37.327515319472354],
                ]
            ),
            "plane_enu": np.array(
                [-0.17370182173996834, -0.14354716951746044, 13.501381332248858]
            ),
            "normal_enu": np.array(
                [0.1694528447927231, 0.14003581535886642, 0.9755386736610859]
            ),
        }
        emitted = triangulate_domain(piece)
        self.assertEqual(len(emitted), 4)
        self.assertTrue(
            all(triangle_orientation(triangle[:, :2]) > 0 for triangle in emitted)
        )

    def assert_triangulation(self, piece):
        triangles = triangulate_domain(piece)
        areas = []
        for triangle in triangles:
            polygon = shapely.Polygon(triangle[:, :2])
            self.assertTrue(piece["domain"].covers(polygon))
            areas.append(polygon.area)
        self.assertAlmostEqual(sum(areas), piece["domain"].area)
        return triangles

    def test_hole_is_never_filled(self):
        original = shape()
        original["holes"] = [[[-1, -1], [-1, 1], [1, 1], [1, -1]]]
        piece = planar_roof_domains(original)[0]
        triangles = self.assert_triangulation(piece)
        self.assertFalse(piece["domain"].covers(shapely.Point(0, 0)))
        self.assertTrue(np.all(triangles[:, :, 2] == 10))
        self.assertEqual(piece["domain"].area, 20)

    def test_band_keeps_disconnected_components(self):
        original = shape()
        original["ring"] = [
            [-3, -3],
            [3, -3],
            [3, 3],
            [1, 3],
            [1, -1],
            [-1, -1],
            [-1, 3],
            [-3, 3],
        ]
        original["roof_bands"][0]["v_interval"]["min"] = 0
        piece = planar_roof_domains(original)[0]
        self.assertEqual(piece["domain"].geom_type, "MultiPolygon")
        self.assertEqual(len(piece["domain"].geoms), 2)
        self.assertFalse(piece["domain"].covers(shapely.Point(0, 1)))
        self.assert_triangulation(piece)

    def test_gable_retains_both_planes(self):
        original = shape()
        left = original["roof_bands"][0]
        left["v_interval"]["max"] = 0
        left["slope_v"], left["normal_enu"] = 1, [0, -(2**-0.5), 2**-0.5]
        right = copy.deepcopy(left)
        right["v_interval"]["min"], right["v_interval"]["max"] = 0, None
        right["slope_v"], right["normal_enu"] = -1, [0, 2**-0.5, 2**-0.5]
        original["roof_bands"].append(right)
        pieces = planar_roof_domains(original)
        self.assertEqual(len(pieces), 2)
        self.assertEqual(checked_union(pieces).area, footprint(original).area)
        self.assertEqual(controls(original, pieces)[0]["plane_normals"], 2)
        for piece in pieces:
            self.assert_triangulation(piece)

    def test_hip_origin_scale_and_deck(self):
        original = shape()
        original.update(roof_kind=2, roof_bands=None, rise_m=4)
        origin = np.array([8.0, -7.0])
        original["hip"] = {
            "available": True,
            "origin": origin.tolist(),
            "maximum_distance_m": 8,
            "facets": [
                {
                    "ring": (np.asarray(original["ring"]) - origin).tolist(),
                    "gradient": [0, 2],
                    "intercept_m": 0,
                    "normal_enu": [0, -(2**-0.5), 2**-0.5],
                    "source_ring": 0,
                    "source_edge": 1,
                }
            ],
        }
        piece = planar_roof_domains(original)[0]
        np.testing.assert_array_equal(piece["plane_enu"], [0, 1, 17])
        self.assertEqual(piece["source_edge"], 1)
        self.assertEqual(piece["domain"].area, footprint(original).area)
        self.assert_triangulation(piece)

    def test_invalid_original_is_not_repaired(self):
        original = shape()
        original["ring"] = [[-1, -1], [1, 1], [1, -1], [-1, 1]]
        with self.assertRaisesRegex(ValueError, "original roof footprint"):
            planar_roof_domains(original)

    def test_nonplanar_remains_without_domains(self):
        original = shape()
        original.update(roof_kind=6, roof_bands=None, rise_m=2)
        self.assertEqual(planar_roof_domains(original), [])

    def test_lost_union_input_is_rejected(self):
        first, second = shapely.box(-2, -1, -1, 1), shapely.box(1, -1, 2, 1)
        pieces = [{"domain": first}, {"domain": second}]
        with (
            patch("building_roof_domains.shapely.union_all", return_value=first),
            self.assertRaisesRegex(ValueError, "lost an input domain"),
        ):
            checked_union(pieces)

    def test_height_and_normal_corruption_are_rejected(self):
        original = shape()
        pieces = planar_roof_domains(original)
        pieces[0]["plane_enu"][2] += 1
        with self.assertRaisesRegex(ValueError, "native height"):
            controls(original, pieces)
        pieces = planar_roof_domains(original)
        pieces[0]["normal_enu"] *= -1
        with self.assertRaisesRegex(ValueError, "native normal"):
            controls(original, pieces)


if __name__ == "__main__":
    unittest.main()
