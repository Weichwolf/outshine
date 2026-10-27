"""Owned, read-only reuse of the existing source and original-fact indexes."""

import time
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

from ...contracts import immutable
from ._end_candidates import CandidateGroup, CandidateWay
from ._source import source_index
from ._types import PortBudget, PortSource


@dataclass(frozen=True, slots=True)
class GroundScopeIndex:
    source: PortSource
    groups: tuple
    ways: tuple
    candidate_binding: Mapping
    index: tuple
    unknown: frozenset
    groups_by_node: Mapping
    ways_by_id: Mapping
    omissions: Mapping
    member_count: int


def input_guard(source, groups, ways, source_binding, candidate_binding, budget):
    if not isinstance(source, PortSource) or not isinstance(budget, PortBudget):
        raise TypeError("existing source and budget required")
    if (
        not isinstance(groups, tuple)
        or not isinstance(ways, tuple)
        or any(not isinstance(g, CandidateGroup) for g in groups)
        or any(not isinstance(w, CandidateWay) for w in ways)
    ):
        raise TypeError("owned original group/Way fact tuples required")
    count = sum(len(g.members) for g in groups)
    reason = bound_input_guard(
        source, groups, ways, count, source_binding, candidate_binding, budget
    )
    return reason, count


def bound_input_guard(source, groups, ways, count, source_binding, binding, budget):
    if (
        len(groups) > budget.input_nodes
        or len(ways) > budget.input_spans
        or count > budget.incidence_visits
    ):
        return "ground_scope_fact_budget"
    if (
        source_binding != source.binding
        or not isinstance(binding, Mapping)
        or not binding
        or binding.get("source_facts_digest")
        != source.binding.get("source_facts_digest")
        or not source.binding.get("source_facts_digest")
    ):
        return "ground_scope_binding_mismatch"
    if len(source.nodes) > budget.input_nodes or len(source.spans) > budget.input_spans:
        return "source_input_budget"
    return ""


def source_omissions(source):
    result = {}
    for row in source.binding.get("omissions", ()):
        result.setdefault((row["Source"], tuple(row["Feature"])), set()).add(
            row["Reason"]
        )
    return result


def group_nodes(groups, selected_nodes, work):
    """Index original groups; None preserves a locally ambiguous node in a batch."""
    by_node, physical, identifiers = {}, set(), set()
    for group in groups:
        if group.identifier in identifiers:
            return None, "duplicate_ground_scope_group"
        identifiers.add(group.identifier)
        for member in group.members:
            key = member.edge[:2], member.physical_end
            if key in physical:
                return None, "duplicate_ground_scope_member"
            physical.add(key)
            if selected_nodes is None or member.domain_node in selected_nodes:
                previous = by_node.setdefault(member.domain_node, group)
                if previous is None or previous.identifier != group.identifier:
                    if selected_nodes is not None:
                        return None, "ambiguous_ground_scope_group"
                    by_node[member.domain_node] = None
            work["source_fact_member_visits"] += 1
    if selected_nodes is not None and by_node.keys() != selected_nodes:
        return None, "missing_ground_scope_original_group"
    return by_node, ""


def prepare_ground_scope_index(
    source, groups, ways, *, source_binding, candidate_binding, budget=None
):
    """Build existing indexes once; do not qualify a field or query terrain.

    Ownership binds the exact immutable input objects. Later requests retain the
    original snapshot-size and incidence admission limits, even with smaller caps.
    """
    budget = PortBudget() if budget is None else budget
    started = time.monotonic()
    work = {"indexed_nodes": 0, "indexed_spans": 0, "source_fact_member_visits": 0}
    reason, count = input_guard(
        source, groups, ways, source_binding, candidate_binding, budget
    )
    if reason:
        return None, reason, work
    if budget.surface.time_limit_s == 0:
        return None, "ground_scope_time_budget", work
    index, reason = source_index(source, budget)
    if reason:
        return None, reason, work
    work.update(indexed_nodes=len(source.nodes), indexed_spans=len(source.spans))
    by_node, reason = group_nodes(groups, None, work)
    if reason:
        return None, reason, work
    by_way = {way.source_way: way for way in ways}
    if len(by_way) != len(ways):
        return None, "duplicate_ground_scope_way", work
    omissions = {k: frozenset(v) for k, v in source_omissions(source).items()}
    nodes, spans, incidence = index
    incidence = {node: tuple(edges) for node, edges in incidence.items()}
    owned = GroundScopeIndex(
        source,
        groups,
        ways,
        immutable(candidate_binding),
        tuple(MappingProxyType(v) for v in (nodes, spans, incidence)),
        frozenset(source.unknown_nodes),
        MappingProxyType(by_node),
        MappingProxyType(by_way),
        MappingProxyType(omissions),
        count,
    )
    if time.monotonic() - started >= budget.surface.time_limit_s:
        return None, "ground_scope_time_budget", work
    return owned, "", work


def indexed_input_guard(
    prepared, source, groups, ways, source_binding, binding, budget
):
    if not isinstance(prepared, GroundScopeIndex):
        raise TypeError("owned GroundScopeIndex required")
    if (
        source is not prepared.source
        or groups is not prepared.groups
        or ways is not prepared.ways
    ):
        return "ground_scope_index_binding_mismatch"
    reason = bound_input_guard(
        source, groups, ways, prepared.member_count, source_binding, binding, budget
    )
    if reason:
        return reason
    if binding != prepared.candidate_binding:
        return "ground_scope_index_binding_mismatch"
    return ""
