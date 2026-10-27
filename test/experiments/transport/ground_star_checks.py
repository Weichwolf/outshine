"""Bound production recipe admission, whole source geometry and terminal values."""

import json
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from branched_ground_terminals import audit_sections
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground_recipe import (
    THREE_ARM_PARAMETERS,
    THREE_ARM_RECIPE,
)
from outshine_ref.transport.canonical_ports._ground_scope import source_key
from outshine_ref.transport.canonical_ports._ground_scope_field import (
    derive_scoped_ground_field,
)
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._ground_terminals import (
    derive_scoped_ground_terminals,
)
from replay_branched_ground import INPUT, SEED
from replay_ground_star import main as replay_star


class GroundStarChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source, cls.groups, cls.ways, cls.binding, _ = load_bridge_source_facts(
            INPUT
        )
        manifest = json.loads((INPUT / "manifest.json").read_text())
        terrain, cls.terrain_binding = terrain_input(
            FIXTURE, {"terrain": manifest["terrain_reference"]}
        )
        cls.terrain = staticmethod(terrain)
        cls.index, reason, _ = prepare_ground_scope_index(
            cls.source,
            cls.groups,
            cls.ways,
            source_binding=cls.source.binding,
            candidate_binding=cls.binding,
        )
        assert not reason
        cls.positive = cls.call()

    @classmethod
    def call(
        cls,
        *,
        producer=derive_scoped_ground_terminals,
        source=None,
        seed=SEED,
        groups=None,
        ways=None,
        terrain=None,
        recipe=THREE_ARM_RECIPE,
        budget=None,
        index=None,
        method="auto",
    ):
        source = cls.source if source is None else source
        return producer(
            source,
            seed,
            cls.groups if groups is None else groups,
            cls.ways if ways is None else ways,
            cls.terrain if terrain is None else terrain,
            source_binding=source.binding,
            candidate_binding=cls.binding,
            terrain_binding=cls.terrain_binding,
            maximum_prior_change_m=0.5,
            geometry_recipe=recipe,
            budget=budget,
            scope_index=index,
            method=method,
        )

    def forbidden(self, _):
        self.fail("rejected admission reached DEM")

    def test_actual_affine_auto_result_survives_the_diagnostic_export(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.ground-star-export."
        ) as directory:
            output = Path(directory) / "affine"
            replay_star(
                SimpleNamespace(
                    output=output,
                    allocation_trace=None,
                    prior_limit=1000,
                    method="auto",
                    plot=False,
                )
            )
            report = json.loads((output / "report.json").read_text())
            self.assertEqual(report["state"], "QualifiedTerminalField")
            self.assertEqual(len(report["terminal_values"]), 3)
            self.assertEqual(
                report["binding"]["procedure"]["maximum_prior_change_m"], 1000
            )
            with np.load(output / "finite.npz", allow_pickle=False) as arrays:
                reconstructed = (
                    arrays["intercept"]
                    + (arrays["vertices"][:, :2] - arrays["origin"])
                    @ arrays["gradient"]
                )
                np.testing.assert_allclose(
                    reconstructed, arrays["vertices"][:, 2], atol=1e-8, rtol=0
                )
                self.assertNotIn("coefficients", arrays.files)

    def test_actual_whole_field_bound_recipe_and_three_own_sections(self):
        prepared, reason, report = self.positive
        self.assertEqual(reason, "")
        self.assertEqual(report["state"], "QualifiedTerminalField")
        self.assertEqual(report["qualified_world_joins"], 0)
        recipe = report["binding"]["source_scope"]["geometry_recipe"]
        self.assertEqual(recipe, THREE_ARM_PARAMETERS)
        self.assertEqual(recipe["version"], THREE_ARM_RECIPE)
        with self.assertRaises(TypeError):
            recipe["cut_and_remaining_arm_reserve_m"] = 0
        quality = report["terminal_quality"]
        self.assertLessEqual(quality["maximum_original_prior_vertex_error_m"], 0.5)
        self.assertLessEqual(quality["maximum_height_chord_bound_m"], 0.01)
        self.assertLessEqual(
            quality["finite_contacts"]["maximum_finite_contact_grade_jump"], 0.005
        )
        self.assertLessEqual(quality["maximum_normalized_finite_grade"], 1.0000001)
        self.assertLessEqual(quality["maximum_normalized_continuous_grade"], 1.0000001)
        sections = audit_sections(
            prepared[0], report["surface"], report["terminal_values"]
        )
        np.testing.assert_allclose(
            [v["own_width_m"] for v in sections], [1.5, 1.5, 2], atol=1e-12, rtol=0
        )
        self.assertEqual(len(report["binding"]["terminal_sections"]), 3)
        self.assertEqual(len(self.source.unknown_nodes), 3348)

    def test_bound_construction_parameters_match_the_actual_stations_and_cuts(self):
        report = self.positive[2]
        parameters = report["binding"]["source_scope"]["geometry_recipe"]
        geometry = report["geometry"]
        reserve = parameters["cut_and_remaining_arm_reserve_m"]
        tolerance = parameters["cut_residual_limit_m"]
        for cut, length, width, stations in zip(
            geometry["cuts_m"],
            geometry["lengths_m"],
            geometry["widths_m"],
            geometry["stations_m"],
            strict=True,
        ):
            self.assertGreaterEqual(length - cut, reserve - tolerance)
            self.assertGreaterEqual(
                cut,
                width / 2 * parameters["minimum_cut_half_width_fraction"] - tolerance,
            )
            expected_band = min(
                parameters["maximum_terminal_band_m"],
                parameters["terminal_band_remaining_arm_fraction"] * (length - cut),
            )
            self.assertAlmostEqual(length - stations[-2], expected_band, places=12)
            if length - cut > parameters["proximal_station_trigger_m"]:
                np.testing.assert_allclose(
                    np.array(stations[1:3]) - cut,
                    width * np.array(parameters["proximal_station_width_factors"]),
                    atol=1e-12,
                    rtol=0,
                )
            else:
                self.assertEqual(len(stations), 3)

    def test_unknown_recipes_are_pending_at_both_public_entries_before_dem(self):
        for producer in (derive_scoped_ground_field, derive_scoped_ground_terminals):
            for recipe in (
                "",
                "outshine/coarse-three-arm-ground/1",
                "other",
                False,
                [],
            ):
                prepared, reason, report = self.call(
                    producer=producer, recipe=recipe, terrain=self.forbidden
                )
                self.assertIsNone(prepared)
                self.assertEqual(reason, "unsupported_ground_geometry_recipe")
                self.assertEqual(report["terrain_samples"], 0)

    def test_default_and_wrong_topology_never_silently_change_geometry(self):
        p, reason, _ = self.call(recipe=None, terrain=self.forbidden)
        self.assertIsNone(p)
        self.assertEqual(reason, "branched_or_closed_ground_field_recipe_required")
        p, reason, r = self.call(seed=(8409897393650800292, 0), terrain=self.forbidden)
        self.assertIsNone(p)
        self.assertEqual(reason, "three_arm_ground_topology_required")
        self.assertEqual(r["terrain_samples"], 0)

    def test_index_reversal_and_larger_work_caps_keep_geometry_values_and_binding(self):
        baseline = self.positive[2]
        budget = PortBudget()
        larger = replace(
            budget,
            requests=budget.requests * 2,
            terrain_samples=budget.terrain_samples * 2,
            surface=replace(
                budget.surface,
                time_limit_s=budget.surface.time_limit_s * 2,
                max_output_vertices=budget.surface.max_output_vertices * 2,
                max_output_faces=budget.surface.max_output_faces * 2,
            ),
        )
        for args in (
            {"index": self.index},
            {
                "groups": tuple(reversed(self.groups)),
                "ways": tuple(reversed(self.ways)),
                "budget": larger,
            },
        ):
            _, reason, report = self.call(**args)
            self.assertEqual(reason, "")
            self.assertEqual(report["binding"], baseline["binding"])
            self.assertEqual(report["terminal_values"], baseline["terminal_values"])
            np.testing.assert_array_equal(
                report["surface"].vertices, baseline["surface"].vertices
            )
            np.testing.assert_array_equal(
                report["surface"].triangles, baseline["surface"].triangles
            )

    def test_stale_index_and_missing_original_group_are_pending_before_dem(self):
        centre_group = self.index.groups_by_node[12196311907019537324]
        groups = tuple(
            g for g in self.groups if g.identifier != centre_group.identifier
        )
        for args in (
            {"groups": groups},
            {"groups": tuple(reversed(self.groups)), "index": self.index},
        ):
            p, reason, report = self.call(terrain=self.forbidden, **args)
            self.assertIsNone(p)
            self.assertTrue(reason)
            self.assertEqual(report["terrain_samples"], 0)

    def test_local_missing_topology_and_frontier_are_not_geometry_permission(self):
        _, spans, _ = self.index.index
        source, feature = source_key(spans[SEED])
        binding = {
            **self.source.binding,
            "omissions": (
                *self.source.binding["omissions"],
                {"Source": source, "Feature": feature, "Reason": 5},
            ),
        }
        bad_source = replace(self.source, binding=binding)
        p, reason, r = self.call(source=bad_source, terrain=self.forbidden)
        self.assertIsNone(p)
        self.assertIn("missing_topology", reason)
        self.assertEqual(r["terrain_samples"], 0)
        centre = self.index.groups_by_node[12196311907019537324]
        groups = tuple(
            replace(g, frontier=True) if g.identifier == centre.identifier else g
            for g in self.groups
        )
        p, reason, r = self.call(groups=groups, terrain=self.forbidden)
        self.assertIsNone(p)
        self.assertIn("frontier", reason)
        self.assertEqual(r["terrain_samples"], 0)

    def test_known_three_request_time_and_payload_caps_precede_dem(self):
        b = PortBudget()
        caps = [
            replace(b, requests=2),
            replace(b, terrain_samples=23),
            replace(b, surface=replace(b.surface, max_queries=2)),
            replace(b, surface=replace(b.surface, geometry_time_limit_s=0)),
            replace(b, surface=replace(b.surface, max_output_vertices=23)),
        ]
        for budget in caps:
            p, reason, r = self.call(budget=budget, terrain=self.forbidden)
            self.assertIsNone(p)
            self.assertTrue(reason)
            self.assertEqual(r["terrain_samples"], 0)

    def test_existing_lp_variant_has_regular_termination_and_finite_prior_limit(self):
        _, reason, report = self.call(method="lp")
        self.assertEqual(reason, "")
        self.assertTrue(report["field"]["solver_termination_complete"])
        self.assertLessEqual(
            report["terminal_quality"]["maximum_original_prior_vertex_error_m"], 0.5
        )
        self.assertEqual(len(report["terminal_values"]), 3)


if __name__ == "__main__":
    unittest.main()
