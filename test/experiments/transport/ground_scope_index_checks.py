"""Actual V2 ground-scope index reuse, ownership and unchanged admission caps."""

import unittest
from dataclasses import FrozenInstanceError, replace
from pathlib import Path
from unittest.mock import Mock, patch

import ground_field_scope_checks as requirements
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports import PortBudget, _ground_scope_field
from outshine_ref.transport.canonical_ports._ground_scope import (
    prepare_ground_field_scope,
)
from outshine_ref.transport.canonical_ports._ground_scope_field import (
    derive_scoped_ground_field,
)
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)

SEED = (8409897393650800292, 0)
OTHER = (11909413124558356807, 0)


class ActualGroundScopeIndex(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fixture = Path(__file__).parent / "fixtures/bridge-source-facts"
        cls.source, cls.groups, cls.ways, cls.binding, _ = load_bridge_source_facts(
            fixture
        )
        import json

        reference = json.loads((fixture / "manifest.json").read_text())[
            "terrain_reference"
        ]
        terrain, cls.terrain_binding = terrain_input(FIXTURE, {"terrain": reference})
        cls.terrain = staticmethod(terrain)
        cls.prepared, reason, cls.build_work = prepare_ground_scope_index(
            cls.source,
            cls.groups,
            cls.ways,
            source_binding=cls.source.binding,
            candidate_binding=cls.binding,
        )
        assert not reason, reason

    def scope(self, seed=SEED, *, indexed=True, budget=None, **changes):
        args = {
            "source": self.source,
            "groups": self.groups,
            "ways": self.ways,
            "source_binding": self.source.binding,
            "candidate_binding": self.binding,
        }
        args.update(changes)
        return prepare_ground_field_scope(
            seed=seed,
            budget=budget,
            scope_index=self.prepared if indexed else None,
            **args,
        )

    def test_actual_builder_owns_read_only_existing_indexes(self):
        self.assertEqual(
            self.build_work,
            {
                "indexed_nodes": 4433,
                "indexed_spans": 4743,
                "source_fact_member_visits": 9486,
            },
        )
        self.assertEqual(self.prepared.member_count, 9486)
        self.assertIs(self.prepared.source, self.source)
        with self.assertRaises(FrozenInstanceError):
            self.prepared.member_count = 0
        for mapping in (
            *self.prepared.index,
            self.prepared.groups_by_node,
            self.prepared.ways_by_id,
            self.prepared.omissions,
        ):
            with self.subTest(mapping=type(mapping)), self.assertRaises(TypeError):
                mapping["changed"] = 0
        self.assertTrue(
            all(isinstance(v, tuple) for v in self.prepared.index[2].values())
        )
        incidence = self.prepared.index[2]
        count = len(incidence)
        with self.assertRaises(KeyError):
            _ = incidence[2**64 - 1]
        self.assertEqual(len(incidence), count)

    def test_scope_and_binding_are_identical_to_single_use_in_both_orders(self):
        baseline = self.scope(indexed=False)
        self.assertEqual(baseline[3], "")
        for seed in (OTHER, SEED):
            current = self.scope(
                seed, budget=replace(PortBudget(), selected_spans=4096)
            )
            self.assertEqual(current[1:4], baseline[1:4])
            self.assertEqual(current[4]["source_fact_member_visits"], 0)
            self.assertEqual(current[4]["qualified_member_visits"], 6)
            self.assertEqual(current[4]["original_group_lookups"], 3)

    def test_one_hundred_local_queries_do_not_rebuild_or_rescan(self):
        with (
            patch(
                "outshine_ref.transport.canonical_ports._ground_scope.source_index",
                side_effect=AssertionError("no repeated source indexing"),
            ),
            patch(
                "outshine_ref.transport.canonical_ports._ground_scope.group_nodes",
                side_effect=AssertionError("no repeated full fact scan"),
            ),
            patch(
                "outshine_ref.transport.canonical_ports._ground_scope.source_omissions",
                side_effect=AssertionError("no repeated omissions scan"),
            ),
            patch(
                "outshine_ref.transport.canonical_ports._ground_scope.input_guard",
                side_effect=AssertionError("no repeated fact tuple scan"),
            ),
        ):
            for i in range(100):
                result = self.scope(SEED if i % 2 else OTHER)
                self.assertEqual(result[3], "")
                self.assertEqual(result[4]["indexed_nodes"], 0)
                self.assertEqual(result[4]["indexed_spans"], 0)
                self.assertEqual(result[4]["source_fact_member_visits"], 0)
                self.assertEqual(result[4]["qualified_member_visits"], 6)
        self.assertEqual(len(self.source.unknown_nodes), 3348)

    def test_later_smaller_caps_preserve_original_admission_and_exact_boundary(self):
        budgets = [
            replace(PortBudget(), incidence_visits=n)
            for n in (9485, 9486, 9489, 9490, 9495, 9496)
        ]
        budgets += [
            replace(PortBudget(), selected_spans=1),
            replace(PortBudget(), input_nodes=4432),
            replace(PortBudget(), input_spans=4742),
        ]
        for budget in budgets:
            with self.subTest(budget=budget):
                old, new = (
                    self.scope(indexed=False, budget=budget),
                    self.scope(budget=budget),
                )
                self.assertEqual(old[1:4], new[1:4])
                self.assertEqual(new[4]["source_fact_member_visits"], 0)
        self.assertEqual(self.scope(budget=budgets[5])[3], "")

    def test_mismatched_inputs_and_binding_do_not_reach_dem(self):
        changes = [
            {"source": replace(self.source)},
            {"groups": self.groups[::-1]},
            {"ways": self.ways[::-1]},
            {"candidate_binding": {**self.binding, "changed_revision": True}},
        ]
        for change in changes:
            sample = Mock(side_effect=AssertionError("no DEM for mismatched index"))
            args = {
                "source": self.source,
                "groups": self.groups,
                "ways": self.ways,
                "source_binding": self.source.binding,
                "candidate_binding": self.binding,
            }
            args.update(change)
            result, reason, report = derive_scoped_ground_field(
                seed=SEED,
                terrain=sample,
                terrain_binding=self.terrain_binding,
                maximum_prior_change_m=3,
                scope_index=self.prepared,
                **args,
            )
            self.assertIsNone(result)
            self.assertEqual(reason, "ground_scope_index_binding_mismatch")
            self.assertEqual(report["terrain_samples"], 0)
            sample.assert_not_called()

    def test_actual_field_and_mesh_are_identical_with_index_reuse(self):
        for method in ("auto", "cone"):
            reports = []
            for indexed in (False, True):
                field, reason, report = derive_scoped_ground_field(
                    self.source,
                    OTHER if indexed else SEED,
                    self.groups,
                    self.ways,
                    self.terrain,
                    source_binding=self.source.binding,
                    candidate_binding=self.binding,
                    terrain_binding=self.terrain_binding,
                    maximum_prior_change_m=3,
                    method=method,
                    scope_index=self.prepared if indexed else None,
                )
                self.assertEqual(reason, "")
                self.assertIsNotNone(field)
                reports.append(report)
            a, b = reports
            self.assertEqual(a["binding"], b["binding"])
            for value in ("surface_source_faces", "source_grade_frames"):
                self.assertEqual(a[value].tobytes(), b[value].tobytes())
            for value in ("vertices", "triangles"):
                self.assertEqual(
                    getattr(a["surface"], value).tobytes(),
                    getattr(b["surface"], value).tobytes(),
                )
            self.assertEqual(a["terrain_samples"], b["terrain_samples"])
            self.assertEqual(b["source_work"]["source_fact_member_visits"], 0)

    def test_missing_terminal_still_fails_before_dem_after_index_build(self):
        group = next(g for g in self.groups if g.identifier.endswith(":1152"))
        member = replace(group.members[1], source_terminal=None)
        changed = replace(group, members=(group.members[0], member))
        groups = tuple(
            changed if g.identifier == group.identifier else g for g in self.groups
        )
        prepared, reason, _ = prepare_ground_scope_index(
            self.source,
            groups,
            self.ways,
            source_binding=self.source.binding,
            candidate_binding=self.binding,
        )
        self.assertEqual(reason, "")
        sample = Mock(side_effect=AssertionError("no DEM for missing terminal"))
        field, reason, report = derive_scoped_ground_field(
            self.source,
            SEED,
            groups,
            self.ways,
            sample,
            source_binding=self.source.binding,
            candidate_binding=self.binding,
            terrain_binding=self.terrain_binding,
            maximum_prior_change_m=3,
            scope_index=prepared,
        )
        self.assertIsNone(field)
        self.assertEqual(reason, "unqualified_ground_scope_exit_terminal")
        self.assertEqual(report["terrain_samples"], 0)
        sample.assert_not_called()

    def test_zero_time_budget_prevents_index_build_and_field_sampling(self):
        budget = replace(
            PortBudget(), surface=replace(PortBudget().surface, time_limit_s=0)
        )
        result, reason, work = prepare_ground_scope_index(
            self.source,
            self.groups,
            self.ways,
            source_binding=self.source.binding,
            candidate_binding=self.binding,
            budget=budget,
        )
        self.assertIsNone(result)
        self.assertEqual(reason, "ground_scope_time_budget")
        self.assertFalse(any(work.values()))


class IndexedGroundScopeRequirements(requirements.ActualGroundFieldScope):
    @classmethod
    def setUpClass(cls):
        requirements.INPUT = Path(__file__).parent / "fixtures/bridge-source-facts"
        super().setUpClass()

    def setUp(self):
        def indexed_scope(source, seed, groups, ways, **kwargs):
            prepared, reason, work = prepare_ground_scope_index(
                source,
                groups,
                ways,
                source_binding=kwargs["source_binding"],
                candidate_binding=kwargs["candidate_binding"],
                budget=kwargs.get("budget"),
            )
            if reason:
                return None, (), None, reason, work
            kwargs["scope_index"] = prepared
            return prepare_ground_field_scope(source, seed, groups, ways, **kwargs)

        for target in (requirements, _ground_scope_field):
            context = patch.object(target, "prepare_ground_field_scope", indexed_scope)
            context.start()
            self.addCleanup(context.stop)


if __name__ == "__main__":
    unittest.main()
