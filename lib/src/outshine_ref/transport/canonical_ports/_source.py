"""Finite normalized incidence closure before terrain sampling or field fitting."""

from collections import defaultdict, deque


def source_index(source, budget):
    if len(source.nodes) > budget.input_nodes or len(source.spans) > budget.input_spans:
        return None, "source_input_budget"
    nodes = {node.identifier: node.longitude_latitude for node in source.nodes}
    spans = {span.identifier: span for span in source.spans}
    if len(nodes) != len(source.nodes) or len(spans) != len(source.spans):
        raise ValueError("duplicate normalized source node or physical span")
    if not set(source.unknown_nodes) <= nodes.keys():
        raise ValueError("unknown frontier must name an existing normalized node")
    incidence = defaultdict(list)
    for span in source.spans:
        if any(node not in nodes for node in span.nodes):
            raise ValueError("normalized span refers to a missing source node")
        for node in span.nodes:
            incidence[node].append(span.identifier)
    return (nodes, spans, incidence), ""


def component(index, seed, unknown, budget):
    _nodes, spans, incidence = index
    if seed not in spans:
        return (), "missing_requested_source", {"incidence_visits": 0}
    queue = deque([seed])
    queued, selected, seen = {seed}, [], set()
    reasons = set()
    work = {"node_lookups": 0, "incidence_visits": 0, "queue_peak": 1}
    while queue:
        if len(selected) >= budget.selected_spans:
            reasons.add("selected_span_budget")
            break
        identity = queue.popleft()
        selected.append(identity)
        for node in spans[identity].nodes:
            if node in seen:
                continue
            seen.add(node)
            work["node_lookups"] += 1
            if node in unknown:
                reasons.add("unknown_source_frontier")
            for adjacent in sorted(incidence[node]):
                if work["incidence_visits"] >= budget.incidence_visits:
                    reasons.add("incidence_visit_budget")
                    break
                work["incidence_visits"] += 1
                if adjacent not in queued:
                    queued.add(adjacent)
                    queue.append(adjacent)
            if "incidence_visit_budget" in reasons:
                break
        if "incidence_visit_budget" in reasons:
            break
        work["queue_peak"] = max(work["queue_peak"], len(queue))
    return tuple(sorted(selected)), ";".join(sorted(reasons)), work


def ground_chain(index, selected):
    nodes, spans, _incidence = index
    adjacency = defaultdict(list)
    widths, gradients = set(), set()
    for identity in selected:
        span = spans[identity]
        if span.bridge:
            return None, "bridge_anchor_recipe_required"
        if span.tunnel:
            return None, "tunnel_height_recipe_required"
        if span.layer != 0:
            return None, "layer_height_recipe_required"
        if span.maximum_gradient is None or span.width_m <= 0:
            return None, "missing_source_profile_recipe"
        widths.add(span.width_m)
        gradients.add(span.maximum_gradient)
        a, b = span.nodes
        adjacency[a].append(b)
        adjacency[b].append(a)
    ends = sorted(node for node, neighbors in adjacency.items() if len(neighbors) == 1)
    if len(ends) != 2 or any(len(v) > 2 for v in adjacency.values()):
        return None, "branched_or_closed_ground_field_recipe_required"
    if len(widths) != 1 or len(gradients) != 1:
        return None, "mixed_ground_profile_recipe_required"
    order, previous = [ends[0]], None
    while True:
        following = [n for n in adjacency[order[-1]] if n != previous]
        if not following:
            break
        previous = order[-1]
        order.append(following[0])
        if len(order) > len(selected) + 1:
            raise ValueError("normalized source chain failed to terminate")
    if len(order) != len(selected) + 1:
        return None, "ambiguous_parallel_source_spans"
    return (
        tuple(nodes[node] for node in order),
        tuple(order),
        next(iter(widths)),
        next(iter(gradients)),
    ), ""


def validate_cut(demand, span, nodes):
    a, b = (nodes[node] for node in span.nodes)
    longitude = (b[0] - a[0] + 180) % 360 - 180
    expected = (
        (a[0] + demand.fraction * longitude + 180) % 360 - 180,
        a[1] + demand.fraction * (b[1] - a[1]),
    )
    actual = demand.anchor_longitude_latitude
    errors = (((actual[0] - expected[0] + 180) % 360 - 180), actual[1] - expected[1])
    if max(map(abs, errors)) > 2e-12:
        return "source_clip_fraction_mismatch"
    level, axis, line, segment = demand.boundary
    cells = 2**level
    extent = (360, 180)
    limit = (180, 90)
    boundary = -limit[axis] + line * extent[axis] / cells
    error = actual[axis] - boundary
    if axis == 0:
        error = (error + 180) % 360 - 180
    if abs(error) > 2e-12:
        return "source_clip_boundary_mismatch"
    other = actual[1 - axis]
    lo = -limit[1 - axis] + segment * extent[1 - axis] / cells
    hi = lo + extent[1 - axis] / cells
    if not lo - 2e-12 <= other <= hi + 2e-12:
        return "source_clip_boundary_segment_mismatch"
    return ""
