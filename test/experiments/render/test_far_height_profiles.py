"""Independent solid/footprint/material controls for plan-first height profiles."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
import shapely
import trimesh
from far_height_profiles import emit, plan_source


def source(holes=False):
    shape = {
        "ring": [[-3.0, -2.0], [3.0, -2.0], [3.0, 2.0], [-3.0, 2.0]],
        "holes": [[[-1.0, -0.5], [-1.0, 0.5], [1.0, 0.5], [1.0, -0.5]]]
        if holes
        else [],
        "centre": [0.0, 0.0],
        "axis_u": [1.0, 0.0],
        "half_v_m": 2.0,
        "roof_kind": 1,
        "rise_m": 2.0,
        "seat_m": 0.0,
        "foot_m": 0.0,
        "sole_m": 0.0,
        "eaves_m": 10.0,
        "hip": None,
        "front_edge": 0,
        "party_wall_edges": [0, 0, 0, 0],
        "opening_style": 0,
        "wall_variant": 0,
        "bay_m": 2.0,
        "floor_m": 3.0,
        "ident": 5,
    }
    return {
        "origin": [0.0, 0.0, 0.0],
        "east": [1.0, 0.0, 0.0],
        "north": [0.0, 1.0, 0.0],
        "up": [0.0, 0.0, 1.0],
        "minimum_height_m": 0.0,
        "wall_colour": [0.2, 0.3, 0.4],
        "shapes": [shape],
    }


def geometric_mesh(mesh):
    points, inverse = np.unique(mesh["positions"], axis=0, return_inverse=True)
    return trimesh.Trimesh(points, inverse[mesh["indices"]], process=False)


class FarHeightProfiles(unittest.TestCase):
    def test_plan_height_range_does_no_cap_or_mesh_expansion(self):
        with patch(
            "far_height_profiles.cap_triangles",
            side_effect=AssertionError("early mesh"),
        ):
            plans = plan_source(17, source())
        self.assertEqual(len(plans), 1)
        p = plans[0]
        self.assertEqual(
            (p.source, p.part, p.bottom, p.roof_low, p.roof_high), (17, 0, 0, 10, 12)
        )
        xy = np.random.default_rng(12).uniform([-3, -2], [3, 2], (1000, 2))
        original_height = 12 - np.abs(xy[:, 1])
        self.assertTrue((p.roof_high >= original_height).all())
        self.assertLessEqual(
            float((p.roof_high - original_height).max()), p.height_change_m
        )

    def test_holes_and_outline_are_closed_positive_volume(self):
        s = source(holes=True)
        plan = plan_source(19, s)[0]
        mesh = emit([plan])
        geometric = geometric_mesh(mesh)
        self.assertTrue(geometric.is_watertight)
        self.assertTrue(geometric.is_winding_consistent)
        self.assertGreater(geometric.volume, 0)
        self.assertAlmostEqual(geometric.volume, (24 - 2) * 12, places=10)
        roof = mesh["profile_binding"][:, 2] == 1
        polygons = [
            shapely.Polygon(t[:, :2]) for t in mesh["positions"][mesh["indices"][roof]]
        ]
        self.assertEqual(
            shapely.area(
                shapely.symmetric_difference(
                    shapely.union_all(polygons), plan.footprint
                )
            ),
            0,
        )
        self.assertFalse(any(p.contains(shapely.Point(0, 0)) for p in polygons))

    def test_winding_independent_without_reordering_original_face_identity(self):
        s = source(holes=True)
        s["shapes"][0]["ring"].reverse()
        s["shapes"][0]["holes"][0].reverse()
        mesh = emit(plan_source(3, s))
        geometric = geometric_mesh(mesh)
        self.assertTrue(geometric.is_watertight)
        self.assertTrue(geometric.is_winding_consistent)
        self.assertAlmostEqual(geometric.volume, 264, places=10)
        binding = mesh["profile_binding"]
        self.assertEqual(set(binding[:, 0]), {3})
        self.assertEqual(set(binding[:, 2]), set(range(10)))

    def test_input_unchanged_and_original_material_categories_retained(self):
        s = source()
        before = copy.deepcopy(s)
        mesh = emit(plan_source(37, s))
        self.assertEqual(s, before)
        self.assertEqual(mesh["positions"].dtype, np.float32)
        self.assertEqual(mesh["normal"].dtype, np.uint32)
        self.assertEqual(mesh["indices"].dtype, np.uint32)
        self.assertGreater(
            len(mesh["positions"]), len(np.unique(mesh["positions"], axis=0))
        )
        faces = mesh["profile_binding"][:, 2]
        roof = np.unique(mesh["indices"][faces == 1])
        floor = np.unique(mesh["indices"][faces == 0])
        self.assertTrue((mesh["uv"][roof, 0] == -82).all())
        self.assertTrue((mesh["uv"][floor, 0] == -89).all())
        np.testing.assert_array_equal(mesh["colour"][roof], np.ones((len(roof), 4)))
        np.testing.assert_array_equal(
            mesh["colour"][floor, :3],
            np.tile(np.array([0.2, 0.3, 0.4], np.float32), (len(floor), 1)),
        )

    def test_unknown_part_is_not_partially_admitted(self):
        s = source()
        other = copy.deepcopy(s["shapes"][0])
        other["roof_kind"] = 6
        s["shapes"].append(other)
        with self.assertRaisesRegex(ValueError, "dome"):
            plan_source(4, s)
        with self.assertRaises(ValueError):
            plan_source(5, None)

    def test_collapsed_local_geometry_requests_original_source(self):
        s = source()
        s["shapes"][0]["ring"] = [[0, 0], [0.0001, 0], [0.0001, 0.0001], [0, 0.0001]]
        with self.assertRaisesRegex(ValueError, "rounding"):
            emit(plan_source(1, s))


if __name__ == "__main__":
    unittest.main()
