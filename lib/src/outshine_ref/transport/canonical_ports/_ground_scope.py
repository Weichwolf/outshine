"""Explicit finite ground-domain qualification with bound typed outer exits.

The original source and logical Unknown remain unchanged. A qualified scope
permits its own geometric field, never a World join or external height claim.
"""

from collections.abc import Mapping
from types import MappingProxyType

from ...contracts import immutable
from ._end_candidates import CandidateGroup, CandidateWay, float_word
from ._source import component, source_chain, source_index
from ._types import PortBudget, PortSource


def inside_coverage(point, coverage):
    lon, lat = point
    return any(
        box["WestDeg"] < lon < box["EastDeg"]
        and box["SouthDeg"] < lat < box["NorthDeg"]
        for box in coverage
    )


def segment_covered(a, b, coverage):
    """Cover the complete native geographic segment, including rectangle seams."""
    delta = ((b[0] - a[0] + 180) % 360 - 180, b[1] - a[1])
    intervals = []
    for box in coverage:
        for offset in (-360, 0, 360):
            lo, hi = 0.0, 1.0
            for axis, lower, upper in (
                (0, box["WestDeg"] + offset, box["EastDeg"] + offset),
                (1, box["SouthDeg"], box["NorthDeg"]),
            ):
                if delta[axis] == 0:
                    if not lower <= a[axis] <= upper:
                        lo, hi = 1.0, 0.0
                        break
                else:
                    x, y = (
                        (lower - a[axis]) / delta[axis],
                        (upper - a[axis]) / delta[axis],
                    )
                    lo, hi = max(lo, min(x, y)), min(hi, max(x, y))
            if lo <= hi:
                intervals.append((lo, hi))
    covered = 0.0
    for lo, hi in sorted(intervals):
        if lo > covered:
            return False
        covered = max(covered, hi)
    return covered >= 1.0


def source_key(span):
    reference = span.attributes["native_source_reference"]
    local = span.attributes["native_path"]["LocalWayBinding"]
    return reference["Source"], tuple(local["FeatureContent"])


def bound_member(member, group, index, ways):
    nodes, spans, _incidence = index
    span = spans.get(member.edge[:2])
    if span is None or span.nodes[member.physical_end] != member.domain_node:
        return None, None, "ground_scope_member_domain_mismatch"
    if nodes[member.domain_node] != group.position:
        return None, None, "ground_scope_group_position_mismatch"
    alias = next(
        (
            row
            for row in span.attributes.get("native_directed_aliases", ())
            if tuple(row["Id"][k] for k in ("PathId", "SegmentOrdinal", "Direction"))
            == member.edge
        ),
        None,
    )
    if (
        alias is None
        or alias["FromNodeId" if member.end == 0 else "ToNodeId"] != member.domain_node
    ):
        return None, None, "ground_scope_directed_binding_mismatch"
    way = ways.get(member.source_way)
    local = span.attributes.get("native_path", {}).get("LocalWayBinding", {})
    step = span.attributes.get("native_source_reference", {}).get("SegmentOrdinal")
    if (
        way is None
        or local.get("StreetWay") != member.source_way
        or type(step) is not int
        or not 0 <= step < way.point_count - 1
    ):
        return None, None, "ground_scope_way_binding_mismatch"
    return span, way, ""


def group_nodes(groups, selected_nodes, work):
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
            if member.domain_node in selected_nodes:
                previous = by_node.setdefault(member.domain_node, group)
                if previous.identifier != group.identifier:
                    return None, "ambiguous_ground_scope_group"
            work["source_fact_member_visits"] += 1
    if by_node.keys() != selected_nodes:
        return None, "missing_ground_scope_original_group"
    return by_node, ""


def qualify_groups(index, selected, by_node, ways, omissions, work, remaining):
    _nodes, _spans, incidence = index
    selected_set, seen, exits, profiles = set(selected), set(), [], {}
    visited = set()
    for node, group in sorted(by_node.items()):
        if group.identifier in visited:
            continue
        visited.add(group.identifier)
        if (
            not group.complete
            or group.filtered_endpoint_count
            or group.original_endpoint_count != len(group.members)
        ):
            return None, "incomplete_ground_scope_original_incidence"
        if group.frontier:
            return None, "ground_scope_source_frontier"
        ground = [m for m in group.members if m.edge[:2] in selected_set]
        external = [m for m in group.members if m.edge[:2] not in selected_set]
        for member in group.members:
            if work["qualified_member_visits"] >= remaining:
                return None, "ground_scope_incidence_budget"
            span, way, reason = bound_member(member, group, index, ways)
            if reason:
                return None, reason
            if 5 in omissions.get(source_key(span), ()):
                return None, "ground_scope_missing_topology"
            if (
                member.source_terminal is True
                and not way.owns_ends[member.physical_end]
            ):
                return None, "ground_scope_unowned_source_end"
            work["qualified_member_visits"] += 1
            if member.edge[:2] in selected_set:
                seen.add((member.edge[:2], member.physical_end))
                if (
                    float_word(way.half_width_word) * 2 != span.width_m
                    or float_word(way.maximum_gradient_word) != span.maximum_gradient
                    or span.maximum_gradient is None
                ):
                    return None, "ground_scope_profile_word_mismatch"
                profiles[span.identifier] = (
                    way.half_width_word,
                    way.maximum_gradient_word,
                )
            else:
                if not (span.bridge or span.layer != 0) or (
                    span.tunnel and span.layer == 0
                ):
                    return None, "unknown_ground_scope_exit"
                if (
                    len(ground) != 1
                    or len(incidence[ground[0].domain_node]) != 1
                    or ground[0].source_terminal is not True
                    or member.source_terminal is not True
                ):
                    return None, "unqualified_ground_scope_exit_terminal"
                exits.append(
                    (
                        group.identifier,
                        ground[0].edge,
                        ground[0].end,
                        member.edge,
                        member.end,
                        member.domain_node,
                        span.bridge,
                        span.tunnel,
                        span.layer,
                        member.source_way,
                    )
                )
        if not external and len(ground) == 1 and ground[0].source_terminal is not True:
            return None, "unqualified_ground_scope_free_end"
    expected = {(edge, end) for edge in selected for end in (0, 1)}
    if seen != expected:
        return None, "incomplete_ground_scope_member_coverage"
    return (tuple(sorted(exits)), tuple(sorted(profiles.items()))), ""


def prepare_ground_field_scope(
    source, seed, groups, ways, *, source_binding, candidate_binding, budget=None
):
    """Prepare the existing ground component with an explicit original-fact proof.

    Returns (existing index, selected members, immutable binding, reason, work).
    No DEM/solver work occurs here. Unknown never changes or becomes an ignore flag.
    """
    budget = PortBudget() if budget is None else budget
    if not isinstance(source, PortSource) or not isinstance(budget, PortBudget):
        raise TypeError("existing source and budget required")
    work = {
        "indexed_nodes": 0,
        "indexed_spans": 0,
        "source_fact_member_visits": 0,
        "qualified_member_visits": 0,
    }

    def pending(reason):
        return None, (), None, reason, work

    if (
        not isinstance(groups, tuple)
        or not isinstance(ways, tuple)
        or any(not isinstance(g, CandidateGroup) for g in groups)
        or any(not isinstance(w, CandidateWay) for w in ways)
    ):
        raise TypeError("owned original group/Way fact tuples required")
    if (
        len(groups) > budget.input_nodes
        or len(ways) > budget.input_spans
        or sum(len(g.members) for g in groups) > budget.incidence_visits
    ):
        return pending("ground_scope_fact_budget")
    if (
        source_binding != source.binding
        or not isinstance(candidate_binding, Mapping)
        or not candidate_binding
        or candidate_binding.get("source_facts_digest")
        != source.binding.get("source_facts_digest")
        or not source.binding.get("source_facts_digest")
    ):
        return pending("ground_scope_binding_mismatch")
    index, reason = source_index(source, budget)
    if reason:
        return pending(reason)
    work.update(indexed_nodes=len(source.nodes), indexed_spans=len(source.spans))
    selected, closure, counted = component(
        index, seed, set(source.unknown_nodes), budget
    )
    work.update(counted)
    if closure not in ("", "unknown_source_frontier"):
        return pending(closure)
    _chain, reason = source_chain(index, selected)
    if reason:
        return pending(reason)
    nodes, spans, _incidence = index
    selected_nodes = {node for edge in selected for node in spans[edge].nodes}
    coverage = source.binding.get("coverage", ())
    if (
        not coverage
        or any(not inside_coverage(nodes[n], coverage) for n in selected_nodes)
        or any(
            not segment_covered(
                nodes[spans[e].nodes[0]], nodes[spans[e].nodes[1]], coverage
            )
            for e in selected
        )
    ):
        return pending("ground_scope_coverage_gap")
    omissions = {}
    for row in source.binding.get("omissions", ()):
        omissions.setdefault((row["Source"], tuple(row["Feature"])), set()).add(
            row["Reason"]
        )
    known_unknown = set()
    for edge in selected:
        if 4 in omissions.get(source_key(spans[edge]), ()):
            known_unknown.update(spans[edge].nodes)
    if not set(source.unknown_nodes).intersection(selected_nodes) <= known_unknown:
        return pending("unqualified_ground_scope_unknown")
    fact_visits = sum(len(group.members) for group in groups)
    if fact_visits > budget.incidence_visits - work["incidence_visits"]:
        return pending("ground_scope_incidence_budget")
    by_node, reason = group_nodes(groups, selected_nodes, work)
    if reason:
        return pending(reason)
    table = {w.source_way: w for w in ways}
    if len(table) != len(ways):
        return pending("duplicate_ground_scope_way")
    qualified, reason = qualify_groups(
        index,
        selected,
        by_node,
        table,
        omissions,
        work,
        budget.incidence_visits
        - work["incidence_visits"]
        - work["source_fact_member_visits"],
    )
    if reason:
        return pending(reason)
    exits, profiles = qualified
    binding = MappingProxyType(
        {
            "procedure": "outshine/finite-ground-domain/1",
            "source_binding": source.binding,
            "candidate_binding": immutable(candidate_binding),
            "members": selected,
            "profiles": profiles,
            "typed_exits": exits,
            "original_groups": tuple(
                sorted(g.identifier for g in set(by_node.values()))
            ),
            "height_policy": "local DEM/recipe field fixed for this scope; outer bridge/layer must adapt or stay Pending",
        }
    )
    return index, selected, binding, "", work
