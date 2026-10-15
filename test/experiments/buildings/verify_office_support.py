"""Actual source floors, support partition negatives and furnished 3D access after subdivision."""

import argparse
import json
from pathlib import Path

import shapely
from building_vertical import level
from evidence import revision
from office_support_plan import refine
from office_support_verification import admission
from room_programme import programme
from room_programme_plan import CONTROLS, source_plan
from verify_furniture import verify


def counterexamples(original, plan):
    record = plan["support_refinement"]["spaces"][0]
    floor, index = record["storey"], record["room_index"]
    current = level(plan, floor)
    bad_ids = [list(ids) for ids in plan["storey_room_ids"]]
    bad_ids[floor][-1] = bad_ids[floor][0]
    missing_door = current | {
        "doors": [d for d in current["doors"] if d["id"] != record["door_id"]]
    }
    escaped = list(current["rooms"])
    escaped[index] = escaped[index].union(shapely.box(10000, 10000, 10001, 10001))
    stale_window = current | {"windows": [dict(w) for w in current["windows"]]}
    stale_window["windows"][0]["rooms"] = [index]
    changes = (
        ("duplicate-room-id", plan | {"storey_room_ids": bad_ids}),
        (
            "removed-support-door",
            plan
            | {
                "storey_plans": [
                    missing_door if i == floor else level(plan, i)
                    for i in range(plan["storeys"])
                ]
            },
        ),
        (
            "escaped-parent-floor",
            plan
            | {
                "storey_plans": [
                    current | {"rooms": escaped} if i == floor else level(plan, i)
                    for i in range(plan["storeys"])
                ]
            },
        ),
        (
            "sanitary-clear-window",
            plan
            | {
                "storey_plans": [
                    stale_window if i == floor else level(plan, i)
                    for i in range(plan["storeys"])
                ]
            },
        ),
    )
    negative = []
    for name, changed in changes:
        try:
            admission(original, changed)
        except ValueError as error:
            negative.append({"name": name, "reason": str(error)})
        else:
            raise AssertionError("Support admission accepted " + name)
    if plan["storeys"] > 1:
        storeys = list(plan["storey_plans"])
        upper = level(plan, 1)
        rooms = list(upper["rooms"])
        rooms[2] = rooms[2].buffer(-0.1)
        storeys[1] = upper | {"rooms": rooms}
        try:
            admission(original, plan | {"storey_plans": storeys})
        except ValueError as error:
            negative.append({"name": "unsupported-office-floor", "reason": str(error)})
        else:
            raise AssertionError(
                "Support admission accepted an unsupported floor change"
            )
    return negative


def run(output):
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    for site, identifier in CONTROLS:
        _source, original, receipt = source_plan(site, identifier)
        fingerprints = [
            [r.wkb_hex for r in level(original, i)["rooms"]]
            for i in range(original["storeys"])
        ]
        for method in ("wall-ends", "sliding-wall"):
            plan = refine(original, method)
            proof = admission(original, plan)
            product = programme(plan, "jittered-anchors")
            support_ids = {r["id"] for r in plan["support_refinement"]["spaces"]}
            furnished_support = [
                r for r in product["rooms"] if r["room_id"] in support_ids
            ]
            if len(furnished_support) != len(support_ids) or any(
                r["result"]["status"] != "complete" for r in furnished_support
            ):
                raise ValueError(
                    "Added support rooms lack their required actual fixtures"
                )
            mesh_proof = verify(plan, product)
            negative = counterexamples(original, plan)
            if fingerprints != [
                [r.wkb_hex for r in level(original, i)["rooms"]]
                for i in range(original["storeys"])
            ]:
                raise ValueError("Support refinement mutated the original input plan")
            result = {
                "id": identifier,
                "site": site,
                "receipt": receipt,
                "method": method,
                "refinement": plan["support_refinement"],
                "plan_proof": proof,
                "furniture_mesh_proof": mesh_proof,
                "negative_controls": negative,
                "programme_status": product["status"],
                "unresolved": product["unresolved"],
                "rooms": [
                    {
                        "id": r["room_id"],
                        "kind": r["kind"],
                        "area_m2": r["area_m2"],
                        "status": r["result"]["status"],
                        "placement_method": r["placement_method"],
                        "missing": r["result"]["missing"],
                    }
                    for r in product["rooms"]
                ],
            }
            rows.append(result)
            print(
                site,
                method,
                product["status"],
                len(negative),
                "negative controls",
                flush=True,
            )
    (output / "office-support-verification.json").write_text(
        json.dumps(revision() | {"status": "pass", "cases": rows}, indent=2) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/buildings/office-space-plan"),
    )
    args = parser.parse_args()
    run(args.output)
