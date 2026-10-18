"""Geometry/coverage counterexamples for the native roof-plan consumer."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
import shapely
from building_roof_domains import (
    checked_union,
    footprint,
    planar_roof_domains,
    triangulate_domain,
)
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
