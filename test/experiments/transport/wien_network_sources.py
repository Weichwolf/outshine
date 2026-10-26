"""Select new ground sources without overwriting previous level/contact decisions."""

import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from capture_tunnel_portals import FIXTURE, PIN, base_surfaces
from retained_surface import surface_domain
from transport_network import source_identity, source_roads, transport_widths
from tunnel_portal_experiment import source_usage
from tunnel_portal_recipe import build

INPUT_PIN = "aaea9506c3459045f5f6c2eb2df4f96b222e47e1acd88f377ec66e9f70126475"


def scene(reference):
    if digest(FIXTURE) != PIN:
        raise ValueError("pinned portal fixture changed")
    fixture = json.loads(FIXTURE.read_text())
    previous, origin = base_surfaces(
        reference / "bridge-continuations/Vienna-continuation-whole-rebuild-solid.npz"
    )
    if origin != fixture["base"]:
        raise ValueError("previous scene source changed")
    model, note = build(fixture, previous)
    if not note["valid"]:
        raise ValueError(str(note))
    return fixture, previous, model, note


def select(path, fixture, note, model, previous):
    if digest(path) != INPUT_PIN:
        raise ValueError("pinned complete Vienna capture changed")
    raw = json.loads(Path(path).read_text())
    if raw["origin_lat_lon"] != fixture["origin_lat_lon"]:
        raise ValueError("source and selected bridge frames differ")
    for receipt in raw["receipts"]:
        if digest(receipt["path"]) != receipt["sha256"]:
            raise ValueError("pinned source bytes changed")
    old = {r["source_id"]: r for r in source_usage(fixture, note)}
    inputs = [
        {
            "line": shapely.LineString(r["coordinates"]),
            "width": r["width_m"],
            "properties": r["properties"],
        }
        for r in raw["roads"]
    ]
    recipes = json.loads(
        Path(__file__).with_name("transport_network_recipes.json").read_text()
    )
    normalized, _normalization = source_roads(inputs, recipes, transport_widths())
    widths = {source_identity(r): r["width"] for r in normalized}
    upper = surface_domain(previous[0])
    # Curbs may fill concave corners by at most the declared two metre radius.
    # Reserve that construction reach rather than a whole tile/bounding box.
    construction_reach = 2.001 + recipes["flatPlan"]["endpointToleranceM"]
    trench = model["open_trench"].buffer(construction_reach)
    roads, reserved = [], []
    for row, source in zip(raw["roads"], inputs, strict=True):
        identity, properties = row["source_id"], row["properties"]
        decision = old[identity]
        reason = ""
        if decision["status"] == "used":
            reason = "existing_grade_separated_plan"
        elif decision["reason"] != "outside_connected_transport_field":
            reason = decision["reason"]
        else:
            line = source["line"]
            width = widths.get(source_identity(source))
            if width is not None and line.buffer(width / 2).intersects(trench):
                reason = "open_portal_air_reservation"
            elif properties.get("subclass") == "subway" and np.any(
                shapely.dwithin(
                    upper,
                    shapely.points(np.asarray(row["coordinates"])[[0, -1]]),
                    construction_reach,
                )
            ):
                reason = "upper_subway_port_profile_not_planned"
        if reason:
            reserved.append({"source_id": identity, "reason": reason})
        else:
            roads.append(row)
    return dict(raw, roads=roads, reserved_sources=reserved), old


def full_ledger(raw, old, planned):
    if len({r["id"] for r in planned}) != len(planned) or {
        r["id"] for r in planned
    } != {r["source_id"] for r in raw["roads"]}:
        raise ValueError("new component ledger must cover each selected source once")
    rows = dict(old)
    for record in raw["reserved_sources"]:
        identity = record["source_id"]
        if rows[identity]["status"] != "used":
            rows[identity] = {
                "source_id": identity,
                "status": "not_used",
                "reason": record["reason"],
            }
    for record in planned:
        identity = record["id"]
        if rows[identity]["status"] == "used":
            raise ValueError("new component overwrote a retained source")
        rows[identity] = {
            "source_id": identity,
            "status": record["status"],
            "reason": record["reason"],
        }
    original = {r["source_id"] for r in raw["original_axes"] + raw["deferred_sources"]}
    if set(rows) != original or any(
        not r["reason"] for r in rows.values() if r["status"] == "not_used"
    ):
        raise ValueError("complete unique source ledger required")
    return list(rows.values())
