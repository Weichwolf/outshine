"""Two retained actual Ground inputs; no fixture or Source qualification changes."""

import json

from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground import prepare_ground
from outshine_ref.transport.canonical_ports._ground_recipe import THREE_ARM_RECIPE
from outshine_ref.transport.canonical_ports._ground_scope import (
    prepare_ground_field_scope,
)
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._ground_star_input import (
    prepare_star_ground,
)
from outshine_ref.transport.canonical_ports._source import source_chain
from replay_branched_ground import INPUT, SEED


def examples():
    source, groups, ways, binding, _ = load_bridge_source_facts(INPUT)
    prepared, reason, _ = prepare_ground_scope_index(
        source, groups, ways, source_binding=source.binding, candidate_binding=binding
    )
    assert not reason
    manifest = json.loads((INPUT / "manifest.json").read_text())
    terrain, terrain_binding = terrain_input(
        FIXTURE, {"terrain": manifest["terrain_reference"]}
    )
    budget = PortBudget()
    for name, seed, recipe in (
        ("chain", (8409897393650800292, 0), None),
        ("star", SEED, THREE_ARM_RECIPE),
    ):
        index, selected, _, reason, _ = prepare_ground_field_scope(
            source,
            seed,
            groups,
            ways,
            source_binding=source.binding,
            candidate_binding=binding,
            scope_index=prepared,
            geometry_recipe=recipe,
        )
        assert not reason
        if recipe is None:
            chain, reason = source_chain(index, selected)
            assert not reason
            anchor, inputs, _ = prepare_ground(chain, terrain, budget)
        else:
            anchor, inputs, _, _ = prepare_star_ground(index, selected, terrain, budget)
        yield (
            name,
            inputs,
            {
                "source_binding": source.binding,
                "candidate_binding": binding,
                "terrain_binding": terrain_binding,
                "seed": seed,
                "source_members": selected,
                "geometry_recipe": recipe,
                "anchor": anchor,
            },
        )
