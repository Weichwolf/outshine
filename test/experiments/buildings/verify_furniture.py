"""Actual furnishing geometry, room containment, object heights and usable portal/activity paths."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
import shapely
from boolean_building_bands import construct
from building_vertical import level
from evidence import revision
from furniture_collision import furnishing_solid
from furniture_counterexamples import controls as actual_controls
from furniture_mesh import build as furnishings
from outshine_ref.buildings.interiors import Furnishing, access, layout, placed
from programme_verification import admission, instance_bounds
from room_programme import programme
from room_programme_plan import CONTROLS, source_plan
from structural_shell_geometry import NativeColumns, assembly, inspect_poses
from verify_structural_shell import access as building_access
from verify_vertical import landing_poses


def verify(plan, product, doors=()):
    programme_proof = admission(plan, product)
    mesh, description = furnishings(product)
    instance_bounds(plan, product, mesh, description)
    furniture = furnishing_solid(mesh)
    shell = construct(plan)
    _, collision = assembly(plan, shell, frames=True)
    combined = collision + furniture
    door_proof = []
    if doors:
        from door_state_verification import check

        combined, door_proof = check(
            combined, doors, plan["base_z_m"] - 1.0, plan["source_top_m"] + 1.0
        )
    building = building_access(plan, combined)
    columns = NativeColumns(
        combined, plan["base_z_m"] - 1.0, plan["source_top_m"] + 1.0
    )
    poses = 0
    minimum = float("inf")
    for room in product["rooms"]:
        current = level(plan, room["storey"])
        shape = current["rooms"][room["room_index"]]
        for door in doors:
            if door["room_id"] == room["room_id"]:
                shape = shape.difference(door["open"])
        expected = access(
            shape, room["portal_anchors"], room["result"]["items"], room["reserved"]
        )
        anchors = np.c_[
            room["portal_anchors"],
            np.full(len(room["portal_anchors"]), room["floor_z_m"]),
        ]
        minimum = min(minimum, inspect_poses(columns, anchors))
        poses += len(anchors)
        for path in expected["paths"]:
            sequence = landing_poses(np.asarray(path.coords), room["floor_z_m"])
            minimum = min(minimum, inspect_poses(columns, sequence))
            poses += len(sequence)
    result = {
        "status": product["status"],
        "programme": programme_proof,
        "instances": len(description["instances"]),
        "triangles": len(mesh["faces"]),
        "templates": description["templates"],
        "activity_path_poses": poses,
        "minimum_headroom_m": minimum,
        "building": building,
        "source_footprint_wkb": plan["footprint"].wkb_hex,
        "source_properties": plan["source_properties"],
        "source_height_interval_m": [plan["base_z_m"], plan["source_top_m"]],
    }
    if doors:
        result["door_states"] = door_proof
    return result


def controls():
    room = shapely.box(0, 0, 7, 5)
    spec = Furnishing("storage", 1.0, 0.4, 1.5, 0.8)
    result = layout(room, [(0.4, 2.5)], (spec, spec), "control")
    if result["status"] != "complete":
        raise AssertionError("Simple bounded furnishing control failed")
    checks = []
    for name, shape, anchors, items, reserve in (
        (
            "blocked-portal",
            room,
            [(0.4, 2.5)],
            [placed(spec, [0.0, 2.5], [0.0, -1.0], "block")],
            (),
        ),
        (
            "escaped-room",
            room,
            [(0.4, 2.5)],
            [placed(spec, [-0.4, 0.0], [1.0, 0.0], "escape")],
            (),
        ),
        (
            "blocked-activity",
            room,
            [(0.4, 2.5)],
            [
                placed(spec, [3.0, 1.0], [1.0, 0.0], "a"),
                placed(spec, [3.0, 1.5], [1.0, 0.0], "b"),
            ],
            (),
        ),
        (
            "reserved-approach",
            room,
            [(0.4, 2.5)],
            [placed(spec, [3.0, 1.0], [1.0, 0.0], "a")],
            (shapely.box(2.0, 0.5, 4.0, 2.0),),
        ),
    ):
        try:
            access(shape, anchors, items, reserve)
        except ValueError as error:
            checks.append({"case": name, "status": "rejected", "reason": str(error)})
        else:
            raise AssertionError("Invalid furnishing control passed: " + name)
    return checks


def run(output):
    output.mkdir(parents=True, exist_ok=True)
    records = []
    negatives = []
    for site, identifier in CONTROLS:
        _row, plan, _receipt = source_plan(site, identifier)
        for method in ("wall-anchors", "jittered-anchors"):
            began = time.perf_counter()
            product = programme(plan, method)
            proof = verify(plan, product)
            if site == "Hameln" and method == "jittered-anchors":
                negatives = actual_controls(plan, product)
            records.append(
                dict(
                    id=identifier,
                    method=method,
                    seconds=time.perf_counter() - began,
                    evidence=product["evidence"],
                    **proof,
                )
            )
            print(
                identifier,
                method,
                product["status"],
                proof["activity_path_poses"],
                "actual poses",
                flush=True,
            )
    report = revision() | {
        "status": "pass",
        "cases": records,
        "controls": controls() + negatives,
        "scope": "Actual bounded office work/meeting/tea/sanitary furnishing controls; services, ventilation, occupancy regulations, other programme families and native admission remain open",
    }
    (output / "verification.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/buildings/room-programme"),
    )
    run(parser.parse_args().output)
