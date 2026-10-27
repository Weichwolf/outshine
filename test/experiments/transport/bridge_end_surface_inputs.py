"""Existing captured Source and shared three-arm Ground producer for #1514."""

import json
import time

from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports._ground_recipe import THREE_ARM_RECIPE
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._ground_terminals import (
    derive_scoped_ground_terminals,
)
from replay_branched_ground import INPUT, SEED


def actual_end_inputs(method="auto"):
    started = time.perf_counter()
    source, groups, ways, binding, reader = load_bridge_source_facts(INPUT)
    decode_ms = 1000 * (time.perf_counter() - started)
    started = time.perf_counter()
    index, reason, work = prepare_ground_scope_index(
        source, groups, ways, source_binding=source.binding, candidate_binding=binding
    )
    assert not reason, reason
    index_ms = 1000 * (time.perf_counter() - started)
    manifest = json.loads((INPUT / "manifest.json").read_text())
    terrain, terrain_binding = terrain_input(
        FIXTURE, {"terrain": manifest["terrain_reference"]}
    )
    started = time.perf_counter()
    prepared, reason, report = derive_scoped_ground_terminals(
        source,
        SEED,
        groups,
        ways,
        terrain,
        source_binding=source.binding,
        candidate_binding=binding,
        terrain_binding=terrain_binding,
        maximum_prior_change_m=0.5,
        geometry_recipe=THREE_ARM_RECIPE,
        method=method,
        scope_index=index,
    )
    assert not reason, reason
    field_ms = 1000 * (time.perf_counter() - started)
    return (
        source,
        index,
        prepared,
        report,
        {
            "source_decode_ms": decode_ms,
            "index_ms": index_ms,
            "ground_field_and_caps_ms": field_ms,
            "reader_work": reader,
            "index_work": work,
        },
    )
