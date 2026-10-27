"""Controlled DTO/source cross-links; no claim of an actual paired export."""

import copy
import unittest
from dataclasses import replace

from mvt_node_group_inputs import NodeGroupPartition, NodeGroupRow
from mvt_source_fact_digest import native_source_fact_digest
from mvt_source_fact_links import check_mvt_source_fact_links
from outshine_ref.transport.canonical_ports import (
    PortBudget,
    PortNode,
    PortSource,
    PortSpan,
)


class NativeSourceFactLinks(unittest.TestCase):
    def setUp(self):
        self.partition = NodeGroupPartition(
            (NodeGroupRow(0, 1, 1), NodeGroupRow(1, 2, 2), NodeGroupRow(3, 1, 0)),
            (1, 2, 3, 4),
            b"p" * 32,
            "a" * 64,
        )
        self.payload = {
            "SourceGeneration": 7,
            "SourceWayCount": 2,
            "Groups": [],
            "Members": [],
            "Ways": [],
        }
        spans = []
        for way, nodes, direction in ((0, (1, 2), 0), (1, (3, 4), 1)):
            edge = {
                "PathId": 100 + way,
                "SegmentOrdinal": 3 + way,
                "Direction": direction,
            }
            alias = {
                "Id": edge,
                "FromNodeId": nodes[direction],
                "ToNodeId": nodes[1 - direction],
            }
            local = {"StreetWay": way, "Feature": 10 + way, "Ring": 20 + way}
            attributes = {
                "native_directed_aliases": [alias],
                "native_path": {"LocalWayBinding": local},
                "native_source_reference": {"SegmentOrdinal": 0},
            }
            spans.append(
                PortSpan(
                    (100 + way, 3 + way),
                    nodes,
                    4.0,
                    None,
                    bool(way),
                    False,
                    attributes,
                    layer=way,
                )
            )
            for physical, node in enumerate(nodes):
                self.payload["Members"].append(
                    {
                        "Edge": dict(edge),
                        "End": physical ^ direction,
                        "DomainNode": node,
                        "NodeGroup": (0, 1, 1, 2)[node - 1],
                        "StreetWay": way,
                        "SourceStep": 0,
                    }
                )
            self.payload["Ways"].append(
                dict(
                    local,
                    PointCount=2,
                    HalfWidthWord=0x40000000,
                    MaximumGradientWord=0,
                    EndStencil=[{"LongitudeDeg": 16.0, "LatitudeDeg": 48.0}] * 4,
                    OwnsEnds=[True, True],
                )
            )
        self.payload["Members"].sort(
            key=lambda m: (
                m["NodeGroup"],
                m["Edge"]["PathId"],
                m["Edge"]["SegmentOrdinal"],
                m["Edge"]["Direction"],
                m["End"],
            )
        )
        for first, count in ((0, 1), (1, 2), (3, 1)):
            self.payload["Groups"].append(
                {
                    "FirstMember": first,
                    "MemberCount": count,
                    "OriginalEndpointCount": count,
                    "FilteredEndpointCount": 0,
                    "Complete": True,
                    "Frontier": False,
                }
            )
        self.source = PortSource(
            tuple(PortNode(i, (16.0, 48.0)) for i in (1, 2, 3, 4)),
            tuple(spans),
            (2, 3),
            {
                "source_identity": {
                    "DatasetId": "outshine/mvt/transport",
                    "Revision": "a" * 64,
                }
            },
        )

    def check(self, payload=None, *, source=None, **kwargs):
        payload = copy.deepcopy(self.payload if payload is None else payload)
        payload["Digest"] = native_source_fact_digest(
            "a" * 64, self.partition.digest, payload
        )
        return check_mvt_source_fact_links(
            source or self.source, self.partition, payload, **kwargs
        )

    def test_actual_directed_reverse_end_source_step_distinct_from_fragment(self):
        before = repr(self.source), copy.deepcopy(self.payload)
        reason, work = self.check()
        self.assertEqual(reason, "")
        self.assertEqual(
            work,
            {
                "indexed_spans": 2,
                "validated_groups": 3,
                "validated_members": 4,
                "expanded_pairs": 0,
            },
        )
        self.assertEqual(before, (repr(self.source), self.payload))
        self.assertEqual(self.source.unknown_nodes, (2, 3))
        self.assertIsNone(self.source.spans[0].maximum_gradient)

    def test_filtered_incoming_count_is_preserved_without_admission(self):
        group = self.payload["Groups"][1]
        group.update(OriginalEndpointCount=3, FilteredEndpointCount=1, Complete=False)
        self.assertEqual(self.check()[0], "")
        group["Complete"] = True
        self.assertEqual(self.check()[0], "source_fact_group_completeness_mismatch")

    def test_redigested_wrong_endpoint_partition_way_step_alias_rejected(self):
        for key, value, expected in (
            ("DomainNode", 4, "source_member_endpoint_mismatch"),
            ("StreetWay", 1, "source_member_way_binding_mismatch"),
            ("SourceStep", 3, "source_member_step_mismatch"),
        ):
            payload = copy.deepcopy(self.payload)
            payload["Members"][0][key] = value
            self.assertEqual(self.check(payload)[0], expected)
        payload = copy.deepcopy(self.payload)
        payload["Members"][0]["Edge"]["Direction"] = 1
        payload["Members"][0]["End"] = 1
        self.assertEqual(
            self.check(payload)[0], "source_member_directed_alias_mismatch"
        )
        payload = copy.deepcopy(self.payload)
        payload["Ways"][0]["Feature"] += 1
        self.assertEqual(self.check(payload)[0], "source_member_way_binding_mismatch")
        payload = copy.deepcopy(self.payload)
        payload["Members"][0].update(End=1, DomainNode=2)
        self.assertEqual(self.check(payload)[0], "source_member_partition_mismatch")

    def test_opposite_directed_alias_cannot_repeat_a_physical_endpoint(self):
        span = self.source.spans[0]
        aliases = span.attributes["native_directed_aliases"] + (
            {
                "Id": {"PathId": 100, "SegmentOrdinal": 3, "Direction": 1},
                "FromNodeId": 2,
                "ToNodeId": 1,
            },
        )
        source = replace(
            self.source,
            spans=(
                replace(
                    span,
                    attributes=dict(span.attributes)
                    | {"native_directed_aliases": aliases},
                ),
                self.source.spans[1],
            ),
        )
        self.payload["Members"][2] = {
            "Edge": {"PathId": 100, "SegmentOrdinal": 3, "Direction": 1},
            "End": 0,
            "DomainNode": 2,
            "NodeGroup": 1,
            "StreetWay": 0,
            "SourceStep": 0,
        }
        self.assertEqual(
            self.check(source=source)[0], "duplicate_physical_source_endpoint"
        )

    def test_csr_sort_and_full_physical_coverage_checked_after_rehash(self):
        payload = copy.deepcopy(self.payload)
        payload["Groups"][1]["FirstMember"] = 0
        self.assertEqual(self.check(payload)[0], "source_fact_group_range_mismatch")
        payload = copy.deepcopy(self.payload)
        payload["Members"][1:3] = reversed(payload["Members"][1:3])
        self.assertEqual(self.check(payload)[0], "noncanonical_source_member_order")
        payload = copy.deepcopy(self.payload)
        payload["Members"].pop()
        payload["Groups"][2].update(
            FirstMember=0, MemberCount=0, OriginalEndpointCount=0
        )
        self.assertEqual(
            self.check(payload)[0], "incomplete_published_source_endpoints"
        )

    def test_frontier_is_not_reconstructed_from_fragment_source_step(self):
        self.payload["Ways"][1]["OwnsEnds"][0] = False
        self.assertEqual(self.check()[0], "")
        self.payload["Groups"][1]["Frontier"] = True
        self.assertEqual(self.check()[0], "")
        self.assertTrue(self.payload["Groups"][1]["Frontier"])

    def test_revision_digest_budget_and_way_table_fail_before_admission(self):
        source = replace(
            self.source, binding={"source_identity": {"Revision": "b" * 64}}
        )
        self.assertEqual(
            self.check(source=source)[0], "source_fact_partition_revision_mismatch"
        )
        payload = copy.deepcopy(self.payload)
        payload["Digest"] = b"x" * 32
        reason, work = check_mvt_source_fact_links(self.source, self.partition, payload)
        self.assertEqual(reason, "source_fact_digest_mismatch")
        self.assertFalse(any(work.values()))
        reason, work = self.check(budget=replace(PortBudget(), incidence_visits=3))
        self.assertEqual(reason, "source_fact_link_budget")
        self.assertFalse(any(work.values()))
        self.payload["Ways"].reverse()
        self.assertEqual(self.check()[0], "source_fact_way_table_mismatch")


if __name__ == "__main__":
    unittest.main()
