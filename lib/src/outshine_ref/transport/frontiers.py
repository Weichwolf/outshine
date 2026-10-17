"""Bounded proposals from explicit original nodes, never geometric proximity."""

import heapq
import math
from collections import defaultdict
from numbers import Integral, Real


def _node_key(value):
    if len(value) != 2 or any(
        not isinstance(v, Integral) or isinstance(v, bool) or not -(2**63) <= v < 2**63
        for v in value
    ):
        raise ValueError("explicit two-integer original node keys required")
    return tuple(int(v) for v in value)


def _checked(records, roots):
    if not 0 <= len(records) <= 20000 or not 0 <= len(roots) <= 64:
        raise ValueError("bounded source and root lists required")
    if sum(len(r["nodes"]) for r in records) > 200000:
        raise ValueError("original node occurrence budget exhausted")
    result = {}
    for r in records:
        key, nodes, length = r["source_id"], r["nodes"], r["length_m"]
        if not isinstance(key, str) or not 1 <= len(key) <= 512 or key in result:
            raise ValueError("unique nonempty source identities required")
        if not 2 <= len(nodes) <= 4096 or type(r["eligible"]) is not bool:
            raise ValueError("bounded original axis and explicit eligibility required")
        if (
            not isinstance(length, Real)
            or isinstance(length, bool)
            or not math.isfinite(length)
            or not 0 <= length <= 1e9
            or not isinstance(r["reason"], str)
            or len(r["reason"]) > 2048
            or bool(r["reason"]) == r["eligible"]
        ):
            raise ValueError("finite source priority and explicit reason required")
        result[key] = {**r, "nodes": tuple(_node_key(n) for n in nodes)}
    if any(
        not isinstance(r["root_id"], str) or not 1 <= len(r["root_id"]) <= 1024
        for r in roots
    ):
        raise ValueError("bounded nonempty root identities required")
    if len({r["root_id"] for r in roots}) != len(roots):
        raise ValueError("unique explicit root identities required")
    return result


def _source_index(records):
    index = defaultdict(list)
    for key in sorted(records):
        row = records[key]
        if not row["eligible"]:
            continue
        endpoints = {row["nodes"][0], row["nodes"][-1]}
        for node in sorted(set(row["nodes"])):
            index[node].append((key, node in endpoints))
    return index


def _proposal(records, root, index, max_sources, max_visits, method):
    queue, queued, selected = [], set(), []
    work = {"node_lookups": 0, "incidence_tests": 0, "heap_pushes": 0}
    parents = {}
    limited = False

    def expand(node, endpoint, parent):
        nonlocal limited
        work["node_lookups"] += 1
        if method == "index":
            candidates = index.get(node, ())
        else:
            candidates = []
            for key, row in records.items():
                if not row["eligible"]:
                    continue
                for position, candidate in enumerate(row["nodes"]):
                    if work["incidence_tests"] >= max_visits:
                        limited = True
                        return
                    work["incidence_tests"] += 1
                    if node == candidate:
                        candidates.append((key, position in (0, len(row["nodes"]) - 1)))
        for key, target_end in candidates:
            if method == "index":
                if work["incidence_tests"] >= max_visits:
                    limited = True
                    return
                work["incidence_tests"] += 1
            if key in queued or not (endpoint or target_end):
                continue
            if parent is None and not target_end:
                continue
            heapq.heappush(queue, (-records[key]["length_m"], key))
            queued.add(key)
            parents[key] = {"source_id": parent, "node": node}
            work["heap_pushes"] += 1

    expand(_node_key(root["node"]), False, None)
    while queue and not limited and len(selected) < max_sources:
        _priority, key = heapq.heappop(queue)
        selected.append(key)
        if len(selected) == max_sources:
            break
        row = records[key]
        endpoints = {row["nodes"][0], row["nodes"][-1]}
        for node in sorted(set(row["nodes"])):
            expand(node, node in endpoints, key)
            if limited:
                break
    return {
        "root_id": root["root_id"],
        "source_ids": selected,
        "connections": {key: parents[key] for key in selected},
        "length_m": sum(records[k]["length_m"] for k in selected),
        "pending_frontier_sources": len(queue),
        "work": work,
        "stop_reason": "frontier_work_budget"
        if limited
        else "module_source_budget"
        if len(selected) == max_sources
        else "frontier_exhausted",
    }


def discover_source_modules(
    records,
    roots,
    *,
    max_sources=8,
    max_proposals=8,
    max_visits=2000000,
    method="index",
):
    if (
        type(max_sources) is not int
        or not 1 <= max_sources <= 8
        or type(max_proposals) is not int
        or not 1 <= max_proposals <= 8
        or type(max_visits) is not int
        or not 1 <= max_visits <= 2000000
        or method not in ("index", "scan")
    ):
        raise ValueError("known bounded source-frontier search required")
    rows = _checked(records, roots)
    ordered = sorted(roots, key=lambda r: r["root_id"])
    index = _source_index(rows) if method == "index" else {}
    proposals = [
        _proposal(rows, r, index, max_sources, max_visits, method)
        for r in ordered[:max_proposals]
    ]
    proposals.sort(key=lambda p: (-p["length_m"], -len(p["source_ids"]), p["root_id"]))
    proposed = {key for p in proposals for key in p["source_ids"]}
    return {
        "proposals": proposals,
        "deferred_roots": [r["root_id"] for r in ordered[max_proposals:]],
        "index_nodes": len(index),
        "index_entries": sum(len(v) for v in index.values()),
        "source_count": len(rows),
        "input_node_occurrences": sum(len(r["nodes"]) for r in rows.values()),
        "source_reasons": {
            key: rows[key]["reason"]
            or (
                "proposed_pending_geometry"
                if key in proposed
                else "outside_bounded_proposals"
            )
            for key in sorted(rows)
        },
        "scope": "Alternative bounded source-module proposals from explicit original nodes; caller owns frame, eligibility, actual old profile and whole geometry admission. No globally optimal or complete network claim.",
    }


def connected_prefixes(proposal):
    """Return whole-source prefixes of a rooted discovery order, halving to one.

    This validates a parent-order witness, not actual node/level/geometry truth.
    At most four alternatives result; the caller's total admission budget still
    applies. A failed larger joint recipe does not prove any source impossible.
    """
    keys, connections = proposal["source_ids"], proposal["connections"]
    if (
        not 0 <= len(keys) <= 8
        or any(not isinstance(k, str) or not 1 <= len(k) <= 512 for k in keys)
        or len(set(keys)) != len(keys)
        or set(connections) != set(keys)
    ):
        raise ValueError("bounded unique original-source discovery order required")
    seen, root = set(), None
    for key in keys:
        row = connections[key]
        if set(row) != {"source_id", "node"}:
            raise ValueError("explicit original-node parent witness required")
        node, parent = _node_key(row["node"]), row["source_id"]
        if parent is None:
            if root is not None and node != root:
                raise ValueError("all initial branches must meet the same root")
            root = node
        elif not isinstance(parent, str) or parent not in seen:
            raise ValueError("every new source must follow its connected parent")
        seen.add(key)
    result, count = [], len(keys)
    while count:
        result.append(tuple(keys[:count]))
        count //= 2
    return tuple(result)
