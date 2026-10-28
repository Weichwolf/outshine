"""Whole bounded cyclic components from the unchanged actual source index."""

import hashlib
import json
import time

from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._source import component
from replay_branched_ground import INPUT


def prepare(*, minimum_cycles=2, maximum_cycles=2, maximum_spans=12):
    if (
        any(type(v) is not int for v in (minimum_cycles, maximum_cycles, maximum_spans))
        or not 2 <= minimum_cycles <= maximum_cycles <= 8
        or not 5 <= maximum_spans <= 32
    ):
        raise ValueError("Bounded cyclic source census required")
    started = time.process_time()
    source, groups, ways, binding, reader = load_bridge_source_facts(INPUT)
    budget = PortBudget()
    indexed, reason, work = prepare_ground_scope_index(
        source,
        groups,
        ways,
        source_binding=source.binding,
        candidate_binding=binding,
        budget=budget,
    )
    if reason:
        raise ValueError(reason)
    nodes, spans, _ = indexed.index
    seen, cases = set(), []
    for seed in sorted(spans):
        if seed in seen:
            continue
        selected, closure, closure_work = component(
            indexed.index, seed, indexed.unknown, budget
        )
        seen.update(selected)
        if closure not in ("", "unknown_source_frontier"):
            continue
        used = {node for edge in selected for node in spans[edge].nodes}
        if (
            not 5 <= len(selected) <= maximum_spans
            or not minimum_cycles <= len(selected) - len(used) + 1 <= maximum_cycles
            or any(
                spans[e].bridge or spans[e].tunnel or spans[e].layer != 0
                for e in selected
            )
        ):
            continue
        cases.append(
            {
                "key": str(min(used)),
                "seed": seed,
                "selected": selected,
                "source_node_ids": tuple(sorted(used)),
                "closure_reason": closure,
                "closure_work": closure_work,
                "original_unknown_nodes": tuple(
                    sorted(used & set(source.unknown_nodes))
                ),
            }
        )
    manifest_path = INPUT / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    terrain, terrain_binding = terrain_input(
        FIXTURE, {"terrain": manifest["terrain_reference"]}
    )
    metadata = {
        "source_binding": source.binding,
        "candidate_binding": binding,
        "terrain_binding": terrain_binding,
        "reader": reader,
        "manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        "source_index_work": work,
        "source_preparation_cpu_s": time.process_time() - started,
        "source_admission": "Pending: explicit unregistered two-cycle geometry only; original Unknown/MissingTopology and native fact omissions retained. No original-qualified end contacts or World joins."
        if minimum_cycles == maximum_cycles == 2
        else "Pending: explicit unregistered multi-loop geometry only; original Unknown/MissingTopology and native fact omissions retained. No original-qualified end contacts or World joins.",
        "qualified_world_joins": 0,
        "qualified_original_end_contacts": 0,
        "original_normalized_nodes": len(nodes),
        "original_normalized_spans": len(spans),
    }
    return indexed.index, cases, terrain, budget, metadata
