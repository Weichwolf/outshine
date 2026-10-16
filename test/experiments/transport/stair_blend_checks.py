"""Whole interfaces, real bridge clearances, and rejected discontinuous counterfields."""

import json
import unittest
from functools import partial

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_blend_audit import continuous_cap, inspect
from stair_blend_constraints import refine, upper_rows, violating_faces
from stair_blend_plan import prepare
from stair_blend_solve import assemble, check_heights, solve
from stair_junction_body import build
from stair_junction_layout import layout as source_layout
from stair_junction_solve import solve as source_solve
from stair_network_inputs import sample_xy
from stair_outer_inputs import FIXTURE, load
from stair_terrain_checks import crossing_layout
from stair_terrain_solid import retaining_body, split_roots


def slope_fixture(slope=1.0):
    xy = np.array([[0.0, 0], [1, 0], [0, 1]])
    return {
        "xy": xy,
        "faces": np.array([[0, 1, 2]]),
        "ground": np.array([False]),
        "targets": np.zeros(3),
        "bed": slope * xy[:, 0],
    }


class BlendContracts(unittest.TestCase):
    def test_steep_constructed_face_is_explicit_and_confined_to_contact_zone(self):
        layout = slope_fixture()
        footprint = shapely.box(-1, -1, 2, 2)
        # This triangle's border is also the explicit outer domain. Keep its prior
        # equal to the declared bed so the fixture tests the constructed cap.
        layout["targets"] = layout["bed"].copy()
        heights, roles, _note = solve(layout, None, footprint, smoothing=0)
        self.assertIsNotNone(heights)
        self.assertTrue(roles[0])
        model = assemble(layout, None, footprint, smoothing=0)
        model["eligible"][:] = False
        with self.assertRaisesRegex(ValueError, "escapes"):
            check_heights(layout, heights, model)

    def test_steep_face_is_not_accepted_as_unmodified_ordinary_soil(self):
        layout = slope_fixture()
        layout["targets"] = layout["bed"].copy()
        heights, _roles, note = solve(
            layout, None, shapely.box(20, 20, 21, 21), smoothing=0
        )
        self.assertIsNone(heights)
        self.assertIn("Infeasible", note["solver_status"])

    def test_upper_interior_overlap_is_checked_even_without_vertex_hits(self):
        layout = slope_fixture(0)
        layout["xy"] *= 10
        upper = Mesh(
            np.array([[2.0, 2, -0.5], [3, 2, -0.5], [2, 3, -0.5]]),
            np.array([[0, 1, 2]]),
        )
        footprint = shapely.box(20, 20, 21, 21)
        matrix, bound, _note = upper_rows(
            layout["xy"], layout["faces"], upper, footprint
        )
        self.assertGreater(float((matrix @ np.zeros(3) - bound).max()), 0.5)
        bad = violating_faces(layout, np.zeros(3), upper, footprint)
        refined, note = refine(layout, bad, lambda p: np.zeros(len(p)))
        self.assertEqual(note["refined_cells"], 1)
        self.assertEqual(len(refined["xy"]), 4)
        self.assertFalse(np.isfinite(refined["bed"][-1]))
        np.testing.assert_array_equal(refined["bed"][:3], layout["bed"])

    def test_old_closed_retaining_counterexample_cannot_use_continuous_envelope(self):
        body, parts = retaining_body(split_roots(crossing_layout()))
        with self.assertRaisesRegex(ValueError, "single continuous"):
            continuous_cap(body, parts["top_faces"])

    def test_derived_envelope_covers_actual_heterogeneous_float32_rounding(self):
        xy = np.array([[0.0, 0], [4, 0], [4, 4], [0, 4], [1.23456789, 1.23456789]])
        heights = 166 + xy @ [0.2, 0.3]
        faces = np.array([[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4]])
        layout = {
            "xy": xy,
            "faces": faces,
            "ground": np.ones(4, bool),
            "targets": heights,
            "bed": heights,
        }
        body, parts = retaining_body(layout)
        coarse = np.array([[0, 1, 2], [0, 2, 3]])
        ground = Mesh(np.c_[xy[:4], heights[:4]], coarse)
        upper = Mesh(ground.vertices + [0, 0, 2], coarse)
        proof = inspect(body, parts, ground, upper)
        errors = [
            abs(row["minimum_clearance_m"]) for row in proof["whole_float32_contact"]
        ]
        self.assertGreater(max(errors), 0)
        self.assertLess(
            max(errors),
            proof["float32_contact_envelope"]["derived_vertical_contact_bound_m"],
        )


class RealBlend(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _case, cls.rows, nodes, stairs, context, _recipe = load()
        source = source_layout(cls.rows, "width", junctions=nodes, stairs=stairs)
        heights, note = source_solve(source)
        if heights is None:
            raise ValueError(note)
        cls.road, data, _geometry = build(source, heights)
        recipe = json.loads(FIXTURE.read_text())
        raster = dict(np.load(FIXTURE.with_name(recipe["elevation"]["window_file"])))
        cls.initial, cls.layout, cls.ground, cls.upper, cls.footprint, cls.plan_note = (
            prepare(
                data,
                cls.rows,
                context,
                raster,
                partial(sample_xy, context=context, raster=raster),
            )
        )
        cls.heights, cls.roles, cls.note = solve(cls.layout, cls.upper, cls.footprint)
        if cls.heights is None:
            raise ValueError(cls.note)
        cls.body, cls.parts = retaining_body(dict(cls.layout, targets=cls.heights))
        cls.proof = inspect(cls.body, cls.parts, cls.ground, cls.upper)

    def test_all_original_ground_facets_and_both_export_height_bands_are_covered(self):
        coverage = self.proof["original_ground_facet_coverage"]
        self.assertEqual(coverage["original_facets"], len(self.ground.triangles))
        self.assertLess(coverage["maximum_parent_height_error_m"], 1e-7)
        for row in self.proof["whole_float32_contact"]:
            self.assertLess(
                abs(row["minimum_clearance_m"]),
                self.proof["float32_contact_envelope"][
                    "derived_vertical_contact_bound_m"
                ],
            )

    def test_every_actual_bridge_and_both_material_class_grades_pass(self):
        for row in self.proof["bridge_clearance"].values():
            self.assertGreaterEqual(row["minimum_clearance_m"], 0)
        self.assertLessEqual(max(self.note["maximum_soil_grade_double_float32"]), 2 / 3)
        self.assertLessEqual(max(self.note["maximum_support_grade_double_float32"]), 10)
        self.assertGreater(self.note["constructed_faces"], 0)

    def test_refinement_adds_free_soil_points_and_preserves_ground_positions(self):
        self.assertGreater(self.plan_note["refinement"]["refined_cells"], 0)
        original = self.initial["faces"][self.initial["ground"]]
        actual = self.layout["faces"][self.layout["ground"]]
        np.testing.assert_array_equal(original, actual)
        np.testing.assert_array_equal(
            self.initial["xy"], self.layout["xy"][: len(self.initial["xy"])]
        )

    def test_shallower_structure_variant_is_not_silently_reclassified(self):
        heights, _roles, note = solve(
            self.layout, self.upper, self.footprint, structure_limit=2
        )
        self.assertIsNone(heights)
        self.assertIn("Infeasible", note["solver_status"])

    def test_the_same_quality_bounds_reject_the_data_only_actual_export(self):
        heights, roles, note = solve(
            self.layout, self.upper, self.footprint, smoothing=0
        )
        self.assertIsNone(heights)
        self.assertIsNone(roles)
        self.assertFalse(note["accepted_field"])
        self.assertEqual(note["solver_status"], "Solved")
        self.assertGreater(note["maximum_candidate_grade_double_float32"][1], 10)


if __name__ == "__main__":
    unittest.main()
