"""Logical water queries do not require fine mesh generation or render LOD."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from shapely.affinity import translate
from test_water_parameter_cut import CELL, root
from water_cell_field import prepare_field, sample_parameters


class FunctionalWater(unittest.TestCase):
    def setUp(self):
        self.local = shapely.Polygon(
            [(0, 0), (1, 0), (1, 1), (0, 1)],
            [[(0.25, 0.25), (0.5, 0.25), (0.5, 0.5), (0.25, 0.5)]],
        )
        self.polygons = [translate(self.local, xoff=CELL[1], yoff=CELL[2])]

    def field(self, polygons=None, keys=("original",)):
        return prepare_field(
            CELL, polygons or self.polygons, keys, root, context="analytic-v1"
        )[0]

    def test_precise_mask_is_used_without_any_render_triangulation(self):
        with patch(
            "shapely.constrained_delaunay_triangles",
            side_effect=AssertionError("fine work requested"),
        ):
            field = self.field()
            result = sample_parameters(
                field, [[0.125, 0.125], [0.375, 0.375], [1.1, 0.5]]
            )
        self.assertEqual(result.states, ("water", "dry", "outside_cell"))
        np.testing.assert_array_equal(result.world[0], [12.5, 12.5, 500])
        self.assertTrue(np.isnan(result.world[1:]).all())
        self.assertEqual(field.report["render_triangles"], 0)
        self.assertEqual(result.source_members.tolist(), [0])

    def test_original_point_sources_are_not_render_face_overlap_candidates(self):
        local = [shapely.box(0, 0, 0.75, 1), shapely.box(0.25, 0, 1, 1)]
        polygons = [translate(p, xoff=CELL[1], yoff=CELL[2]) for p in local]
        field = self.field(polygons, ("left", "right"))
        result = sample_parameters(field, [[0.125, 0.5], [0.5, 0.5], [0.875, 0.5]])
        self.assertEqual(result.source_offsets.tolist(), [0, 1, 3, 4])
        self.assertEqual(result.source_members.tolist(), [0, 0, 1, 1])
        reverse = self.field(polygons[::-1], ("right", "left"))
        other = sample_parameters(reverse, [[0.125, 0.5], [0.5, 0.5], [0.875, 0.5]])
        np.testing.assert_array_equal(result.world, other.world)
        for a, b in zip(range(3), range(3), strict=True):
            one = result.source_members[
                result.source_offsets[a] : result.source_offsets[a + 1]
            ]
            two = other.source_members[
                other.source_offsets[b] : other.source_offsets[b + 1]
            ]
            self.assertEqual(
                {field.source_keys[j] for j in one},
                {reverse.source_keys[j] for j in two},
            )

    def test_unavailable_root_refuses_the_whole_field_and_stops_early(self):
        calls = []

        def pending(keys):
            calls.append(keys)
            return root(keys) if len(calls) == 1 else ("pending_sources", None)

        field, report = prepare_field(
            CELL, self.polygons, ("original",), pending, context="v1"
        )
        self.assertIsNone(field)
        self.assertEqual(report["state"], "pending_sources")
        self.assertEqual(len(calls), 2)

    def test_membership_budget_keeps_no_partial_point_or_world_value(self):
        field = self.field(self.polygons * 2, ("a", "b"))
        result = sample_parameters(
            field, [[0.125, 0.125], [0.75, 0.75]], member_limit=1
        )
        self.assertEqual(result.states, ("membership_budget_exhausted",) * 2)
        self.assertEqual(result.source_members.tolist(), [])
        self.assertTrue(np.isnan(result.world).all())

    def test_boundary_and_immutable_outputs_do_not_expand_the_mask(self):
        field = self.field()
        result = sample_parameters(field, [[0, 0], [1, 1], [-np.spacing(1.0), 0.5]])
        self.assertEqual(result.states, ("water", "water", "outside_cell"))
        for array in (field.world, field.roots, result.world, result.source_members):
            self.assertFalse(array.flags.writeable)
        with self.assertRaisesRegex(ValueError, "bounded finite"):
            sample_parameters(field, [[np.nan, 0]])


if __name__ == "__main__":
    unittest.main()
