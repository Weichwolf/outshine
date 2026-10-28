"""Qualify the actual paired native source, never generate oracles or read DEM."""

import argparse
import copy
import gzip
import json
import math
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import bridge_source_fact_inputs as inputs
from mvt_node_group_inputs import read_mvt_node_groups
from mvt_source_fact_digest import native_source_fact_digest
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._end_candidates import (
    float_word,
    inspect_bridge_end_candidates,
)


class ActualBridgeSourceFacts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = INPUT
        cls.source, cls.groups, cls.ways, cls.binding, cls.work = (
            inputs.load_bridge_source_facts(cls.root)
        )
        cls.native = json.loads(
            gzip.decompress((cls.root / "native.json.gz").read_bytes())
        )
        cls.facts = json.loads(
            gzip.decompress((cls.root / "source-facts.json.gz").read_bytes())
        )
        cls.manifest = json.loads((cls.root / "manifest.json").read_bytes())

    def inspect(self):
        return inspect_bridge_end_candidates(
            self.source,
            self.groups,
            self.ways,
            source_binding=self.source.binding,
            candidate_binding=self.binding,
        )

    def test_actual_paired_scope_and_complete_published_endpoint_coverage(self):
        self.assertEqual(len(self.source.nodes), len(self.native["Nodes"]))
        self.assertEqual(len(self.source.spans) * 2, len(self.facts["Members"]))
        self.assertEqual(len(self.groups), len(self.facts["Groups"]))
        self.assertEqual(len(self.ways), len(self.facts["Ways"]))
        self.assertEqual(
            self.work["profiles"]["bound_profile_paths"], len(self.native["Paths"])
        )
        self.assertEqual(
            self.work["links"]["validated_members"], len(self.facts["Members"])
        )
        self.assertEqual(
            self.binding["network_identity"], self.native["SourceIdentity"]
        )
        self.assertEqual(
            self.binding["source_generation"], self.native["SourceGeneration"]
        )

    def test_raw_binary32_profile_and_original_height_tags_are_both_required(self):
        unqualified_positive_word = 0
        for span in self.source.spans:
            recipe = span.attributes["recipe"]
            grade = float_word(recipe["native_maximum_gradient_word"])
            self.assertEqual(
                span.width_m, 2 * float_word(recipe["native_half_width_word"])
            )
            if (
                span.attributes["ground_height_recipe_qualified"]
                and math.isfinite(grade)
                and 0 < grade <= 10
            ):
                self.assertEqual(span.maximum_gradient, grade)
            else:
                self.assertIsNone(span.maximum_gradient)
                unqualified_positive_word += int(math.isfinite(grade) and grade > 0)
        self.assertGreater(unqualified_positive_word, 0)

    def test_native_frontier_and_original_counts_are_preserved_literally(self):
        with self.assertRaises(TypeError):
            self.binding["source_generation"] = 0
        with self.assertRaises(TypeError):
            self.binding["network_identity"]["Revision"] = "a" * 64
        for group, row in zip(self.groups, self.facts["Groups"], strict=True):
            self.assertEqual(
                (
                    group.frontier,
                    group.complete,
                    group.original_endpoint_count,
                    group.filtered_endpoint_count,
                ),
                (
                    row["Frontier"],
                    row["Complete"],
                    row["OriginalEndpointCount"],
                    row["FilteredEndpointCount"],
                ),
            )
        self.assertGreater(sum(g.frontier for g in self.groups), 0)
        if self.facts["schema"] in (
            "outshine/native-mvt-source-facts/2", "outshine/native-mvt-source-facts/3"
        ):
            self.assertEqual(
                tuple(m.source_terminal for g in self.groups for m in g.members),
                tuple(m["SourceTerminal"] for m in self.facts["Members"]),
            )
            self.assertTrue(
                any(
                    not m["SourceTerminal"]
                    and m["SourceStep"] == 0
                    and m["End"] ^ m["Edge"]["Direction"] == 0
                    for m in self.facts["Members"]
                )
            )

    def test_v1_extra_terminal_fields_are_not_digest_bound_witnesses(self):
        facts = copy.deepcopy(self.facts)
        facts["schema"] = "outshine/native-mvt-source-facts/1"
        for member in facts["Members"]:
            member["SourceTerminal"] = True
        true_digest = native_source_fact_digest(
            self.native["SourceIdentity"]["Revision"],
            bytes(facts["NodeGroups"]["Digest"]),
            facts,
        )
        for member in facts["Members"]:
            member["SourceTerminal"] = False
        self.assertEqual(
            true_digest,
            native_source_fact_digest(
                self.native["SourceIdentity"]["Revision"],
                bytes(facts["NodeGroups"]["Digest"]),
                facts,
            ),
        )
        partition, reason, _work = read_mvt_node_groups(
            self.source, 1, facts["NodeGroups"]
        )
        self.assertEqual(reason, "")
        groups, ways = inputs.candidate_values(self.source, partition, facts)
        self.assertTrue(
            all(
                member.source_terminal is None
                for group in groups
                for member in group.members
            )
        )
        result = inspect_bridge_end_candidates(
            self.source,
            groups,
            ways,
            source_binding=self.source.binding,
            candidate_binding=self.binding,
        )
        self.assertFalse(any(row["selected"] for row in result["groups"]))

    def test_v2_terminal_bit_is_hash_bound_before_source_preparation(self):
        facts = copy.deepcopy(self.facts)
        if facts["schema"] == "outshine/native-mvt-source-facts/1":
            self.assertTrue(
                all(m.source_terminal is None for g in self.groups for m in g.members)
            )
            return
        facts["Members"][0]["SourceTerminal"] = not facts["Members"][0][
            "SourceTerminal"
        ]
        with self.assertRaisesRegex(ValueError, "native fact digest"):
            inputs.check_scope(self.native, facts, self.manifest, PortBudget())

    def test_v2_native_readattribute_witness_is_checked_against_original_source(self):
        native = copy.deepcopy(self.native)
        if self.facts["schema"] == "outshine/native-mvt-source-facts/1":
            self.assertEqual(
                self.work["profiles"]["checked_height_profile_tag_records"], 0
            )
            return
        tag = next(
            t
            for p in native["Paths"]
            for t in p["LocalWayBinding"]["HeightProfileTags"].values()
            if t is not None
        )
        tag["NumberWord"] ^= 1
        raw = gzip.decompress((self.root / "source.mvt.gz").read_bytes())
        with self.assertRaisesRegex(ValueError, "height tag value mismatch"):
            inputs.native_recipes(native, self.facts, raw)
        self.assertEqual(
            self.work["profiles"]["checked_height_profile_tag_records"],
            2 * len(self.native["Paths"]),
        )

    def test_actual_transition_candidates_stay_pending_without_ground(self):
        before = self.source.nodes, self.source.spans, self.source.unknown_nodes
        result = self.inspect()
        self.assertEqual(result["state"], "Pending")
        self.assertEqual(result["reason"], "")
        self.assertEqual(result["qualified_contacts"], 0)
        self.assertEqual(result["work"]["expanded_pairs"], 0)
        selected = [row for row in result["groups"] if row["selected"]]
        if self.facts["schema"] == "outshine/native-mvt-source-facts/1":
            self.assertEqual(selected, [])
            self.assertIn(
                "missing_source_terminal", {row["reason"] for row in result["groups"]}
            )
            self.assertFalse(any(result["work"].values()))
        else:
            self.assertGreater(len(selected), 0)
        for row in selected:
            self.assertEqual(row["reason"], "missing_ground_profile")
            self.assertEqual(len(row["selected"]), 2)
            self.assertNotEqual(row["selected"][0][2], row["selected"][1][2])
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

    def test_retained_fixture_preserves_its_exact_unknown_count(self):
        pinned = Path(__file__).with_name("fixtures") / "bridge-source-facts"
        source, _, _, _, _ = inputs.load_bridge_source_facts(pinned)
        self.assertEqual(len(source.unknown_nodes), 3348)

    def test_source_only_loading_never_reads_or_decodes_terrain(self):
        original = Path.read_bytes

        def read(path):
            self.assertIn(
                path.name,
                (
                    "manifest.json",
                    "native.json.gz",
                    "source-facts.json.gz",
                    "source.mvt.gz",
                ),
            )
            return original(path)

        with patch.object(Path, "read_bytes", read):
            source, *_ = inputs.load_bridge_source_facts(self.root)
        self.assertEqual(source.binding, self.source.binding)
        self.assertEqual(self.work["profiles"]["decoded_geometry_words"], 0)
        self.assertEqual(
            self.work["profiles"]["additional_geometry_payload_copy_bytes"], 0
        )

    def test_independent_snapshot_generation_way_count_and_scope_mismatch(self):
        for key in ("SourceGeneration", "SourceWayCount"):
            native = copy.deepcopy(self.native)
            native[key] += 1
            with (
                self.subTest(key=key),
                self.assertRaisesRegex(ValueError, "independent snapshot scope"),
            ):
                inputs.check_scope(native, self.facts, self.manifest, PortBudget())
        facts = copy.deepcopy(self.facts)
        facts["NetworkIdentity"]["Revision"] = "a" * 64
        with self.assertRaisesRegex(ValueError, "paired source scope"):
            inputs.check_scope(self.native, facts, self.manifest, PortBudget())

    def test_actual_legacy_snapshot_cannot_be_mixed_with_current_facts(self):
        from canonical_port_native_inputs import FIXTURE

        legacy = json.loads(gzip.decompress((FIXTURE / "native.json.gz").read_bytes()))
        with self.assertRaisesRegex(
            ValueError, "paired source scope|independent snapshot scope"
        ):
            inputs.check_scope(legacy, self.facts, self.manifest, PortBudget())

    def test_budget_avoids_original_source_profile_and_terrain_work(self):
        original = Path.read_bytes

        def read(path):
            self.assertNotEqual(path.name, "source.mvt.gz")
            return original(path)

        with (
            patch.object(Path, "read_bytes", read),
            patch.object(inputs, "native_recipes") as profiles,
            self.assertRaisesRegex(ValueError, "paired source input budget"),
        ):
            inputs.load_bridge_source_facts(
                self.root, budget=replace(PortBudget(), input_nodes=1)
            )
        profiles.assert_not_called()

    def test_corrupt_paired_file_fails_before_source_preparation(self):
        original = Path.read_bytes

        def read(path):
            data = original(path)
            return (
                bytes([data[0] ^ 1]) + data[1:]
                if path.name == "source-facts.json.gz"
                else data
            )

        with (
            patch.object(Path, "read_bytes", read),
            patch.object(inputs, "source_input") as source,
            self.assertRaisesRegex(ValueError, "compressed digest"),
        ):
            inputs.load_bridge_source_facts(self.root)
        source.assert_not_called()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    args, remaining = parser.parse_known_args()
    INPUT = args.input
    unittest.main(argv=[__file__] + remaining)
