"""Actual directed/physical identity controls for the proposed retained-fact consumer."""

import struct
import unittest
from dataclasses import replace

from outshine_ref.transport.canonical_ports import (
    PortBudget,
    PortNode,
    PortSource,
    PortSpan,
)
from outshine_ref.transport.canonical_ports._end_candidates import (
    CandidateGroup,
    CandidateMember,
    CandidateWay,
    inspect_bridge_end_candidates,
)


def word(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


def pair(offset=0):
    """Controlled native-shaped records, not an actual MVT capture."""
    west, anchor, east = (
        (16.0 + offset * 0.01, 48.0),
        (16.001 + offset * 0.01, 48.0),
        (16.002 + offset * 0.01, 48.0),
    )
    grade = struct.unpack("<f", struct.pack("<I", word(0.12)))[0]
    nodes = tuple(
        PortNode(4 * offset + i, xy)
        for i, xy in ((1, west), (2, anchor), (3, anchor), (4, east))
    )
    spans, ways, members = [], [], []
    for bridge, coordinates, ids, ordinal in (
        (False, (west, anchor), (nodes[0].identifier, nodes[1].identifier), 3),
        (True, (anchor, east), (nodes[2].identifier, nodes[3].identifier), 7),
    ):
        path = 100 + 2 * offset + int(bridge)
        way = 2 * offset + int(bridge)
        aliases = tuple(
            {
                "Id": {
                    "PathId": path,
                    "SegmentOrdinal": ordinal,
                    "Direction": direction,
                },
                "FromNodeId": ids[direction],
                "ToNodeId": ids[1 - direction],
            }
            for direction in (0, 1)
        )
        attributes = {
            "native_directed_aliases": aliases,
            "native_path": {"LocalWayBinding": {"StreetWay": way}},
            "native_source_reference": {"SegmentOrdinal": 0},
        }
        spans.append(
            PortSpan(
                (path, ordinal),
                ids,
                4.0,
                grade,
                bridge,
                False,
                attributes,
                layer=int(bridge),
            )
        )
        ways.append(
            CandidateWay(
                way, 2, coordinates, coordinates, (True, True), word(2.0), word(0.12)
            )
        )
        end = 0 if bridge else 1
        members.append(CandidateMember((path, ordinal, 0), end, ids[end], way))
    group = CandidateGroup(
        f"controlled/{offset}", anchor, tuple(members), 2, 0, True, False
    )
    return nodes, tuple(spans), tuple(ways), group


class BridgeEndCandidates(unittest.TestCase):
    def setUp(self):
        nodes, spans, self.ways, self.group = pair()
        self.source = PortSource(
            nodes, spans, (2, 3), {"scope": "controlled source only", "revision": 1}
        )
        self.binding = {"scope": "controlled candidate content only", "revision": 1}

    def inspect(self, groups=None, source=None, ways=None, **kwargs):
        return inspect_bridge_end_candidates(
            source or self.source,
            (self.group,) if groups is None else groups,
            self.ways if ways is None else ways,
            source_binding=self.source.binding,
            candidate_binding=self.binding,
            **kwargs,
        )

    def reason(self, result):
        self.assertEqual(result["qualified_contacts"], 0)
        self.assertEqual(result["state"], "Pending")
        self.assertEqual(result["work"]["expanded_pairs"], 0)
        return result["groups"][0]["reason"] if result["groups"] else result["reason"]

    def test_selected_pair_retains_distinct_domain_nodes_and_missing_ground(self):
        before = repr(self.source)
        result = self.inspect()
        self.assertEqual(self.reason(result), "missing_ground_profile")
        selected = result["groups"][0]["selected"]
        self.assertEqual(selected, (((101, 7), 0, 3), ((100, 3), 1, 2)))
        self.assertNotEqual(selected[0][2], selected[1][2])
        self.assertEqual(self.source.unknown_nodes, (2, 3))
        self.assertEqual(repr(self.source), before)

    def test_reverse_directed_member_and_member_order_have_same_physical_result(self):
        reverse = tuple(
            replace(member, edge=(*member.edge[:2], 1), end=1 - member.end)
            for member in self.group.members[::-1]
        )
        result = self.inspect((replace(self.group, members=reverse),))
        self.assertEqual(result, self.inspect())

    def test_only_published_reverse_alias_is_supported_without_inventing_forward(self):
        reverse_spans = tuple(
            replace(
                span,
                attributes=dict(span.attributes)
                | {
                    "native_directed_aliases": span.attributes[
                        "native_directed_aliases"
                    ][1:]
                },
            )
            for span in self.source.spans
        )
        source = replace(self.source, spans=reverse_spans)
        self.assertEqual(
            self.reason(self.inspect(source=source)), "missing_directed_edge_binding"
        )
        reverse = tuple(
            replace(member, edge=(*member.edge[:2], 1), end=1 - member.end)
            for member in self.group.members
        )
        result = self.inspect((replace(self.group, members=reverse),), source=source)
        self.assertEqual(self.reason(result), "missing_ground_profile")

    def test_incomplete_frontier_and_junctions_do_not_index_or_select(self):
        cases = (
            (
                replace(self.group, original_endpoint_count=3),
                "incomplete_original_incidence",
            ),
            (
                replace(
                    self.group, original_endpoint_count=3, filtered_endpoint_count=1
                ),
                "incomplete_original_incidence",
            ),
            (replace(self.group, complete=False), "incomplete_original_incidence"),
            (replace(self.group, frontier=True), "source_frontier"),
            (
                replace(
                    self.group,
                    members=self.group.members[:1],
                    original_endpoint_count=1,
                ),
                "ambiguous_incidence",
            ),
        )
        for group, expected in cases:
            with self.subTest(expected=expected):
                result = self.inspect((group,))
                self.assertEqual(self.reason(result), expected)
                self.assertFalse(any(result["work"].values()))
                self.assertFalse(result["groups"][0]["selected"])

    def test_coincident_endpoint_from_another_domain_is_not_a_binding(self):
        ground, bridge = self.group.members
        group = replace(self.group, members=(ground, replace(bridge, domain_node=2)))
        self.assertEqual(
            self.reason(self.inspect((group,))), "member_domain_binding_mismatch"
        )

    def test_source_fragment_ordinal_cannot_be_replaced_by_source_step(self):
        ground, bridge = self.group.members
        group = replace(self.group, members=(ground, replace(bridge, edge=(101, 0, 0))))
        self.assertEqual(
            self.reason(self.inspect((group,))), "missing_physical_source_edge"
        )

    def test_hidden_same_domain_incidence_is_not_a_unique_terminal(self):
        other = replace(self.source.spans[1], identifier=(999, 0), nodes=(3, 1))
        source = replace(self.source, spans=self.source.spans + (other,))
        self.assertEqual(
            self.reason(self.inspect(source=source)), "interior_domain_incidence"
        )

    def test_source_stencil_and_owned_end_are_separate_evidence(self):
        first, second = self.ways
        cases = (
            (
                replace(second, point_count=3, first_segment=first.first_segment),
                "interior_source_crossing",
            ),
            (replace(second, owns_ends=(False, True)), "unowned_source_end"),
            (replace(second, maximum_gradient_word=None), "missing_source_profile"),
            (
                replace(second, maximum_gradient_word=word(float("nan"))),
                "missing_source_profile",
            ),
            (
                replace(second, maximum_gradient_word=word(0.2)),
                "source_profile_word_mismatch",
            ),
        )
        for way, expected in cases:
            with self.subTest(expected=expected):
                self.assertEqual(self.reason(self.inspect(ways=(first, way))), expected)

    def test_profile_double_is_not_silently_used_for_float_word(self):
        span = replace(self.source.spans[1], maximum_gradient=0.12)
        source = replace(self.source, spans=(self.source.spans[0], span))
        self.assertEqual(
            self.reason(self.inspect(source=source)), "source_profile_word_mismatch"
        )

    def test_batch_indexes_source_once_and_reorders_groups_exactly(self):
        nodes, spans, ways, group = pair(1)
        source = replace(
            self.source,
            nodes=self.source.nodes + nodes,
            spans=self.source.spans + spans,
        )
        forward = self.inspect(
            (self.group, group), source=source, ways=self.ways + ways
        )
        backward = self.inspect(
            (group, self.group), source=source, ways=ways + self.ways
        )
        self.assertEqual(forward["groups"], backward["groups"][::-1])
        self.assertEqual(forward["work"], backward["work"])
        self.assertEqual(forward["work"]["indexed_spans"], 4)
        self.assertEqual(forward["work"]["indexed_nodes"], 8)
        self.assertEqual(forward["work"]["member_visits"], 4)
        self.assertTrue(
            all(row["reason"] == "missing_ground_profile" for row in forward["groups"])
        )

    def test_duplicate_retained_endpoint_across_groups_is_not_reused(self):
        second = replace(self.group, identifier="different/original/group")
        result = self.inspect((self.group, second))
        self.assertEqual(self.reason(result), "duplicate_retained_endpoint")
        self.assertFalse(any(result["work"].values()))

    def test_scopes_and_small_budgets_remain_pending(self):
        wrong = replace(self.source, binding={"scope": "wrong"})
        self.assertEqual(
            self.reason(self.inspect(source=wrong)),
            "source_or_candidate_binding_mismatch",
        )
        for budget, expected in (
            (PortBudget(incidence_visits=1), "candidate_work_budget"),
            (PortBudget(input_spans=1), "candidate_work_budget"),
            (PortBudget(input_nodes=3), "source_input_budget"),
        ):
            with self.subTest(expected=expected):
                result = self.inspect(budget=budget)
                self.assertEqual(self.reason(result), expected)
                self.assertFalse(any(result["work"].values()))


if __name__ == "__main__":
    unittest.main()
