"""Pinned actual lower geometry, used edge ownership, and explicit old ground sources."""

import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from rail_graph_inputs import read_window
from tilted_rail_inputs import GROUND_PIN, PINS
from transport_surface_families import graph_records, old_ground_records
from wien_support_experiment import BRIDGE_PIN

HERE = Path(__file__).resolve().parent
BODY_PIN = "19c392460839a4a7aebaec570982190d2ea18fccc3bd5aedac792be690697a4b"
REPORT_PIN = "f97c5ab63888c1ac34153664e836b9b6c825aa54309f57ad27b1c5e67ef01963"


def inputs(network, baseline):
    if digest(network / "ground.npz") != GROUND_PIN:
        raise ValueError("pinned actual lower traffic surface required")
    source, raw, graphs, _bed, bindings = read_window(
        network, "ViennaNetwork", files=PINS
    )
    with np.load(network / "ground.npz") as d:
        ground = Mesh(d["vertices"], d["faces"])
        road = d["road"]
    usage = json.loads(
        (network / "results/ViennaNetwork/components/source-usage.json").read_text()
    )
    used = {r["id"] for r in usage if r["status"] != "not_used"}
    records = graph_records(graphs, raw, used)
    fixture = HERE / "fixtures/bridge-continuations-vienna.json"
    if digest(fixture) != BRIDGE_PIN:
        raise ValueError("original selected old lower source fixture required")
    records.extend(old_ground_records(json.loads(fixture.read_text())["sources"]))
    path = baseline / "after-body.npz"
    if digest(path) != BODY_PIN or digest(baseline / "comparison.json") != REPORT_PIN:
        raise ValueError("unchanged upper/lower hardware baseline required")
    with np.load(path) as d:
        arrays = {k: d[k] for k in d.files}
    positions, faces, slots = (
        arrays[k] for k in ["vertices", "triangles", "materials"]
    )
    selected = np.flatnonzero((slots == 0) | (slots == 5))
    if (
        len(selected) != len(ground.triangles)
        or not np.array_equal(
            positions[faces[selected]], ground.vertices[ground.triangles]
        )
        or not np.array_equal(slots[selected], np.where(road, 0, 5))
    ):
        raise ValueError("unique actual lower top faces differ from the pinned ground")
    bindings.update(
        ground_sha256=GROUND_PIN,
        baseline_body_sha256=BODY_PIN,
        baseline_report_sha256=REPORT_PIN,
        old_fixture_sha256=BRIDGE_PIN,
        origin_lat_lon=source["origin_lat_lon"],
    )
    return ground, road, selected[road], records, arrays, bindings
