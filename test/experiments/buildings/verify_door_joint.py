"""Check source-preserving room choice, pre-reserved motion and actual assembled states."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from building_vertical import level
from door_furnishing import joint
from door_mesh import expand, template
from door_support_plan import select
from evidence import revision
from office_support_plan import refine
from office_support_verification import admission
from room_programme import programme
from room_programme_plan import CONTROLS, source_plan
from verify_furniture import verify


def motion(plan, product, selection):
    rooms = {record["room_id"]: record for record in product["rooms"]}
    source_ids = {record["id"] for record in plan["support_refinement"]["spaces"]}
    selected = {row["room_id"] for row in selection["selected"]}
    missing = {row["room_id"] for row in selection["unresolved"]}
    if selected & missing or selected | missing != source_ids:
        raise ValueError("Door selection lost or duplicated a support-space outcome")
    count = 0
    for row in selection["selected"]:
        record = rooms[row["room_id"]]
        room = level(plan, row["storey"])["rooms"][record["room_index"]]
        if row["motion"].difference(room).area > 1e-8:
            raise ValueError("Door movement escapes its actual room")
        for item in record["result"]["items"]:
            for field in ("footprint", "activity"):
                if item[field].intersection(row["motion"]).area > 1e-9:
                    raise ValueError(
                        "Door movement occupies a required object or activity"
                    )
        fixed = template(row["leaf"])
        for angle in np.linspace(0.0, 90.0, 181):
            body = expand(row["leaf"], fixed, float(angle))
            projected = shapely.MultiPoint(body["vertices"][:, :2]).convex_hull
            if not row["motion"].covers(projected):
                raise ValueError("Actual leaf mesh leaves its reserved motion envelope")
            if projected.difference(room).area > 1e-8:
                raise ValueError("Actual moving leaf escapes its room")
            count += 1
    return count


def counterexamples(plan, product, selection):
    negatives = []
    try:
        motion(plan, product, selection | {"selected": selection["selected"][1:]})
    except ValueError as error:
        negatives.append({"case": "missing-door-outcome", "reason": str(error)})
    else:
        raise AssertionError("Missing support door was silently admitted")
    first = selection["selected"][0]
    records = list(product["rooms"])
    index = next(i for i, r in enumerate(records) if r["room_id"] == first["room_id"])
    items = list(records[index]["result"]["items"])
    items[0] = items[0] | {"activity": first["motion"]}
    records[index] = records[index] | {
        "result": records[index]["result"] | {"items": tuple(items)}
    }
    try:
        motion(plan, product | {"rooms": tuple(records)}, selection)
    except ValueError as error:
        negatives.append({"case": "occupied-motion-activity", "reason": str(error)})
    else:
        raise AssertionError("Occupied movement reservation was silently admitted")
    return negatives


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Joint door evidence needs a clean frozen implementation")
    records = []
    for site, identifier in CONTROLS:
        _row, original, receipt = source_plan(site, identifier)
        old = refine(original)
        old_product = programme(old, "jittered-anchors")
        old_selection = select(old, old_product)
        _fixed_product, fixed_selection = joint(old)
        plan = refine(original, door_aware=True)
        partition = admission(original, plan)
        product, selection = joint(plan)
        if selection["unresolved"] or len(selection["selected"]) != 2:
            raise AssertionError("Pinned full-size support functions remain unresolved")
        baseline = programme(plan, "jittered-anchors")
        support = {r["room_id"] for r in selection["selected"]}
        for before, after in zip(baseline["rooms"], product["rooms"], strict=True):
            if before["room_id"] in support:
                continue
            if before["room_id"] != after["room_id"]:
                raise AssertionError("Joint furnishing changed another room identity")
            for a, b in zip(
                before["result"]["items"], after["result"]["items"], strict=True
            ):
                np.testing.assert_array_equal(a["centre"], b["centre"])
        replay, repeated = joint(plan)
        for a, b in zip(product["rooms"], replay["rooms"], strict=True):
            for x, y in zip(a["result"]["items"], b["result"]["items"], strict=True):
                if x["id"] != y["id"]:
                    raise AssertionError("Repeated plan lost furniture identity")
                np.testing.assert_array_equal(x["centre"], y["centre"])
        if [r["leaf"] for r in selection["selected"]] != [
            r["leaf"] for r in repeated["selected"]
        ]:
            raise AssertionError("Door plans depend on previous evaluation")
        count = motion(plan, product, selection)
        records.append(
            {
                "site": site,
                "id": identifier,
                "receipt": receipt,
                "postcheck_doors": len(old_selection["selected"]),
                "reserve_fixed_rooms_doors": len(fixed_selection["selected"]),
                "door_aware_partition_doors": len(selection["selected"]),
                "partition": partition,
                "actual_mesh_poses": count,
                "counterexamples": counterexamples(plan, product, selection),
                "assembled": verify(plan, product, selection["selected"]),
            }
        )
        print(site, "joint source/mesh/state controls passed", flush=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(proof | {"cases": records}, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    run(parser.parse_args().output)
