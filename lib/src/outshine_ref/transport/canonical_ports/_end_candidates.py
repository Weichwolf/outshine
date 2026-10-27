"""Bound pre-domain facts select local end candidates without welding World nodes.

A retained candidate is not a metric contact. The current producer has no bound
qualified ground-field lease; even a selected pair explicitly stays Pending.
Native export spelling belongs to its adapter, not these Python value objects.
"""

import math
import struct
from collections.abc import Mapping
from dataclasses import dataclass

from ._source import source_index
from ._types import PortBudget, PortSource, edge_id, geographic, node_id


@dataclass(frozen=True, slots=True)
class CandidateMember:
    edge: tuple[int, int, int]
    end: int
    domain_node: int
    source_way: int

    def __post_init__(self):
        if not isinstance(self.edge, tuple) or len(self.edge) != 3:
            raise ValueError("actual directed native edge identity required")
        edge_id(self.edge[:2])
        if type(self.edge[2]) is not int or self.edge[2] not in (0, 1):
            raise ValueError("actual native forward/reverse direction required")
        if type(self.end) is not int or self.end not in (0, 1):
            raise ValueError("actual directed From/To end required")
        node_id(self.domain_node)
        if type(self.source_way) is not int or not 0 <= self.source_way < 2**32:
            raise ValueError("actual local StreetWay index required")

    @property
    def physical_end(self):
        return self.end ^ self.edge[2]


@dataclass(frozen=True, slots=True)
class CandidateWay:
    """Only exported terminal stencils and profile words from one bound Way."""

    source_way: int
    point_count: int
    first_segment: tuple[tuple[float, float], tuple[float, float]]
    last_segment: tuple[tuple[float, float], tuple[float, float]]
    owns_ends: tuple[bool, bool]
    half_width_word: int
    maximum_gradient_word: int | None

    def __post_init__(self):
        if type(self.source_way) is not int or not 0 <= self.source_way < 2**32:
            raise ValueError("actual local StreetWay index required")
        if type(self.point_count) is not int or not 2 <= self.point_count < 2**32:
            raise ValueError("actual source ring point count required")
        for segment in (self.first_segment, self.last_segment):
            if not isinstance(segment, tuple) or len(segment) != 2:
                raise ValueError("two actual bound source coordinates required")
            for coordinate in segment:
                geographic(coordinate)
        if (
            not isinstance(self.owns_ends, tuple)
            or len(self.owns_ends) != 2
            or any(type(value) is not bool for value in self.owns_ends)
        ):
            raise ValueError("actual original OwnsEnds required")
        for word in (self.half_width_word, self.maximum_gradient_word):
            if word is not None and (type(word) is not int or not 0 <= word < 2**32):
                raise ValueError("actual binary32 word or missing profile required")
        if self.half_width_word is None:
            raise ValueError("actual half-width word required")


@dataclass(frozen=True, slots=True)
class CandidateGroup:
    """One original Network-node group; never reconstructed by position."""

    identifier: str
    position: tuple[float, float]
    members: tuple[CandidateMember, ...]
    original_endpoint_count: int
    filtered_endpoint_count: int
    complete: bool
    frontier: bool

    def __post_init__(self):
        if not isinstance(self.identifier, str) or not self.identifier:
            raise ValueError("bound candidate group identity required")
        geographic(self.position)
        if not isinstance(self.members, tuple) or any(
            not isinstance(member, CandidateMember) for member in self.members
        ):
            raise ValueError("immutable actual member tuple required")
        for count in (self.original_endpoint_count, self.filtered_endpoint_count):
            if type(count) is not int or count < 0:
                raise ValueError(
                    "actual nonnegative original incidence counts required"
                )
        if type(self.complete) is not bool or type(self.frontier) is not bool:
            raise ValueError("explicit completeness/frontier evidence required")


def float_word(word):
    return None if word is None else struct.unpack("<f", struct.pack("<I", word))[0]


def group_guard(group):
    if (
        not group.complete
        or group.filtered_endpoint_count
        or group.original_endpoint_count != len(group.members)
    ):
        return "incomplete_original_incidence"
    if group.frontier:
        return "source_frontier"
    if len(group.members) != 2:
        return "ambiguous_incidence"
    physical = {(member.edge[:2], member.physical_end) for member in group.members}
    if len(physical) != 2 or len({member.edge[:2] for member in group.members}) != 2:
        return "duplicate_physical_member"
    return ""


def member_guard(member, group, way, index):
    nodes, spans, incidence = index
    span = spans.get(member.edge[:2])
    if span is None:
        return None, "missing_physical_source_edge"
    if span.nodes[member.physical_end] != member.domain_node:
        return None, "member_domain_binding_mismatch"
    if nodes[member.domain_node] != group.position:
        return None, "member_position_mismatch"
    if len(incidence[member.domain_node]) != 1:
        return None, "interior_domain_incidence"
    attributes = span.attributes
    aliases = attributes.get("native_directed_aliases", ())
    actual = next(
        (edge for edge in aliases if edge["Id"]["Direction"] == member.edge[2]), None
    )
    if (
        actual is None
        or actual["Id"]["PathId"] != member.edge[0]
        or actual["Id"]["SegmentOrdinal"] != member.edge[1]
    ):
        return None, "missing_directed_edge_binding"
    if actual["FromNodeId" if member.end == 0 else "ToNodeId"] != member.domain_node:
        return None, "directed_end_binding_mismatch"
    local = attributes.get("native_path", {}).get("LocalWayBinding", {})
    if way is None or local.get("StreetWay") != member.source_way:
        return None, "missing_bound_source_way"
    step = attributes.get("native_source_reference", {}).get("SegmentOrdinal")
    end = member.physical_end
    if type(step) is not int or not 0 <= step < way.point_count - 1:
        return None, "source_step_binding_mismatch"
    terminal = (end == 0 and step == 0 and group.position == way.first_segment[0]) or (
        end == 1
        and step == way.point_count - 2
        and group.position == way.last_segment[1]
    )
    if not terminal:
        return None, "interior_source_crossing"
    if not way.owns_ends[end]:
        return None, "unowned_source_end"
    width, gradient = (
        float_word(way.half_width_word),
        float_word(way.maximum_gradient_word),
    )
    if (
        width is None
        or gradient is None
        or not math.isfinite(width)
        or not math.isfinite(gradient)
        or width <= 0
        or not 0 < gradient <= 10
    ):
        return None, "missing_source_profile"
    if span.width_m != 2 * width or span.maximum_gradient != gradient:
        return None, "source_profile_word_mismatch"
    if span.tunnel:
        return None, "tunnel_attachment_recipe_required"
    return span, ""


def inspect_group(group, ways, index, work):
    reason = group_guard(group)
    if reason:
        return {"group": group.identifier, "reason": reason, "selected": ()}
    selected = []
    for member in group.members:
        work["member_visits"] += 1
        span, reason = member_guard(member, group, ways.get(member.source_way), index)
        if reason:
            return {"group": group.identifier, "reason": reason, "selected": ()}
        selected.append((member, span))
    bridge = [pair for pair in selected if pair[1].bridge]
    ground = [pair for pair in selected if not pair[1].bridge]
    reason = ""
    if len(bridge) != 1 or len(ground) != 1:
        reason = "bridge_ground_pair_required"
    elif bridge[0][0].domain_node == ground[0][0].domain_node:
        reason = "distinct_mvt_domain_binding_required"
    elif ground[0][1].layer != 0:
        reason = "ground_layer_recipe_required"
    elif bridge[0][1].width_m != ground[0][1].width_m:
        reason = "width_transition_recipe_required"
    if reason:
        return {"group": group.identifier, "reason": reason, "selected": ()}
    members = tuple(
        (member.edge[:2], member.physical_end, member.domain_node)
        for member, _span in bridge + ground
    )
    return {
        "group": group.identifier,
        "reason": "missing_ground_profile",
        "selected": members,
    }


def inspect_bridge_end_candidates(
    source, groups, ways, *, source_binding, candidate_binding, budget=None
):
    """One bounded source index for all groups; no metric/contact admission.

    The export adapter verifies candidate content/version binding before this
    call. SourceOrigin is an identity token, never a retained native-array owner.
    No adjacency pairs, proximity groups, World mutation or Unknown clearing.
    """
    if budget is None:
        budget = PortBudget()
    if not isinstance(source, PortSource) or not isinstance(budget, PortBudget):
        raise TypeError("existing bound PortSource/PortBudget required")
    if not isinstance(groups, tuple):
        raise TypeError("immutable retained candidate groups required")
    if not isinstance(ways, tuple):
        raise TypeError("immutable bound Way fact table required")
    work = {
        "indexed_nodes": 0,
        "indexed_spans": 0,
        "member_visits": 0,
        "expanded_pairs": 0,
    }

    def result(rows, reason=""):
        return {
            "state": "Pending",
            "reason": reason,
            "groups": rows,
            "work": work,
            "source_binding": source.binding,
            "candidate_binding": candidate_binding,
            "qualified_contacts": 0,
        }

    if len(groups) > budget.requests or len(ways) > budget.input_spans:
        return result((), "candidate_work_budget")
    if any(not isinstance(group, CandidateGroup) for group in groups):
        raise TypeError("immutable retained candidate groups required")
    if any(not isinstance(way, CandidateWay) for way in ways):
        raise TypeError("immutable bound Way fact table required")
    if sum(len(group.members) for group in groups) > budget.incidence_visits:
        return result((), "candidate_work_budget")
    reason = ""
    if (
        source_binding != source.binding
        or not isinstance(candidate_binding, Mapping)
        or not candidate_binding
    ):
        reason = "source_or_candidate_binding_mismatch"
    elif len({group.identifier for group in groups}) != len(groups):
        reason = "duplicate_candidate_group"
    if not reason:
        endpoints = set()
        for group in groups:
            for member in group.members:
                key = member.edge[:2], member.physical_end
                if key in endpoints:
                    reason = "duplicate_retained_endpoint"
                endpoints.add(key)
    guards = tuple(group_guard(group) for group in groups)
    index = None
    if not reason and any(not guard for guard in guards):
        if len(source.unknown_nodes) > budget.input_nodes:
            return result((), "source_input_budget")
        index, reason = source_index(source, budget)
        if not reason:
            work.update(
                indexed_nodes=len(source.nodes), indexed_spans=len(source.spans)
            )
    table = {}
    if not reason:
        table = {way.source_way: way for way in ways}
        if len(table) != len(ways):
            reason = "duplicate_bound_source_way"
    rows = tuple(
        {
            "group": group.identifier,
            "reason": reason or guard,
            "selected": (),
        }
        if reason or guard
        else inspect_group(group, table, index, work)
        for group, guard in zip(groups, guards, strict=True)
    )
    return result(rows, reason)
