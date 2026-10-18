"""Native-recipe wall counterexamples; source metadata is not opaque coverage."""

import copy
import unittest

import numpy as np
from building_shell_walls import (
    accepted_triangles,
    hip_breaks,
    shape_panels,
    shell_wall_groups,
)
from outshine_ref.geometry.occlusion import classify_boxes
from roof_fusion import fused_roof_umbra


def source():
    return {
        "index": 7,
        "surface": {
            "origin": [0, 0, 0],
            "east": [1, 0, 0],
            "north": [0, 1, 0],
            "up": [0, 0, 1],
            "shapes": [
                {
                    "roof_kind": 0,
                    "bottom_m": -2,
                    "roof_deck_m": 8,
                    "ring": [[0, 0], [10, 0], [10, 10], [0, 10]],
                    "holes": [],
                }
            ],
        },
    }


def call():
    return {"source": 7, "accepted": True, "detail": 1, "openings": False}


class ShellWalls(unittest.TestCase):
    def test_only_concrete_retained_closed_shell_is_admitted(self):
        for change in (
            {"source": 8},
            {"accepted": False},
            {"detail": 0},
            {"detail": 2},
            {"openings": True},
            {"openings": None},
        ):
            with self.subTest(change=change), self.assertRaises(ValueError):
                shell_wall_groups(source(), call() | change, retained=True)
        with self.assertRaises(ValueError):
            shell_wall_groups(source(), call())

    def test_literal_quad_winding_and_source_not_mutated(self):
        original = source()
        before = copy.deepcopy(original)
        groups, counts = shell_wall_groups(original, call(), retained=True)
        self.assertEqual(original, before)
        self.assertEqual(counts["candidate_triangles"], 8)
        np.testing.assert_array_equal(
            groups[0]["triangles"],
            [
                [[0, 0, -2], [10, 0, -2], [10, 0, 8]],
                [[0, 0, -2], [10, 0, 8], [0, 0, 8]],
            ],
        )

    def test_native_short_edge_skip_does_not_fill_the_gap(self):
        shape = source()["surface"]["shapes"][0]
        shape["ring"].insert(1, [0.049, 0])
        groups, counts = shape_panels(shape)
        self.assertEqual(counts["native_short_edges_skipped"], 1)
        self.assertEqual([g["edge"] for g in groups], [1, 2, 3, 4])
        self.assertEqual(groups[0]["local"][0, 0, 0], 0.049)

    def test_millimetre_collapse_is_removed_before_coverage(self):
        original = np.array(
            [
                [[0, 0, 0], [0.0004, 0, 0], [0, 0, 1]],
                [[0, 0, 0], [1, 0, 0], [0, 0, 1]],
            ]
        )
        self.assertEqual(accepted_triangles(original).shape, (1, 3, 3))
        tiny = source()
        shape = tiny["surface"]["shapes"][0]
        shape.update(bottom_m=0, roof_deck_m=0.0004)
        groups, counts = shell_wall_groups(tiny, call(), retained=True)
        self.assertEqual(groups, [])
        self.assertEqual(counts["native_local_degenerate_triangles"], 8)

    def test_courtyard_edges_keep_their_original_front_side(self):
        original = source()
        original["surface"]["shapes"][0]["holes"] = [[[2, 2], [2, 8], [8, 8], [8, 2]]]
        groups, _ = shell_wall_groups(original, call(), retained=True)
        inner = [g for g in groups if g["ring"] == 1]
        self.assertEqual(len(inner), 4)
        triangle = inner[0]["triangles"][0]
        normal = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
        self.assertGreater(normal[0], 0)
        self.assertEqual({g["edge"] for g in inner}, {0, 1, 2, 3})

    def test_hip_internal_intersection_splits_wall(self):
        original = source()
        shape = original["surface"]["shapes"][0]
        shape.update(
            roof_kind=2,
            hip={
                "available": True,
                "origin": [0, 0],
                "precision_m": 1e-12,
                "facets": [{"ring": [[5, -1], [5, 1], [6, 1]]}],
            },
        )
        cuts = hip_breaks(shape, np.array([0.0, 0.0]), np.array([10.0, 0.0]))
        self.assertEqual(len(cuts), 2)
        np.testing.assert_allclose(cuts, [0.5, 0.55], atol=0, rtol=1e-15)
        panels, _ = shape_panels(shape)
        self.assertEqual(len([p for p in panels if p["edge"] == 0]), 3)

    def test_unknown_break_recipe_stays_excluded(self):
        original = source()
        original["surface"]["shapes"][0]["roof_kind"] = 6
        groups, counts = shell_wall_groups(original, call(), retained=True)
        self.assertEqual(groups, [])
        self.assertEqual(sum(counts.values()), 1)

    def test_actual_vertical_mesh_needs_explicit_opacity_and_front_eye(self):
        groups, _ = shell_wall_groups(source(), call(), retained=True)
        mesh = groups[0]["triangles"]
        observer = np.array([[4.9, -4.1, 2.9], [5.1, -3.9, 3.1]])
        target = np.array([[[4.9, 2.9, 2.9], [5.1, 3.1, 3.1]]])
        self.assertEqual(len(fused_roof_umbra(mesh, observer).low), 0)
        certificate = fused_roof_umbra(mesh, observer, certified_opaque=True)
        self.assertTrue(classify_boxes(target, certificate)[0].all())
        backside = observer.copy()
        backside[:, 1] += 8
        self.assertEqual(
            len(fused_roof_umbra(mesh, backside, certified_opaque=True).low), 0
        )


if __name__ == "__main__":
    unittest.main()
