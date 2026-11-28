"""Explicit finite ground-domain qualification with bound typed outer exits.

The original source and logical Unknown remain unchanged. A qualified scope
permits its own geometric field, never a World join or external height claim.
"""

from types import MappingProxyType

from ...contracts import immutable
from ._end_candidates import float_word
from ._ground_recipe import (
    NETWORK_RECIPE,
    geometry_parameters,
    known_geometry_recipe,
    source_network,
    source_star,
)
from ._ground_scope_index import (
    group_nodes,
    indexed_input_guard,
    input_guard,
    source_omissions,
)
from ._source import component, source_chain, source_index
from ._types import PortBudget, PortSource


def inside_coverage(point, coverage):
    """Interior of the rectangle union on wrapped longitude, including cell seams.

    Each of the four open local quadrants needs a rectangle extending into it.
    For a finite axis-aligned union this is equivalent to a covered neighborhood;
    merely belonging to one or more closed rectangles would admit outer edges.
    """
    lon, lat = point
    quadrants = 0
    for box in coverage:
        south = box["SouthDeg"] < lat <= box["NorthDeg"]
        north = box["SouthDeg"] <= lat < box["NorthDeg"]
        for offset in (-360, 0, 360):
            west, east = box["WestDeg"] + offset, box["EastDeg"] + offset
            left = west < lon <= east
            right = west <= lon < east
            quadrants |= (
                int(left and south)
                | (int(left and north) << 1)
                | (int(right and south) << 2)
                | (int(right and north) << 3)
            )
            if quadrants == 15:
                return True
    return False


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
    source,
    seed,
    groups,
    ways,
    *,
    source_binding,
    candidate_binding,
    budget=None,
    scope_index=None,
    geometry_recipe=None,
):
    """Prepare the existing ground component with an explicit original-fact proof.

    Returns (existing index, selected members, immutable binding, reason, work).
    No DEM/solver work occurs here. Unknown never changes or becomes an ignore flag.
    An explicit known geometry_recipe admits only its complete bounded topology;
    None keeps the original chain path. Unknown recipes never select a fallback.
    Optional scope_index owns the exact same input objects and avoids repeated
    global scans. Its construction work is separate; original admission caps hold.
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

    if not known_geometry_recipe(geometry_recipe):
        return pending("unsupported_ground_geometry_recipe")
    if geometry_recipe is not None and (
        budget.requests < 3 or budget.surface.max_queries < 3
    ):
        return pending("ground_terminal_query_budget")
    if scope_index is None:
        reason, fact_visits = input_guard(
            source, groups, ways, source_binding, candidate_binding, budget
        )
        if reason:
            return pending(reason)
        index, reason = source_index(source, budget)
        work.update(indexed_nodes=len(source.nodes), indexed_spans=len(source.spans))
        unknown = set(source.unknown_nodes)
    else:
        reason = indexed_input_guard(
            scope_index, source, groups, ways, source_binding, candidate_binding, budget
        )
        index, unknown, fact_visits = (
            scope_index.index,
            scope_index.unknown,
            scope_index.member_count,
        )
    if reason:
        return pending(reason)
    selected, closure, counted = component(index, seed, unknown, budget)
    work.update(counted)
    if closure not in ("", "unknown_source_frontier"):
        return pending(closure)
    shape, reason = (
        source_chain(index, selected)
        if geometry_recipe is None
        else (
            source_network(index, selected)
            if geometry_recipe == NETWORK_RECIPE
            else source_star(index, selected)
        )
    )
    if reason:
        return pending(reason)
    if geometry_recipe == NETWORK_RECIPE and min(
        budget.requests, budget.surface.max_queries
    ) < len(shape[1]):
        return pending("ground_terminal_query_budget")
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
    omissions = (
        source_omissions(source) if scope_index is None else scope_index.omissions
    )
    known_unknown = set()
    for edge in selected:
        if 4 in omissions.get(source_key(spans[edge]), ()):
            known_unknown.update(spans[edge].nodes)
    if not unknown.intersection(selected_nodes) <= known_unknown:
        return pending("unqualified_ground_scope_unknown")
    if fact_visits > budget.incidence_visits - work["incidence_visits"]:
        return pending("ground_scope_incidence_budget")
    if scope_index is None:
        by_node, reason = group_nodes(groups, selected_nodes, work)
        table = {w.source_way: w for w in ways}
    else:
        by_node = {
            n: scope_index.groups_by_node[n]
            for n in selected_nodes
            if n in scope_index.groups_by_node
        }
        work["original_group_lookups"] = len(selected_nodes)
        if any(group is None for group in by_node.values()):
            reason = "ambiguous_ground_scope_group"
        else:
            reason = (
                ""
                if by_node.keys() == selected_nodes
                else "missing_ground_scope_original_group"
            )
        table = scope_index.ways_by_id
    if reason:
        return pending(reason)
    if len(table) != len(ways):
        return pending("duplicate_ground_scope_way")
    qualified, reason = qualify_groups(
        index,
        selected,
        by_node,
        table,
        omissions,
        work,
        budget.incidence_visits - work["incidence_visits"] - fact_visits,
    )
    if reason:
        return pending(reason)
    exits, profiles = qualified
    binding = MappingProxyType(
        {
            "procedure": "outshine/finite-ground-domain/1",
            "source_binding": source.binding,
            "candidate_binding": immutable(candidate_binding)
            if scope_index is None
            else scope_index.candidate_binding,
            "members": selected,
            "profiles": profiles,
            "typed_exits": exits,
            "original_groups": tuple(
                sorted(g.identifier for g in set(by_node.values()))
            ),
            "height_policy": "local DEM/recipe field fixed for this scope; outer bridge/layer must adapt or stay Pending",
            **(
                {"geometry_recipe": geometry_parameters(geometry_recipe)}
                if geometry_recipe is not None
                else {}
            ),
        }
    )
    return index, selected, binding, "", work
