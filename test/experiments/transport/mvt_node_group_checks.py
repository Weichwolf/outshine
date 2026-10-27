"""Controlled checks of the actual #1486 header protocol, not a native capture."""

import copy
import unittest
from dataclasses import FrozenInstanceError, replace

from mvt_node_group_inputs import read_mvt_node_groups
from outshine_ref.transport.canonical_ports import PortBudget, PortNode, PortSource

NODES = (0x0102030405060708, 0x1112131415161718, 0x2122232425262728)
# Independently encoded literal LE wire bytes, checked with openssl dgst -sha256.
DIGEST = bytes.fromhex(
    "262aa336dc6ce51b5dba231954987cc173286d3b7599a1f955c2cd683ef6b261"
)


class NativeNodeGroupProtocol(unittest.TestCase):
    def setUp(self):
        self.source = PortSource(
            tuple(PortNode(node, (16.0, 48.0)) for node in NODES),
            (),
            (),
            {
                "source_identity": {
                    "DatasetId": "outshine/mvt/transport",
                    "Revision": "a" * 64,
                },
                "scope": "controlled header-field protocol only",
            },
        )
        self.payload = {
            "Groups": [
                {"FirstNode": 0, "NodeCount": 2, "SourceConnectionCount": 2},
                {"FirstNode": 2, "NodeCount": 1, "SourceConnectionCount": 0},
            ],
            "Nodes": list(NODES),
            "Digest": list(DIGEST),
        }

    def read(self, payload=None, source=None, **kwargs):
        return read_mvt_node_groups(
            source or self.source,
            1,
            self.payload if payload is None else payload,
            **kwargs,
        )

    def test_literal_native_byte_protocol_uint64_and_coincident_distinct_origins(self):
        before = self.source.nodes, self.source.spans, self.source.unknown_nodes
        partition, reason, work = self.read()
        self.assertEqual(reason, "")
        self.assertEqual(partition.digest, DIGEST)
        self.assertEqual(partition.nodes, NODES)
        self.assertEqual(partition.nodes_of(partition.groups[0]), NODES[:2])
        self.assertEqual(partition.nodes_of(partition.groups[1]), NODES[2:])
        self.assertEqual(
            tuple(group.source_connection_count for group in partition.groups), (2, 0)
        )
        self.assertFalse(partition.complete_original_incidence)
        self.assertEqual(
            work,
            {
                "validated_source_nodes": 3,
                "validated_groups": 2,
                "digest_node_words": 3,
                "expanded_pairs": 0,
            },
        )
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

    def test_owned_partition_does_not_borrow_mutable_dump_arrays(self):
        partition, reason, _work = self.read()
        self.assertFalse(reason)
        self.payload["Nodes"].reverse()
        self.payload["Groups"][0]["SourceConnectionCount"] = 99
        self.assertEqual(partition.nodes, NODES)
        self.assertEqual(partition.groups[0].source_connection_count, 2)
        with self.assertRaises(FrozenInstanceError):
            partition.groups[0].node_count = 99

    def test_empty_captured_partition_differs_from_missing_capture(self):
        source = replace(self.source, nodes=())
        digest = bytes.fromhex(
            "c6dd54fe334f7481b62b976580d500b0e141644953263d3bb62bdcccc808ecac"
        )
        partition, reason, work = self.read(
            {"Groups": [], "Nodes": [], "Digest": digest}, source=source
        )
        self.assertFalse(reason)
        self.assertEqual(partition.groups, ())
        self.assertEqual(partition.nodes, ())
        self.assertFalse(partition.complete_original_incidence)
        self.assertFalse(any(work.values()))

    def test_not_requested_budget_exceeded_and_missing_state_stay_pending(self):
        for state, expected in (
            (0, "node_groups_not_requested"),
            (2, "node_group_capture_budget_exceeded"),
            (None, "missing_node_group_capture_state"),
            (True, "invalid_node_group_capture_state"),
        ):
            with self.subTest(state=state):
                partition, reason, work = read_mvt_node_groups(self.source, state, None)
                self.assertIsNone(partition)
                self.assertEqual(reason, expected)
                self.assertFalse(any(work.values()))
        self.assertEqual(
            read_mvt_node_groups(self.source, 0, self.payload)[1],
            "node_group_state_payload_mismatch",
        )
        self.assertEqual(
            read_mvt_node_groups(self.source, 1, None)[1],
            "missing_captured_node_groups",
        )

    def test_csr_ranges_are_validated_even_though_firstnode_is_not_hashed(self):
        for key, value, expected in (
            ("FirstNode", 1, "invalid_node_group_csr_range"),
            ("NodeCount", 4, "invalid_node_group_csr_range"),
            ("NodeCount", 0, "invalid_node_group_record"),
            ("SourceConnectionCount", True, "invalid_node_group_record"),
            ("SourceConnectionCount", 2**64, "invalid_node_group_record"),
        ):
            with self.subTest(key=key, value=value):
                payload = copy.deepcopy(self.payload)
                payload["Groups"][0][key] = value
                self.assertEqual(self.read(payload)[1], expected)

    def test_partition_must_cover_existing_nodes_once_without_float_ids(self):
        for nodes, expected in (
            ([NODES[0], NODES[0], NODES[2]], "incomplete_published_node_partition"),
            ([NODES[0], NODES[1], 55], "incomplete_published_node_partition"),
            ([float(NODES[0]), NODES[1], NODES[2]], "invalid_partition_node"),
            ([0, NODES[1], NODES[2]], "invalid_partition_node"),
        ):
            with self.subTest(nodes=nodes):
                payload = dict(self.payload, Nodes=nodes)
                self.assertEqual(self.read(payload)[1], expected)

    def test_canonical_node_and_group_order_is_required(self):
        payload = copy.deepcopy(self.payload)
        payload["Nodes"][:2] = reversed(payload["Nodes"][:2])
        self.assertEqual(self.read(payload)[1], "noncanonical_node_group_order")
        payload = {
            "Groups": [
                {"FirstNode": 0, "NodeCount": 1, "SourceConnectionCount": 0},
                {"FirstNode": 1, "NodeCount": 2, "SourceConnectionCount": 2},
            ],
            "Nodes": [NODES[2], NODES[0], NODES[1]],
            "Digest": DIGEST,
        }
        self.assertEqual(self.read(payload)[1], "noncanonical_node_group_order")

    def test_bound_revision_count_and_digest_changes_cannot_be_silently_accepted(self):
        source = replace(
            self.source,
            binding={
                "source_identity": {
                    "DatasetId": "outshine/mvt/transport",
                    "Revision": "b" * 64,
                }
            },
        )
        self.assertEqual(self.read(source=source)[1], "node_group_digest_mismatch")
        payload = copy.deepcopy(self.payload)
        payload["Groups"][0]["SourceConnectionCount"] = 3
        self.assertEqual(self.read(payload)[1], "node_group_digest_mismatch")
        self.assertEqual(
            self.read(dict(self.payload, Digest=DIGEST[:-1]))[1],
            "invalid_node_group_digest",
        )
        self.assertEqual(
            self.read(source=replace(self.source, binding={"source_identity": None}))[
                1
            ],
            "node_group_source_identity_required",
        )

    def test_group_members_retain_one_actual_position(self):
        source = replace(
            self.source,
            nodes=(
                self.source.nodes[0],
                replace(self.source.nodes[1], longitude_latitude=(17.0, 48.0)),
                self.source.nodes[2],
            ),
        )
        self.assertEqual(self.read(source=source)[1], "node_group_position_mismatch")

    def test_input_bound_precedes_allocating_a_source_index(self):
        partition, reason, work = self.read(budget=PortBudget(input_nodes=2))
        self.assertIsNone(partition)
        self.assertEqual(reason, "node_group_input_budget")
        self.assertFalse(any(work.values()))


if __name__ == "__main__":
    unittest.main()
