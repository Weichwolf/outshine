"""Bounded proposals from explicit original nodes, never geometric proximity."""

import heapq
import math
from collections import defaultdict
from numbers import Integral


def node_key(value):
    if len(value) != 2 or any(
        not isinstance(v, Integral) or isinstance(v, bool) for v in value
    ):
        raise ValueError("explicit two-integer original node keys required")
    return tuple(int(v) for v in value)


def checked(records, roots):
    if not 0 <= len(records) <= 20000 or not 0 <= len(roots) <= 64:
        raise ValueError("bounded source and root lists required")
    if sum(len(r["nodes"]) for r in records) > 200000:
        raise ValueError("original node occurrence budget exhausted")
    result = {}
    for r in records:
        key, nodes, length = r["source_id"], r["nodes"], r["length_m"]
        if not isinstance(key, str) or not key or key in result:
            raise ValueError("unique nonempty source identities required")
        if not 2 <= len(nodes) <= 4096 or type(r["eligible"]) is not bool:
            raise ValueError("bounded original axis and explicit eligibility required")
        if not math.isfinite(length) or length < 0 or not isinstance(r["reason"], str):
            raise ValueError("finite source priority and explicit reason required")
        result[key] = {**r, "nodes": tuple(node_key(n) for n in nodes)}
    if len({r["root_id"] for r in roots}) != len(roots):
        raise ValueError("unique explicit root identities required")
    return result


def source_index(records):
    index = defaultdict(list)
    for key in sorted(records):
        row = records[key]
        if not row["eligible"]:
            continue
        endpoints = {row["nodes"][0], row["nodes"][-1]}
        for node in sorted(set(row["nodes"])):
            index[node].append((key, node in endpoints))
    return index


def proposal(records, root, index, max_sources, max_visits, method):
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

    expand(node_key(root["node"]), False, None)
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


def discover(
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
    rows = checked(records, roots)
    ordered = sorted(roots, key=lambda r: r["root_id"])
    index = source_index(rows) if method == "index" else {}
    proposals = [
        proposal(rows, r, index, max_sources, max_visits, method)
        for r in ordered[:max_proposals]
    ]
    proposals.sort(key=lambda p: (-p["length_m"], -len(p["source_ids"]), p["root_id"]))
    return {
        "proposals": proposals,
        "deferred_roots": [r["root_id"] for r in ordered[max_proposals:]],
        "index_nodes": len(index),
        "index_entries": sum(len(v) for v in index.values()),
        "source_count": len(rows),
        "input_node_occurrences": sum(len(r["nodes"]) for r in rows.values()),
        "scope": "Alternative bounded source-module proposals from explicit original nodes; caller owns frame, eligibility, actual old profile and whole geometry admission. No globally optimal or complete network claim.",
    }
