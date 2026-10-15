"""Pinned actual terrain/field and explicit whole transport-object reservations."""

import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from curb_plan import select, source_masks
from outshine_ref.contracts import Mesh
from rail_graph_inputs import read_window
from retained_surface import surface_domain
from tilted_rail_inputs import FIELD_PIN, GROUND_PIN, PINS
from wien_network_sources import scene
from wien_support_experiment import BRIDGE_PIN
from wien_support_verify import unpack

HERE = Path(__file__).resolve().parent
BASE_REPORT = "4817ff034347ae8c86a670db7326a72afee5756bf38d042c75346eec39153539"
BASE_BODY = "1ea4b2ed2eb343772f2c803c78ee0da21d372755010b8b7289754ade26618f41"
PIERS = "00492bf969f5502931d2d0273ec01c7a82cfbc34dc63baac195551a23afa9ce7"


def inputs(network, baseline, supports, reference):
    for name, pin in [("ground.npz", GROUND_PIN), ("field.npz", FIELD_PIN)]:
        if digest(network / name) != pin:
            raise ValueError("unchanged actual curb terrain/field required")
    if (
        digest(baseline / "comparison.json") != BASE_REPORT
        or digest(baseline / "after-body.npz") != BASE_BODY
    ):
        raise ValueError(
            "unchanged actual complete material/hardware baseline required"
        )
    with np.load(network / "ground.npz") as d:
        ground = Mesh(d["vertices"], d["faces"])
        road = d["road"]
    with np.load(network / "field.npz") as d:
        field = Mesh(d["vertices"], d["faces"])
    with np.load(baseline / "after-body.npz") as d:
        before = {k: d[k] for k in d.files}
    _source, raw, graphs, _bed, binding = read_window(
        network, "ViennaNetwork", files=PINS
    )
    usage = json.loads(
        (network / "results/ViennaNetwork/components/source-usage.json").read_text()
    )
    used = {r["id"] for r in usage if r["status"] != "not_used"}
    fixture = HERE / "fixtures/bridge-continuations-vienna.json"
    if digest(fixture) != BRIDGE_PIN or digest(supports / "shifted.npz") != PIERS:
        raise ValueError(
            "unchanged original bridge sources and actual physical piers required"
        )
    old = json.loads(fixture.read_text())["sources"]
    _fixture, previous, model, _note = scene(reference)
    _bodies, contacts, _packed = unpack(supports / "shifted.npz")
    protected = shapely.union_all(
        [
            surface_domain(previous[0]),
            model["open_trench"],
            surface_domain(model["roof"]),
            surface_domain(model["walls"]),
            *[surface_domain(pair[0]) for pair in contacts],
        ]
    ).buffer(0.2)
    traffic = surface_domain(Mesh(ground.vertices, ground.triangles[road]))
    world = surface_domain(ground)
    eligible, owners, other, ports = source_masks(graphs, raw, used, old)
    candidates, excluded, work = select(
        traffic, world, eligible, owners, other, ports, protected
    )
    binding.update(
        origin_lat_lon=_source["origin_lat_lon"],
        selected_kind="ground",
        ground_sha256=GROUND_PIN,
        field_sha256=FIELD_PIN,
        baseline_report_sha256=BASE_REPORT,
        baseline_body_sha256=BASE_BODY,
        piers_sha256=PIERS,
        bridge_fixture_sha256=BRIDGE_PIN,
    )
    return ground, field, before, candidates, traffic, world, excluded, binding, work
