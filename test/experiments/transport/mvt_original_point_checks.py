"""Controlled v4 wire/binding cases, not evidence of original native incidence."""

import copy
import gzip
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import mvt_source_fact_digest_checks as legacy
from bridge_source_fact_inputs import load_bridge_source_facts
from mvt_source_fact_digest import (
    native_source_fact_digest,
    validate_missing_source_ways,
)

UNKNOWN = 2**32 - 1
FIXTURE = Path(__file__).with_name("fixtures") / "missing-way-source-facts"


class OriginalPointWire(unittest.TestCase):
    digest = legacy.NativeSourceFactDigest.digest

    def setUp(self):
        legacy.OriginalMissingWayFacts.setUp(self)
        self.payload["schema"] = "outshine/native-mvt-source-facts/4"
        row = self.payload["MissingWays"][0]
        row["OriginalWayState"] = 1
        row["OriginalPoints"] = [
            {"State": state, "Group": group}
            for state, group in (
                (0, UNKNOWN),
                (1, UNKNOWN),
                (2, 0),
                (0, UNKNOWN),
                (1, UNKNOWN),
            )
        ]

    def test_independent_little_endian_vector(self):
        # Independent 576-byte int.to_bytes/array('d') wire checked with OpenSSL.
        self.assertEqual(
            self.digest().hex(),
            "91f4d55b5ccef13bbdc176715dd1da2a6cce76dd44692fd6bcb08ca778c777ce",
        )
        validate_missing_source_ways(self.native, self.payload)

    def test_every_ordered_point_state_and_group_is_bound(self):
        before = self.digest()
        for index in range(5):
            payload = copy.deepcopy(self.payload)
            point = payload["MissingWays"][0]["OriginalPoints"][index]
            point.update(State=1 if point["State"] == 0 else 0, Group=UNKNOWN)
            self.assertNotEqual(before, self.digest(payload))
        payload = copy.deepcopy(self.payload)
        payload["Groups"].append(copy.deepcopy(payload["Groups"][0]))
        before = self.digest(payload)
        payload["MissingWays"][0]["OriginalPoints"][2]["Group"] = 1
        self.assertNotEqual(before, self.digest(payload))
        payload = copy.deepcopy(self.payload)
        points = payload["MissingWays"][0]["OriginalPoints"]
        points[0], points[2] = points[2], points[0]
        self.assertNotEqual(self.digest(), self.digest(payload))

    def test_all_way_states_are_bound_without_claiming_absence(self):
        digests = []
        for state in range(5):
            payload = copy.deepcopy(self.payload)
            row = payload["MissingWays"][0]
            row["OriginalWayState"] = state
            row["OriginalPoints"] = [
                {"State": 0, "Group": UNKNOWN} for _ in row["Points"]
            ]
            digests.append(self.digest(payload))
        self.assertEqual(len(set(digests)), 5)

    def test_inexact_way_cannot_claim_any_node_assignment(self):
        for state in (0, 2, 3, 4):
            for point_state in (1, 2):
                payload = copy.deepcopy(self.payload)
                row = payload["MissingWays"][0]
                row["OriginalWayState"] = state
                row["OriginalPoints"][0] = {
                    "State": point_state,
                    "Group": 0 if point_state == 2 else UNKNOWN,
                }
                with (
                    self.subTest(state=state, point_state=point_state),
                    self.assertRaises(ValueError),
                ):
                    self.digest(payload)

    def test_invalid_state_count_and_group_fail(self):
        for key, values in (
            ("OriginalWayState", (None, True, -1, 5, 256, "1")),
            ("State", (None, True, -1, 3, 256, "2")),
            ("Group", (None, True, -1, 1, UNKNOWN, 2**32, "0")),
        ):
            for value in values:
                payload = copy.deepcopy(self.payload)
                row = payload["MissingWays"][0]
                target = row if key == "OriginalWayState" else row["OriginalPoints"][2]
                target[key] = value
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    self.digest(payload)
        for state in (0, 1):
            payload = copy.deepcopy(self.payload)
            payload["MissingWays"][0]["OriginalPoints"][0] = {
                "State": state,
                "Group": 0,
            }
            with self.assertRaises(ValueError):
                self.digest(payload)
        for count in (0, 4, 6):
            payload = copy.deepcopy(self.payload)
            row = payload["MissingWays"][0]
            row["OriginalPoints"] = [
                {"State": 0, "Group": UNKNOWN} for _ in range(count)
            ]
            with self.assertRaisesRegex(ValueError, "per complete polyline"):
                self.digest(payload)

    def test_missing_records_and_existing_limits_fail(self):
        for key in ("OriginalWayState", "OriginalPoints"):
            payload = copy.deepcopy(self.payload)
            del payload["MissingWays"][0][key]
            with self.assertRaises((ValueError, TypeError)):
                self.digest(payload)
        for kwargs in ({"point_limit": 4}, {"row_limit": 0}, {"version": 3}):
            with self.assertRaises(ValueError):
                self.digest(**kwargs)

    def test_v3_cannot_publish_unbound_v4_fields(self):
        payload = copy.deepcopy(self.payload)
        payload["schema"] = "outshine/native-mvt-source-facts/3"
        with self.assertRaisesRegex(ValueError, "v4 digest binding"):
            validate_missing_source_ways(self.native, payload)
        for row in payload["MissingWays"]:
            del row["OriginalWayState"], row["OriginalPoints"]
        validate_missing_source_ways(self.native, payload)


def controlled_pair(destination, change=None):
    """Extend a genuine old pair with controlled states; never a native v4 oracle."""
    manifest = json.loads((FIXTURE / "manifest.json").read_bytes())
    native = json.loads(gzip.decompress((FIXTURE / "native.json.gz").read_bytes()))
    facts = json.loads(gzip.decompress((FIXTURE / "source-facts.json.gz").read_bytes()))
    facts["schema"] = "outshine/native-mvt-source-facts/4"
    manifest["source_facts_schema"] = facts["schema"]
    for row in facts["MissingWays"]:
        row["OriginalWayState"] = 1
        row["OriginalPoints"] = [
            {"State": 2, "Group": 0},
            {"State": 1, "Group": UNKNOWN},
        ]
    facts["Digest"] = list(
        native_source_fact_digest(
            native["SourceIdentity"]["Revision"],
            bytes(facts["NodeGroups"]["Digest"]),
            facts,
        )
    )
    if change is not None:
        change(facts)
    manifest["source_facts_schema"] = facts["schema"]
    for name in manifest["files"]:
        if name == "source-facts.json.gz":
            raw = json.dumps(facts, separators=(",", ":")).encode()
            data = gzip.compress(raw, mtime=0)
            manifest["files"][name] = {
                "bytes": len(data),
                "raw_bytes": len(raw),
                "sha256": hashlib.sha256(data).hexdigest(),
                "raw_sha256": hashlib.sha256(raw).hexdigest(),
            }
        else:
            data = (FIXTURE / name).read_bytes()
        (destination / name).write_bytes(data)
    (destination / "manifest.json").write_text(json.dumps(manifest))
    return facts


class OriginalPointReader(unittest.TestCase):
    def test_invalid_or_unbound_states_fail_before_source_preparation(self):
        def downgrade(facts):
            facts["schema"] = "outshine/native-mvt-source-facts/3"

        def invalid_group(facts):
            facts["MissingWays"][0]["OriginalPoints"][0]["Group"] = UNKNOWN

        def incomplete(facts):
            facts["MissingWays"][0]["OriginalPoints"].pop()

        def valid_mutation(facts):
            facts["MissingWays"][0]["OriginalPoints"][0].update(State=1, Group=UNKNOWN)

        for change in (downgrade, invalid_group, incomplete, valid_mutation):
            with tempfile.TemporaryDirectory(prefix="outshine-point-states-") as folder:
                controlled_pair(Path(folder), change)
                with (
                    self.subTest(change=change.__name__),
                    patch("bridge_source_fact_inputs.source_input") as source,
                    self.assertRaises(ValueError),
                ):
                    load_bridge_source_facts(folder)
                source.assert_not_called()

    def test_controlled_v4_reaches_recursively_immutable_source_binding(self):
        with tempfile.TemporaryDirectory(prefix="outshine-point-states-") as folder:
            facts = controlled_pair(Path(folder))
            source, _, _, binding, _ = load_bridge_source_facts(folder)
        row = source.binding["missing_original_ways"][0]
        self.assertEqual(row["OriginalWayState"], 1)
        self.assertEqual(row["OriginalPoints"][0]["Group"], 0)
        self.assertEqual(row["OriginalPoints"][1]["Group"], UNKNOWN)
        self.assertEqual(
            source.binding["source_facts_digest"], binding["source_facts_digest"]
        )
        self.assertEqual(source.binding["source_facts_schema"], facts["schema"])
        facts["MissingWays"][0]["OriginalPoints"][0]["Group"] = 123
        self.assertEqual(row["OriginalPoints"][0]["Group"], 0)
        with self.assertRaises(TypeError):
            row["OriginalPoints"][0]["Group"] = 1
        with self.assertRaises(TypeError):
            row["OriginalPoints"] = ()

    def test_original_v3_pair_gets_no_invented_point_facts(self):
        source, *_ = load_bridge_source_facts(FIXTURE)
        self.assertEqual(
            source.binding["source_facts_schema"], "outshine/native-mvt-source-facts/3"
        )
        for row in source.binding["missing_original_ways"]:
            self.assertNotIn("OriginalWayState", row)
            self.assertNotIn("OriginalPoints", row)


if __name__ == "__main__":
    unittest.main()
