"""Actual source/finite/profile controls for the experimental three-arm recipe."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from branched_ground_layout import layout
from branched_ground_source import checked_star, source_input
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground_scope_field import (
    derive_scoped_ground_field,
)
from replay_branched_ground import INPUT, SEED, run


class BranchedGroundChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import json

        cls.bundle = load_bridge_source_facts(INPUT)
        manifest = json.loads((INPUT / "manifest.json").read_text())
        cls.terrain, cls.terrain_binding = terrain_input(
            FIXTURE, {"terrain": manifest["terrain_reference"]}
        )
        cls.terrain = staticmethod(cls.terrain)
        cls.positive = run(cls.bundle, cls.terrain)

    def test_actual_whole_field_and_three_own_frame_sections(self):
        report, inputs, field, mesh = self.positive
        self.assertEqual(report["state"], "FiniteGeometryDiagnostic")
        self.assertTrue(report["field"]["solver_termination_complete"])
        quality = report["terminals"]["whole_quality"]
        self.assertTrue(quality["geometry_complete"])
        self.assertLessEqual(quality["maximum_original_prior_vertex_error_m"], 0.5)
        self.assertLessEqual(quality["maximum_height_chord_bound_m"], 0.01)
        self.assertLessEqual(quality["maximum_normalized_continuous_grade"], 1.0000001)
        self.assertLessEqual(quality["maximum_normalized_finite_grade"], 1.0000001)
        self.assertEqual(quality["finite_contacts"]["original_ports"], 8)
        self.assertLessEqual(
            quality["finite_contacts"]["maximum_finite_contact_grade_jump"], 0.005
        )
        sections = report["terminals"]["independent_sections"]
        np.testing.assert_allclose(
            [v["own_width_m"] for v in sections], [1.5, 1.5, 2], atol=1e-12, rtol=0
        )
        self.assertLess(max(v["own_longitudinal_error_m"] for v in sections), 1e-12)
        owners = np.zeros(len(inputs.surface.vertices), int)
        owners[inputs.surface.triangles.ravel()] = np.repeat(inputs.face_parts, 3)
        heights, _ = field.plan.field.evaluate(
            inputs.surface.vertices[:, :2], owners, field.plan.plane_coefficients
        )
        self.assertLessEqual(
            abs(heights + field.plan.datum_m - inputs.surface.vertices[:, 2]).max(), 0.5
        )
        self.assertFalse(inputs.surface.vertices.flags.writeable)
        self.assertFalse(mesh.vertices.flags.writeable)
        with self.assertRaises(ValueError):
            field.domain.part_at(np.array([1000.0, 1000.0]))

    def test_original_observations_are_preserved_when_adding_plane_station(self):
        source, groups, ways, binding, _ = self.bundle
        index, edges, centre, leaves, _ = checked_star(
            source, SEED, groups, ways, binding, PortBudget()
        )
        _, original, _, _ = source_input(
            source,
            index,
            edges,
            centre,
            leaves,
            self.terrain,
            PortBudget(),
            proximal=False,
        )
        revised = self.positive[1]
        self.assertEqual(len(original.surface.vertices), 20)
        self.assertEqual(len(revised.surface.vertices), 24)
        self.assertLessEqual(
            set(map(tuple, original.surface.vertices)),
            set(map(tuple, revised.surface.vertices)),
        )

    def test_reversing_original_fact_order_keeps_the_same_mesh_and_values(self):
        source, groups, ways, binding, reader = self.bundle
        reordered = (
            source,
            tuple(reversed(groups)),
            tuple(reversed(ways)),
            binding,
            reader,
        )
        result = run(reordered, self.terrain)
        np.testing.assert_array_equal(result[3].vertices, self.positive[3].vertices)
        np.testing.assert_array_equal(result[3].triangles, self.positive[3].triangles)
        self.assertEqual(
            result[0]["terminals"]["values"], self.positive[0]["terminals"]["values"]
        )

    def test_original_chain_producer_still_rejects_before_dem(self):
        source, groups, ways, binding, _ = self.bundle

        def forbidden(_):
            self.fail("unchanged chain producer reached DEM")

        field, reason, report = derive_scoped_ground_field(
            source,
            SEED,
            groups,
            ways,
            forbidden,
            source_binding=source.binding,
            candidate_binding=binding,
            terrain_binding=self.terrain_binding,
            maximum_prior_change_m=0.5,
        )
        self.assertIsNone(field)
        self.assertEqual(reason, "branched_or_closed_ground_field_recipe_required")
        self.assertEqual(report["terrain_samples"], 0)

    def test_missing_group_and_incomplete_original_incidence_remain_negative(self):
        source, groups, ways, binding, _ = self.bundle
        index, _, centre, _, _ = checked_star(
            source, SEED, groups, ways, binding, PortBudget()
        )
        target = index.groups_by_node[centre]
        missing = tuple(g for g in groups if g.identifier != target.identifier)
        broken = tuple(
            replace(g, members=g.members[:-1])
            if g.identifier == target.identifier
            else g
            for g in groups
        )
        for values in (missing, broken):
            with self.assertRaises(ValueError):
                checked_star(source, SEED, values, ways, binding, PortBudget())

    def test_known_caps_precede_dem_and_bad_dem_is_not_a_zero_height(self):
        source, groups, ways, binding, _ = self.bundle
        budget = PortBudget()
        index, edges, centre, leaves, _ = checked_star(
            source, SEED, groups, ways, binding, budget
        )
        caps = [
            replace(budget, requests=2),
            replace(budget, terrain_samples=21),
            replace(budget, surface=replace(budget.surface, max_queries=2)),
            replace(budget, surface=replace(budget.surface, time_limit_s=0)),
            replace(budget, surface=replace(budget.surface, max_output_vertices=21)),
        ]

        def forbidden(_):
            self.fail("known insufficient budget reached DEM")

        for cap in caps:
            with self.assertRaises(ValueError):
                source_input(source, index, edges, centre, leaves, forbidden, cap)
        with self.assertRaisesRegex(
            ValueError, "missing_or_invalid_bound_terrain_samples"
        ):
            source_input(
                source,
                index,
                edges,
                centre,
                leaves,
                lambda xy: np.full(len(xy), np.nan),
                budget,
            )

    def test_missing_coverage_is_not_hidden_by_the_topology_recipe(self):
        source, groups, ways, binding, _ = self.bundle
        without_coverage = replace(source, binding={**source.binding, "coverage": ()})
        with self.assertRaisesRegex(ValueError, "ground_scope_coverage_gap"):
            checked_star(without_coverage, SEED, groups, ways, binding, PortBudget())

    def test_elapsed_geometry_budget_stops_before_dem(self):
        source, groups, ways, binding, _ = self.bundle
        budget = PortBudget()
        index, edges, centre, leaves, _ = checked_star(
            source, SEED, groups, ways, binding, budget
        )
        clock = [10.0]

        def expired_layout(*args, **kwargs):
            result = layout(*args, **kwargs)
            clock[0] = 10.2
            return result

        def forbidden(_):
            self.fail("expired geometry stage reached DEM")

        budget = replace(
            budget, surface=replace(budget.surface, geometry_time_limit_s=0.1)
        )
        with (
            patch(
                "branched_ground_source.time.monotonic", side_effect=lambda: clock[0]
            ),
            patch(
                "outshine_ref.transport.canonical_ports._ground_star_input.layout",
                side_effect=expired_layout,
            ),
            self.assertRaisesRegex(ValueError, "three_arm_ground_geometry_deadline"),
        ):
            source_input(source, index, edges, centre, leaves, forbidden, budget)

    def test_total_time_in_source_guards_is_not_restarted_for_geometry(self):
        clock = [10.0]

        def expired_guards(*args, **kwargs):
            result = checked_star(*args, **kwargs)
            clock[0] = 10.2
            return result

        def forbidden(_):
            self.fail("expired source preparation reached DEM")

        budget = PortBudget()
        budget = replace(budget, surface=replace(budget.surface, time_limit_s=0.1))
        with (
            patch(
                "replay_branched_ground.time.monotonic", side_effect=lambda: clock[0]
            ),
            patch("replay_branched_ground.checked_star", side_effect=expired_guards),
            self.assertRaisesRegex(ValueError, "three_arm_ground_total_deadline"),
        ):
            run(self.bundle, forbidden, budget=budget)

    def test_elapsed_dem_time_prevents_a_late_fit(self):
        clock = [10.0]

        def slow_terrain(xy):
            result = self.terrain(xy)
            clock[0] = 10.2
            return result

        budget = PortBudget()
        budget = replace(budget, surface=replace(budget.surface, time_limit_s=0.1))
        with (
            patch(
                "replay_branched_ground.time.monotonic", side_effect=lambda: clock[0]
            ),
            patch("replay_branched_ground.fit_ground") as fitting,
        ):
            with self.assertRaisesRegex(ValueError, "three_arm_ground_total_deadline"):
                run(self.bundle, slow_terrain, budget=budget)
            fitting.assert_not_called()

    def test_acute_arms_keep_the_original_hub_and_its_full_profiles(self):
        angles = np.deg2rad([0, 15, 30])
        ends = 10 * np.c_[np.cos(angles), np.sin(angles)]
        raw, _ = layout(ends, [1.5] * 3, [0.2] * 3)
        triangles = shapely.polygons(raw["xy"][raw["faces"][raw["owners"] == 0]])
        core = shapely.coverage_union_all(triangles)
        self.assertTrue(shapely.contains(core, shapely.Point(0, 0)))
        tangent = ends / np.linalg.norm(ends, axis=1)[:, None]
        normals = np.c_[-tangent[:, 1], tangent[:, 0]]
        for normal in normals:
            self.assertTrue(
                shapely.covers(
                    core, shapely.LineString([-0.75 * normal, 0.75 * normal])
                )
            )

    def test_short_overlapping_arms_do_not_gain_a_fake_surface(self):
        with self.assertRaises(ValueError):
            layout(
                np.array([[3, 0], [-3, 0], [0, 0.3]]), [1.5, 1.5, 2], [0.2, 0.2, 0.06]
            )


if __name__ == "__main__":
    unittest.main()
