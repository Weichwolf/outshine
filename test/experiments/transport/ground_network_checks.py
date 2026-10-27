"""Actual five-span geometry, strict source admission and old qualified boundaries."""

import json
import time
import unittest
from dataclasses import replace

import numpy as np
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from ground_network_probe import run
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground_network_input import (
    prepare_network_ground,
)
from outshine_ref.transport.canonical_ports._ground_recipe import NETWORK_RECIPE
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._ground_terminals import (
    derive_scoped_ground_terminals,
)
from outshine_ref.transport.canonical_ports._source import component
from replay_branched_ground import INPUT
from replay_ground_network import SEED


class ActualGroundNetwork(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bundle = load_bridge_source_facts(INPUT)
        manifest = json.loads((INPUT / "manifest.json").read_text())
        terrain, cls.terrain_binding = terrain_input(
            FIXTURE, {"terrain": manifest["terrain_reference"]}
        )
        cls.terrain = staticmethod(terrain)
        cls.positive = run(cls.bundle, cls.terrain, seed=SEED)

    def forbidden(self, _):
        self.fail("rejected request reached DEM")

    def call(self, *, seed=SEED, terrain=None, budget=None):
        source, groups, ways, binding, _ = self.bundle
        return derive_scoped_ground_terminals(
            source,
            seed,
            groups,
            ways,
            self.terrain if terrain is None else terrain,
            source_binding=source.binding,
            candidate_binding=binding,
            terrain_binding=self.terrain_binding,
            maximum_prior_change_m=0.5,
            geometry_recipe=NETWORK_RECIPE,
            budget=budget,
        )

    def test_five_bound_spans_form_a_valid_whole_surface_without_contact_authority(
        self,
    ):
        report, inputs, _, mesh = self.positive
        self.assertEqual(len(report["source_scope"]["members"]), 5)
        self.assertEqual((len(mesh.vertices), len(mesh.triangles)), (30, 28))
        self.assertEqual(len(inputs.port_edges), 10)
        quality = report["terminals"]["whole_quality"]
        self.assertTrue(quality["geometry_complete"])
        self.assertLessEqual(quality["maximum_original_prior_vertex_error_m"], 0.5)
        self.assertLessEqual(
            report["independent_maximum_normalized_finite_grade"], 1.0000001
        )
        self.assertLessEqual(
            quality["finite_contacts"]["maximum_finite_contact_grade_jump"], 0.005
        )
        self.assertEqual(
            report["source_admission_reason"], "ground_scope_missing_topology"
        )
        self.assertEqual(
            report["terminals"]["unavailable_reason"], "terminal_source_identity"
        )
        self.assertFalse(report["terminals"]["values"])
        self.assertEqual(report["qualified_world_joins"], 0)
        self.assertEqual(
            sum(row["method"] == "linear" for row in report["geometry"]["cut_fits"]), 1
        )
        self.assertEqual(
            sum(row["method"] == "common" for row in report["geometry"]["cut_fits"]), 2
        )
        self.assertFalse(mesh.vertices.flags.writeable)
        self.assertEqual(
            report["original_unknown_count"], len(self.bundle[0].unknown_nodes)
        )

    def test_original_facts_order_does_not_change_the_five_span_surface(self):
        source, groups, ways, binding, reader = self.bundle
        shuffled = (
            replace(source, nodes=source.nodes[::-1], spans=source.spans[::-1]),
            groups[::-1],
            ways[::-1],
            binding,
            reader,
        )
        other = run(shuffled, self.terrain, seed=SEED)
        np.testing.assert_array_equal(other[3].vertices, self.positive[3].vertices)
        np.testing.assert_array_equal(other[3].triangles, self.positive[3].triangles)

    def test_missing_topology_stays_pending_before_dem_at_the_shared_entry(self):
        field, reason, report = self.call(terrain=self.forbidden)
        self.assertIsNone(field)
        self.assertEqual(reason, "ground_scope_missing_topology")
        self.assertEqual(report["terrain_samples"], 0)

    def test_same_recipe_keeps_the_actual_qualified_three_arm_field_within_limits(self):
        field, reason, report = self.call(seed=(5699117369358000712, 1))
        self.assertFalse(reason)
        self.assertIsNotNone(field)
        self.assertEqual(report["state"], "QualifiedTerminalField")
        self.assertEqual(len(report["terminal_values"]), 3)
        self.assertLessEqual(
            report["terminal_quality"]["maximum_original_prior_vertex_error_m"], 0.5
        )
        self.assertEqual(
            report["source_scope"]["geometry_recipe"]["version"], NETWORK_RECIPE
        )
        self.assertEqual(
            report["geometry"]["cut_fits"][0]["attempted_methods"], ["linear"]
        )
        self.assertEqual(report["qualified_world_joins"], 0)

    def test_finite_input_caps_and_missing_dem_do_not_produce_zero_heights(self):
        source, groups, ways, binding, _ = self.bundle
        index, reason, _ = prepare_ground_scope_index(
            source,
            groups,
            ways,
            source_binding=source.binding,
            candidate_binding=binding,
        )
        self.assertFalse(reason)
        budget = PortBudget()
        selected, _, _ = component(index.index, SEED, index.unknown, budget)
        for capped in (
            replace(budget, requests=2),
            replace(budget, terrain_samples=29),
            replace(budget, surface=replace(budget.surface, max_output_faces=27)),
        ):
            with self.assertRaises(ValueError):
                prepare_network_ground(
                    index.index,
                    selected,
                    self.forbidden,
                    capped,
                    deadline=time.monotonic() + 10,
                )
        with self.assertRaisesRegex(
            ValueError, "missing_or_invalid_bound_terrain_samples"
        ):
            prepare_network_ground(
                index.index,
                selected,
                lambda x: np.full(len(x), np.nan),
                budget,
                deadline=time.monotonic() + 10,
            )


if __name__ == "__main__":
    unittest.main()
