"""Actual shared physical coverage and local rejection before expensive work."""

import unittest
from dataclasses import replace
from unittest.mock import Mock

import numpy as np
import shapely
from coincident_ground_checks import audit
from coincident_ground_cover import generate
from coincident_ground_geometry import layout
from coincident_ground_inputs import prepare
from coincident_ground_sources import prepare_sources
from ground_cyclic_source import source_component


class CoincidentCoverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.index, cls.case, terrain, cls.budget, cls.metadata = prepare()
        cls.terrain = staticmethod(terrain)
        cls.selected = cls.case["selected"]
        cls.shared = generate(cls.index, cls.selected, terrain, cls.budget)
        cls.fresh = generate(cls.index, cls.selected, terrain, cls.budget, shared=False)

    def test_actual_complete_source_bands_and_directed_members_are_retained(self):
        result = audit(self.shared, self.index, self.selected)
        self.assertEqual(result["original_members"], 6)
        self.assertEqual(result["original_end_profiles"], 12)
        self.assertEqual((result["vertices"], result["triangles"]), (6, 4))
        self.assertLessEqual(result["maximum_finite_isotropic_grade"], 0.025)

    def test_sharing_changes_work_without_changing_product(self):
        for key in ("vertices", "triangles"):
            np.testing.assert_array_equal(
                getattr(self.shared["mesh"], key), getattr(self.fresh["mesh"], key)
            )
        for key in ("observations", "sections", "axes"):
            np.testing.assert_array_equal(self.shared[key], self.fresh[key])
        self.assertEqual(self.shared["source"], self.fresh["source"])
        self.assertEqual(self.shared["domain"].wkb, self.fresh["domain"].wkb)
        self.assertEqual(self.shared["work"]["constructed_band_polygons"], 2)
        self.assertEqual(self.fresh["work"]["constructed_band_polygons"], 6)

    def test_metric_source_footprint_is_fixed_when_dem_heights_are_lifted(self):
        points = self.shared["inputs"].surface.vertices[:, :2]
        for point in points:
            self.assertTrue(self.shared["domain"].covers(shapely.Point(point)))
        self.assertGreater(
            self.shared["work"]["maximum_discarded_horizontal_height_projection_m"], 0
        )

    def test_original_multicycle_guard_remains_unchanged(self):
        with self.assertRaisesRegex(ValueError, "parallel_or_self_span"):
            source_component(
                self.index,
                self.selected,
                minimum_cycles=3,
                maximum_cycles=8,
                maximum_spans=32,
                prefix="multicycle",
            )

    def reject_before_terrain(self, index, selected, budget, reason):
        terrain = Mock(side_effect=AssertionError("unneeded terrain query"))
        with self.assertRaisesRegex(ValueError, reason):
            generate(index, selected, terrain, budget)
        terrain.assert_not_called()

    def test_incomplete_component_is_not_repaired_by_geometric_proximity(self):
        self.reject_before_terrain(
            self.index, self.selected[:-1], self.budget, "incomplete_source_component"
        )

    def test_missing_or_mixed_profiles_and_other_layers_remain_local_failures(self):
        nodes, original, incidence = self.index
        first = self.selected[0]
        for change, reason in (
            ({"maximum_gradient": None}, "ground_profile_required"),
            ({"width_m": 3.6}, "mixed_profiles"),
            ({"layer": 1}, "ground_profile_required"),
            ({"bridge": True}, "ground_profile_required"),
            ({"tunnel": True}, "ground_profile_required"),
        ):
            with self.subTest(change=change):
                spans = original | {first: replace(original[first], **change)}
                self.reject_before_terrain(
                    (nodes, spans, incidence), self.selected, self.budget, reason
                )

    def test_input_output_and_complete_observation_budgets_precede_terrain(self):
        for budget, reason in (
            (replace(self.budget, selected_spans=1), "selected_span_budget"),
            (replace(self.budget, terrain_samples=1), "mesh_or_sample_budget"),
            (replace(self.budget, terrain_samples=6), "complete_observation_budget"),
            (
                replace(
                    self.budget, surface=replace(self.budget.surface, max_vertices=1)
                ),
                "domain_vertex_budget",
            ),
        ):
            with self.subTest(reason=reason):
                self.reject_before_terrain(self.index, self.selected, budget, reason)

    def test_zero_geometry_time_does_no_terrain_work(self):
        budget = replace(
            self.budget,
            surface=replace(self.budget.surface, geometry_time_limit_s=0),
        )
        self.reject_before_terrain(
            self.index, self.selected, budget, "geometry_deadline"
        )

    def test_distinct_source_nodes_at_equal_positions_are_not_welded(self):
        source = self.shared["source"]
        xy = self.shared["axes"].copy()
        xy[1] = xy[0]
        with self.assertRaisesRegex(ValueError, "distinct_nodes_at_same_position"):
            layout(source, xy, self.budget)

    def test_extent_and_span_length_domains_are_explicit(self):
        with self.assertRaisesRegex(ValueError, "extent_budget"):
            layout(self.shared["source"], self.shared["axes"] * 100, self.budget)

    def test_crossing_bands_with_different_source_endpoints_do_not_create_a_join(self):
        source = replace(
            self.shared["source"], node_ids=(1, 2, 3, 4), bands=((1, 2), (3, 4))
        )
        with self.assertRaisesRegex(ValueError, "nonincident_band_intersection"):
            layout(source, np.array([[-2, 0], [2, 0], [0, -2], [0, 2]]), self.budget)

    def test_missing_dem_and_large_height_residual_never_produce_a_patch(self):
        with self.assertRaisesRegex(ValueError, "missing_actual_dem_stencil"):
            self.terrain(np.array([[0.0, 0.0]]))
        with self.assertRaisesRegex(ValueError, "missing_or_invalid_terrain"):
            generate(
                self.index,
                self.selected,
                lambda p: np.full(len(p), np.nan),
                self.budget,
            )

        def contradictory(points):
            result = np.full(len(points), 160.0)
            result[0] += 10
            return result

        with self.assertRaisesRegex(ValueError, "original_prior_limit"):
            generate(self.index, self.selected, contradictory, self.budget)

    def test_member_order_is_deterministic_and_original_objects_are_borrowed(self):
        a = prepare_sources(self.index, self.selected, self.budget)
        b = prepare_sources(self.index, tuple(reversed(self.selected)), self.budget)
        self.assertEqual(a, b)
        for member in a.members:
            self.assertIs(member, self.index[1][member.identifier])


if __name__ == "__main__":
    unittest.main()
