"""One offline floor/port context for individual and joint stair comparisons."""

import time
from dataclasses import dataclass

import numpy as np
from indoor_floor_geometry import produce_floor_contacts
from indoor_floor_inputs import FloorInputs, load_inputs
from indoor_floor_plan import FloorLimits
from indoor_stair_contacts import bind_stairs


@dataclass(frozen=True)
class StairContext:
    inputs: FloorInputs
    requests: tuple
    floors: dict
    bindings: tuple
    original_responses: tuple
    source_cost: dict
    floor_cost: dict
    old_ready_count: int


def load_stair_context(root):
    cpu, wall = time.process_time(), time.perf_counter()
    inputs = load_inputs(root)
    source_cost = {
        "cpu_s": time.process_time() - cpu,
        "wall_s": time.perf_counter() - wall,
    }
    cpu, wall = time.process_time(), time.perf_counter()
    original = {r.identifier: r for r in inputs.requests}
    requests = tuple(
        original.get(r.identifier, r)
        for r in inputs.constraints
        if r.purpose == "flat_indoor_walkway"
    )
    floors = produce_floor_contacts(
        inputs.parts,
        requests,
        inputs.ground,
        method="adapted",
        constraints=inputs.constraints,
        limits=FloorLimits(max_requests=128),
    )
    floor_cost = {
        "cpu_s": time.process_time() - cpu,
        "wall_s": time.perf_counter() - wall,
    }
    old = produce_floor_contacts(
        inputs.parts,
        inputs.requests,
        inputs.ground,
        method="adapted",
        constraints=inputs.constraints,
    )
    choices = {r["source_id"]: r for r in floors["choices"]}
    old_ready = [r for r in old["choices"] if r["status"] == "ready"]
    for row in old_ready:
        np.testing.assert_array_equal(row["xyz"], choices[row["source_id"]]["xyz"])
    return StairContext(
        inputs,
        requests,
        floors,
        tuple(bind_stairs(inputs, floors)),
        tuple(old["choices"]),
        source_cost,
        floor_cost,
        len(old_ready),
    )
