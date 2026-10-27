"""Actual V2 finite-ground source scope; negative cases never invoke terrain."""

import argparse
import copy
import hashlib
import json
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import Mock

import numpy as np
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground_scope import (
    prepare_ground_field_scope,
    segment_covered,
)
from outshine_ref.transport.canonical_ports._ground_scope_field import (
    derive_scoped_ground_field,
)
from outshine_ref.transport.canonical_ports._source import component

SEED = (8409897393650800292, 0)
OTHER = (11909413124558356807, 0)


class ActualGroundFieldScope(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source, cls.groups, cls.ways, cls.binding, _ = load_bridge_source_facts(
            INPUT
        )
        manifest = json.loads((INPUT / "manifest.json").read_bytes())
        ref = manifest["terrain_reference"]
        cls.terrain_binding = {
            "sources": [ref],
            "source_datum": ref["source_datum"],
            "height_mapping": ref["height_mapping"],
        }

    def prepare(self, *, source=None, groups=None, ways=None, seed=SEED, budget=None):
        source = self.source if source is None else source
        return prepare_ground_field_scope(
            source,
            seed,
            self.groups if groups is None else groups,
            self.ways if ways is None else ways,
            source_binding=source.binding,
            candidate_binding=self.binding,
            budget=budget,
        )

    def pending_before_dem(
        self, reason, *, source=None, groups=None, ways=None, budget=None
    ):
        source = self.source if source is None else source
        terrain = Mock(side_effect=AssertionError("negative scope must not sample DEM"))
        prepared, actual, report = derive_scoped_ground_field(
            source,
            SEED,
            self.groups if groups is None else groups,
            self.ways if ways is None else ways,
            terrain,
            source_binding=source.binding,
            candidate_binding=self.binding,
            terrain_binding=self.terrain_binding,
            maximum_prior_change_m=0.5,
            budget=budget,
        )
        self.assertIsNone(prepared)
        self.assertEqual(actual, reason)
        self.assertEqual(report["terrain_samples"], 0)
        terrain.assert_not_called()

    def change_group(self, **updates):
        return tuple(
            replace(g, **updates) if g.identifier.endswith(":1152") else g
            for g in self.groups
        )

    def test_actual_component_and_typed_exits_are_bound_without_unknown_change(self):
        before = self.source.nodes, self.source.spans, self.source.unknown_nodes
        index, selected, binding, reason, work = self.prepare()
        self.assertEqual(reason, "")
        self.assertEqual(selected, tuple(sorted((SEED, OTHER))))
        self.assertEqual(len(binding["typed_exits"]), 2)
        self.assertEqual(
            {(row[6], row[8]) for row in binding["typed_exits"]},
            {(True, 1), (False, 1)},
        )
        self.assertEqual(work["qualified_member_visits"], 6)
        self.assertEqual(len(self.source.unknown_nodes), 3348)
        self.assertTrue(
            all(
                a is b
                for a, b in zip(
                    before,
                    (self.source.nodes, self.source.spans, self.source.unknown_nodes),
                    strict=True,
                )
            )
        )
        with self.assertRaises(TypeError):
            binding["members"] = ()
        self.assertEqual(
            component(index, SEED, set(self.source.unknown_nodes), PortBudget())[1],
            "unknown_source_frontier",
        )

    def test_seed_and_fact_order_do_not_change_scope(self):
        a = self.prepare()
        b = self.prepare(seed=OTHER, groups=self.groups[::-1], ways=self.ways[::-1])
        self.assertEqual(a[1], b[1])
        self.assertEqual(a[2], b[2])
        self.assertEqual(a[3], b[3])

    def test_controlled_sampler_field_is_deterministic_and_binding_is_semantic(self):
        # Positive fitter control only: actual source geometry, deliberately
        # controlled flat model heights, never presented as the captured DEM.
        terrain = lambda p: np.full(len(p), 188.0)
        terrain_binding = {
            "sources": [{"kind": "controlled_constant_model", "revision": "1"}],
            "source_datum": "controlled model metres",
            "height_mapping": "controlled constant188m/1",
        }

        def derive(seed, groups, ways, budget):
            return derive_scoped_ground_field(
                self.source,
                seed,
                groups,
                ways,
                terrain,
                source_binding=self.source.binding,
                candidate_binding=self.binding,
                terrain_binding=terrain_binding,
                maximum_prior_change_m=0.5,
                budget=budget,
            )

        a, reason, report_a = derive(SEED, self.groups, self.ways, PortBudget())
        self.assertEqual(reason, "")
        b, reason, report_b = derive(
            OTHER,
            self.groups[::-1],
            self.ways[::-1],
            replace(PortBudget(), selected_spans=4096),
        )
        self.assertEqual(reason, "")
        self.assertEqual(report_a["state"], "QualifiedField")
        self.assertEqual(report_a["binding"], report_b["binding"])
        self.assertEqual(report_a["qualified_world_joins"], 0)
        self.assertEqual(a[0], b[0])
        points = a[1].domain.triangles.mean(axis=1)
        for point in points:
            ha, ga = a[1].evaluate(point)
            hb, gb = b[1].evaluate(point)
            self.assertEqual(ha, hb)
            np.testing.assert_array_equal(ga, gb)
        self.assertEqual(len(self.source.unknown_nodes), 3348)

    def test_actual_dem_keeps_impossible_half_metre_prior_pending(self):
        from outshine_ref.transport.canonical_ports._ground import prepare_ground
        from outshine_ref.transport.canonical_ports._source import source_chain

        manifest = json.loads((INPUT / "manifest.json").read_bytes())
        ref = manifest["terrain_reference"]
        self.assertEqual(
            hashlib.sha256((FIXTURE / "dem.webp").read_bytes()).hexdigest(),
            ref["source_sha256"],
        )
        terrain, binding = terrain_input(FIXTURE, {"terrain": ref})
        prepared, reason, report = derive_scoped_ground_field(
            self.source,
            SEED,
            self.groups,
            self.ways,
            terrain,
            source_binding=self.source.binding,
            candidate_binding=self.binding,
            terrain_binding=binding,
            maximum_prior_change_m=0.5,
        )
        self.assertIsNone(prepared)
        self.assertEqual(reason, "ground_field_unavailable:ground_prior_error_limit")
        self.assertIsNotNone(report["source_scope"])
        self.assertEqual(report["binding"]["procedure"]["maximum_prior_change_m"], 0.5)
        index, selected, *_ = self.prepare()
        chain, reason = source_chain(index, selected)
        self.assertEqual(reason, "")
        _anchor, inputs, _samples = prepare_ground(chain, terrain, PortBudget())
        delta = inputs.surface.vertices[1::2] - inputs.surface.vertices[::2]
        # A valid gradient ellipse implies the conservative Euclidean bound
        # |grad h|<=g+cross. Each pair is an actual original strip edge.
        bound = (
            abs(delta[:, 2]) - (chain[3] + 0.025) * np.linalg.norm(delta[:, :2], axis=1)
        ) / 2
        self.assertGreater(float(max(bound)), 0.5)

    def test_actual_width_quarter_recipe_builds_identical_whole_fields(self):
        ref = json.loads((INPUT / "manifest.json").read_bytes())["terrain_reference"]
        self.assertEqual(
            hashlib.sha256((FIXTURE / "dem.webp").read_bytes()).hexdigest(),
            ref["source_sha256"],
        )
        terrain, terrain_binding = terrain_input(FIXTURE, {"terrain": ref})
        width = next(p.width_m for p in self.source.spans if p.identifier == SEED)
        limit = width / 4
        self.assertEqual(limit, 3.0)

        def derive(seed, groups, ways, budget):
            return derive_scoped_ground_field(
                self.source,
                seed,
                groups,
                ways,
                terrain,
                source_binding=self.source.binding,
                candidate_binding=self.binding,
                terrain_binding=terrain_binding,
                maximum_prior_change_m=limit,
                budget=budget,
            )

        a, reason, ra = derive(SEED, self.groups, self.ways, PortBudget())
        self.assertEqual(reason, "")
        b, reason, rb = derive(
            OTHER,
            self.groups[::-1],
            self.ways[::-1],
            replace(PortBudget(), selected_spans=4096),
        )
        self.assertEqual(reason, "")
        self.assertEqual(ra["state"], "QualifiedField")
        self.assertEqual(ra["field"]["selected_method"], "affine")
        self.assertEqual(ra["binding"], rb["binding"])
        self.assertEqual(ra["binding"]["procedure"]["maximum_prior_change_m"], 3.0)
        self.assertEqual(a[0], b[0])
        np.testing.assert_array_equal(ra["surface"].vertices, rb["surface"].vertices)
        np.testing.assert_array_equal(ra["surface"].triangles, rb["surface"].triangles)
        np.testing.assert_array_equal(
            ra["surface"].vertices[:, :2], ra["source_prior_surface"].vertices[:, :2]
        )
        np.testing.assert_array_equal(
            ra["surface"].triangles, ra["source_prior_surface"].triangles
        )
        self.assertLessEqual(
            float(
                max(
                    abs(
                        ra["surface"].vertices[:, 2]
                        - ra["source_prior_surface"].vertices[:, 2]
                    )
                )
            ),
            limit,
        )
        self.assertFalse(ra["surface"].vertices.flags.writeable)
        self.assertFalse(ra["surface"].triangles.flags.writeable)
        self.assertEqual(ra["terrain_samples"], 30)
        self.assertEqual(len(self.source.unknown_nodes), 3348)

    def test_missing_original_group_fails_before_dem(self):
        groups = tuple(g for g in self.groups if not g.identifier.endswith(":1152"))
        self.pending_before_dem("missing_ground_scope_original_group", groups=groups)

    def test_filtered_and_frontier_groups_fail_before_dem(self):
        self.pending_before_dem(
            "incomplete_ground_scope_original_incidence",
            groups=self.change_group(
                complete=False, original_endpoint_count=3, filtered_endpoint_count=1
            ),
        )
        self.pending_before_dem(
            "ground_scope_source_frontier", groups=self.change_group(frontier=True)
        )

    def test_missing_external_member_is_not_a_natural_free_end(self):
        group = next(g for g in self.groups if g.identifier.endswith(":1152"))
        self.pending_before_dem(
            "incomplete_ground_scope_original_incidence",
            groups=self.change_group(
                members=tuple(m for m in group.members if m.edge[:2] == SEED)
            ),
        )

    def test_unknown_exit_domain_and_missing_terminal_fail_before_dem(self):
        external = (15363644256701286015, 0)
        source = replace(
            self.source,
            spans=tuple(
                replace(p, bridge=False, layer=0) if p.identifier == external else p
                for p in self.source.spans
            ),
        )
        self.pending_before_dem("unknown_ground_scope_exit", source=source)
        group = next(g for g in self.groups if g.identifier.endswith(":1152"))
        self.pending_before_dem(
            "unqualified_ground_scope_exit_terminal",
            groups=self.change_group(
                members=tuple(
                    replace(m, source_terminal=False) if m.edge[:2] == external else m
                    for m in group.members
                )
            ),
        )

    def test_reason5_is_not_reclassified_as_a_known_exit(self):
        span = next(p for p in self.source.spans if p.identifier == SEED)
        ref = span.attributes["native_source_reference"]
        local = span.attributes["native_path"]["LocalWayBinding"]
        extra = {
            "Source": ref["Source"],
            "Feature": local["FeatureContent"],
            "Reason": 5,
        }
        source = replace(
            self.source,
            binding=dict(self.source.binding)
            | {"omissions": self.source.binding["omissions"] + (extra,)},
        )
        self.pending_before_dem("ground_scope_missing_topology", source=source)

    def test_missing_profile_word_and_source_profile_fail_before_dem(self):
        ways = tuple(
            replace(w, maximum_gradient_word=None) if w.source_way == 1398 else w
            for w in self.ways
        )
        self.pending_before_dem("ground_scope_profile_word_mismatch", ways=ways)
        source = replace(
            self.source,
            spans=tuple(
                replace(p, maximum_gradient=None) if p.identifier == SEED else p
                for p in self.source.spans
            ),
        )
        self.pending_before_dem("missing_source_profile_recipe", source=source)

    def test_missing_coverage_and_unexplained_unknown_are_not_ignored(self):
        source = replace(
            self.source, binding=dict(self.source.binding) | {"coverage": ()}
        )
        self.pending_before_dem("ground_scope_coverage_gap", source=source)
        source = replace(
            self.source, binding=dict(self.source.binding) | {"omissions": ()}
        )
        self.pending_before_dem("unqualified_ground_scope_unknown", source=source)

    def test_existing_work_and_time_budgets_fail_before_dem(self):
        self.pending_before_dem(
            "selected_span_budget;unknown_source_frontier",
            budget=replace(PortBudget(), selected_spans=1),
        )
        self.pending_before_dem(
            "ground_scope_fact_budget", budget=replace(PortBudget(), incidence_visits=1)
        )
        self.pending_before_dem(
            "ground_scope_time_budget",
            budget=replace(
                PortBudget(), surface=replace(PortBudget().surface, time_limit_s=0)
            ),
        )
        self.pending_before_dem(
            "ground_field_unavailable:ground_input_or_output_budget",
            budget=replace(
                PortBudget(),
                surface=replace(PortBudget().surface, max_output_vertices=1),
            ),
        )

    def test_coverage_union_rejects_a_gap_and_handles_short_dateline_span(self):
        boxes = (
            {"WestDeg": 0.0, "EastDeg": 1.0, "SouthDeg": 0.0, "NorthDeg": 1.0},
            {"WestDeg": 1.0, "EastDeg": 2.0, "SouthDeg": 0.0, "NorthDeg": 1.0},
        )
        self.assertTrue(segment_covered((0.25, 0.5), (1.75, 0.5), boxes))
        gap = copy.deepcopy(boxes)
        gap[1]["WestDeg"] = 1.1
        self.assertFalse(segment_covered((0.25, 0.5), (1.75, 0.5), gap))
        wrap = (
            {"WestDeg": 170.0, "EastDeg": 180.0, "SouthDeg": 0.0, "NorthDeg": 1.0},
            {"WestDeg": -180.0, "EastDeg": -170.0, "SouthDeg": 0.0, "NorthDeg": 1.0},
        )
        self.assertTrue(segment_covered((179.0, 0.5), (-179.0, 0.5), wrap))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(__file__).parent / "fixtures/bridge-source-facts",
    )
    args, remaining = parser.parse_known_args()
    INPUT = args.input
    unittest.main(argv=[__file__] + remaining)
